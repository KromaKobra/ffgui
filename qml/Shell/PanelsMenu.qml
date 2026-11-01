pragma ComponentBehavior: Bound
import QtQuick
import FFgui.Controls
import "PanelRegistry.js" as Registry

// Every panel with a check mark; core regions are listed but can't close.
// Ends with the workspace reset actions.
FfMenu {
    id: panelsMenu

    required property DockShell dock
    required property WorkspaceController workspaces

    width: 280

    Instantiator {
        model: Registry.panels
        delegate: FfMenuItem {
            id: entry
            required property var modelData
            readonly property var panel: panelsMenu.dock.panel(entry.modelData.id)
            text: entry.modelData.title + (entry.modelData.core ? "   " + qsTr("(always shown)") : "")
            checkable: true
            checked: !!entry.panel && entry.panel.isOpen
            enabled: !!entry.panel && !entry.modelData.core
            onTriggered: {
                if (entry.panel.isOpen)
                    entry.panel.close();
                else
                    entry.panel.open();
                entry.checked = Qt.binding(() => !!entry.panel && entry.panel.isOpen);
            }
        }
        onObjectAdded: (index, object) => panelsMenu.insertItem(index, object)
        onObjectRemoved: (index, object) => panelsMenu.removeItem(object)
    }
    FfMenuSeparator { }
    FfMenuItem {
        text: qsTr("Reset “%1” workspace").arg(panelsMenu.workspaces.current)
        onTriggered: panelsMenu.workspaces.reset()
    }
    FfMenuItem { text: qsTr("Reset all workspaces"); onTriggered: panelsMenu.workspaces.resetAll() }
}
