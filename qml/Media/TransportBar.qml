pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// One row under the Media View: play, stop, loop, markers, position, preview
// volume, channel solo, preview speed and the A/B switch. Space plays and the
// arrow keys nudge, as in ffplay.
Rectangle {
    id: transport

    readonly property bool roomy: width >= 720
    readonly property var channels: {
        const n = App.project.track.channels || 0;
        return n === 1 ? ["M"] : n === 2 ? ["L", "R"] : (App.project.track.channelNames || []);
    }

    implicitHeight: Metrics.transportHeight
    color: Theme.base

    Rectangle { width: parent.width; height: 1; color: Theme.line }

    RowLayout {
        anchors { fill: parent; leftMargin: 6; rightMargin: 8 }
        spacing: Metrics.spacingTight

        FfToolButton {
            text: Glyphs.previous
            enabled: App.transport.canPlay
            tip: qsTr("Previous marker")
            onClicked: App.transport.previousMarker()
        }
        FfToolButton {
            text: App.transport.playing ? Glyphs.pause : Glyphs.play
            font.pixelSize: Metrics.fontTitle
            enabled: App.transport.canPlay
            tip: qsTr("Play / pause (Space)")
            onClicked: App.transport.toggle()
        }
        FfToolButton {
            text: Glyphs.stop
            enabled: App.transport.canPlay
            tip: qsTr("Stop")
            onClicked: App.transport.stop(App.selection.isRange ? App.selection.start : 0)
        }
        FfToolButton {
            text: Glyphs.loop
            checked: App.transport.loop
            tip: qsTr("Loop the selection")
            onClicked: App.transport.loop = !App.transport.loop
        }
        FfToolButton {
            text: Glyphs.next
            enabled: App.transport.canPlay
            tip: qsTr("Next marker")
            onClicked: App.transport.nextMarker()
        }

        Label {
            Layout.leftMargin: Metrics.spacingLarge
            font.family: Metrics.monoFamily
            font.pixelSize: Metrics.fontBody
            text: Format.timecode(App.transport.position, true) + "  /  " + Format.timecode(App.transport.duration, true)
        }

        ToolSeparator { visible: transport.roomy }

        Label { visible: transport.roomy; text: Glyphs.speaker; font.pixelSize: Metrics.fontBody; opacity: 0.7 }
        FfSlider {
            visible: transport.roomy
            Layout.preferredWidth: 80
            value: App.transport.volume
            liveText: qsTr("Preview volume %1%").arg(Math.round(value * 100))
            tip: qsTr("Playback volume (preview only)")
            onMoved: App.transport.volume = value
        }
        Repeater {
            model: transport.channels
            delegate: FfToolButton {
                required property int index
                required property string modelData
                visible: transport.roomy
                text: modelData
                font.pixelSize: Metrics.fontCaption
                checked: (App.transport.soloMask & (1 << index)) !== 0
                tip: qsTr("Solo %1").arg(modelData)
                onClicked: App.transport.toggleSolo(index)
            }
        }
        FfComboBox {
            visible: transport.roomy
            Layout.preferredWidth: 78
            model: ["0.5×", "0.75×", "1×", "1.5×", "2×"]
            currentIndex: [0.5, 0.75, 1, 1.5, 2].indexOf(App.transport.speed)
            tip: qsTr("Playback speed (preview only)")
            onActivated: index => App.transport.speed = [0.5, 0.75, 1, 1.5, 2][index]
        }

        Item { Layout.fillWidth: true }

        FfToolButton {
            text: "ffplay"
            font.pixelSize: Metrics.fontCaption
            enabled: App.project.hasInput
            tip: qsTr("Open in ffplay from the playhead, for reference listening")
            onClicked: App.openInFfplay()
        }
        ToolSeparator { }

        Label { text: "A/B"; font.pixelSize: Metrics.fontCaption; opacity: 0.6 }
        Row {
            // Disabled buttons get no hover, so the reason hangs on the row.
            HoverHandler { id: abHover }
            TipLabel { visible: abHover.hovered && !App.transport.abEnabled; text: App.transport.abReason }
            Repeater {
                model: [["original", qsTr("A  Original")], ["processed", qsTr("B  Processed")]]
                delegate: FfButton {
                    required property var modelData
                    text: transport.roomy ? modelData[1] : modelData[1].charAt(0)
                    font.pixelSize: Metrics.fontCaption
                    implicitHeight: Metrics.controlHeight
                    enabled: App.transport.abEnabled
                    flat: App.transport.ab !== modelData[0]
                    highlighted: App.transport.ab === modelData[0]
                    onClicked: App.transport.ab = modelData[0]
                }
            }
        }
    }
}
