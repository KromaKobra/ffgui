import QtQuick
import FFgui.Theme

// A coloured dot: green ok, amber warn, red error, grey unknown.
Rectangle {
    id: dot

    property string status: "unknown"
    property string tip

    signal clicked()

    implicitWidth: 10
    implicitHeight: 10
    radius: width / 2
    color: Theme.health(dot.status)

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: dot.clicked() }
    TipLabel { visible: hover.hovered && dot.tip !== ""; text: dot.tip }
}
