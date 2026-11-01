import QtQuick
import QtTest
import FFgui.Backend
import FFgui.Shell
import "../../qml/Shell/Workspaces.js" as Workspaces

// Screenshots of every workspace, for comparing with the mock-up. Runs only
// when FFGUI_SCREENSHOT_DIR is set (scripts/screenshots.sh does that). With
// ffmpeg around, the generated fixture is loaded first so the panels show data.
TestCase {
    id: test
    name: "WorkspaceScreenshots"
    when: windowShown

    property var win: null

    Component { id: mainComponent; Main { width: 1600; height: 960 } }

    function initTestCase() {
        if (testEnv.screenshotDir === "")
            return;
        test.win = mainComponent.createObject(null);
        tryVerify(() => test.win.workspaces.started, 2000);
        if (testEnv.loadFixture !== "") {
            App.project.addFiles(["file://" + testEnv.loadFixture]);
            tryVerify(() => App.analysis.state === "done" && App.media.ready && !App.media.loading, 30000);
            App.selection.select("input", { index: 0 });
        }
    }

    function cleanupTestCase() {
        if (test.win)
            test.win.destroy();
    }

    function test_screenshot_data() { return Workspaces.names.map(n => ({ tag: n, name: n })); }
    function test_screenshot(data) {
        if (testEnv.screenshotDir === "")
            skip("FFGUI_SCREENSHOT_DIR is not set");
        test.win.workspaces.switchTo(data.name);
        wait(500);                      // let the dock's layout animation finish
        const file = testEnv.screenshotDir + "/ffgui-" + data.name.toLowerCase().replace(/[^a-z]+/g, "-") + ".png";
        grabImage(test.win.contentItem).save(file);
    }
}
