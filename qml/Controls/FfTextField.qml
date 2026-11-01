import QtQuick
import QtQuick.Controls
import FFgui.Theme

TextField {
    id: control

    property string tip
    property bool mono: false

    font.pixelSize: Metrics.fontBody
    selectByMouse: true
    ToolTip.visible: control.tip !== "" && control.hovered && !control.activeFocus
    ToolTip.delay: 600
    ToolTip.text: control.tip
    Binding on font.family {
        when: control.mono
        value: Metrics.monoFamily
    }
}
