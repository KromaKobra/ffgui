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

    // The delegates stay put while the values change; only the count is
    // the model.
    Repeater {
        model: grid.readouts.length
        delegate: Item {
            id: readout
            required property int index
            readonly property var modelData: grid.readouts[index] ?? ({})
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
