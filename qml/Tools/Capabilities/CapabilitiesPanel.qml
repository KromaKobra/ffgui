pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// Capabilities: what this FFmpeg build supports, and which ffgui features
// each optional library unlocks (ffmpeg -version, -buildconf, -filters,
// -encoders, -muxers).
Item {
    id: panel

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ColumnLayout {
            Layout.margins: Metrics.spacingWide
            spacing: Metrics.spacingTight
            Label { text: App.caps.summary; font.bold: true; font.pixelSize: Metrics.fontTitle }
            Caption {
                visible: App.caps.state === "ready"
                text: qsTr("%1 filters · %2 encoders · %3 muxers").arg(App.caps.filterCount)
                          .arg(App.caps.encoderCount).arg(App.caps.muxerCount)
            }
            Caption {
                visible: App.caps.ffmpegPath !== ""
                text: App.caps.ffmpegPath
                font.family: Metrics.monoFamily
            }
        }

        EmptyState {
            visible: App.caps.state !== "ready"
            Layout.fillWidth: true
            Layout.fillHeight: true
            glyph: Glyphs.warning
            title: App.caps.state === "missing" ? qsTr("FFmpeg wasn't found") : qsTr("Checking FFmpeg…")
            text: App.caps.state === "missing"
                  ? qsTr("Install ffmpeg (with ffprobe and ffplay), or set its location in Settings.")
                  : qsTr("Reading the version, the build configuration and the filter list.")
            actionText: App.caps.state === "missing" ? qsTr("Check again") : ""
            onAction: App.caps.refresh()
        }

        ListView {
            visible: App.caps.state === "ready"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: App.caps.libraries
            ScrollBar.vertical: ScrollBar { }
            delegate: StripedRow {
                id: lib
                required property int index
                row: lib.index
                required property string name
                required property bool found
                required property string unlocks
                width: ListView.view.width
                height: 32
                RowLayout {
                    anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                    spacing: Metrics.margin
                    Text {
                        text: lib.found ? Glyphs.check : Glyphs.cross
                        color: lib.found ? Theme.ok : Theme.error
                        font.pixelSize: Metrics.fontLarge
                    }
                    Text {
                        text: lib.name
                        color: Theme.text
                        font.family: Metrics.monoFamily
                        font.pixelSize: Metrics.fontCaption
                        Layout.preferredWidth: 120
                    }
                    Text {
                        text: (lib.found ? "" : qsTr("Unavailable: ")) + lib.unlocks
                        color: lib.found ? Theme.textMuted : Theme.warn
                        font.pixelSize: Metrics.fontCaption
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }
}
