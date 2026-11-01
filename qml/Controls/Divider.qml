import QtQuick
import QtQuick.Layouts
import FFgui.Theme

// A one-pixel line across a layout.
Rectangle {
    Layout.fillWidth: true
    implicitHeight: 1
    implicitWidth: 1
    color: Theme.divider
}
