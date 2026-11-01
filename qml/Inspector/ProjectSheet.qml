import QtQuick
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// Nothing selected: the project summary.
ColumnLayout {
    spacing: Metrics.spacingLarge

    Item { implicitHeight: 4 }

    Section {
        title: qsTr("Summary")
        Prop { key: qsTr("Inputs"); value: String(App.project.inputs.count) }
        Prop {
            key: qsTr("Chain")
            value: App.project.steps.count === 0 ? qsTr("no steps")
                   : qsTr("%1 active of %2 steps").arg(App.project.activeSteps).arg(App.project.steps.count)
        }
        Prop {
            key: qsTr("Outputs")
            value: {
                const names = [];
                for (let i = 0; i < App.project.outputs.count; ++i)
                    names.push(App.project.outputs.value(i, "fileName"));
                return App.project.outputs.revision >= 0 && names.length ? names.join("\n") : qsTr("none yet");
            }
        }
        Prop {
            visible: App.project.hasInput
            key: qsTr("Length after trim")
            value: Format.timecode(App.project.keptEnd - App.project.keptStart, true)
        }
        Prop {
            visible: App.project.outputs.count > 0
            key: qsTr("Estimated size")
            value: App.project.totalSizeText
        }
    }

    Hint {
        Layout.margins: Metrics.margin
        text: qsTr("Select an input, a track, a time range in the Media View, a chain step or an output to see its properties here.")
    }
}
