import QtQuick
import FFgui.Controls
import FFgui.Theme

// The wire between two blocks, labelled with the audio format at that point
// ("48 kHz · fltp · stereo"). It turns amber where the format changes. The +
// opens the step picker to insert a step here.
Item {
    id: conn

    property int insertAt: 0
    property string format
    property bool changed: false
    property bool showPlus: true

    signal addRequested(Item anchor, int insertAt)

    width: Metrics.connectorWidth
    height: Metrics.stepHeight

    Rectangle {
        x: 2
        y: conn.height / 2 - 1
        width: conn.width - 10
        height: 2
        color: conn.changed ? Theme.warn : Theme.borderStrong
    }
    Text {
        x: conn.width - 11
        y: conn.height / 2 - 8
        text: Glyphs.flowArrow
        color: conn.changed ? Theme.warn : Theme.borderStrong
        font.pixelSize: Metrics.fontSmall
    }
    Text {
        width: conn.width
        y: conn.height / 2 - 20
        horizontalAlignment: Text.AlignHCenter
        text: conn.format
        color: conn.changed ? Theme.warn : Theme.textDim
        font.pixelSize: Metrics.fontTiny
        elide: Text.ElideRight
    }
    Rectangle {
        id: plus
        objectName: "addStepAt" + conn.insertAt
        visible: conn.showPlus
        x: conn.width / 2 - 9
        y: conn.height / 2 + 6
        width: 18
        height: 18
        radius: 9
        color: plusHover.hovered ? Theme.accent : Theme.control
        border.color: Theme.borderStrong
        Text { anchors.centerIn: parent; text: Glyphs.add; color: Theme.textOnAccent; font.pixelSize: Metrics.fontLarge }
        HoverHandler { id: plusHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: conn.addRequested(plus, conn.insertAt) }
        TipLabel { visible: plusHover.hovered; text: qsTr("Add a step here") }
    }
}
