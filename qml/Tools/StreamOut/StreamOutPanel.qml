pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme
Item {
    id: panel
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.spacingLarge
        RowLayout {
            FfTextField { id: destinationName; Layout.preferredWidth: 110; placeholderText: qsTr("Name") }
            FfComboBox { id: protocol; model: ["RTMP", "Icecast", "SRT", "HLS"]; Layout.preferredWidth: 95 }
            FfTextField { id: address; Layout.fillWidth: true; placeholderText: protocol.currentIndex === 3 ? qsTr("/folder/stream.m3u8") : protocol.currentIndex === 1 ? "icecast://source:password@host:8000/live" : protocol.currentIndex === 2 ? "srt://host:9000" : "rtmp://host/live/key" }
            FfButton { text: qsTr("Add"); enabled: address.text.trim() !== ""; onClicked: { App.stream.add(destinationName.text || protocol.currentText, ["rtmp", "icecast", "srt", "hls"][protocol.currentIndex], address.text.trim(), bitrate.value); address.text = ""; } }
        }
        RowLayout {
            Caption { text: qsTr("Bitrate (kb/s)") }
            FfSpinBox { id: bitrate; from: 8; to: 512; value: 128; stepSize: 8 }
            Item { Layout.fillWidth: true }
            FfButton { text: qsTr("Stop all"); enabled: App.destinations.count > 0; onClicked: App.stream.stopAll() }
        }
        EmptyState {
            visible: App.destinations.count === 0
            Layout.fillWidth: true; Layout.fillHeight: true
            title: qsTr("No destinations")
            text: qsTr("Add RTMP, Icecast, SRT, or a new local HLS playlist path. Each destination gets its own connection.")
        }
        ListView {
            visible: App.destinations.count > 0
            Layout.fillWidth: true; Layout.fillHeight: true
            spacing: Metrics.spacing
            clip: true
            model: App.destinations
            delegate: Rectangle {
                id: destination
                required property int index
                required property string name
                required property string url
                required property string status
                required property real bitrate
                required property string error
                required property var history
                readonly property bool active: status === "live" || status === "connecting"
                width: ListView.view.width
                height: error !== "" ? 102 : 80
                color: Theme.card
                radius: Metrics.radius
                ColumnLayout {
                    anchors { fill: parent; margins: 8 }
                    RowLayout {
                        StatusDot { color: destination.status === "live" ? Theme.ok : destination.status === "error" ? Theme.error : destination.status === "connecting" ? Theme.warn : Theme.textFaint; tip: destination.status }
                        Caption { text: destination.name + " · " + destination.status; Layout.fillWidth: true }
                        Caption { text: Format.bitrate(destination.bitrate) }
                        FfButton { text: destination.active ? qsTr("Stop") : qsTr("Start"); enabled: destination.active || App.project.hasInput; onClicked: destination.active ? App.stream.stop(destination.index) : App.stream.start(destination.index) }
                        FfToolButton { text: Glyphs.remove; enabled: !destination.active; onClicked: App.stream.remove(destination.index) }
                    }
                    Caption { text: destination.url; Layout.fillWidth: true; elide: Text.ElideMiddle }
                    ToolPlot { values: destination.history.map(v => Math.min(1, v / 512000)); kind: "history"; tone: Theme.accent; gridColor: Theme.line; Layout.fillWidth: true; Layout.preferredHeight: 18 }
                    Hint { visible: destination.error !== ""; text: destination.error }
                }
            }
        }
    }
}
