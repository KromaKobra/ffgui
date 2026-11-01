pragma ComponentBehavior: Bound
import QtQuick
import FFgui.Backend
import FFgui.Theme

// The fixed Input block on the left of the chain and Output block on the right.
Rectangle {
    id: end

    property bool isInput: true
    readonly property bool selected: App.selection.kind === (end.isInput ? "input" : "output")

    width: Metrics.endBlockWidth
    height: Metrics.stepHeight
    radius: Metrics.radiusLarge
    color: Theme.ioBlock
    border.width: end.selected ? 2 : 1
    border.color: end.selected ? Theme.accent : Theme.ioBlockBorder

    TapHandler {
        onTapped: {
            if (end.isInput && App.project.hasInput)
                App.selection.select("input", { index: App.project.currentInput });
            else if (!end.isInput && App.project.outputs.count > 0)
                App.selection.select("output", { index: Math.max(0, App.project.currentOutput) });
        }
    }

    Column {
        x: 10
        y: 7
        width: end.width - 20
        spacing: 3

        Text {
            text: end.isInput ? qsTr("INPUT") : qsTr("OUTPUT")
            color: Theme.ioLabel
            font.pixelSize: Metrics.fontTiny
            font.bold: true
        }
        Text {
            visible: end.isInput
            width: parent.width
            text: App.project.hasInput ? App.project.input.name : qsTr("No input yet")
            color: App.project.hasInput ? Theme.text : Theme.textDim
            font.pixelSize: Metrics.fontBody
            font.bold: App.project.hasInput
            elide: Text.ElideMiddle
        }
        Text {
            visible: end.isInput && App.project.hasInput
            text: (App.project.track.codec || "") + " · " + (App.project.track.channels || 0) + " ch"
            color: Theme.textDim
            font.pixelSize: Metrics.fontSmall
        }
        Text {
            visible: !end.isInput && App.project.outputs.count === 0
            text: qsTr("Choose a format below")
            color: Theme.textDim
            font.pixelSize: Metrics.fontCaption
        }
        Repeater {
            model: end.isInput ? null : App.project.outputs
            delegate: Text {
                required property string fileName
                width: end.width - 20
                text: fileName
                color: Theme.text
                font.pixelSize: Metrics.fontCaption
                elide: Text.ElideMiddle
            }
        }
    }
}
