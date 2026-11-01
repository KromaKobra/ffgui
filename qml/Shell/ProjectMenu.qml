import QtQuick
import FFgui.Backend
import FFgui.Controls

// New, open and save the project (a .ffgui recipe file).
FfMenu {
    FfMenuItem { text: qsTr("New project"); onTriggered: App.project.newProject() }
    FfMenuItem { text: qsTr("Open…") + "\tCtrl+O"; onTriggered: App.requestDialog("openProject") }
    FfMenuSeparator { }
    FfMenuItem {
        text: qsTr("Save") + "\tCtrl+S"
        onTriggered: App.project.fileUrl.toString() !== "" ? App.project.save() : App.requestDialog("saveProject")
    }
    FfMenuItem { text: qsTr("Save as…") + "\tCtrl+Shift+S"; onTriggered: App.requestDialog("saveProject") }
}
