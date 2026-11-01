pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// The Meters strip: live levels of what is playing, or the values at the
// playhead when stopped. Peak and RMS per channel with peak hold and a clip
// light, short-term loudness against the Normalize target, true peak, LUFS
// and stereo correlation. Clicking the readouts opens Scopes.
Item {
    id: meters

    readonly property bool wide: width >= 120
    readonly property real target: App.meters.target
    readonly property bool hasTarget: !isNaN(meters.target)
    readonly property real floor: App.meters.floor

    function db(v) { return v <= meters.floor ? "−∞" : Format.number(v); }

    Rectangle { anchors.fill: parent; color: Theme.well }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.spacing
        spacing: Metrics.spacing

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Metrics.spacingSmall

            DbScale {
                visible: meters.wide
                Layout.fillHeight: true
                floor: meters.floor
            }

            Repeater {
                model: App.meters.channelNames
                delegate: LevelMeter {
                    required property int index
                    required property string modelData
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.maximumWidth: 26
                    floor: meters.floor
                    level: App.meters.peak[index] ?? meters.floor
                    body: App.meters.rms[index] ?? meters.floor
                    held: App.meters.hold[index] ?? meters.floor
                    clipped: App.meters.clip[index] ?? false
                    label: modelData
                    onResetClip: App.meters.resetClip(index)
                }
            }

            // Short-term loudness against the Normalize target.
            LevelMeter {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.maximumWidth: 26
                Layout.leftMargin: Metrics.spacingSmall
                floor: meters.floor
                level: App.meters.momentary
                body: App.meters.shortTerm
                held: App.meters.shortTerm
                targetLine: meters.target
                label: "LU"
            }
        }

        ReadoutGrid {
            Layout.fillWidth: true
            readouts: [
                { label: "TP", value: meters.db(App.meters.truePeak) + (meters.wide ? " dBTP" : "") },
                { label: "M", value: meters.db(App.meters.momentary) },
                { label: "S", value: meters.db(App.meters.shortTerm) },
                {
                    label: "I",
                    value: meters.db(App.meters.integrated) + (meters.wide ? " LUFS" : ""),
                    // Loudness compliance: integrated within ±1 LU of the target.
                    color: !meters.hasTarget || App.meters.integrated <= meters.floor ? Theme.text
                           : Math.abs(App.meters.integrated - meters.target) <= 1 ? Theme.ok : Theme.errorSoft
                }
            ]
            HoverHandler { cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: App.requestPanel("scopes") }
        }

        CorrelationMeter {
            visible: App.meters.channelNames.length === 2
            Layout.fillWidth: true
            value: App.meters.correlation
        }
    }
}
