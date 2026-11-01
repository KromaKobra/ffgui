import QtQuick
import FFgui.Theme

// A small rounded label: container types, COPY, inline warnings.
Rectangle {
    id: badge

    property alias text: label.text
    property color tone: Theme.neutral
    property color textColor: Theme.text
    property bool outline: false
    property string tip

    implicitWidth: label.implicitWidth + 10
    implicitHeight: label.implicitHeight + 4
    radius: Metrics.radius
    color: badge.outline ? "transparent" : badge.tone
    border.width: badge.outline ? 1 : 0
    border.color: badge.tone

    Text {
        id: label
        anchors.centerIn: parent
        color: badge.textColor
        font.pixelSize: Metrics.fontSmall
        font.bold: true
    }

    HoverHandler { id: hover; enabled: badge.tip !== "" }
    TipLabel { visible: hover.hovered; text: badge.tip }
}
