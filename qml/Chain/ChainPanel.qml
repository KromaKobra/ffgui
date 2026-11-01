pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// 3 · Chain: the processing steps between a fixed Input and Output block,
// as a row or as a graph. Nothing here touches the source file.
Item {
    id: chain

    function openPicker(anchor, index) {
        picker.insertAt = index;
        const p = anchor.mapToItem(chain, 0, anchor.height + 4);
        picker.x = Math.max(4, Math.min(p.x - 20, chain.width - picker.width - 4));
        picker.y = Math.max(4, Math.min(p.y, chain.height - picker.height - 4));
        picker.open();
    }

    Rectangle { anchors.fill: parent; color: Theme.chainBackground }

    StepPicker { id: picker }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        PanelToolbar {
            spacing: Metrics.spacingTight
            SegmentedControl {
                options: [{ value: false, label: qsTr("Row") }, { value: true, label: qsTr("Graph") }]
                current: App.view.chainGraph
                onActivated: value => App.view.chainGraph = value
            }
            Caption {
                Layout.leftMargin: Metrics.margin
                Layout.fillWidth: true
                text: App.project.steps.count === 0
                      ? qsTr("No steps: the input is converted as it is. Use + to add one.")
                      : qsTr("%1 of %2 steps active · nothing touches the source file")
                            .arg(App.project.activeSteps).arg(App.project.steps.count)
            }
        }

        Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: App.view.chainGraph ? graphMode : rowMode
        }
    }

    Component {
        id: rowMode
        ChainRowView { onAddRequested: (anchor, at) => chain.openPicker(anchor, at) }
    }
    Component {
        id: graphMode
        ChainGraphView { }
    }
}
