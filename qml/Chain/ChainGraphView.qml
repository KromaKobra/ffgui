pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// The same chain as nodes and wires. A row is a graph with one path, so
// switching back and forth never loses work. Several inputs or outputs
// (mixing, ducking, splits) need graph mode.
Flickable {
    id: graph

    readonly property int stepCount: App.project.steps.count
    readonly property int count: graph.stepCount + 2

    function nodeX(k) { return 24 + k * 178; }
    function nodeY(k) { return 22 + (k % 2) * 44; }

    clip: true
    contentWidth: graph.nodeX(graph.count) + 24
    contentHeight: Math.max(height, 230)
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.horizontal: ScrollBar { }
    ScrollBar.vertical: ScrollBar { }

    Canvas {
        id: wires
        width: graph.contentWidth
        height: graph.contentHeight
        Connections {
            target: App.project.steps
            function onRevisionChanged() { wires.requestPaint(); }
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.fillStyle = Theme.card;
            for (let x = 10; x < width; x += 20)
                for (let y = 10; y < height; y += 20)
                    ctx.fillRect(x, y, 1.5, 1.5);
            ctx.lineWidth = 2;
            for (let k = 0; k + 1 < graph.count; ++k) {
                const bypassed = k >= 1 && k <= graph.stepCount && App.project.steps.value(k - 1, "bypass") === true;
                ctx.strokeStyle = bypassed ? Theme.borderStrong : Theme.wire;
                const x0 = graph.nodeX(k) + 130, y0 = graph.nodeY(k) + 26;
                const x1 = graph.nodeX(k + 1), y1 = graph.nodeY(k + 1) + 26;
                ctx.beginPath();
                ctx.moveTo(x0, y0);
                ctx.bezierCurveTo(x0 + 30, y0, x1 - 30, y1, x1, y1);
                ctx.stroke();
            }
        }
    }

    NodeCard {
        x: graph.nodeX(0)
        y: graph.nodeY(0)
        isStep: false
        hasInput: false
        title: "in  " + (App.project.input.name || qsTr("no input"))
        detail: App.project.hasInput
                ? Format.signal({ rate: App.project.track.rate, fmt: App.project.track.sampleFmt, layout: App.project.track.layout })
                : ""
        selected: App.selection.kind === "input"
        onClicked: if (App.project.hasInput) App.selection.select("input", { index: App.project.currentInput })
    }

    Repeater {
        model: App.project.steps
        delegate: NodeCard {
            required property int index
            required property string glyph
            required property string name
            required property string filter
            required property bool bypass
            x: graph.nodeX(index + 1)
            y: graph.nodeY(index + 1)
            title: glyph + "  " + name
            detail: filter
            bypassed: bypass
            selected: App.selection.kind === "step" && App.selection.index === index
            onClicked: App.selection.select("step", { index: index })
        }
    }

    NodeCard {
        x: graph.nodeX(graph.count - 1)
        y: graph.nodeY(graph.count - 1)
        isStep: false
        hasOutput: false
        title: "out  " + qsTr("%n files", "", App.project.outputs.count)
        selected: App.selection.kind === "output"
        onClicked: if (App.project.outputs.count > 0) App.selection.select("output", { index: Math.max(0, App.project.currentOutput) })
    }

    // Where a second input would join: graphs allow mixes, ducking, channel
    // splits and crossovers.
    Item {
        x: graph.nodeX(0)
        y: 150
        width: 220
        height: 46
        DashedRect { anchors.fill: parent }
        Text {
            anchors.centerIn: parent
            width: parent.width - 16
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: qsTr("Drop a track here to mix it in")
            color: Theme.textDim
            font.pixelSize: Metrics.fontSmall
        }
    }
}
