import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Controls
import FFgui.Theme

// The one quality control, which changes form with the codec: a bitrate
// field, a VBR quality slider or a compression level. `quality` is the codec's
// quality spec ({ kind, label, min, max, step }), empty when it has none.
RowLayout {
    id: control

    property var quality: ({})
    property real value: 0
    property real kbps: 0

    signal edited(real value)

    readonly property bool hasQuality: !!control.quality.kind

    Layout.fillWidth: false
    visible: control.hasQuality
    spacing: Metrics.spacingSmall

    Caption { text: control.quality.label || ""; font.pixelSize: Metrics.fontBody; opacity: 0.7 }
    FfSlider {
        visible: control.quality.kind !== "bitrate"
        Layout.preferredWidth: 90
        from: control.quality.min || 0
        to: control.quality.max || 1
        stepSize: control.quality.step || 1
        value: control.value
        liveText: control.quality.kind === "vbr" ? "V" + Math.round(value) : qsTr("level %1").arg(Math.round(value))
        onMoved: control.edited(value)
    }
    FfSpinBox {
        visible: control.quality.kind === "bitrate"
        Layout.preferredWidth: 110
        from: control.quality.min || 0
        to: control.quality.max || 1
        stepSize: control.quality.step || 1
        value: control.value
        onValueModified: control.edited(value)
    }
    Label {
        font.family: Metrics.monoFamily
        font.pixelSize: Metrics.fontCaption
        text: control.quality.kind === "vbr" ? "V" + control.value + " ≈" + Math.round(control.kbps) + "k"
              : control.quality.kind === "level" ? qsTr("level %1").arg(control.value)
              : "kb/s"
    }
}
