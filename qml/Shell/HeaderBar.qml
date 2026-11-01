pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Sources
import FFgui.Theme

// The fixed header: project name, Add input, the workspace switcher, the
// command palette's search field, undo/redo and the Panels menu.
ToolBar {
    id: bar

    required property DockShell dock
    required property WorkspaceController workspaces

    signal paletteRequested(string text)

    implicitHeight: Metrics.headerHeight

    background: Rectangle {
        color: Theme.chrome
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Metrics.spacingWide
        anchors.rightMargin: Metrics.spacing
        spacing: Metrics.spacing

        Label {
            text: "ffgui"
            font.bold: true
            font.pixelSize: Metrics.fontBrand
            color: Theme.accent
        }
        MenuButton {
            visible: bar.width >= 1200
            text: App.project.name + (App.project.modified ? " •" : "")
            Layout.maximumWidth: 240
            tip: qsTr("New, open and save the project")
            menu: ProjectMenu { }
        }

        ToolSeparator { }

        MenuButton {
            text: Glyphs.add + " " + qsTr("Add input")
            menu: AddInputMenu { }
        }

        ToolSeparator { }

        // Workspace switcher. Ctrl+1 … Ctrl+8.
        Row {
            spacing: 1
            Repeater {
                model: bar.workspaces.names
                delegate: FfToolButton {
                    id: tab
                    required property int index
                    required property string modelData
                    text: modelData.replace("&", "&&")         // a single & would be a mnemonic
                    checked: bar.workspaces.current === modelData
                    tip: bar.workspaces.descriptions[modelData] + "   (Ctrl+" + (index + 1) + ")"
                    onClicked: bar.workspaces.switchTo(modelData)
                    Shortcut {
                        sequence: "Ctrl+" + (tab.index + 1)
                        context: Qt.ApplicationShortcut
                        onActivated: bar.workspaces.switchTo(tab.modelData)
                    }
                }
            }
        }

        Item { Layout.fillWidth: true }

        FfTextField {
            visible: bar.width >= 1320
            placeholderText: qsTr("Search features, panels, filters…   Ctrl+K")
            Layout.preferredWidth: 270
            Layout.minimumWidth: 120
            // Typing here opens the palette with what was typed.
            onTextEdited: {
                bar.paletteRequested(text);
                text = "";
            }
            onActiveFocusChanged: if (activeFocus) bar.paletteRequested("")
        }

        FfToolButton {
            text: Glyphs.undo
            enabled: App.undo.canUndo
            tip: App.undo.canUndo ? qsTr("Undo %1 (Ctrl+Z)").arg(App.undo.undoText) : qsTr("Undo")
            onClicked: App.undo.undo()
        }
        FfToolButton {
            text: Glyphs.redo
            enabled: App.undo.canRedo
            tip: App.undo.canRedo ? qsTr("Redo %1 (Ctrl+Shift+Z)").arg(App.undo.redoText) : qsTr("Redo")
            onClicked: App.undo.redo()
        }

        FfToolButton { text: Glyphs.more; tip: qsTr("FFmpeg tool settings"); onClicked: App.requestDialog("settings") }

        MenuButton {
            text: qsTr("Panels") + " " + Glyphs.open
            alignRight: true
            menu: PanelsMenu { dock: bar.dock; workspaces: bar.workspaces }
        }
    }
}
