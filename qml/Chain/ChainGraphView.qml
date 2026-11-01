pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// The chain as nodes and wires. A linear chain shows its input, steps and
// outputs; adding a source or a multi-port node (mix, duck, crossfade, merge,
// splits, convolution) makes it a graph. Wire an output port to an input port
// by clicking one, then the other; clicking a connected input disconnects it.
ColumnLayout {
    id: graph

    signal addRequested()

    property string selectedNode: ""
    property string wireFrom: ""
    property int wirePort: 0

    readonly property int cardWidth: 150
    readonly property int portTop: 40
    readonly property int portPitch: 18
    readonly property var operations: {
        const revision = App.catalog.steps.revision;
        const keys = ["amix", "duck", "crossfade", "merge", "split", "channelsplit", "crossover", "convolve"];
        return revision < 0 ? [] : keys.map(key => App.catalog.steps.get(App.catalog.steps.find("key", key))).filter(s => !!s.key);
    }
    // Every input track, capped so a huge folder can't flood the menu.
    readonly property var sourceChoices: {
        const revision = App.project.inputs.revision;
        const list = [];
        for (let i = 0; revision >= 0 && i < App.project.inputs.count && list.length < 64; ++i) {
            const row = App.project.inputs.get(i);
            const tracks = row.tracks || [];
            if (tracks.length <= 1)
                list.push({ input: i, audio: -1, label: row.name });
            else
                for (let t = 0; t < tracks.length && list.length < 64; ++t)
                    list.push({ input: i, audio: tracks[t].audioIndex, label: qsTr("%1 · audio %2").arg(row.name).arg(tracks[t].audioIndex + 1) });
        }
        return list;
    }
    // Input ports with a connection, per node uid.
    readonly property var connected: {
        const revision = App.project.graphLinks.revision;
        const map = {};
        for (let i = 0; revision >= 0 && i < App.project.graphLinks.count; ++i) {
            const link = App.project.graphLinks.get(i);
            (map[link.to] = map[link.to] || []).push(link.toPort);
        }
        return map;
    }

    function node(id) {
        const row = App.project.graphNodes.find("uid", id);
        return row < 0 ? null : App.project.graphNodes.get(row);
    }
    function port(id, output, index) {
        const n = graph.node(id);
        return n ? { x: n.x + (output ? graph.cardWidth : 0), y: n.y + graph.portTop + (index + 0.5) * graph.portPitch } : { x: 0, y: 0 };
    }
    function selectNode(id) {
        graph.selectedNode = id;
        const n = graph.node(id);
        if (!n) return;
        if (n.kind === "step") App.selection.select("step", { index: n.stepIndex });
        else if (n.kind === "source") {
            const input = App.project.inputs.find("uid", n.inputUid);
            App.project.currentInput = input;
            App.selection.select("input", { index: input });
        } else App.selection.select("output", { index: App.project.outputs.find("uid", n.outputUid) });
    }
    // A track dropped at `point` (view coordinates): a new source there, wired
    // into the card it lands on, if any.
    function dropTrack(input, audio, point) {
        const p = graph.mapToItem(surface.contentItem, point.x, point.y);
        let target = null;
        for (let i = 0; i < App.project.graphNodes.count; ++i) {
            const n = App.project.graphNodes.get(i);
            const h = graph.portTop + graph.portPitch * Math.max(1, n.inputs, n.outputs);
            if (p.x >= n.x && p.x <= n.x + graph.cardWidth && p.y >= n.y && p.y <= n.y + h)
                target = n;
        }
        const x = target ? target.x - 200 : p.x - graph.cardWidth / 2;
        const y = target ? target.y + 70 : p.y - graph.portTop / 2;
        return App.project.routeTrack(input, audio, Math.max(0, x), Math.max(0, y), target ? target.uid : "");
    }

    function inputClicked(id, port) {
        if (graph.wireFrom !== "") {
            App.project.connectGraph(graph.wireFrom, graph.wirePort, id, port);
            graph.wireFrom = "";
            return;
        }
        for (let i = 0; i < App.project.graphLinks.count; ++i) {
            const link = App.project.graphLinks.get(i);
            if (link.to === id && link.toPort === port) {
                App.project.disconnectGraph(link.uid);
                return;
            }
        }
    }

    spacing: Metrics.spacingSmall

    Flow {
        id: toolbar
        Layout.fillWidth: true
        Layout.leftMargin: Metrics.spacing
        Layout.rightMargin: Metrics.spacing
        Layout.topMargin: Metrics.spacingSmall
        spacing: Metrics.spacingSmall

        MenuButton {
            objectName: "graphAddSource"
            text: Glyphs.add + " " + qsTr("Source")
            tip: qsTr("Add an input track as another graph source")
            enabled: graph.sourceChoices.length > 0
            menu: FfMenu {
                id: sourceMenu
                objectName: "graphSourceMenu"
                Instantiator {
                    model: graph.sourceChoices
                    delegate: FfMenuItem {
                        required property var modelData
                        text: modelData.label
                        onTriggered: App.project.addGraphSource(modelData.input, modelData.audio)
                    }
                    onObjectAdded: (index, object) => sourceMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => sourceMenu.removeItem(object)
                }
            }
        }
        MenuButton {
            objectName: "graphAddOperation"
            text: Glyphs.add + " " + qsTr("Node")
            tip: qsTr("Mix, duck, crossfade, merge or split signals")
            enabled: App.project.hasInput
            menu: FfMenu {
                id: operationMenu
                objectName: "graphOperationMenu"
                Instantiator {
                    model: graph.operations
                    delegate: FfMenuItem {
                        required property var modelData
                        text: modelData.name + "  ·  " + modelData.filter
                        enabled: modelData.available
                        onTriggered: App.project.addGraphOperation(modelData.key)
                    }
                    onObjectAdded: (index, object) => operationMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => operationMenu.removeItem(object)
                }
            }
        }
        FfToolButton {
            text: Glyphs.add + " " + qsTr("Effect…")
            tip: qsTr("Add a processing step in front of the selected output")
            enabled: App.project.hasInput
            onClicked: graph.addRequested()
        }
        FfToolButton {
            objectName: "graphRemoveNode"
            text: Glyphs.remove + " " + qsTr("Remove")
            tip: qsTr("Remove the selected node; what fed it flows on to what it fed")
            enabled: graph.selectedNode !== "" && graph.node(graph.selectedNode) !== null
            onClicked: { App.project.removeGraphNode(graph.selectedNode); graph.selectedNode = ""; }
        }
        FfComboBox {
            id: connectionChoice
            width: 200
            enabled: App.project.graphLinks.count > 0
            model: App.project.graphLinks
            textRole: "label"
            displayText: count > 0 ? currentText : qsTr("No connections")
            tip: qsTr("Connections (output → input port)")
        }
        FfToolButton {
            text: Glyphs.remove
            tip: qsTr("Remove the chosen connection")
            enabled: connectionChoice.currentIndex >= 0 && App.project.graphLinks.count > 0
            onClicked: App.project.disconnectGraph(App.project.graphLinks.value(connectionChoice.currentIndex, "uid"))
        }
        Label {
            objectName: "graphHint"
            width: Math.min(implicitWidth, toolbar.width)
            height: connectionChoice.height
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            leftPadding: Metrics.spacing
            font.pixelSize: Metrics.fontCaption
            color: App.project.graphError !== "" && graph.wireFrom === "" ? Theme.errorSoft : Theme.textDim
            text: graph.wireFrom !== "" ? qsTr("Now click an input port · click empty space to cancel")
                : App.project.graphError !== "" ? Glyphs.warning + " " + App.project.graphError
                : qsTr("Drag nodes to arrange them · click an output port, then an input port")
        }
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        EmptyState {
            anchors.fill: parent
            visible: App.project.graphNodes.count === 0
            glyph: Glyphs.add
            title: qsTr("Nothing to connect yet")
            text: qsTr("Add an input, then mix, duck or split it with other sources here.")
            actionText: qsTr("Add files…")
            onAction: App.requestDialog("addFiles")
        }

        Flickable {
            id: surface
            objectName: "graphSurface"
            anchors.fill: parent
            visible: App.project.graphNodes.count > 0
            clip: true
            readonly property var bounds: {
                const revision = App.project.graphNodes.revision;
                let w = width, h = height;
                for (let i = 0; revision >= 0 && i < App.project.graphNodes.count; ++i) {
                    const n = App.project.graphNodes.get(i);
                    w = Math.max(w, n.x + graph.cardWidth + 40);
                    h = Math.max(h, n.y + graph.portTop + graph.portPitch * Math.max(1, n.inputs, n.outputs) + 40);
                }
                return { w: Math.min(10500, w), h: Math.min(10500, h) };
            }
            contentWidth: bounds.w
            contentHeight: bounds.h
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.horizontal: ScrollBar { }
            ScrollBar.vertical: ScrollBar { }

            MouseArea {
                width: surface.contentWidth
                height: surface.contentHeight
                onClicked: { graph.wireFrom = ""; graph.selectedNode = ""; }
                HoverHandler {
                    id: hover
                    enabled: graph.wireFrom !== ""
                    onPointChanged: wires.requestPaint()
                }
            }
            Canvas {
                id: wires
                // Paints only the visible viewport, however far apart the nodes are.
                x: surface.contentX
                y: surface.contentY
                width: surface.width
                height: surface.height
                Connections { target: App.project.graphNodes; function onRevisionChanged() { wires.requestPaint(); } }
                Connections { target: App.project.graphLinks; function onRevisionChanged() { wires.requestPaint(); } }
                Connections { target: graph; function onWireFromChanged() { wires.requestPaint(); } }
                onXChanged: requestPaint()
                onYChanged: requestPaint()
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d");
                    ctx.reset();
                    ctx.fillStyle = Theme.card;
                    for (let x = 10 - wires.x % 20; x < width; x += 20)
                        for (let y = 10 - wires.y % 20; y < height; y += 20)
                            ctx.fillRect(x, y, 1.5, 1.5);
                    ctx.lineWidth = 2;
                    const curve = (a, b) => {
                        const bend = Math.max(30, Math.abs(b.x - a.x) / 2);
                        ctx.beginPath();
                        ctx.moveTo(a.x - wires.x, a.y - wires.y);
                        ctx.bezierCurveTo(a.x + bend - wires.x, a.y - wires.y, b.x - bend - wires.x, b.y - wires.y, b.x - wires.x, b.y - wires.y);
                        ctx.stroke();
                    };
                    for (let i = 0; i < App.project.graphLinks.count; ++i) {
                        const link = App.project.graphLinks.get(i);
                        const from = graph.node(link.from);
                        ctx.strokeStyle = from && from.bypass ? Theme.borderStrong : Theme.wire;
                        curve(graph.port(link.from, true, link.fromPort), graph.port(link.to, false, link.toPort));
                    }
                    if (graph.wireFrom !== "" && hover.hovered) {
                        ctx.strokeStyle = Theme.accentSoft;
                        ctx.setLineDash([5, 4]);
                        curve(graph.port(graph.wireFrom, true, graph.wirePort), hover.point.position);
                    }
                }
            }
            Repeater {
                model: App.project.graphNodes
                delegate: NodeCard {
                    id: card
                    required property int index
                    readonly property var info: {
                        const revision = App.project.graphNodes.revision;
                        return revision >= 0 ? App.project.graphNodes.get(card.index) : ({});
                    }
                    objectName: "graphNode_" + info.uid
                    x: info.x || 0
                    y: info.y || 0
                    width: graph.cardWidth
                    portTop: graph.portTop
                    portPitch: graph.portPitch
                    title: info.label || info.uid || ""
                    detail: info.error || info.detail || ""
                    isStep: info.kind === "step"
                    failed: !!info.error
                    inputCount: info.inputs || 0
                    outputCount: info.outputs || 0
                    inputNames: info.inputNames || []
                    outputNames: info.outputNames || []
                    connectedInputs: graph.connected[info.uid] || []
                    pendingOutput: graph.wireFrom === info.uid ? graph.wirePort : -1
                    bypassed: !!info.bypass
                    selected: graph.selectedNode === info.uid
                    onClicked: graph.selectNode(card.info.uid)
                    onPortClicked: (output, port) => {
                        if (output) {
                            graph.wireFrom = card.info.uid;
                            graph.wirePort = port;
                        } else
                            graph.inputClicked(card.info.uid, port);
                    }
                    DragHandler {
                        xAxis.minimum: 0
                        yAxis.minimum: 0
                        onActiveChanged: {
                            if (active) {
                                graph.selectNode(card.info.uid);
                                return;
                            }
                            App.project.moveGraphNode(card.info.uid, card.x, card.y);
                            // The drag replaced the position bindings; follow the model again (undo).
                            card.x = Qt.binding(() => card.info.x || 0);
                            card.y = Qt.binding(() => card.info.y || 0);
                        }
                    }
                }
            }
        }
    }
}
