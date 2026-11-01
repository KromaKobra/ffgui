import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Theme

// The numbered stage marker: 1 Load, 2 Inspect, 3 Shape, 4 Deliver.
RowLayout {
    id: stage

    property int number: 1
    property string title

    Layout.fillWidth: false
    spacing: Metrics.spacing

    Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        radius: 9
        color: Theme.accent
        Text {
            anchors.centerIn: parent
            text: stage.number
            color: Theme.textOnAccent
            font.bold: true
            font.pixelSize: Metrics.fontCaption
        }
    }
    Label {
        visible: stage.title !== ""
        text: stage.title
        font.bold: true
    }
}
