pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// Compare: Original vs Processed, or two inputs, side by side: aligned
// waveforms, the difference, an offset finder and similarity scores.
Item {
    id: panel

    property int mode: 0        // 0 = original vs processed, 1 = two inputs
    readonly property bool possible: panel.mode === 0 ? App.project.activeSteps > 0 : App.project.inputs.count > 1

    component Trace: Item {
        id: trace
        property string label
        property color tone: Theme.wavePeak
        property alias content: holder.data
        Text { text: trace.label; color: Theme.textDim; font.pixelSize: Metrics.fontSmall }
        Rectangle {
            id: holder
            anchors { fill: parent; topMargin: 14 }
            color: Theme.well
            radius: Metrics.radiusSmall
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.spacingLarge
        spacing: Metrics.spacing

        RowLayout {
            Layout.fillWidth: true
            FfComboBox {
                Layout.preferredWidth: 240
                model: [qsTr("Original vs Processed"), qsTr("Two inputs")]
                currentIndex: panel.mode
                onActivated: index => panel.mode = index
            }
            Item { Layout.fillWidth: true }
            Caption { text: qsTr("Offset: %1").arg("—") }
            FfButton {
                text: qsTr("Find offset")
                font.pixelSize: Metrics.fontCaption
                enabled: panel.possible
            }
        }

        EmptyState {
            visible: !panel.possible
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: panel.mode === 0 ? qsTr("Nothing to compare yet") : qsTr("Add a second input")
            text: panel.mode === 0 ? qsTr("Add a step to the chain: Compare then shows what it changes.")
                                   : qsTr("Two inputs are compared aligned, with an offset finder and SDR / PSNR scores.")
        }

        RowLayout {
            visible: panel.possible
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Metrics.spacingWide

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Metrics.spacingSmall
                Trace { label: panel.mode === 0 ? qsTr("A · Original") : qsTr("A"); Layout.fillWidth: true; Layout.fillHeight: true }
                Trace { label: panel.mode === 0 ? qsTr("B · Processed") : qsTr("B"); Layout.fillWidth: true; Layout.fillHeight: true }
                Trace { label: qsTr("Difference"); tone: Theme.warn; Layout.fillWidth: true; Layout.preferredHeight: 34 }
            }

            ColumnLayout {
                Layout.fillWidth: false
                Layout.preferredWidth: 190
                Layout.alignment: Qt.AlignTop
                spacing: Metrics.spacingTight
                Prop { key: "SDR"; value: "—" }
                Prop { key: "PSNR"; value: "—" }
                Prop { key: qsTr("Peak change"); value: "—" }
                Prop { key: qsTr("Checksum"); value: "—" }
            }
        }
    }
}
