pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// Stream out: live destinations (RTMP, Icecast, SRT, HLS) with connection
// lights and their bitrate.
Item {
    id: panel

    function light(status) {
        return status === "live" ? Theme.ok : status === "connecting" ? Theme.warn
             : status === "error" ? Theme.error : Theme.textFaint;
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.spacingLarge
        spacing: Metrics.spacing

        EmptyState {
            visible: App.destinations.count === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            glyph: Glyphs.url
            title: qsTr("No destinations")
            text: qsTr("Stream the project's output live to RTMP (YouTube, Twitch), Icecast, SRT or an HLS folder. Each destination gets its own connection.")
        }

        ListView {
            visible: App.destinations.count > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Metrics.spacingSmall
            clip: true
            model: App.destinations
            delegate: Rectangle {
                id: dest
                required property string name
                required property string url
                required property string format
                required property string status
                required property real bitrate
                width: ListView.view.width
                height: 44
                radius: Metrics.radiusMedium
                color: Theme.card
                StatusDot {
                    x: 10
                    anchors.verticalCenter: parent.verticalCenter
                    color: panel.light(dest.status)
                    tip: dest.status
                }
                Column {
                    x: 30
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 40
                    Text {
                        text: dest.name + "   ·   " + dest.format + (dest.bitrate > 0 ? "   ·   " + Format.bitrate(dest.bitrate) : "")
                        color: Theme.text
                        font.pixelSize: Metrics.fontBody
                        font.bold: true
                    }
                    Text { width: parent.width; text: dest.url; color: Theme.textDim; font.pixelSize: Metrics.fontSmall; elide: Text.ElideMiddle }
                }
            }
        }
    }
}
