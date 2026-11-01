import QtQuick
import QtQuick.Controls
import FFgui.Theme

Switch {
    id: control

    property string tip

    font.pixelSize: Metrics.fontBody
    ToolTip.visible: control.tip !== "" && control.hovered
    ToolTip.delay: 600
    ToolTip.text: control.tip
}
