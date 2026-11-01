import QtQuick
import FFgui.Theme

// A thin progress bar for rows and status lines.
Rectangle {
    id: strip

    property real value: 0
    property color tone: Theme.accent

    implicitHeight: 6
    implicitWidth: 70
    radius: height / 2
    color: Theme.meterTrack

    Rectangle {
        width: Math.max(0, Math.min(1, strip.value)) * parent.width
        height: parent.height
        radius: parent.radius
        color: strip.tone
    }
}
