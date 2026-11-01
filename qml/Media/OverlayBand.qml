pragma ComponentBehavior: Bound
import QtQuick
import FFgui.Theme

// A coloured band over a time range: a chain step's range. Its edges can be
// dragged to edit the range; `edited` reports the new start and end.
Item {
    id: band

    property string label
    property color tone: Theme.accent
    property bool editable: true
    // Maps between seconds and x in the parent.
    property var xOf: t => t
    property var tOf: x => x
    property real start: 0
    property real end: 0

    signal edited(real start, real end)
    signal clicked()

    x: band.xOf(band.start)
    width: Math.max(2, band.xOf(band.end) - x)

    Rectangle { anchors.fill: parent; color: band.tone; opacity: 0.08 }
    Rectangle { width: parent.width; height: 3; color: band.tone }
    Rectangle {
        y: 3
        width: Math.min(parent.width, title.implicitWidth + 10)
        height: 15
        color: band.tone
        clip: true
        Text {
            id: title
            anchors.centerIn: parent
            text: band.label
            color: Theme.textOnAccent
            font.pixelSize: Metrics.fontTiny
            font.bold: true
        }
        TapHandler { onTapped: band.clicked() }
    }

    component Edge: MouseArea {
        property bool isStart: true
        width: 8
        height: band.height
        x: isStart ? -4 : band.width - 4
        enabled: band.editable
        hoverEnabled: true
        cursorShape: Qt.SizeHorCursor
        onPositionChanged: ev => {
            if (!pressed)
                return;
            const t = band.tOf(mapToItem(band.parent, ev.x, ev.y).x);
            if (isStart)
                band.edited(Math.min(t, band.end), band.end);
            else
                band.edited(band.start, Math.max(t, band.start));
        }
    }
    Edge { isStart: true }
    Edge { isStart: false }
}
