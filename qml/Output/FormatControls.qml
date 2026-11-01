pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// Preset, container, codec (only those the container accepts) and the
// quality control for the current output target.
RowLayout {
    id: controls

    property int index: -1
    property var target: ({})

    readonly property var containers: App.catalog.containers
    readonly property var codecs: App.catalog.revision >= 0 ? App.catalog.codecsFor(controls.target.container || "") : []
    readonly property var codec: App.catalog.codec(controls.target.codec || "")

    component Field: FfComboBox {
        Layout.preferredWidth: 120
        Layout.minimumWidth: 70
        Layout.fillWidth: true
        Layout.maximumWidth: 170
    }

    Layout.fillWidth: false
    spacing: Metrics.spacingLarge

    Field {
        readonly property var names: {
            const list = [];
            for (let i = 0; i < App.catalog.presets.count; ++i)
                list.push(App.catalog.presets.value(i, "name"));
            return list.concat([qsTr("Custom")]);
        }
        model: names
        currentIndex: Math.max(0, names.indexOf(controls.target.preset || "Custom"))
        tip: qsTr("Preset")
        onActivated: index => {
            if (index < names.length - 1)
                App.project.applyPreset(controls.index, names[index]);
        }
    }
    Field {
        Layout.preferredWidth: 80
        model: controls.containers.map(c => c.label)
        currentIndex: controls.containers.findIndex(c => c.key === controls.target.container)
        tip: qsTr("Container")
        onActivated: index => App.project.setOutput(controls.index, { container: controls.containers[index].key })
    }
    Field {
        model: controls.codecs.map(c => c.label)
        currentIndex: controls.codecs.findIndex(c => c.key === controls.target.codec)
        tip: qsTr("Codec (%1)").arg(controls.target.codec || "")
        onActivated: index => App.project.setOutput(controls.index, { codec: controls.codecs[index].key })
    }
    QualityControl {
        quality: controls.codec.quality || {}
        value: controls.target.quality || 0
        kbps: controls.target.kbps || 0
        onEdited: v => App.project.setOutput(controls.index, { quality: v })
    }
}
