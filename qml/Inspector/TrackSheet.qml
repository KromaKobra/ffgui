import QtQuick
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// A track: codec, layout diagram, sample format, counts, dispositions and
// side data (ReplayGain and the like).
ColumnLayout {
    id: sheet

    property int index: -1
    property int trackIndex: -1
    readonly property var track: (row.values.tracks || [])[sheet.trackIndex] || ({})
    readonly property var disposition: sheet.track.disposition || {}
    readonly property var sideData: sheet.track.sideData || []

    spacing: Metrics.spacingLarge

    ModelRow {
        id: row
        model: App.project.inputs
        row: sheet.index
    }

    Item { implicitHeight: 4 }

    Section {
        title: qsTr("Codec")
        Prop { key: "codec_name"; value: sheet.track.codec || ""; mono: true }
        Prop { key: "codec_long_name"; value: sheet.track.codecLong || "" }
        Prop { key: "profile"; value: sheet.track.profile || "—" }
        Prop { key: "sample_fmt"; value: sheet.track.sampleFmt || ""; mono: true }
        Prop { key: "sample_rate"; value: sheet.track.rate ? sheet.track.rate + " Hz" : "—" }
        Prop { key: "bits_per_sample"; value: sheet.track.depth ? String(sheet.track.depth) : qsTr("— (not stored)") }
        Prop { key: "bit_rate"; value: sheet.track.bitrate ? Format.bitrate(sheet.track.bitrate) : "—" }
        Prop { key: "nb_frames"; value: sheet.track.frames ? Number(sheet.track.frames).toLocaleString(Qt.locale(), "f", 0) : "—" }
    }

    Section {
        title: qsTr("Channel layout: %1").arg(sheet.track.layout || "?")
        ChannelLayoutDiagram {
            Layout.fillWidth: true
            Layout.margins: Metrics.spacing
            names: sheet.track.channelNames || []
        }
    }

    Section {
        title: qsTr("Disposition and side data")
        Prop { key: "default"; value: sheet.track.isDefault ? qsTr("yes") : qsTr("no") }
        Prop { key: "language"; value: sheet.track.lang || "und" }
        Prop { key: "title"; value: sheet.track.title || "—" }
        Repeater {
            model: Object.keys(sheet.disposition).filter(k => k !== "default" && sheet.disposition[k])
            delegate: Prop {
                required property string modelData
                key: modelData
                value: qsTr("yes")
            }
        }
        Prop {
            key: qsTr("side data")
            value: sheet.sideData.length ? sheet.sideData.join("\n") : qsTr("none")
        }
    }
}
