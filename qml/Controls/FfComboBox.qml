import QtQuick
import QtQuick.Controls
import FFgui.Theme

ComboBox {
    id: control

    property string tip

    font.pixelSize: Metrics.fontBody
    ToolTip.visible: control.tip !== "" && control.hovered && !control.popup.visible
    ToolTip.delay: 600
    ToolTip.text: control.tip
}
