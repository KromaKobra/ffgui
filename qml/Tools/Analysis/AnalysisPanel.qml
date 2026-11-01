pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// Analysis: the full measurement report for the current input (loudness,
// levels, spectrum, silence, decode errors), exportable as JSON or CSV.
Item {
    id: panel

    readonly property var results: App.analysis.results
    readonly property var loudness: panel.results.loudness || {}
    readonly property var levels: panel.results.levels || {}
    readonly property var spectrum: panel.results.spectrum || {}
    readonly property var silence: panel.results.silence || []
    readonly property var decode: panel.results.decode || {}
    readonly property real target: isNaN(App.meters.target) ? -23 : App.meters.target
    readonly property real silenceTotal: panel.silence.reduce((sum, s) => sum + s.end - s.start, 0)

    EmptyState {
        anchors.fill: parent
        visible: !App.project.hasInput
        glyph: "Σ"
        title: qsTr("Nothing to analyze yet")
        text: qsTr("Add an input: levels, loudness, silence and clipping are measured in the background.")
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        visible: App.project.hasInput
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: scroll.availableWidth
            spacing: Metrics.spacingLarge

            RowLayout {
                Layout.margins: Metrics.margin
                Layout.bottomMargin: 0
                Label { text: App.project.input.name || ""; font.bold: true; Layout.fillWidth: true; elide: Text.ElideMiddle }
                Badge {
                    text: App.analysis.running ? qsTr("MEASURING %1%").arg(Math.round(App.analysis.progress * 100))
                          : App.analysis.state === "done" ? qsTr("COMPLETE")
                          : App.analysis.state === "failed" ? qsTr("FAILED") : qsTr("WAITING")
                    tone: App.analysis.state === "done" ? Theme.okDeep : App.analysis.state === "failed" ? Theme.errorDeep : Theme.neutral
                }
            }
            Hint {
                visible: App.analysis.state === "failed"
                Layout.leftMargin: Metrics.margin
                text: App.analysis.error
            }

            Section {
                title: qsTr("Loudness (EBU R128, ebur128)")
                visible: App.analysis.ready("loudness")
                Prop {
                    key: qsTr("Integrated")
                    value: Format.number(panel.loudness.integrated) + " LUFS   "
                           + (Math.abs(panel.loudness.integrated - panel.target) <= 1
                              ? qsTr("(target %1: pass)").arg(panel.target)
                              : qsTr("(target %1: %2 LU off)").arg(panel.target).arg(Format.signedNumber(panel.target - panel.loudness.integrated)))
                    valueColor: Math.abs(panel.loudness.integrated - panel.target) <= 1 ? Theme.ok : Theme.errorSoft
                }
                Prop { key: qsTr("Loudness range"); value: Format.number(panel.loudness.lra) + " LU" }
                Prop { key: qsTr("True peak"); value: Format.signedNumber(panel.loudness.truePeak) + " dBTP" }
                Prop { key: qsTr("Max short-term"); value: Format.number(panel.loudness.maxShortTerm) + " LUFS" }
            }
            Section {
                title: qsTr("Levels (astats)")
                visible: App.analysis.ready("levels")
                Prop { key: qsTr("Peak"); value: Format.dbValue(panel.levels.peak) + " dBFS" }
                Prop { key: qsTr("RMS"); value: Format.dbValue(panel.levels.rms) + " dBFS" }
                Prop { key: qsTr("Crest factor"); value: Format.number(panel.levels.peak - panel.levels.rms) + " dB" }
                Prop {
                    key: qsTr("DC offset")
                    value: (panel.levels.dc || []).map(v => Format.signedNumber(v * 100, 3) + " %").join(" / ")
                }
                Prop {
                    key: qsTr("Clipping")
                    value: (panel.levels.clipSamples || 0) > 0
                           ? qsTr("%1 samples at full scale, in %2 spots").arg(panel.levels.clipSamples).arg((panel.levels.clips || []).length)
                           : qsTr("none")
                    valueColor: (panel.levels.clipSamples || 0) > 0 ? Theme.errorSoft : Theme.text
                }
            }
            Section {
                title: qsTr("Spectrum (aspectralstats)")
                visible: App.analysis.ready("spectrum")
                Prop { key: qsTr("Centroid"); value: Format.frequency(panel.spectrum.centroid) }
                Prop { key: qsTr("Roll-off (85%)"); value: Format.frequency(panel.spectrum.rolloff) }
                Prop { key: qsTr("Flatness"); value: Format.number(panel.spectrum.flatness, 2) }
            }
            Section {
                visible: App.analysis.ready("silence")
                title: qsTr("Silence · %n regions", "", panel.silence.length) + ", " + Format.number(panel.silenceTotal, 0) + " s"
                expanded: false
                Repeater {
                    model: panel.silence
                    delegate: Prop {
                        required property var modelData
                        key: Format.timecode(modelData.start, true)
                        value: Format.number(modelData.end - modelData.start) + " s"
                        mono: true
                    }
                }
            }
            Section {
                title: qsTr("Decoding")
                visible: App.analysis.ready("decode")
                Prop {
                    key: qsTr("Errors")
                    value: String(panel.decode.errors || 0)
                    valueColor: (panel.decode.errors || 0) > 0 ? Theme.errorSoft : Theme.text
                }
                Prop { key: qsTr("Warnings"); value: String(panel.decode.warnings || 0) }
                Repeater {
                    model: (panel.decode.messages || []).slice(0, 8)
                    delegate: Hint {
                        required property string modelData
                        text: modelData
                    }
                }
            }

            ButtonRow {
                FfButton {
                    text: qsTr("Analyze again")
                    enabled: !App.analysis.running
                    onClicked: App.analysis.restart()
                }
                FfButton {
                    text: qsTr("Export JSON…")
                    enabled: App.analysis.state === "done"
                    onClicked: App.requestDialog("exportAnalysis", { format: "json" })
                }
                FfButton {
                    text: qsTr("Export CSV…")
                    enabled: App.analysis.state === "done"
                    onClicked: App.requestDialog("exportAnalysis", { format: "csv" })
                }
            }
        }
    }
}
