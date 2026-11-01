import QtQuick
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// Copy / Edit / Strip. Edit opens the Metadata panel.
RowLayout {
    id: policy

    property int index: -1
    property string current: "copy"

    Layout.fillWidth: false
    spacing: Metrics.spacingSmall

    Caption { text: qsTr("Metadata"); font.pixelSize: Metrics.fontBody; opacity: 0.7 }
    SegmentedControl {
        options: [{ value: "copy", label: qsTr("Copy") }, { value: "edit", label: qsTr("Edit") },
                  { value: "strip", label: qsTr("Strip") }]
        current: policy.current
        onActivated: value => {
            App.project.setOutput(policy.index, { metadata: value });
            if (value === "edit")
                App.requestPanel("metadata");
        }
    }
}
