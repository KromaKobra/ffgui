import QtQuick
import FFgui.Theme

// The small switch on a chain step: green when active, grey when bypassed.
Rectangle {
    id: toggle

    property bool active: true

    signal toggled()

    implicitWidth: 26
    implicitHeight: 13
    radius: height / 2
    color: toggle.active ? Theme.okDeep : Theme.neutral

    Rectangle {
        x: toggle.active ? parent.width - width - 2 : 2
        y: 2
        width: 9
        height: 9
        radius: 4.5
        color: Theme.textOnAccent
        Behavior on x { NumberAnimation { duration: 90 } }
    }
    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: toggle.toggled() }
    TipLabel {
        visible: hover.hovered
        text: toggle.active ? qsTr("Active: click to bypass") : qsTr("Bypassed: click to enable")
    }
}
