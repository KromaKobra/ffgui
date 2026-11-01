import QtQuick
import QtTest
import FFgui.Backend
import FFgui.Shell

// Loading a real file through the real window: the Sources card fills from
// ffprobe, the audio decodes for the Media View, the analysis finishes, the
// Inspector sheets show it, and none of it logs a warning. Never plays audio.
TestCase {
    id: test
    name: "Load"
    when: windowShown

    property var win: null
    readonly property var anyWarning: /^(?!This plugin does not support propagateSizeHints).*/

    Component { id: mainComponent; Main { } }

    function initTestCase() {
        if (testEnv.loadFixture === "")
            return;
        failOnWarning(test.anyWarning);
        test.win = mainComponent.createObject(null);
        tryVerify(() => test.win.workspaces.started, 2000);
    }

    function cleanupTestCase() {
        if (test.win)
            test.win.destroy();
    }

    function init() {
        if (testEnv.loadFixture === "")
            skip("ffmpeg not found, no fixture");
        failOnWarning(test.anyWarning);
    }

    function test_1_probe_fills_the_card() {
        App.project.addFiles(["file://" + testEnv.loadFixture]);
        compare(App.project.inputs.count, 1);
        compare(App.project.currentInput, 0);
        tryVerify(() => App.project.inputs.get(0).probing === false, 15000);
        const input = App.project.inputs.get(0);
        compare(input.container, "MKA");
        compare(input.name, "fixture.mka");
        verify(Math.abs(input.duration - 4.0) < 0.01);
        compare(input.tags.title, "Fixture");
        compare(input.chapters.length, 2);
        compare(input.chapters[1].title, "Quiet");
        compare(input.tracks.length, 1);
        compare(input.tracks[0].codec, "flac");
        compare(input.tracks[0].channels, 2);
        compare(input.tracks[0].rate, 48000);
        compare(App.project.track.channelNames.length, 2);
        // Adding the same file again selects it instead of duplicating it.
        App.project.addFiles(["file://" + testEnv.loadFixture]);
        compare(App.project.inputs.count, 1);
    }

    function test_2_transport_follows_the_input() {
        verify(Math.abs(App.transport.duration - 4.0) < 0.01);
        verify(App.transport.canPlay);
        compare(App.transport.markers.length, 2);
        App.transport.seek(1.0);
        App.transport.nextMarker();
        verify(Math.abs(App.transport.position - 1.5) < 0.01);
        App.transport.seek(0);
    }

    function test_3_decodes_for_the_media_view() {
        tryVerify(() => App.media.ready && !App.media.loading, 20000);
        compare(App.media.error, "");
        compare(App.media.channels, 2);
        verify(Math.abs(App.media.levelAt(0, 0.75, 0.05) + 20) < 0.2);     // the tone
        verify(App.media.levelAt(0, 3.0, 0.05) < -90);                      // the silence
        verify(!isNaN(App.media.spectrumAt(0, 0.75, 1000)));
    }

    function test_4_analysis_finishes() {
        tryVerify(() => App.analysis.state === "done", 30000);
        const r = App.analysis.results;
        compare(r.silence.length, 1);
        verify(Math.abs(r.silence[0].start - 1.5) < 0.05);
        verify(Math.abs(r.levels.peak + 20) < 0.1);
        verify(r.loudness.integrated !== undefined);
        compare(r.decode.errors, 0);
        compare(r.clipping.samples, 0);
        tryVerify(() => App.project.inputs.get(0).health === "ok", 2000);
        compare(App.project.inputs.get(0).healthText, "Decodes cleanly");
        verify(!isNaN(App.analysis.seriesAt("shortTerm", 1.0)));
    }

    function test_5_inspector_sheets_and_selection_stats() {
        App.selection.select("input", { index: 0 });
        wait(50);
        App.project.currentTrack = 0;
        App.selection.select("track", { index: 0, track: 0 });
        wait(50);
        App.selection.select("range", { start: 0.25, end: 1.25 });
        tryVerify(() => App.analysis.rangeStats.start === 0.25, 15000);
        verify(Math.abs(App.analysis.rangeStats.peak + 20) < 0.1);
        verify(App.analysis.rangeStats.lufs !== undefined);
        App.selection.clear();
    }

    function test_6_meters_show_the_playhead() {
        App.transport.seek(0.75);
        tryVerify(() => Math.abs(App.meters.peak[0] + 20) < 0.5, 2000);
        verify(!App.meters.live);
        App.transport.seek(3.0);
        tryVerify(() => App.meters.peak[0] <= App.meters.floor, 2000);
    }

    function test_7_layers_and_workspaces_live() {
        for (const name of ["Inspect", "Edit", "Master", "Restore", "Default"]) {
            test.win.workspaces.switchTo(name);
            wait(30);
        }
        App.view.setLayer("loudness", true);
        App.view.setLayer("phase", true);
        App.view.setLayer("dc", true);
        App.view.setLayer("grid", true);
        wait(50);
        test.win.workspaces.resetAll();
    }

    function test_7b_shape_and_deliver() {
        test.win.workspaces.switchTo("Edit");
        App.view.chainGraph = false;
        wait(100);
        const plus = findChild(test.win.contentItem, "addStepAt0");
        verify(plus !== null);
        mouseClick(plus);
        wait(100);
        const search = findChild(test.win, "stepSearch");
        verify(search !== null);
        search.text = "gain";
        search.forceActiveFocus();
        keyClick(Qt.Key_Return);
        tryCompare(App.project.steps, "count", 1, 2000);
        compare(App.project.steps.get(0).type, "gain");
        App.undo.undo();
        compare(App.project.steps.count, 0);
        App.undo.redo();
        compare(App.project.steps.count, 1);
        App.project.addStep("highpass");
        wait(50);
        const block = findChild(test.win.contentItem, "chainStep0");
        verify(block !== null);
        mouseDrag(block, 90, 35, 250, 0);
        tryVerify(() => App.project.steps.get(1).type === "gain", 2000);
        App.undo.undo();
        compare(App.project.steps.get(0).type, "gain");
        App.project.removeStep(1);
        App.selection.select("step", { index: 0 });
        App.project.addOutput("Archive FLAC");
        App.project.setOutput(0, { pattern: "qml-render" });
        tryVerify(() => App.command.valid && App.render.blocker === "", 10000);
        tryVerify(() => App.media.processed.ready && !App.media.processed.loading, 15000);
        compare(App.media.processed.error, "");
        const render = findChild(test.win.contentItem, "renderAll");
        verify(render !== null);
        mouseClick(render);
        tryVerify(() => App.queue.count > 0 && App.queue.get(App.queue.count - 1).status === "done", 20000);
        verify(!App.render.active);
        App.project.removeStep(0);
        App.project.removeOutput(0);
        App.selection.clear();
    }

    function test_8_removing_clears_everything() {
        App.project.removeInput(0);
        compare(App.project.inputs.count, 0);
        verify(!App.project.hasInput);
        tryVerify(() => !App.media.ready, 2000);
        compare(App.analysis.state, "idle");
        compare(App.transport.duration, 0);
    }
}
