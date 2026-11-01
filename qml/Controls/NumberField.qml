import QtQuick

// A typed numeric entry with a real minus sign. Out-of-range values are
// clamped; text that isn't a number is put back.
FfTextField {
    id: field

    property real value: 0
    property real from: -Infinity
    property real to: Infinity
    property int decimals: 1

    signal edited(real value)

    function show(v) { return Number(v).toFixed(field.decimals).replace("-", "−"); }

    mono: true
    horizontalAlignment: Text.AlignRight
    text: field.show(field.value)
    onEditingFinished: {
        const v = Number(field.text.trim().replace("−", "-").replace(",", "."));
        if (isNaN(v) || field.text.trim() === "") {
            field.text = Qt.binding(() => field.show(field.value));
            return;
        }
        const clamped = Math.max(field.from, Math.min(field.to, v));
        field.text = Qt.binding(() => field.show(field.value));
        if (clamped !== field.value)
            field.edited(+clamped.toFixed(field.decimals));
    }
}
