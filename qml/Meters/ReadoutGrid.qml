pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import FFgui.Theme

// Label / value pairs under the meters: TP, M, S, I. A row can carry its own
// value colour (loudness compliance).
GridLayout {
    id: grid

    // [{ label, value, color }]
    property var readouts: []

    columns: 2
    columnSpacing: Metrics.spacingSmall
    rowSpacing: 1

    Repeater {
        model: grid.readouts
        delegate: Item {
            id: readout
            required property var modelData
            Layout.columnSpan: 2
            Layout.fillWidth: true
            implicitHeight: 14
            Text { text: readout.modelData.label; color: Theme.textDim; font.pixelSize: Metrics.fontSmall }
            Text {
                anchors.right: parent.right
                text: readout.modelData.value
                font.pixelSize: Metrics.fontSmall
                font.family: Metrics.monoFamily
                color: readout.modelData.color || Theme.text
            }
        }
    }
}
