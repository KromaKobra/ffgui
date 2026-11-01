import QtQuick
import QtQuick.Controls
import FFgui.Theme

// A selectable chip, e.g. an output target in the Output bar.
Rectangle {
    id: pill

    property alias text: label.text
    property bool current: false
    property string tip

    signal clicked()

    implicitWidth: label.implicitWidth + 18
    implicitHeight: Metrics.controlHeight
    radius: Metrics.radiusMedium
    color: pill.current ? Theme.selected : hover.hovered ? Theme.line : Theme.chip
    border.width: 1
    border.color: pill.current ? Theme.accent : Theme.border

    Label {
        id: label
        anchors.centerIn: parent
        font.pixelSize: Metrics.fontBody
    }
    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: pill.clicked() }
    TipLabel { visible: hover.hovered && pill.tip !== ""; text: pill.tip }
}
