import QtQuick
import QtTest
import FFgui.Backend
import FFgui.Theme
import FFgui.Tools.Metadata
import FFgui.Tools.Compare
import FFgui.Tools.Scopes
import FFgui.Tools.Routing
import FFgui.Tools.Transcript

TestCase {
    id: test
    name: "Tooling"
    when: windowShown
    visible: true
    width: 1000
    height: 700
    readonly property var anyWarning: /^(?!This plugin does not support propagateSizeHints).*/
    Component { id: metadataComponent; MetadataPanel { width: 600; height: 600 } }
    Component { id: compareComponent; ComparePanel { width: 800; height: 600 } }
    Component { id: scopesComponent; ScopesPanel { width: 800; height: 300 } }
    Component { id: routingComponent; RoutingPanel { width: 900; height: 600 } }
    Component { id: transcriptComponent; TranscriptPanel { width: 700; height: 500 } }
    function initTestCase() {
        if (testEnv.loadFixture === "") return;
        failOnWarning(test.anyWarning);
        App.project.addFiles(["file://" + testEnv.loadFixture]);
        tryVerify(() => App.media.ready && !App.media.loading, 20000);
    }
    function init() {
        if (testEnv.loadFixture === "") skip("ffmpeg not found");
        failOnWarning(test.anyWarning);
    }
    function test_metadata() {
        const panel = createTemporaryObject(metadataComponent, test);
        verify(panel !== null);
        App.project.setTag("custom", "Value");
        compare(panel.tags.custom, "Value");
        App.project.setChapter(0, 0.1, 1.4, "Edited chapter");
        compare(panel.chapters[0].title, "Edited chapter");
        App.undo.undo();
        compare(panel.chapters[0].title, "Tone");
        App.project.setTrackMetadata(0, { title: "Voice", lang: "eng", isDefault: true });
        wait(50);
        verify(grabImage(panel).width > 0);
    }
    function test_compare_and_scopes() {
        const panel = createTemporaryObject(compareComponent, test);
        verify(panel !== null);
        App.project.addStep("gain", { volume: -6 });
        App.compare.run();
        tryVerify(() => !App.compare.active, 20000);
        compare(App.compare.error, "");
        verify(App.compare.result.traceA.length > 0);
        verify(!App.compare.result.equal);
        wait(50);
        verify(grabImage(panel).width > 0);
        const scopes = createTemporaryObject(scopesComponent, test);
        tryVerify(() => App.scopes.ready, 10000);
        scopes.tile = false;
        for (const kind of scopes.kinds) {
            scopes.mode = kind.value;
            wait(30);
            const plot = findChild(scopes, "scopePlot");
            verify(plot !== null);
            verify(plot.width > 100 && plot.height > 80);
            const rendered = grabImage(plot);
            verify(rendered.width > 0);
            if (kind.value === "phase") {
                compare(plot.kind, "phase");
                compare(plot.tone, Theme.wavePeak);
                compare(plot.source, App.scopes);
                verify(App.scopes.values.phase > 0.95);
                const point = plot.mapToItem(scopes, plot.width * 0.7, plot.height * 0.5);
                const scopeImage = grabImage(scopes);
                compare(scopeImage.pixel(Math.floor(point.x), Math.floor(point.y)), Theme.wavePeak);
            }
        }
    }
    function test_routing_and_transcript() {
        const routing = createTemporaryObject(routingComponent, test);
        routing.setGain(0, 0, -0.5);
        verify(routing.panFilter.indexOf("-0.5*c0") >= 0);
        App.project.addStep("mix", { matrix: routing.panFilter });
        verify(App.project.steps.get(App.project.steps.count - 1).filterText.indexOf("pan") >= 0);
        const panel = createTemporaryObject(transcriptComponent, test);
        App.transcript.addSegment(0, 1, "First");
        verify(App.transcript.setSegment(0, 0.2, 1.2, "Edited"));
        const target = "file://" + testEnv.tempDir + "/qml-transcript.srt";
        verify(App.transcript.exportFile(target, "srt"));
        App.transcript.removeSegment(0);
        verify(App.transcript.importFile(target));
        compare(App.transcript.get(0).text, "Edited");
        wait(50);
        verify(grabImage(panel).width > 0);
    }
}
