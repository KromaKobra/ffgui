import QtQuick
import QtQuick.Controls
import FFgui.Theme

// ffgui's push button. Feature modules use the Ff* controls instead of
// QtQuick.Controls inputs, so Quilt's own controls can replace the Fusion ones
// here, in one file each.
Button {
    id: control

    // Tooltip text; empty for none.
    property string tip
    property int tipDelay: 600

    font.pixelSize: Metrics.fontBody
    ToolTip.visible: control.tip !== "" && control.hovered
    ToolTip.delay: control.tipDelay
    ToolTip.text: control.tip
}
