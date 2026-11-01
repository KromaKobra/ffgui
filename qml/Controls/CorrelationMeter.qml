import QtQuick
import FFgui.Theme

// Stereo correlation from −1 (cancels) to +1 (mono).
Item {
    id: meter

    property real value: 0
    property string label: "corr " + meter.value.toFixed(2).replace("-", "−")

    implicitHeight: 22
    implicitWidth: 60

    Rectangle {
        y: 2
        width: parent.width
        height: 6
        radius: Metrics.radiusSmall
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: Theme.error }
            GradientStop { position: 0.5; color: Theme.neutral }
            GradientStop { position: 1; color: Theme.ok }
        }
    }
    Rectangle {
        x: (Math.max(-1, Math.min(1, meter.value)) + 1) / 2 * (parent.width - 3)
        width: 3
        height: 10
        color: Theme.textOnAccent
    }
    Text {
        y: 11
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        text: meter.label
        color: Theme.textDim
        font.pixelSize: Metrics.fontTiny
    }
}
