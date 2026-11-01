pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// Generate: tones, silence, noise and formulas from lavfi sources (speech
// too, when the build has flite). The result appears as a new input.
Item {
    id: panel

    property string kind: "tone"
    property real frequency: 1000
    property real amplitude: 0.5
    property real length: 5
    property int rate: 48000
    property string noiseColor: "pink"
    property string formula: "0.5*sin(440*2*PI*t)"
    property string speech: qsTr("Testing, one, two, three")

    // Reading state makes this re-check once the capabilities arrive.
    readonly property bool flite: App.caps.state === "ready" && App.caps.hasFilter("flite")
    readonly property string source: {
        switch (panel.kind) {
        case "tone": return "sine=frequency=" + Math.round(panel.frequency) + ":sample_rate=" + panel.rate + ":duration=" + panel.length + ",volume=" + (panel.amplitude * 8);
        case "silence": return "anullsrc=r=" + panel.rate + ":cl=stereo:d=" + panel.length;
        case "noise": return "anoisesrc=color=" + panel.noiseColor + ":amplitude=" + panel.amplitude + ":r=" + panel.rate + ":d=" + panel.length;
        case "formula": return "aevalsrc='" + panel.formula + "':s=" + panel.rate + ":d=" + panel.length;
        default: return "flite=text='" + panel.speech.replace(/'/g, "") + "'";
        }
    }
    readonly property string name: {
        switch (panel.kind) {
        case "tone": return qsTr("Tone %1 Hz").arg(Math.round(panel.frequency));
        case "silence": return qsTr("Silence");
        case "noise": return qsTr("%1 noise").arg(panel.noiseColor);
        case "formula": return qsTr("Formula");
        default: return qsTr("Speech");
        }
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: scroll.availableWidth - 20
            x: 10
            spacing: Metrics.spacingLarge

            Item { implicitHeight: 2 }
            SegmentedControl {
                options: [{ value: "tone", label: qsTr("Tone") }, { value: "silence", label: qsTr("Silence") },
                          { value: "noise", label: qsTr("Noise") }, { value: "formula", label: qsTr("Formula") },
                          { value: "speech", label: qsTr("Speech"), enabled: panel.flite,
                            tip: panel.flite ? "" : qsTr("Needs flite, which this build lacks") }]
                current: panel.kind
                onActivated: value => panel.kind = value
            }
            ParamControl {
                visible: panel.kind === "tone"
                option: ({ key: "frequency", label: qsTr("Frequency"), unit: "Hz", min: 20, max: 20000, step: 1, "default": 1000 })
                value: panel.frequency
                onEdited: v => panel.frequency = v
            }
            ParamControl {
                visible: panel.kind === "tone"
                option: ({ key: "amplitude", label: qsTr("Amplitude"), unit: "", min: 0, max: 1, step: 0.01, "default": 0.5 })
                value: panel.amplitude
                onEdited: v => panel.amplitude = v
            }
            ColumnLayout {
                visible: panel.kind === "noise"
                Layout.fillWidth: true
                Label { text: qsTr("Colour"); font.pixelSize: Metrics.fontBody }
                Flow {
                    Layout.fillWidth: true
                    spacing: Metrics.spacingSmall
                    Repeater {
                        model: ["white", "pink", "brown", "blue", "violet"]
                        delegate: Rectangle {
                            id: swatch
                            required property string modelData
                            width: 26
                            height: 26
                            radius: 13
                            color: Theme.noise[modelData]
                            border.width: panel.noiseColor === modelData ? 3 : 0
                            border.color: Theme.accent
                            HoverHandler { id: swatchHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: panel.noiseColor = swatch.modelData }
                            TipLabel { visible: swatchHover.hovered; text: swatch.modelData }
                        }
                    }
                }
                ParamControl {
                    option: ({ key: "amplitude", label: qsTr("Amplitude"), unit: "", min: 0, max: 1, step: 0.01, "default": 0.5 })
                    value: panel.amplitude
                    onEdited: v => panel.amplitude = v
                }
            }
            FormRow {
                visible: panel.kind === "formula"
                label: qsTr("Expression")
                FfTextField {
                    Layout.fillWidth: true
                    mono: true
                    text: panel.formula
                    tip: qsTr("aevalsrc expression in t (seconds), e.g. 0.5*sin(440*2*PI*t)")
                    onEditingFinished: panel.formula = text
                }
            }
            FfTextField {
                visible: panel.kind === "speech"
                Layout.fillWidth: true
                text: panel.speech
                onEditingFinished: panel.speech = text
            }
            ParamControl {
                visible: panel.kind !== "speech"
                option: ({ key: "duration", label: qsTr("Length"), unit: "s", min: 0.5, max: 600, step: 0.5, "default": 5 })
                value: panel.length
                onEdited: v => panel.length = v
            }
            FormRow {
                visible: panel.kind !== "speech"
                label: qsTr("Sample rate")
                FfComboBox {
                    Layout.fillWidth: true
                    model: ["44.1 kHz", "48 kHz", "96 kHz"]
                    currentIndex: [44100, 48000, 96000].indexOf(panel.rate)
                    onActivated: index => panel.rate = [44100, 48000, 96000][index]
                }
            }
            FfButton {
                Layout.fillWidth: true
                text: App.project.generating ? qsTr("Generating…") : qsTr("Generate as new input")
                enabled: !App.project.generating
                highlighted: true
                onClicked: App.project.addGenerator(panel.source, panel.name)
            }
            CommandText { text: "-f lavfi -i \"" + panel.source + "\"" }
        }
    }
}
