pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// An output target: file naming, signal format, encoder and container
// options, metadata policy.
ColumnLayout {
    id: sheet

    property int index: -1
    readonly property var target: row.values
    readonly property var codec: App.catalog.codec(sheet.target.codec || "")
    readonly property var container: App.catalog.container(sheet.target.container || "")
    readonly property var encoderOptions: App.catalog.encoderOptions(sheet.target.codec || "")
    readonly property var muxerOptions: App.catalog.muxerOptions(sheet.container.muxer || "")

    function set(changes) { App.project.setOutput(sheet.index, changes); }

    spacing: Metrics.spacingLarge

    ModelRow {
        id: row
        model: App.project.outputs
        row: sheet.index
    }

    Item { implicitHeight: 4 }

    Section {
        title: qsTr("File")
        FormRow {
            label: qsTr("Name")
            labelWidth: 60
            FfTextField {
                Layout.fillWidth: true
                text: sheet.target.pattern || ""
                tip: qsTr("{name} is the input's file name without its extension")
                onEditingFinished: if (text !== sheet.target.pattern) sheet.set({ pattern: text })
            }
            Caption { text: "." + (sheet.container.extension || "") }
        }
        FormRow {
            label: qsTr("Folder")
            labelWidth: 60
            Label {
                text: sheet.target.folder || qsTr("Next to the input")
                font.pixelSize: Metrics.fontCaption
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }
            FfButton {
                text: qsTr("Choose…")
                onClicked: App.requestDialog("outputFolder", { index: sheet.index })
            }
        }
        Prop { key: qsTr("Estimated size"); value: sheet.target.sizeText || "—" }
    }

    Section {
        title: qsTr("Signal format")
        Repeater {
            model: [
                { label: qsTr("Sample rate"), key: "rate",
                  choices: [[qsTr("Same as source"), 0], ["44.1 kHz", 44100], ["48 kHz", 48000], ["96 kHz", 96000]] },
                { label: qsTr("Bit depth"), key: "depth",
                  choices: [[qsTr("Same as source"), 0], ["16-bit", 16], ["24-bit", 24], ["32-bit float", 32]] },
                { label: qsTr("Channels"), key: "channels",
                  choices: [[qsTr("Same as source"), 0], [qsTr("Mono"), 1], [qsTr("Stereo"), 2]] }
            ]
            delegate: FormRow {
                id: signalRow
                required property var modelData
                label: modelData.label
                FfComboBox {
                    Layout.fillWidth: true
                    enabled: signalRow.modelData.key !== "depth" || !sheet.codec.lossy
                    model: signalRow.modelData.choices.map(c => c[0])
                    currentIndex: signalRow.modelData.choices.findIndex(c => c[1] === sheet.target[signalRow.modelData.key])
                    onActivated: index => {
                        const change = {};
                        change[signalRow.modelData.key] = signalRow.modelData.choices[index][1];
                        sheet.set(change);
                    }
                }
            }
        }
    }

    Section {
        title: qsTr("Encoder: %1").arg(sheet.codec.label || sheet.target.codec || "")
        Prop { key: "codec"; value: sheet.target.codec || ""; mono: true }
        Prop { key: qsTr("bitrate"); value: sheet.target.kbps > 0 ? "≈ " + Math.round(sheet.target.kbps) + " kb/s" : "—" }
        Prop {
            key: qsTr("stream copy")
            value: sheet.target.reencodeReason ? qsTr("no: %1").arg(sheet.target.reencodeReason) : qsTr("yes")
        }
    }

    Section {
        title: qsTr("Encoder options (%1)").arg(sheet.encoderOptions.length)
        expanded: false
        Repeater {
            model: sheet.encoderOptions
            delegate: Prop {
                required property var modelData
                key: modelData.key
                value: String(modelData["default"] ?? "")
                mono: true
            }
        }
        Hint { text: qsTr("Generated from ffmpeg -h encoder=%1").arg(sheet.target.codec || "") }
    }

    Section {
        title: qsTr("Container options (%1)").arg(sheet.muxerOptions.length)
        expanded: false
        Repeater {
            model: sheet.muxerOptions
            delegate: Prop {
                required property var modelData
                key: modelData.key
                value: String(modelData["default"] ?? "")
                mono: true
            }
        }
        Hint { text: qsTr("Generated from ffmpeg -h muxer=%1").arg(sheet.container.muxer || "") }
    }

    Section {
        title: qsTr("Metadata")
        RowLayout {
            Repeater {
                model: [["copy", qsTr("Copy")], ["edit", qsTr("Edit")], ["strip", qsTr("Strip")]]
                delegate: FfRadioButton {
                    required property var modelData
                    text: modelData[1]
                    checked: sheet.target.metadata === modelData[0]
                    onClicked: sheet.set({ metadata: modelData[0] })
                }
            }
        }
        Hint {
            text: sheet.target.metadata === "copy" ? qsTr("Tags, chapters and cover art are copied from the input.")
                  : sheet.target.metadata === "strip" ? qsTr("All metadata is removed.")
                  : qsTr("Tags are set from the Metadata panel.")
        }
    }
}
