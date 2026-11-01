pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// Routing: input channels (rows) against output channels (columns). Click a
// cell to route it, type a gain for a custom mix. The matrix becomes a
// "Channel mix" step (ffmpeg's pan filter).
Item {
    id: panel

    readonly property var inputNames: {
        const n = App.project.track.channels || 0;
        return App.project.track.channelNames && App.project.track.channelNames.length === n
               ? App.project.track.channelNames : Array.from({ length: n }, (_, i) => "c" + i);
    }
    property int outputCount: 2
    readonly property var outputNames: panel.outputCount === 1 ? ["FC"] : panel.outputCount === 2 ? ["FL", "FR"]
                                       : ["FL", "FR", "FC", "LFE", "BL", "BR"].slice(0, panel.outputCount)
    // gains[row][column]; starts as a straight (identity) routing.
    property var gains: []

    function reset() {
        panel.gains = panel.inputNames.map((_, r) => panel.outputNames.map((_, c) => r === c ? 1 : 0));
    }
    function gain(r, c) { return panel.gains[r] ? (panel.gains[r][c] || 0) : 0; }
    function setGain(r, c, v) {
        const g = panel.gains.map(row => row.slice());
        if (!g[r] || c < 0) return;
        g[r][c] = v;
        panel.gains = g;
    }

    readonly property string panFilter: {
        const layout = panel.outputCount === 1 ? "mono" : panel.outputCount === 2 ? "stereo" : panel.outputCount + "c";
        const out = panel.outputNames.map((name, c) => {
            const terms = panel.inputNames.map((_, r) => panel.gain(r, c) ? +panel.gain(r, c).toFixed(3) + "*c" + r : "")
                                            .filter(x => x);
            return "c" + c + "=" + (terms.length ? terms.join("+") : "0*c0");
        });
        return layout + "|" + out.join("|");
    }

    onInputNamesChanged: reset()
    onOutputCountChanged: reset()
    Component.onCompleted: reset()

    EmptyState {
        anchors.fill: parent
        visible: !App.project.hasInput
        title: qsTr("No channels to route")
        text: qsTr("Add an input. Its channels become the rows of a routing matrix you can turn into a downmix or a remap.")
    }

    ColumnLayout {
        visible: App.project.hasInput
        anchors.fill: parent
        anchors.margins: Metrics.margin
        spacing: Metrics.spacingLarge

        RowLayout {
            Caption { text: qsTr("Output channels") }
            SegmentedControl {
                options: [{ value: 1, label: qsTr("Mono") }, { value: 2, label: qsTr("Stereo") }, { value: 6, label: "5.1" }]
                current: panel.outputCount
                onActivated: value => panel.outputCount = value
            }
            Item { Layout.fillWidth: true }
            FfButton { text: qsTr("Reset"); font.pixelSize: Metrics.fontCaption; onClicked: panel.reset() }
        }

        GridLayout {
            columns: panel.outputNames.length + 1
            columnSpacing: Metrics.spacingSmall
            rowSpacing: Metrics.spacingSmall

            Item { implicitWidth: 170; implicitHeight: 1 }
            Repeater {
                model: panel.outputNames
                delegate: Caption {
                    required property string modelData
                    text: qsTr("Out %1").arg(modelData)
                    horizontalAlignment: Text.AlignHCenter
                    Layout.preferredWidth: 64
                }
            }
            Repeater {
                model: panel.inputNames.length * (panel.outputNames.length + 1)
                delegate: Item {
                    id: cell
                    required property int index
                    readonly property int row: Math.floor(index / (panel.outputNames.length + 1))
                    readonly property int col: index % (panel.outputNames.length + 1) - 1
                    readonly property real gain: cell.col >= 0 ? panel.gain(cell.row, cell.col) : 0
                    implicitWidth: cell.col < 0 ? 170 : 64
                    implicitHeight: Metrics.controlHeight
                    Label {
                        visible: cell.col < 0
                        anchors.verticalCenter: parent.verticalCenter
                        text: (App.project.input.name || "") + " · " + panel.inputNames[cell.row]
                        font.pixelSize: Metrics.fontCaption
                        elide: Text.ElideMiddle
                        width: parent.width
                    }
                    Rectangle {
                        visible: cell.col >= 0
                        anchors.fill: parent
                        radius: Metrics.radius
                        color: cell.gain ? Theme.accentFill : Theme.row
                        border.color: Theme.border
                        NumberField {
                            anchors.fill: parent
                            value: cell.gain
                            from: -4; to: 4; decimals: 3
                            onEdited: v => panel.setGain(cell.row, cell.col, v)
                        }
                    }
                }
            }
        }

        CommandText { text: "pan=" + panel.panFilter }
        RowLayout {
            FfButton {
                text: qsTr("Add as Channel mix step")
                highlighted: true
                onClicked: App.project.addStep("mix", { matrix: panel.panFilter })
            }
        }
        Item { Layout.fillHeight: true }
    }
}
