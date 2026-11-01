import QtQuick
import FFgui.Theme

// A clickable row with hover and selected states.
Rectangle {
    id: row

    property bool selected: false
    readonly property bool hovered: hover.hovered

    signal clicked()
    signal contextMenu(point position)

    implicitHeight: Metrics.rowHeight
    radius: Metrics.radius
    color: row.selected ? Theme.selected : hover.hovered ? Theme.hover : Theme.row

    HoverHandler { id: hover }
    TapHandler { onTapped: row.clicked() }
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: point => row.contextMenu(point.position)
    }
}
