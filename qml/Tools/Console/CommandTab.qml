import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// The full command, multi-line and editable for experts. Edits are one-way:
// run the text as it is, or copy it. They are not read back into the Chain.
ColumnLayout {
    id: tab

    property bool edited: false

    spacing: Metrics.spacingSmall

    Connections {
        target: App.command
        function onChanged() { if (!tab.edited) editor.text = App.command.multiline; }
    }

    ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        FfTextArea {
            id: editor
            text: App.command.multiline
            placeholderText: App.command.placeholder
            mono: true
            font.pixelSize: Metrics.fontCaption
            color: Theme.command
            wrapMode: TextEdit.NoWrap
            onTextChanged: tab.edited = activeFocus && editor.text !== App.command.multiline
        }
    }
    ButtonRow {
        Layout.margins: Metrics.spacing
        FfButton {
            text: qsTr("Run as-is")
            enabled: editor.text.trim() !== "" && !App.console.active
            tip: qsTr("Runs exactly this text with ffmpeg; output goes to the Log tab")
            onClicked: App.runCommand(editor.text)
        }
        FfButton { text: qsTr("Stop"); visible: App.console.active; onClicked: App.console.stop() }
        Caption { visible: App.console.error !== ""; text: App.console.error }
        FfButton {
            text: qsTr("Copy")
            enabled: editor.text.trim() !== ""
            onClicked: App.copyText(editor.text)
        }
        FfButton {
            text: qsTr("Revert")
            visible: tab.edited
            onClicked: {
                tab.edited = false;
                editor.text = App.command.multiline;
            }
        }
        Item { Layout.fillWidth: true }
        Caption {
            text: tab.edited ? qsTr("Edited: the chain doesn't change") : qsTr("Always matches the project")
        }
    }
}
