pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// An input: the full probe report in collapsible sections.
ColumnLayout {
    id: sheet

    property int index: -1
    readonly property var input: row.values
    readonly property var tags: sheet.input.tags || {}
    readonly property var tracks: sheet.input.tracks || []
    readonly property var chapters: sheet.input.chapters || []

    spacing: Metrics.spacingLarge

    ModelRow {
        id: row
        model: App.project.inputs
        row: sheet.index
    }

    Item { implicitHeight: 4 }

    Section {
        title: qsTr("Format")
        Prop { key: "filename"; value: sheet.input.path || sheet.input.name || ""; mono: true }
        Prop { key: "format_name"; value: sheet.input.formatName || ""; mono: true }
        Prop { key: "format_long_name"; value: sheet.input.formatLong || "" }
        Prop {
            key: "duration"
            value: Format.timecode(sheet.input.duration || 0, true) + "  (" + Format.number(sheet.input.duration || 0, 3) + " s)"
        }
        Prop { key: "size"; value: sheet.input.size > 0 ? Format.bytes(sheet.input.size) : "—" }
        Prop { key: "bit_rate"; value: sheet.input.bitrate > 0 ? Format.bitrate(sheet.input.bitrate) : "—" }
        Prop { key: "nb_streams"; value: String(sheet.tracks.length) }
        Prop { key: "nb_chapters"; value: String(sheet.chapters.length) }
    }

    Section {
        title: qsTr("Health")
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font.pixelSize: Metrics.fontCaption
            text: sheet.input.healthText || qsTr("Not checked yet")
            color: Theme.health(sheet.input.health)
        }
    }

    Section {
        title: qsTr("Tags (%1)").arg(Object.keys(sheet.tags).length)
        Repeater {
            model: Object.keys(sheet.tags)
            delegate: Prop {
                required property string modelData
                key: modelData
                value: String(sheet.tags[modelData])
            }
        }
    }

    Section {
        title: qsTr("Streams")
        Repeater {
            model: sheet.tracks
            delegate: Prop {
                required property int index
                required property var modelData
                key: "#" + index + " " + qsTr("audio")
                value: [modelData.codec, modelData.rate ? modelData.rate + " Hz" : "", modelData.layout, modelData.sampleFmt]
                       .filter(s => !!s).join(", ")
            }
        }
    }

    Section {
        visible: sheet.chapters.length > 0
        title: qsTr("Chapters")
        expanded: false
        Repeater {
            model: sheet.chapters
            delegate: Prop {
                required property var modelData
                key: Format.timecode(modelData.start)
                value: modelData.title || ""
            }
        }
    }

    ButtonRow {
        FfButton {
            text: qsTr("Copy as JSON")
            onClicked: App.copyText(JSON.stringify(sheet.input, null, 2))
        }
        FfButton {
            text: qsTr("Edit tags…")
            onClicked: App.requestPanel("metadata")
        }
    }
}
