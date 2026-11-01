pragma ComponentBehavior: Bound
import QtQuick
import FFgui.Theme

// A row of mutually exclusive buttons. `options` is a list of strings, or of
// { value, label, tip, enabled } objects; `current` is the selected value.
Row {
    id: control

    property var options: []
    property var current
    property int fontSize: Metrics.fontBody

    signal activated(var value)

    spacing: 1

    Repeater {
        model: control.options
        delegate: FfToolButton {
            required property var modelData
            readonly property bool isObject: typeof modelData === "object"
            readonly property var value: isObject ? modelData.value : modelData
            text: isObject ? modelData.label : String(modelData)
            tip: isObject && modelData.tip ? modelData.tip : ""
            enabled: !isObject || modelData.enabled !== false
            font.pixelSize: control.fontSize
            checked: control.current === value
            onClicked: control.activated(value)
        }
    }
}
