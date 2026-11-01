import QtQuick
import FFgui.Backend
import FFgui.Controls

// The ways to add an input: file, folder (batch), URL, a recording or a
// generated signal. Used by the Sources "+" and the header bar.
FfMenu {
    FfMenuItem { text: qsTr("File…"); onTriggered: App.requestDialog("addFiles") }
    FfMenuItem { text: qsTr("Folder (batch)…"); onTriggered: App.requestDialog("addFolder") }
    FfMenuItem { text: qsTr("URL…"); onTriggered: App.requestDialog("addUrl") }
    FfMenuSeparator { }
    FfMenuItem { text: qsTr("Record from device"); onTriggered: App.requestPanel("record") }
    FfMenuItem { text: qsTr("Generate signal"); onTriggered: App.requestPanel("generate") }
}
