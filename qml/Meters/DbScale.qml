pragma ComponentBehavior: Bound
import QtQuick
import FFgui.Theme

// dB tick labels beside the level bars. `topInset`/`bottomInset` match the
// bars' clip light and label so the ticks line up with the meter track.
Item {
    id: dbScale

    property real floor: -60
    property var marks: [0, -6, -12, -18, -24, -36, -48]
    property real topInset: 9
    property real bottomInset: 15

    function frac(db) { return Math.max(0, Math.min(1, (db - dbScale.floor) / -dbScale.floor)); }

    implicitWidth: 22

    Repeater {
        model: dbScale.marks
        delegate: Text {
            required property int modelData
            y: dbScale.topInset + (1 - dbScale.frac(modelData)) * (dbScale.height - dbScale.topInset - dbScale.bottomInset) - 5
            width: 20
            horizontalAlignment: Text.AlignRight
            text: String(modelData).replace("-", "−")
            color: Theme.textFaint
            font.pixelSize: Metrics.fontTiny
        }
    }
}
