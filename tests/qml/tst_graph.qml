import QtQuick
import QtTest
import FFgui.Backend
import FFgui.Chain

// Graph mode through the real panel: add a second input as a source and a
// Mix node from the toolbar menus, wire and unwire ports, drag and undo,
// select a source, keep Row mode locked while branches remain, save and
// reopen, then remove nodes until the graph is a row again.
TestCase {
    id: test
    name: "Graph"
    when: windowShown
    visible: true
    width: 1100; height: 500
    Component { id: panelComponent; ChainPanel { width: 1100; height: 500 } }

    function initTestCase() {
        if (testEnv.loadFixture === "" || testEnv.secondFixture === "") return;
        failOnWarning(/.*/);
        App.project.addFiles(["file://" + testEnv.loadFixture, "file://" + testEnv.secondFixture]);
        tryCompare(App.caps, "state", "ready", 10000);
        tryVerify(() => App.project.inputs.count === 2 && !App.project.inputs.get(0).probing && !App.project.inputs.get(1).probing, 10000);
        App.project.currentInput = 0;
        tryVerify(() => App.media.ready && !App.media.loading, 10000);
        App.project.addOutput("Archive FLAC");
        App.view.chainGraph = true;
    }

    function card(panel, uid) { return findChild(panel, "graphNode_" + uid); }
    function chooseFromMenu(panel, button, menu, index) {
        mouseClick(findChild(panel, button));
        const m = findChild(panel, menu);
        tryVerify(() => m.opened);
        mouseClick(m.itemAt(index));
        tryVerify(() => !m.opened);
    }

    function test_edit_connect_drag_undo_save() {
        if (testEnv.loadFixture === "" || testEnv.secondFixture === "") skip("ffmpeg unavailable");
        failOnWarning(/.*/);
        const panel = createTemporaryObject(panelComponent, test);
        // A linear chain shows as input → output; a second input becomes a source.
        compare(App.project.graphNodes.count, 2);
        chooseFromMenu(panel, "graphAddSource", "graphSourceMenu", 1);
        const source = App.project.graphNodes.get(App.project.graphNodes.count - 1);
        compare(source.inputUid, App.project.inputs.get(1).uid);
        verify(App.project.graphActive);
        chooseFromMenu(panel, "graphAddOperation", "graphOperationMenu", 0);
        const mix = App.project.steps.get(0).uid;
        compare(App.project.steps.get(0).type, "amix");
        verify(!App.project.canUseRow);
        verify(App.project.graphError.indexOf("Connect input 2") >= 0);

        // Output port, then input port; the second input of the mix is the new source.
        wait(30);
        mouseClick(findChild(card(panel, source.uid), "outputPort0"));
        compare(App.project.currentInput, 0); // a port click doesn't select the node
        mouseClick(findChild(card(panel, mix), "inputPort1"));
        compare(App.project.graphError, "");
        const links = App.project.graphLinks.count;
        // Clicking a connected input disconnects it; undo restores it.
        mouseClick(findChild(card(panel, mix), "inputPort1"));
        compare(App.project.graphLinks.count, links - 1);
        verify(App.project.graphError !== "");
        App.undo.undo();
        compare(App.project.graphLinks.count, links);
        compare(App.project.graphError, "");

        // Dragging moves the node; undo moves the card back.
        const mixCard = card(panel, mix);
        const oldX = mixCard.x;
        const point = mixCard.mapToItem(panel, 60, 12);
        mouseDrag(panel, point.x, point.y, 60, 40, Qt.LeftButton);
        wait(30);
        verify(App.project.graphNodes.get(App.project.graphNodes.find("uid", mix)).x > oldX + 20);
        compare(card(panel, mix), mixCard);
        App.undo.undo();
        compare(App.project.graphNodes.get(App.project.graphNodes.find("uid", mix)).x, oldX);
        compare(mixCard.x, oldX);

        // Selecting a source shows its input.
        mouseClick(card(panel, source.uid), 60, 12);
        compare(App.project.currentInput, 1);
        App.project.currentInput = 0;

        // Row mode stays locked while the graph branches.
        App.view.chainGraph = false;
        verify(App.view.chainGraph);
        const file = "file://" + testEnv.tempDir + "/qml-graph.ffgui";
        verify(App.project.save(file));
        App.project.newProject();
        verify(App.project.open(file));
        tryVerify(() => App.project.graphError === "", 10000);
        verify(App.project.graphActive);
        verify(card(panel, mix) !== null);

        // Removing the mix reconnects the main path; removing the extra source
        // leaves a single path that can return to a row.
        mouseClick(card(panel, mix), 60, 12);
        mouseClick(findChild(panel, "graphRemoveNode"));
        compare(App.project.steps.count, 0);
        verify(!App.project.canUseRow);
        App.project.removeGraphNode(source.uid);
        verify(App.project.canUseRow);
        compare(App.project.graphError, "");
        verify(App.project.useRow());
        verify(!App.project.graphActive);
        App.view.chainGraph = false;
        verify(!App.view.chainGraph);
        verify(grabImage(panel).width > 0);
    }

    // Tracks dragged from Sources: the chain's input in Row mode, a wired
    // source in Graph mode (one undo step). The drop handler is driven directly;
    // offscreen windows have no native drag and drop.
    function test_track_drop() {
        if (testEnv.loadFixture === "" || testEnv.secondFixture === "") skip("ffmpeg unavailable");
        failOnWarning(/.*/);
        const panel = createTemporaryObject(panelComponent, test);
        const area = findChild(panel, "chainArea");
        App.view.chainGraph = false;
        App.project.currentInput = 0;
        panel.dropTrack("1:0:0", 50, 50);
        compare(App.project.currentInput, 1);
        App.project.currentInput = 0;

        App.view.chainGraph = true;
        const mix = App.project.addGraphOperation("amix");
        wait(30);
        const target = card(panel, mix).mapToItem(area, 30, 20);
        panel.dropTrack("1:0:0", target.x, target.y);
        const source = App.project.graphNodes.get(App.project.graphNodes.count - 1);
        compare(source.kind, "source");
        compare(source.inputUid, App.project.inputs.get(1).uid);
        compare(App.project.graphError, "");
        verify(App.project.graphLinks.find("from", source.uid) >= 0);
        compare(App.undo.undoText, "Route track");
        App.undo.undo();
        compare(App.project.graphNodes.find("uid", source.uid), -1);
        panel.dropTrack("not a track", 10, 10);
    }
}
