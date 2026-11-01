import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Theme

// One option of a chain step, driven by its OptionSpec:
//   { key, label, unit, type, min, max, step, default, choices, runtime, help }
// Numbers get a slider and a typed entry, choices a combo box, booleans a
// switch, strings a text field. Every control has a reset button. While a
// slider is dragged its tooltip shows the value and the option text it makes.
ColumnLayout {
    id: param

    required property var option
    required property var value

    signal edited(var value)

    readonly property string type: param.option.type || "number"
    readonly property bool numeric: param.type === "number"
    readonly property real step: param.option.step > 0 ? param.option.step : 1
    readonly property int decimals: !param.numeric ? 0 : param.step >= 1 ? 0 : param.step >= 0.1 ? 1 : 2
    readonly property var defaultValue: param.option["default"]
    readonly property bool isDefault: String(param.value) === String(param.defaultValue)
    readonly property string valueText: param.numeric ? Number(param.value).toFixed(param.decimals).replace("-", "−")
                                                      : String(param.value)

    Layout.fillWidth: true
    spacing: 0

    RowLayout {
        Layout.fillWidth: true
        spacing: Metrics.spacingSmall

        Label {
            text: param.option.label || param.option.key
            font.pixelSize: Metrics.fontBody
            elide: Text.ElideRight
            Layout.fillWidth: true
            HoverHandler { id: labelHover }
            TipLabel {
                visible: labelHover.hovered && !!param.option.help
                text: param.option.help || ""
            }
        }
        NumberField {
            visible: param.numeric
            Layout.preferredWidth: 72
            implicitHeight: 24
            font.pixelSize: Metrics.fontCaption
            value: param.numeric ? Number(param.value) : 0
            from: param.numeric ? param.option.min : 0
            to: param.numeric ? param.option.max : 0
            decimals: param.decimals
            onEdited: v => param.edited(v)
        }
        Label {
            visible: param.numeric && !!param.option.unit
            text: param.option.unit || ""
            font.pixelSize: Metrics.fontCaption
            opacity: 0.7
        }
        FfToolButton {
            objectName: "reset"
            text: Glyphs.reset
            implicitWidth: 22
            implicitHeight: 22
            opacity: param.isDefault ? 0.15 : 0.9
            enabled: !param.isDefault
            tip: qsTr("Reset to %1").arg(String(param.defaultValue))
            tipDelay: 500
            onClicked: param.edited(param.defaultValue)
        }
    }

    FfSlider {
        visible: param.numeric
        Layout.fillWidth: true
        from: param.numeric ? param.option.min : 0
        to: param.numeric ? param.option.max : 1
        stepSize: param.numeric ? param.step : 0
        value: param.numeric ? Number(param.value) : 0
        liveText: param.valueText + (param.option.unit ? " " + param.option.unit : "") + "   " + Glyphs.arrow + "   "
                  + param.option.key + "=" + param.valueText
                  + (param.option.runtime ? "   " + qsTr("(applies live)") : "")
        onMoved: param.edited(+value.toFixed(param.decimals))
    }

    FfComboBox {
        visible: param.type === "choice"
        Layout.fillWidth: true
        model: param.option.choices || []
        currentIndex: (param.option.choices || []).indexOf(String(param.value))
        onActivated: param.edited(currentText)
    }

    FfSwitch {
        visible: param.type === "bool"
        text: param.value ? qsTr("On") : qsTr("Off")
        checked: !!param.value
        onToggled: param.edited(checked)
    }

    FfTextField {
        visible: param.type === "string"
        Layout.fillWidth: true
        mono: true
        text: param.type === "string" ? String(param.value) : ""
        onEditingFinished: if (text !== String(param.value)) param.edited(text)
    }
}
