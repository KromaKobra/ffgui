pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// Inspector: a property sheet for whatever is selected (an input, a track, a
// chain step, a time range or an output target), the project otherwise.
Item {
    id: inspector

    readonly property string kind: App.selection.kind
    readonly property int index: App.selection.index

    ModelRow {
        id: stepRow
        model: App.project.steps
        row: inspector.kind === "step" ? inspector.index : -1
    }
    readonly property var stepSpec: stepRow.valid ? App.catalog.step(stepRow.values.type) : ({})

    readonly property var heading: {
        switch (inspector.kind) {
        case "input": return [qsTr("Input"), App.project.inputs.value(inspector.index, "name") || ""];
        case "track": return [qsTr("Track"), (App.project.inputs.value(inspector.index, "name") || "") + " · #" + App.selection.track];
        case "step": return [qsTr("Chain step"), stepRow.values.name || ""];
        case "range": return [qsTr("Time selection"), ""];
        case "output": return [qsTr("Output"), App.project.outputs.value(inspector.index, "fileName") || ""];
        default: return [qsTr("Project"), App.project.name];
        }
    }

    Rectangle { anchors.fill: parent; color: Theme.panel }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        PanelToolbar {
            implicitHeight: 34
            Label {
                text: inspector.heading[0].toUpperCase()
                font.pixelSize: Metrics.fontSmall
                font.bold: true
                opacity: 0.5
            }
            Label {
                text: inspector.heading[1]
                font.bold: true
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }
            FfToolButton {
                visible: inspector.kind !== "none"
                text: Glyphs.close
                font.pixelSize: Metrics.fontCaption
                tip: qsTr("Clear the selection")
                onClicked: App.selection.clear()
            }
        }
        Divider { }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Flickable {
                id: sheetViewport
                contentWidth: width
                contentHeight: sheetLoader.height
                boundsBehavior: Flickable.StopAtBounds
                clip: true

                Loader {
                    id: sheetLoader
                    // Explicit content sizing also handles late model updates.
                    height: (item as Item)?.implicitHeight ?? 0
                    width: sheetViewport.width
                    onLoaded: sheetViewport.contentY = 0
                    sourceComponent: ({ none: projectSheet, input: inputSheet, track: trackSheet, step: stepSheet,
                                        range: rangeSheet, output: outputSheet })[inspector.kind] ?? projectSheet
                }
            }
        }

        StepFooter {
            visible: inspector.kind === "step" && stepRow.valid
            Layout.fillWidth: true
            filter: inspector.stepSpec.filter || ""
            primaryFilter: inspector.stepSpec.primaryFilter || ""
            filterText: stepRow.values.filterText || ""
        }
    }

    Component { id: projectSheet; ProjectSheet { } }
    Component { id: inputSheet; InputSheet { index: inspector.index } }
    Component { id: trackSheet; TrackSheet { index: inspector.index; trackIndex: App.selection.track } }
    Component { id: stepSheet; StepSheet { index: inspector.index } }
    Component { id: rangeSheet; RangeSheet { } }
    Component { id: outputSheet; OutputSheet { index: inspector.index } }
}
