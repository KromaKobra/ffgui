pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// 1 · Sources: one card per input with its probe summary.
Item {
    id: sources

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        PanelToolbar {
            Caption {
                text: qsTr("%n inputs", "", App.project.inputs.count)
                Layout.fillWidth: true
            }
            MenuButton {
                text: Glyphs.add
                font.pixelSize: Metrics.fontBrand
                tip: qsTr("Add input (or drop files anywhere in the window)")
                menu: AddInputMenu { }
            }
        }

        EmptyState {
            visible: App.project.inputs.count === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            glyph: Glyphs.file
            title: qsTr("No inputs yet")
            text: qsTr("Drop audio files anywhere in the window, or add a file, a folder, a URL, a recording or a generated signal.")
            actionText: qsTr("Add files…")
            onAction: App.requestDialog("addFiles")
        }

        ListView {
            id: list
            visible: App.project.inputs.count > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Metrics.spacing
            topMargin: Metrics.spacingLarge
            bottomMargin: Metrics.spacingLarge
            leftMargin: Metrics.spacingLarge
            rightMargin: Metrics.spacingLarge
            model: App.project.inputs
            ScrollBar.vertical: ScrollBar { }
            delegate: InputCard {
                width: list.width - list.leftMargin - list.rightMargin
            }
            footer: Hint {
                width: list.width - list.leftMargin - list.rightMargin
                topPadding: Metrics.spacing
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Drag a track into the Chain or onto an output to route it")
            }
        }
    }
}
