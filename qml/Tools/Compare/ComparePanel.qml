pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme
Item {
    id: panel
    property int mode: 0
    property int a: 0
    property int b: 1
    readonly property var result: App.compare.result
    function score(key) {
        const value = panel.result[key];
        return value === undefined ? "—" : value === Infinity ? "+∞ dB" : value === -Infinity ? "−∞ dB" : Format.number(value, 2) + " dB";
    }
    component Trace: ColumnLayout {
        id: trace
        property string title
        property var samples: []
        property color tone: Theme.wavePeak
        Layout.fillWidth: true
        Layout.fillHeight: true
        Caption { text: trace.title }
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 8
            color: Theme.well
            ToolPlot { anchors.fill: parent; values: trace.samples; kind: "waveform"; tone: trace.tone; gridColor: Theme.line }
        }
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.spacingLarge
        RowLayout {
            FfComboBox { model: [qsTr("Original vs Processed"), qsTr("Two inputs")]; currentIndex: panel.mode; onActivated: index => { panel.mode = index; App.compare.cancel(); } }
            FfButton { text: App.compare.active ? qsTr("Cancel") : qsTr("Compare"); enabled: App.project.hasInput; onClicked: App.compare.active ? App.compare.cancel() : App.compare.run(panel.mode, panel.mode === 0 ? -1 : panel.a, panel.b) }
            FfButton { text: qsTr("Find offset"); enabled: !App.compare.active && panel.result.suggestedOffset !== undefined; onClicked: App.compare.findOffset() }
        }
        RowLayout {
            visible: panel.mode === 1
            Caption { text: "A" }
            FfComboBox { Layout.fillWidth: true; model: App.project.inputs; textRole: "name"; currentIndex: panel.a; onActivated: index => panel.a = index }
            Caption { text: "B" }
            FfComboBox { Layout.fillWidth: true; model: App.project.inputs; textRole: "name"; currentIndex: panel.b; onActivated: index => panel.b = index }
        }
        RowLayout {
            Caption { text: qsTr("B offset (seconds)") }
            NumberField { value: App.compare.offset; from: -3600; to: 3600; decimals: 3; Layout.preferredWidth: 100; onEdited: v => App.compare.offset = v }
            Caption { text: panel.result.confidence !== undefined ? qsTr("Alignment confidence %1%").arg(Math.round(panel.result.confidence * 100)) : "" }
        }
        Hint { visible: App.compare.error !== ""; text: App.compare.error }
        EmptyState {
            visible: !App.compare.active && panel.result.traceA === undefined && App.compare.error === ""
            Layout.fillWidth: true; Layout.fillHeight: true
            title: qsTr("Choose what to compare")
            text: qsTr("Compare the current input with its processing chain, or choose two inputs.")
        }
        Caption { visible: App.compare.active; text: qsTr("Decoding and measuring…") }
        RowLayout {
            visible: panel.result.traceA !== undefined
            Layout.fillWidth: true; Layout.fillHeight: true
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true
                Trace { title: qsTr("A · Original / first input"); samples: panel.result.traceA || [] }
                Trace { title: qsTr("B · Processed / second input"); samples: panel.result.traceB || [] }
                Trace { title: qsTr("Difference"); tone: Theme.warn; samples: panel.result.difference || [] }
            }
            ColumnLayout {
                Layout.alignment: Qt.AlignTop
                Layout.preferredWidth: 170
                Prop { key: "SDR"; value: panel.score("sdr") }
                Prop { key: "PSNR"; value: panel.score("psnr") }
                Prop { key: qsTr("Peak change"); value: panel.result.peakChange !== undefined ? Format.signedNumber(panel.result.peakChange) + " dB" : "—" }
                Prop { key: qsTr("PCM checksum"); value: panel.result.equal === undefined ? "—" : panel.result.equal ? qsTr("Identical") : qsTr("Different") }
            }
        }
        Hint { text: qsTr("Scores measure the overlap at 48 kHz stereo; PCM checksums cover each aligned stream. Traces show the first 30 seconds; offset search uses the envelope within ±2 seconds.") }
    }
}
