import QtQuick
import QtQuick.Controls

// A slider whose tooltip follows the drag ("live parameter tooltip"): set
// `liveText` to the value with its unit and the option text it produces.
Slider {
    id: control

    property string liveText
    property string tip

    ToolTip.visible: control.pressed ? control.liveText !== "" : control.tip !== "" && control.hovered
    ToolTip.delay: control.pressed ? 0 : 600
    ToolTip.text: control.pressed ? control.liveText : control.tip
}
