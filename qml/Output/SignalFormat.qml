pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// Sample rate, bit depth and channels, each "same as source" by default.
RowLayout {
    id: signalFormat

    property int index: -1
    property var target: ({})
    readonly property bool lossy: App.catalog.codec(signalFormat.target.codec || "").lossy === true

    component Field: FfComboBox {
        id: field
        property string key
        property var choices: []
        Layout.preferredWidth: 120
        textRole: "text"
        model: field.choices
        currentIndex: field.choices.findIndex(c => c.value === signalFormat.target[field.key])
        onActivated: index => {
            const change = {};
            change[field.key] = field.choices[index].value;
            App.project.setOutput(signalFormat.index, change);
        }
    }

    Layout.fillWidth: false
    spacing: Metrics.spacingLarge

    Field {
        key: "rate"
        Layout.preferredWidth: 116
        choices: [{ text: qsTr("Rate: source"), value: 0 }, { text: "44.1 kHz", value: 44100 },
                  { text: "48 kHz", value: 48000 }, { text: "96 kHz", value: 96000 }]
    }
    Field {
        key: "depth"
        Layout.preferredWidth: 112
        enabled: !signalFormat.lossy
        choices: [{ text: signalFormat.lossy ? qsTr("Depth: n/a") : qsTr("Depth: source"), value: 0 },
                  { text: "16-bit", value: 16 }, { text: "24-bit", value: 24 }]
    }
    Field {
        key: "channels"
        Layout.preferredWidth: 136
        choices: [{ text: qsTr("Channels: source"), value: 0 }, { text: qsTr("Mono"), value: 1 },
                  { text: qsTr("Stereo"), value: 2 }]
    }
}
