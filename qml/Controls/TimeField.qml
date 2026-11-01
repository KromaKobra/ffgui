import QtQuick
import FFgui.Theme

// A time entry. Show `text` (formatted by the caller); on Enter or focus-out
// the typed text is parsed ("1:02:03.5", "02:03", "75.2") and `edited` reports
// seconds. Text that isn't a time is put back.
FfTextField {
    id: field

    property real seconds: 0
    property string display

    signal edited(real seconds)

    function parse(value) {
        const parts = String(value).trim().replace("−", "-").split(":");
        if (parts.length === 0 || parts.length > 3 || parts[0] === "")
            return NaN;
        let total = 0;
        for (let i = 0; i < parts.length; ++i) {
            const v = Number(parts[i]);
            if (isNaN(v) || v < 0 || (i > 0 && v >= 60))
                return NaN;
            total = total * 60 + v;
        }
        return total;
    }

    mono: true
    text: field.display
    onEditingFinished: {
        const t = field.parse(field.text);
        if (isNaN(t))
            field.text = Qt.binding(() => field.display);
        else if (t !== field.seconds)
            field.edited(t);
    }
}
