pragma ComponentBehavior: Bound
import QtQuick

// A menu of on/off items built from `items` ([{ key, label, enabled }]).
// `isChecked(key)` decides each check mark; `toggled` reports clicks. Items
// declared inside the menu come after the generated ones.
FfMenu {
    id: listMenu

    property var items: []
    property var isChecked: function(key) { return false; }

    signal toggled(string key, bool on)

    Instantiator {
        model: listMenu.items
        delegate: FfMenuItem {
            id: entry
            required property var modelData
            text: modelData.label
            checkable: true
            checked: listMenu.isChecked(modelData.key)
            enabled: modelData.enabled !== false
            onTriggered: {
                listMenu.toggled(entry.modelData.key, entry.checked);
                entry.checked = Qt.binding(() => listMenu.isChecked(entry.modelData.key));
            }
        }
        onObjectAdded: (index, object) => listMenu.insertItem(index, object)
        onObjectRemoved: (index, object) => listMenu.removeItem(object)
    }
}
