import QtQuick
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// Render, with a split menu: render all, the selection only, or add to the queue.
RowLayout {
    Layout.fillWidth: false
    spacing: 0

    FfButton {
        objectName: visible ? "renderAll" : ""
        text: qsTr("Render")
        highlighted: true
        font.bold: true
        enabled: !App.render.active && App.render.blocker === ""
        tip: App.render.blocker !== "" ? App.render.blocker : qsTr("Render all outputs (Ctrl+Enter)")
        onClicked: App.render.start("all")
    }
    MenuButton {
        text: Glyphs.open
        above: true
        menu: FfMenu {
            FfMenuItem {
                text: qsTr("Render all")
                enabled: App.render.blocker === ""
                onTriggered: App.render.start("all")
            }
            FfMenuItem {
                text: qsTr("Render selection only")
                enabled: App.render.blocker === "" && App.selection.isRange
                onTriggered: App.render.start("selection")
            }
            FfMenuItem {
                text: qsTr("Add to queue")
                enabled: App.render.blocker === ""
                onTriggered: App.render.enqueue()
            }
        }
    }
}
