pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// A chain step's controls: common options first, the rest under Advanced
// (design principle 6), each with a reset; then its time range.
ColumnLayout {
    id: sheet

    property int index: -1
    readonly property var step: row.values
    readonly property bool copyTrim: App.project.outputs.count > 0 && outputRow.values.reencodeReason === ""
    readonly property var params: sheet.step.params || {}
    readonly property var spec: App.catalog.revision >= 0 ? App.catalog.step(sheet.step.type || "") : ({})
    readonly property var common: sheet.spec.common || []
    readonly property var advanced: {
        // Curated extras, then every other option the filter has (from -h),
        // except those the step derives itself (a graph node's ports).
        if (App.catalog.revision < 0) return [];
        const curated = (sheet.spec.common || []).concat(sheet.spec.advanced || []).map(o => o.key).concat(sheet.spec.fixed || []);
        const all = App.catalog.filterOptions(sheet.spec.primaryFilter || "");
        return (sheet.spec.advanced || []).concat(all.filter(o => curated.indexOf(o.key) < 0));
    }

    function optionFor(option) {
        // A Trim can't go past the end of the input.
        if (sheet.step.type === "trim" && App.project.duration > 0)
            return Object.assign({}, option, { max: App.project.duration });
        return option;
    }

    spacing: Metrics.spacingLarge

    ModelRow {
        id: row
        model: App.project.steps
        row: sheet.index
    }

    ModelRow {
        id: outputRow
        model: App.project.outputs
        row: App.project.currentOutput
    }

    Item { implicitHeight: 4 }

    RowLayout {
        Layout.leftMargin: Metrics.margin
        Layout.rightMargin: Metrics.margin
        Label { text: sheet.spec.glyph || ""; color: Theme.accentGlyph; font.pixelSize: 18 }
        Label {
            text: sheet.spec.blurb || ""
            opacity: 0.7
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        FfSwitch {
            checked: !sheet.step.bypass
            tip: checked ? qsTr("Active") : qsTr("Bypassed")
            onToggled: App.project.setBypass(sheet.index, !checked)
        }
    }

    Label {
        visible: !!sheet.step.warning
        Layout.leftMargin: Metrics.margin
        Layout.fillWidth: true
        text: Glyphs.warning + " " + qsTr("This step %1 — lower the gain, or add a limiter after it").arg(sheet.step.warning || "")
        color: Theme.errorSoft
        font.pixelSize: Metrics.fontCaption
        wrapMode: Text.Wrap
    }

    Section {
        title: qsTr("Options")
        visible: sheet.common.length > 0
        Repeater {
            model: sheet.common
            delegate: ParamControl {
                required property var modelData
                option: sheet.optionFor(modelData)
                value: sheet.params[modelData.key] ?? modelData["default"]
                onEdited: v => App.project.setParam(sheet.index, modelData.key, v)
            }
        }
        Label {
            visible: sheet.step.type === "trim"
            text: Glyphs.check + " " + (sheet.copyTrim
                  ? qsTr("Snaps to nearest packet: stream copy cuts at packet boundaries")
                  : qsTr("Sample-exact: atrim decodes, so the cut lands on the exact sample"))
            color: sheet.copyTrim ? Theme.warn : Theme.ok
            font.pixelSize: Metrics.fontCaption
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }

    Hint {
        visible: sheet.common.length === 0
        Layout.margins: Metrics.margin
        text: qsTr("This step has no common options.")
    }

    Section {
        title: qsTr("Time range")
        visible: sheet.step.timeline !== "trim"
        TimeRangeEditor {
            supported: sheet.step.rangeSupported !== false
            unsupportedText: qsTr("Range not supported by this filter: %1 has no timeline support.").arg(sheet.spec.primaryFilter || "")
            enabledRange: !!sheet.step.hasRange
            start: sheet.step.rangeStart || 0
            end: sheet.step.rangeEnd || 0
            onToggled: on => {
                if (on)
                    App.project.setRange(sheet.index, App.selection.isRange ? App.selection.start : 0,
                                         App.selection.isRange ? App.selection.end : App.project.duration);
                else
                    App.project.clearRange(sheet.index);
            }
            onEdited: (s, e) => App.project.setRange(sheet.index, s, e)
        }
    }

    Section {
        title: qsTr("Advanced (%1)").arg(sheet.advanced.length)
        visible: sheet.advanced.length > 0
        expanded: false
        Repeater {
            model: sheet.advanced
            delegate: ParamControl {
                required property var modelData
                option: modelData
                value: sheet.params[modelData.key] ?? modelData["default"]
                onEdited: v => App.project.setParam(sheet.index, modelData.key, v)
            }
        }
        Hint { text: qsTr("Generated from ffmpeg -h filter=%1").arg(sheet.spec.primaryFilter || "") }
    }

    ButtonRow {
        FfButton {
            text: qsTr("Remove step")
            onClicked: App.project.removeStep(sheet.index)
        }
    }
}
