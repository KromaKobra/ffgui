pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// Presets: output presets (applied to the current output target) and chain
// presets (replace the chain), built-in and user-saved.
Item {
    id: panel

    component Heading: Label {
        font.pixelSize: Metrics.fontSmall
        font.bold: true
        opacity: 0.5
        topPadding: Metrics.margin
        leftPadding: Metrics.margin
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: scroll.availableWidth
                spacing: 0

                Heading { text: qsTr("OUTPUT") }
                Repeater {
                    model: App.catalog.presets
                    delegate: FfItemDelegate {
                        required property string name
                        required property string codecLabel
                        required property string description
                        required property bool builtIn
                        Layout.fillWidth: true
                        text: name + "   ·   " + codecLabel + (builtIn ? "" : "   " + qsTr("(yours)"))
                        highlighted: App.project.outputs.value(App.project.currentOutput, "preset") === name
                        tip: description
                        onClicked: {
                            if (App.project.outputs.count === 0)
                                App.project.addOutput(name);
                            else
                                App.project.applyPreset(App.project.currentOutput, name);
                        }
                    }
                }
                Heading { text: qsTr("CHAIN") }
                Repeater {
                    model: App.catalog.chainPresets
                    delegate: FfItemDelegate {
                        required property string name
                        required property string description
                        Layout.fillWidth: true
                        text: name
                        tip: description
                        onClicked: App.project.applyChainPreset(name)
                    }
                }
            }
        }

        ButtonRow {
            FfButton { text: qsTr("Save current…"); font.pixelSize: Metrics.fontCaption; onClicked: App.requestDialog("savePreset") }
            FfButton { text: qsTr("Import…"); font.pixelSize: Metrics.fontCaption; onClicked: App.requestDialog("importPresets") }
            FfButton { text: qsTr("Export…"); font.pixelSize: Metrics.fontCaption; onClicked: App.requestDialog("exportPresets") }
        }
    }
}
