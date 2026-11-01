import QtQuick
import QtQuick.Controls
import FFgui.Theme

// A clickable list row (step picker, presets, palette results).
ItemDelegate {
    id: control

    property string tip

    font.pixelSize: Metrics.fontBody
    ToolTip.visible: control.tip !== "" && control.hovered
    ToolTip.delay: 600
    ToolTip.text: control.tip
}
