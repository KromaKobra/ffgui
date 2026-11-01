pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// Record: device, input level, format, limits, splitting, monitoring. A
// finished recording appears as a new input.
Item {
    id: panel

    property int device: 0
    property int format: 0
    property int limit: 0
    property bool split: false
    property bool monitor: true

    readonly property var formats: [
        { label: "WAV · 48 kHz · 24-bit", ext: "wav", args: "-c:a pcm_s24le -ar 48000" },
        { label: "FLAC · 48 kHz · 24-bit", ext: "flac", args: "-c:a flac -ar 48000 -sample_fmt s32" },
        { label: "WAV · 44.1 kHz · 16-bit", ext: "wav", args: "-c:a pcm_s16le -ar 44100" }
    ]
    readonly property var limits: [
        { label: qsTr("No limit"), args: "" }, { label: qsTr("10 minutes"), args: "-t 600" },
        { label: qsTr("1 hour"), args: "-t 3600" }, { label: "2 GB", args: "-fs 2000000000" }
    ]
    readonly property string deviceId: App.devices.value(panel.device, "id") || ""
    readonly property string backend: App.devices.value(panel.device, "backend") || "pulse"

    EmptyState {
        anchors.fill: parent
        visible: App.devices.count === 0
        glyph: Glyphs.device
        title: qsTr("No capture devices found")
        text: qsTr("Recording uses PulseAudio or PipeWire (pulse) and ALSA devices that ffmpeg can see.")
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        visible: App.devices.count > 0
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: scroll.availableWidth - 20
            x: 10
            spacing: Metrics.spacingLarge

            Item { implicitHeight: 2 }
            Caption { text: qsTr("Device (%1)").arg(panel.backend) }
            FfComboBox {
                Layout.fillWidth: true
                model: App.devices
                textRole: "name"
                currentIndex: panel.device
                onActivated: index => panel.device = index
            }

            GridLayout {
                columns: 2
                Layout.fillWidth: true
                Caption { text: qsTr("Format") }
                FfComboBox {
                    Layout.fillWidth: true
                    model: panel.formats.map(f => f.label)
                    currentIndex: panel.format
                    onActivated: index => panel.format = index
                }
                Caption { text: qsTr("Stop after") }
                FfComboBox {
                    Layout.fillWidth: true
                    model: panel.limits.map(l => l.label)
                    currentIndex: panel.limit
                    onActivated: index => panel.limit = index
                }
            }
            FfCheckBox { text: qsTr("Split into 15-minute files"); checked: panel.split; onToggled: panel.split = checked }
            FfCheckBox { text: qsTr("Monitor while recording"); checked: panel.monitor; onToggled: panel.monitor = checked }

            CommandText {
                text: ["ffmpeg -f", panel.backend, "-i", panel.deviceId, panel.limits[panel.limit].args,
                       panel.formats[panel.format].args,
                       panel.split ? "-f segment -segment_time 900 rec_%03d." + panel.formats[panel.format].ext
                                   : "rec." + panel.formats[panel.format].ext].filter(s => s !== "").join(" ")
            }
        }
    }
}
