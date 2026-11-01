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

    readonly property var codecs: ["pcm_s24le", "flac", "pcm_s16le"]

    property int device: 0
    property int format: 0
    onFormatChanged: App.capture.output = App.capture.output.replace(/\.[^/.]+$/, "." + panel.formats[panel.format].ext)
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

    ColumnLayout {
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 8 }
        id: recordActions
        RowLayout {
            Layout.fillWidth: true
            FfButton { text: qsTr("Refresh devices"); enabled: !App.capture.refreshing && !App.capture.active; onClicked: App.capture.refreshDevices() }
            Caption { text: App.capture.active ? qsTr("Recording %1").arg(Format.timecode(App.capture.elapsed, true)) : App.capture.refreshing ? qsTr("Looking for devices…") : qsTr("Ready") }
            Caption { text: App.capture.active ? Format.dbValue(App.capture.level) + " dBFS" : "" }
        }
        Label { visible: App.capture.error !== ""; text: App.capture.error; color: Theme.errorSoft; wrapMode: Text.Wrap; Layout.fillWidth: true }
    }

    EmptyState {
        anchors { fill: parent; topMargin: recordActions.height + 16 }
        visible: App.devices.count === 0
        glyph: Glyphs.device
        title: qsTr("No capture devices found")
        text: qsTr("Recording uses PulseAudio or PipeWire (pulse) and ALSA devices that ffmpeg can see.")
    }

    ScrollView {
        id: scroll
        anchors { fill: parent; topMargin: recordActions.height + 16 }
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
                enabled: !App.capture.active
                Layout.fillWidth: true
                model: App.devices
                textRole: "name"
                currentIndex: panel.device
                onActivated: index => panel.device = index
            }

            GridLayout {
                enabled: !App.capture.active
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
            FfCheckBox { enabled: !App.capture.active; text: qsTr("Split into 15-minute files"); checked: panel.split; onToggled: panel.split = checked }
            FfCheckBox { enabled: !App.capture.active; text: qsTr("Monitor input level"); checked: panel.monitor; onToggled: panel.monitor = checked }

            FormRow {
                enabled: !App.capture.active
                label: qsTr("Save to")
                FfTextField { Layout.fillWidth: true; text: App.capture.output; onEditingFinished: App.capture.output = text }
                FfButton { text: qsTr("Choose…"); onClicked: App.requestDialog("recordingFile", { extension: panel.formats[panel.format].ext }) }
            }
            RowLayout {
                FfButton {
                    objectName: "startRecording"
                    text: App.capture.active ? qsTr("Stop recording") : qsTr("Record")
                    highlighted: true
                    enabled: App.capture.active || panel.deviceId !== ""
                    onClicked: {
                        if (App.capture.active) { App.capture.stop(); return; }
                        const path = App.capture.output;
                        if (panel.split && path.indexOf("%") < 0)
                            App.capture.output = path.replace(/(\.[^/.]+)$/, "_%03d$1");
                        App.capture.start({ backend: panel.backend, device: panel.deviceId,
                            codec: panel.codecs[panel.format], rate: panel.format === 2 ? 44100 : 48000,
                            depth: panel.format === 2 ? 16 : 24, monitor: panel.monitor,
                            seconds: panel.limit === 1 ? 600 : panel.limit === 2 ? 3600 : 0,
                            maxBytes: panel.limit === 3 ? 2000000000 : 0, splitSeconds: panel.split ? 900 : 0 });
                    }
                }
            }
            CommandText {
                text: ["ffmpeg -f", panel.backend, "-i", panel.deviceId, panel.limits[panel.limit].args,
                       panel.formats[panel.format].args,
                       panel.split ? "-f segment -segment_time 900 rec_%03d." + panel.formats[panel.format].ext
                                   : "rec." + panel.formats[panel.format].ext].filter(s => s !== "").join(" ")
            }
        }
    }
}
