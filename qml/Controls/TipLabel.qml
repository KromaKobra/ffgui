import QtQuick
import QtQuick.Controls

// A delayed tooltip for plain items (not Controls), e.g. inside a Badge:
//     HoverHandler { id: hover }
//     TipLabel { visible: hover.hovered; text: "Why" }
ToolTip {
    delay: 500
}
