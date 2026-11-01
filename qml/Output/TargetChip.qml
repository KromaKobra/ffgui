import QtQuick
import FFgui.Backend
import FFgui.Controls

// One output file in the Output bar. Clicking makes it the current target
// and shows it in the Inspector.
Pill {
    id: chip

    required property int index
    required property string fileName
    required property string container
    required property string codecLabel
    required property string preset

    text: chip.fileName
    current: chip.index === App.project.currentOutput
    tip: chip.container.toUpperCase() + " · " + chip.codecLabel + " · " + chip.preset
    onClicked: {
        App.project.currentOutput = chip.index;
        App.selection.select("output", { index: chip.index });
    }
}
