import QtQuick
import FFgui.Theme

// A rounded block: input cards, destinations, step blocks.
Rectangle {
    property bool current: false
    property bool selected: false

    radius: Metrics.radiusLarge
    color: current ? Theme.cardCurrent : Theme.card
    border.width: 1
    border.color: selected ? Theme.accent : current ? Theme.cardBorderCurrent : Theme.cardBorder
}
