pragma ComponentBehavior: Bound
import QtQuick
import FFgui.Theme

// One vertical level bar: peak (faint) and RMS (solid) over the same
// gradient, a peak-hold line, a clip light that latches until clicked, and an
// optional target line. Values are in dB; `floor` is the bottom of the scale.
Item {
    id: meter

    property real level: floor          // peak
    property real body: floor           // RMS
    property real held: floor
    property bool clipped: false
    property string label
    property real targetLine: NaN
    property real floor: -60

    signal resetClip()

    function frac(db) { return Math.max(0, Math.min(1, (db - meter.floor) / -meter.floor)); }

    implicitWidth: 16

    Rectangle {
        id: clipLight
        width: parent.width
        height: 6
        radius: Metrics.radiusSmall
        color: meter.clipped ? Theme.clip : Theme.control
        TapHandler { onTapped: meter.resetClip() }
    }

    Item {
        id: track
        anchors { top: clipLight.bottom; topMargin: 3; bottom: name.top; bottomMargin: 3 }
        width: parent.width

        Rectangle { anchors.fill: parent; color: Theme.meterTrack; radius: Metrics.radiusSmall }

        Repeater {
            model: [[meter.level, 0.45], [meter.body, 1]]
            delegate: Item {
                required property var modelData
                anchors.bottom: parent.bottom
                width: parent.width
                height: meter.frac(modelData[0]) * track.height
                clip: true
                opacity: modelData[1]
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: track.height
                    gradient: Gradient {
                        GradientStop { position: 0; color: Theme.meterHot }
                        GradientStop { position: 0.1; color: Theme.meterWarm }
                        GradientStop { position: 0.3; color: Theme.meterGood }
                        GradientStop { position: 1; color: Theme.meterLow }
                    }
                }
            }
        }
        Rectangle {
            y: (1 - meter.frac(meter.held)) * track.height
            width: parent.width
            height: 2
            color: Theme.playhead
        }
        Rectangle {
            visible: !isNaN(meter.targetLine)
            y: (1 - meter.frac(meter.targetLine)) * track.height
            x: -2
            width: parent.width + 4
            height: 2
            color: Theme.target
        }
    }

    Text {
        id: name
        anchors.bottom: parent.bottom
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        text: meter.label
        color: Theme.textDim
        font.pixelSize: Metrics.fontTiny
    }
}
