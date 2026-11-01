pragma ComponentBehavior: Bound
import QtQuick
import FFgui.Backend
import FFgui.Theme

// One channel lane: the waveform on top and the spectrogram beneath, with a
// draggable divider between them, plus the per-channel overlays (clipping
// marks, DC offset line).
Item {
    id: lane

    required property int channel
    required property string name

    property real viewStart: 0
    property real viewEnd: 1
    property real labelWidth: Metrics.laneLabelWidth
    property real nyquist: 24000
    property var clips: []              // times of clipped samples
    property real dc: NaN               // linear DC offset of this channel

    readonly property bool showWave: App.view.layers.waveform === true
    readonly property bool showSpec: App.view.layers.spectrogram === true
    readonly property real share: !lane.showSpec ? 0 : !lane.showWave ? 1 : App.view.spectrogramShare
    readonly property real waveHeight: (height - 1) * (1 - lane.share)
    readonly property real tracksWidth: width - lane.labelWidth
    readonly property real span: Math.max(1e-6, lane.viewEnd - lane.viewStart)

    // The waveform and spectrogram renderers fill these.
    property alias waveArea: wave
    property alias spectrogramArea: spec

    function xOf(t) { return lane.labelWidth + (t - lane.viewStart) / lane.span * lane.tracksWidth; }

    Rectangle { width: lane.labelWidth; height: parent.height; color: Theme.laneLabel }
    Text {
        width: lane.labelWidth
        y: 6
        horizontalAlignment: Text.AlignHCenter
        text: lane.name
        color: Theme.textMuted
        font.bold: true
        font.pixelSize: Metrics.fontCaption
    }
    Rectangle { y: parent.height - 1; width: parent.width; height: 1; color: Theme.line }

    Item {
        id: wave
        visible: lane.showWave
        x: lane.labelWidth
        width: lane.tracksWidth
        height: lane.waveHeight
        clip: true
        Rectangle { y: parent.height / 2; width: parent.width; height: 1; color: Theme.waveCentre }
    }

    // Clipping marks above the samples at full scale.
    Repeater {
        model: App.view.layers.clipping && lane.showWave ? lane.clips : []
        delegate: Rectangle {
            required property real modelData
            visible: modelData >= lane.viewStart && modelData <= lane.viewEnd
            x: lane.xOf(modelData) - 1
            y: 1
            width: 3
            height: 7
            color: Theme.clip
        }
    }

    // DC offset line at the channel's average.
    Rectangle {
        visible: App.view.layers.dc === true && lane.showWave && !isNaN(lane.dc)
        x: lane.labelWidth
        y: lane.waveHeight / 2 * (1 - (isNaN(lane.dc) ? 0 : lane.dc))
        width: lane.tracksWidth
        height: 1
        color: Theme.dcLine
        Text {
            anchors { right: parent.right; bottom: parent.top; rightMargin: 6 }
            text: "DC " + Format.signedNumber((isNaN(lane.dc) ? 0 : lane.dc) * 100, 2) + " %"
            color: Theme.dcLine
            font.pixelSize: Metrics.fontTiny
        }
    }

    Item {
        id: spec
        visible: lane.showSpec
        x: lane.labelWidth
        y: lane.waveHeight + (lane.showWave ? 1 : 0)
        width: lane.tracksWidth
        height: parent.height - y - 1
        clip: true
        Rectangle { anchors.fill: parent; color: Theme.well; z: -1 }
        Text {
            x: 4
            y: 2
            z: 1
            text: Format.frequency(lane.nyquist)
            color: Theme.textOnAccent
            opacity: 0.5
            font.pixelSize: Metrics.fontTiny
        }
        Text {
            x: 4
            z: 1
            anchors.bottom: parent.bottom
            text: Format.frequency(20)
            color: Theme.textOnAccent
            opacity: 0.5
            font.pixelSize: Metrics.fontTiny
        }
    }

    // Divider between waveform and spectrogram.
    Rectangle {
        visible: lane.showWave && lane.showSpec
        x: lane.labelWidth
        y: lane.waveHeight - 1
        width: lane.tracksWidth
        height: 3
        z: 2
        color: divider.containsMouse || divider.pressed ? Theme.accent : Theme.line
        MouseArea {
            id: divider
            anchors.fill: parent
            anchors.margins: -3
            hoverEnabled: true
            cursorShape: Qt.SplitVCursor
            onPositionChanged: ev => {
                if (pressed)
                    App.view.spectrogramShare = 1 - mapToItem(lane, ev.x, ev.y).y / lane.height;
            }
        }
    }
}
