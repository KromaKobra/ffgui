import QtQuick
import QtQuick.Controls
import FFgui.Theme

// ffgui's tool button. For a button that shows a state it doesn't own (a
// workspace tab, a mode), bind `checked` and leave `checkable` false: clicks
// then never break the binding.
ToolButton {
    id: control

    property string tip
    property int tipDelay: 600

    font.pixelSize: Metrics.fontBody
    ToolTip.visible: control.tip !== "" && control.hovered
    ToolTip.delay: control.tipDelay
    ToolTip.text: control.tip
}
