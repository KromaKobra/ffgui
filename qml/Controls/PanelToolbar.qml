import QtQuick
import QtQuick.Layouts
import FFgui.Theme

// The header row at the top of a panel.
Item {
    id: bar

    default property alias content: row.data
    property alias spacing: row.spacing

    Layout.fillWidth: true
    implicitHeight: Metrics.panelHeaderHeight
    implicitWidth: row.implicitWidth + 14

    RowLayout {
        id: row
        anchors { fill: parent; leftMargin: Metrics.margin; rightMargin: Metrics.spacingSmall }
        spacing: Metrics.spacing
    }
}
