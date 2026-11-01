import QtQuick
import FFgui.Theme

// The hover crosshair with its readout (time, level, frequency, note).
Item {
    id: cross

    property real px: 0
    property real py: 0
    property real leftEdge: 0
    property string text

    Rectangle { x: cross.px; width: 1; height: cross.height; color: Theme.crosshair; opacity: 0.35 }
    Rectangle { y: cross.py; x: cross.leftEdge; width: cross.width - cross.leftEdge; height: 1; color: Theme.crosshair; opacity: 0.2 }
    Rectangle {
        x: Math.min(cross.px + 12, cross.width - width - 4)
        y: Math.max(4, cross.py - 26)
        width: readout.implicitWidth + 12
        height: 20
        radius: Metrics.radius
        color: Theme.tooltip
        opacity: 0.85
        Text {
            id: readout
            anchors.centerIn: parent
            color: Theme.textOnAccent
            font.pixelSize: Metrics.fontSmall
            font.family: Metrics.monoFamily
            text: cross.text
        }
    }
}
