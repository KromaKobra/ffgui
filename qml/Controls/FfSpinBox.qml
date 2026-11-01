import QtQuick
import QtQuick.Controls
import FFgui.Theme

SpinBox {
    id: control

    property string tip

    font.pixelSize: Metrics.fontBody
    editable: true
    ToolTip.visible: control.tip !== "" && control.hovered
    ToolTip.delay: 600
    ToolTip.text: control.tip
}
