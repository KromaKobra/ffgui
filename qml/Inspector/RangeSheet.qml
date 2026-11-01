pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// A time selection: start, end and length (editable), and its stats.
ColumnLayout {
    id: sheet

    readonly property real start: App.selection.start
    readonly property real end: App.selection.end
    readonly property var stats: App.analysis.rangeStats
    readonly property bool measured: sheet.stats.start === sheet.start && sheet.stats.end === sheet.end

    function selectRange(s, e) {
        App.selection.select("range", { start: Math.min(s, e), end: Math.max(s, e), channel: App.selection.channel });
    }

    spacing: Metrics.spacingLarge

    // Measure each new selection once the user stops dragging.
    Timer {
        id: measure
        interval: 300
        onTriggered: App.analysis.measureRange(sheet.start, sheet.end)
    }
    onStartChanged: measure.restart()
    onEndChanged: measure.restart()
    Component.onCompleted: measure.restart()

    Item { implicitHeight: 4 }

    Section {
        title: qsTr("Range")
        TimecodeRow {
            label: qsTr("Start")
            seconds: sheet.start
            display: Format.timecode(sheet.start, true)
            onEdited: s => sheet.selectRange(s, sheet.end)
        }
        TimecodeRow {
            label: qsTr("End")
            seconds: sheet.end
            display: Format.timecode(sheet.end, true)
            onEdited: s => sheet.selectRange(sheet.start, s)
        }
        TimecodeRow {
            label: qsTr("Length")
            seconds: sheet.end - sheet.start
            display: Format.timecode(sheet.end - sheet.start, true)
            onEdited: s => sheet.selectRange(sheet.start, sheet.start + s)
        }
        Caption {
            visible: App.selection.channel >= 0
            text: qsTr("Channel %1 only").arg(App.selection.channel + 1)
        }
    }

    Section {
        title: qsTr("Selection stats")
        Hint { visible: !sheet.measured; text: qsTr("Measuring…") }
        Prop { visible: sheet.measured; key: qsTr("Peak"); value: Format.dbValue(sheet.stats.peak) + " dBFS" }
        Prop { visible: sheet.measured; key: qsTr("RMS"); value: Format.dbValue(sheet.stats.rms) + " dBFS" }
        Prop { visible: sheet.measured; key: qsTr("Loudness"); value: Format.dbValue(sheet.stats.lufs) + " LUFS" }
        Prop {
            visible: sheet.measured
            key: qsTr("DC offset")
            value: (sheet.stats.dc || []).map(v => Format.signedNumber(v * 100, 2) + " %").join(" / ")
        }
        Prop {
            visible: sheet.measured && sheet.stats.correlation !== undefined
            key: qsTr("Correlation")
            value: Format.number(sheet.stats.correlation || 0, 2)
        }
    }

    ButtonRow {
        FfButton {
            text: qsTr("Loop")
            checkable: true
            checked: App.transport.loop
            onToggled: App.transport.loop = checked
        }
        FfButton {
            text: qsTr("Export selection…")
            onClicked: App.render.start("selection")
        }
    }
}
