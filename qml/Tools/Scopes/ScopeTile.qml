import QtQuick
import FFgui.Theme

// One scope in the Scopes panel. Its renderer fills `content`.
Rectangle {
    id: tile

    required property string kind
    required property string title
    property alias content: holder.data

    color: Theme.well
    border.color: Theme.card
    radius: Metrics.radiusMedium

    Text { x: 8; y: 5; text: tile.title; color: Theme.textDim; font.pixelSize: Metrics.fontSmall }
    Item {
        id: holder
        anchors { fill: parent; margins: 8; topMargin: 22 }
    }
}
