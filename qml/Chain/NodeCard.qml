pragma ComponentBehavior: Bound
import QtQuick
import FFgui.Theme

// A node in graph mode: a title, a detail line and its ports. Inputs sit on
// the left edge and outputs on the right, `portTop` + i × `portPitch` down;
// hollow inputs are not connected yet.
Rectangle {
    id: node

    property string title
    property string detail
    property bool isStep: true
    property bool selected: false
    property bool bypassed: false
    property bool failed: false
    property int inputCount: 1
    property int outputCount: 1
    property var inputNames: []
    property var outputNames: []
    property var connectedInputs: []
    property int pendingOutput: -1
    property int portTop: 40
    property int portPitch: 18

    signal portClicked(bool output, int port)
    signal clicked()

    width: 150
    height: node.portTop + node.portPitch * Math.max(1, node.inputCount, node.outputCount)
    radius: Metrics.radiusLarge
    color: node.isStep ? Theme.card : Theme.ioBlock
    border.width: node.selected || node.failed ? 2 : 1
    border.color: node.failed ? Theme.errorSoft : node.selected ? Theme.accent : Theme.blockBorder
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
            color: node.failed ? Theme.errorSoft : Theme.textDim
            font.pixelSize: Metrics.fontSmall
            font.family: Metrics.monoFamily
            elide: Text.ElideRight
        }
    }
    Repeater {
        model: node.inputCount
        delegate: Item {
            id: inputPort
            required property int index
            readonly property bool connected: node.connectedInputs.indexOf(inputPort.index) >= 0
            y: node.portTop + inputPort.index * node.portPitch
            width: node.width / 2
            height: node.portPitch
            Rectangle {
                objectName: "inputPort" + inputPort.index
                x: -6; y: (parent.height - height) / 2
                width: 12; height: 12; radius: 6
                color: inputPort.connected ? Theme.wire : Theme.chainBackground
                border.width: 2
                border.color: Theme.wire
                MouseArea { anchors.fill: parent; anchors.margins: -3; preventStealing: true; onClicked: node.portClicked(false, inputPort.index) }
            }
            Text {
                x: 10
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 12
                text: node.inputNames[inputPort.index] || ""
                color: Theme.textMuted
                font.pixelSize: Metrics.fontSmall
                elide: Text.ElideRight
            }
        }
    }
    Repeater {
        model: node.outputCount
        delegate: Item {
            id: outputPort
            required property int index
            x: node.width / 2
            y: node.portTop + outputPort.index * node.portPitch
            width: node.width / 2
            height: node.portPitch
            Rectangle {
                objectName: "outputPort" + outputPort.index
                x: parent.width - 6; y: (parent.height - height) / 2
                width: 12; height: 12; radius: 6
                color: node.pendingOutput === outputPort.index ? Theme.accentSoft : Theme.accent
                border.width: node.pendingOutput === outputPort.index ? 2 : 0
                border.color: Theme.textBright
                MouseArea { anchors.fill: parent; anchors.margins: -3; preventStealing: true; onClicked: node.portClicked(true, outputPort.index) }
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 12
                horizontalAlignment: Text.AlignRight
                text: node.outputNames[outputPort.index] || ""
                color: Theme.textMuted
                font.pixelSize: Metrics.fontSmall
                elide: Text.ElideRight
            }
        }
    }
}
