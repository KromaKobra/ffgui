import QtQuick
import FFgui.Theme

// A node in graph mode, with input and output pads.
Rectangle {
    id: node

    property string title
    property string detail
    property bool isStep: true
    property bool selected: false
    property bool bypassed: false
    property bool hasInput: true
    property bool hasOutput: true

    signal clicked()

    width: 130
    height: 52
    radius: Metrics.radiusLarge
    color: node.isStep ? Theme.card : Theme.ioBlock
    border.width: node.selected ? 2 : 1
    border.color: node.selected ? Theme.accent : Theme.blockBorder
    opacity: node.bypassed ? 0.45 : 1

    TapHandler { onTapped: node.clicked() }

    Column {
        x: 9
        y: 7
        width: node.width - 18
        spacing: 3
        Text {
            width: parent.width
            text: node.title
            color: Theme.text
            font.pixelSize: Metrics.fontCaption
            font.bold: true
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            text: node.detail
            color: Theme.textDim
            font.pixelSize: Metrics.fontSmall
            font.family: Metrics.monoFamily
            elide: Text.ElideRight
        }
    }
    Rectangle {
        visible: node.hasInput
        x: -5
        y: node.height / 2 - 5
        width: 10
        height: 10
        radius: 5
        color: Theme.wire
    }
    Rectangle {
        visible: node.hasOutput
        x: node.width - 5
        y: node.height / 2 - 5
        width: 10
        height: 10
        radius: 5
        color: Theme.wire
    }
}
