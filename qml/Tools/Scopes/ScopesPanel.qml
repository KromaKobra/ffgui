pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// Scopes: live meters of what is playing. Pick one, or tile several.
Item {
    id: panel

    property string mode: "spectrum"
    property var tiled: ["spectrum", "vectorscope", "histogram"]
    property bool tile: true

    readonly property var kinds: [
        { value: "spectrum", label: qsTr("Spectrum") }, { value: "cqt", label: qsTr("Musical") },
        { value: "vectorscope", label: qsTr("Vectorscope") }, { value: "phase", label: qsTr("Phase") },
        { value: "histogram", label: qsTr("Histogram") }, { value: "bits", label: qsTr("Bit scope") }
    ]
    function label(kind) { return (panel.kinds.find(k => k.value === kind) || {}).label || kind; }

    Rectangle { anchors.fill: parent; color: Theme.canvas }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Metrics.spacing
        spacing: Metrics.spacing

        RowLayout {
            Layout.fillWidth: true
            SegmentedControl {
                options: panel.kinds
                current: panel.tile ? "" : panel.mode
                fontSize: Metrics.fontCaption
                onActivated: value => {
                    if (!panel.tile) {
                        panel.mode = value;
                        return;
                    }
                    const t = panel.tiled.slice();
                    const i = t.indexOf(value);
                    if (i >= 0 && t.length > 1)
                        t.splice(i, 1);
                    else if (i < 0)
                        t.push(value);
                    panel.tiled = panel.kinds.map(k => k.value).filter(k => t.indexOf(k) >= 0);
                }
            }
            Item { Layout.fillWidth: true }
            FfCheckBox { text: qsTr("Tile"); checked: panel.tile; onToggled: panel.tile = checked }
            Caption { text: App.transport.playing ? Glyphs.dot + " " + qsTr("live") : qsTr("at playhead") }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Metrics.spacing
            Repeater {
                model: panel.tile ? panel.tiled : [panel.mode]
                delegate: ScopeTile {
                    id: scopeTile
                    required property string modelData
                    kind: modelData
                    title: panel.label(modelData)
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    content: Item {
                        anchors.fill: parent
                        ToolPlot { objectName: "scopePlot"; anchors.fill: parent; source: App.scopes; kind: scopeTile.kind; tone: Theme.wavePeak; gridColor: Theme.line }
                        Text {
                            anchors.centerIn: parent
                            visible: !App.scopes.ready
                            text: App.project.hasInput ? qsTr("Decoding scope samples…") : qsTr("No input")
                            color: Theme.textFaint
                            font.pixelSize: Metrics.fontCaption
                        }
                    }
                }
            }
        }
    }
}
