import QtQuick
import FFgui.Theme

// A chapter or marker flag hanging from the ruler. `room` is the space to the
// next flag, so labels never overlap.
Item {
    id: flag

    property string title
    property real room: 140

    Rectangle { width: 1; height: 16; color: Theme.chapter }
    Rectangle {
        x: 1
        width: Math.max(6, Math.min(label.implicitWidth + 8, 140, flag.room))
        height: 13
        color: Theme.chapter
        radius: Metrics.radiusSmall
        Text {
            id: label
            anchors { left: parent.left; leftMargin: 4; verticalCenter: parent.verticalCenter }
            width: parent.width - 8
            text: flag.title
            color: Theme.textOnWarn
            font.pixelSize: Metrics.fontTiny
            font.bold: true
            elide: Text.ElideRight
        }
    }
}
