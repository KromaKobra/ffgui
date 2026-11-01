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

    // A track dragged from Sources ("input:track:audioIndex"): in Graph mode
    // it becomes a source where it lands; in Row mode the chain's input.
    function dropTrack(data, x, y) {
        const parts = String(data).split(":").map(Number);
        if (parts.length !== 3 || parts.some(isNaN))
            return;
        if (App.view.chainGraph) {
            const view = mode.item as ChainGraphView;
            if (view)
                view.dropTrack(parts[0], parts[2], view.mapFromItem(mode, x, y));
            return;
        }
        App.project.currentInput = parts[0];
        App.project.currentTrack = parts[1];
        App.selection.select("track", { index: parts[0], track: parts[1] });
    }

    function openPicker(anchor, index) {
        picker.insertAt = index;
        const p = anchor.mapToItem(chain, 0, anchor.height + 4);
        picker.x = Math.max(4, Math.min(p.x - 20, chain.width - picker.width - 4));
        picker.y = Math.max(4, Math.min(p.y, chain.height - picker.height - 4));
        picker.open();
    }

    Rectangle { anchors.fill: parent; color: Theme.chainBackground }

    StepPicker { id: picker }
    Connections {
        target: App
        function onSelectionStepRequested() { chain.openPicker(chain, App.project.steps.count); }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        PanelToolbar {
            spacing: Metrics.spacingTight
            SegmentedControl {
                options: [{ value: false, label: qsTr("Row"), enabled: App.project.canUseRow, tip: App.project.canUseRow ? "" : qsTr("This graph has branches") }, { value: true, label: qsTr("Graph") }]
                current: App.view.chainGraph
                onActivated: value => { if (value || App.project.useRow()) App.view.chainGraph = value; }
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

        Item {
            objectName: "chainArea"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Loader {
                id: mode
                anchors.fill: parent
                sourceComponent: App.view.chainGraph ? graphMode : rowMode
            }
            DropArea {
                id: trackDrop
                anchors.fill: parent
                keys: ["application/x-ffgui-track"]
                onDropped: drop => {
                    chain.dropTrack(drop.getDataAsString("application/x-ffgui-track"), drop.x, drop.y);
                    drop.acceptProposedAction();
                }
            }
            Rectangle {
                anchors.fill: parent
                visible: trackDrop.containsDrag
                color: "transparent"
                border.width: 2
                border.color: Theme.accent
                radius: Metrics.radius
            }
        }
    }

    Component {
        id: rowMode
        ChainRowView { onAddRequested: (anchor, at) => chain.openPicker(anchor, at) }
    }
    Component {
        id: graphMode
        ChainGraphView { onAddRequested: chain.openPicker(chain, App.project.steps.count) }
    }
}
