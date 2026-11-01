import QtQuick
import FFgui.Theme

// The whole input in one strip, with the visible window highlighted. Drag the
// window to scroll; click elsewhere to jump there.
Item {
    id: strip

    property real duration: 1
    property real viewStart: 0
    property real viewEnd: 1
    property real position: 0
    // The waveform drawn behind the window (an OverviewItem once there is data).
    property alias content: contentHolder.data

    signal scrollTo(real start)

    function xOf(t) { return t / Math.max(1e-6, strip.duration) * strip.width; }

    Rectangle { anchors.fill: parent; color: Theme.well; radius: Metrics.radiusSmall }
    Item { id: contentHolder; anchors.fill: parent }

    Rectangle {
        id: window
        x: strip.xOf(strip.viewStart)
        width: Math.max(4, strip.xOf(strip.viewEnd) - x)
        height: parent.height
        color: Theme.rangeFill
        border.color: Theme.accent
        radius: Metrics.radiusSmall
        opacity: 0.8
    }
    Rectangle {
        x: strip.xOf(strip.position)
        width: 1
        height: parent.height
        color: Theme.playhead
    }

    MouseArea {
        anchors.fill: parent
        property real grab: 0
        onPressed: ev => {
            const t = ev.x / strip.width * strip.duration;
            const span = strip.viewEnd - strip.viewStart;
            grab = t >= strip.viewStart && t <= strip.viewEnd ? t - strip.viewStart : span / 2;
            strip.scrollTo(t - grab);
        }
        onPositionChanged: ev => {
            if (pressed)
                strip.scrollTo(ev.x / strip.width * strip.duration - grab);
        }
    }
}
