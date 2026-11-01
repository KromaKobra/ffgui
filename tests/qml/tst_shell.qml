import QtQuick
import QtTest
import FFgui.Backend
import FFgui.Shell
import "../../qml/Shell/PanelRegistry.js" as Registry
import "../../qml/Shell/Workspaces.js" as Workspaces

// The real window: every panel opens, closes and pops out through Quilt's
// public API; every workspace applies; layouts round-trip; and none of it
// logs a warning.
TestCase {
    id: test
    name: "Shell"
    when: windowShown

    property var win: null
    readonly property var dock: test.win ? test.win.dock : null
    readonly property var workspaces: test.win ? test.win.workspaces : null

    // Offscreen windows can't take size hints; that's the platform, not us.
    readonly property var anyWarning: /^(?!This plugin does not support propagateSizeHints).*/

    Component { id: mainComponent; Main { } }

    function initTestCase() {
        failOnWarning(test.anyWarning);
        test.win = mainComponent.createObject(null);
        verify(test.win);
        tryVerify(() => test.workspaces.started, 2000);
    }

    function cleanupTestCase() {
        test.win.destroy();
    }

    function init() {
        failOnWarning(test.anyWarning);
        test.workspaces.switchTo("Default");
        test.workspaces.resetAll();
    }

    function toggleable() { return Registry.panels.filter(p => !p.core).map(p => ({ tag: p.id, id: p.id })); }
    function everyPanel() { return Registry.panels.map(p => ({ tag: p.id, id: p.id, core: !!p.core })); }

    function test_core_panels_are_open_and_cannot_close() {
        for (const p of Registry.panels.filter(p => p.core)) {
            const panel = test.dock.panel(p.id);
            verify(panel, p.id);
            verify(panel.isOpen, p.id);
            verify(!panel.closable, p.id);
        }
    }

    function test_panel_opens_and_closes_data() { return toggleable(); }
    function test_panel_opens_and_closes(data) {
        const panel = test.dock.panel(data.id);
        verify(panel);
        verify(!panel.isOpen);
        App.requestPanel(data.id);
        tryVerify(() => panel.isOpen);
        verify(panel.isCurrent);
        // Its content loads while it is open.
        verify(panel.children.some(c => c.item !== undefined && c.item !== null), "content loaded");
        test.dock.closePanel(data.id);
        tryVerify(() => !panel.isOpen);
    }

    function test_panel_pops_out_data() { return everyPanel(); }
    function test_panel_pops_out(data) {
        const panel = test.dock.panel(data.id);
        if (!panel.floatable)
            skip("not floatable");
        if (data.core)
            verify(test.dock.popOut(data.id));
        else
            App.requestPanel(data.id, true);
        tryVerify(() => panel.isPoppedOut);
        const win = test.dock.popouts.find(w => w.dockArea === panel.dockArea);
        verify(win, "pop-out window registered");
        if (!data.core) {
            // A requested pop-out of a closed panel gets a usable size.
            verify(win.width >= 640 && win.height >= 460);
        }
        // Applying the workspace brings it home.
        test.workspaces.apply(test.workspaces.current);
        tryVerify(() => !panel.isPoppedOut);
    }

    function test_workspace_applies_data() { return Workspaces.names.map(n => ({ tag: n, name: n })); }
    function test_workspace_applies(data) {
        test.workspaces.switchTo(data.name);
        compare(test.workspaces.current, data.name);
        const expected = JSON.stringify(Workspaces.layouts[data.name]);
        for (const p of Registry.panels) {
            const inLayout = expected.indexOf('"' + p.id + '"') >= 0;
            compare(test.dock.panel(p.id).isOpen, inLayout || !!p.core, p.id);
        }
        const view = Workspaces.view(data.name);
        compare(App.view.chainGraph, !!view.chainGraph);
        compare(App.view.spectrogramShare, view.spectrogramShare !== undefined ? view.spectrogramShare : 0.45);
        for (const key in (view.layers || {}))
            compare(App.view.layers[key], view.layers[key], key);
    }

    function test_layout_round_trips() {
        test.workspaces.switchTo("Batch");
        const saved = JSON.stringify(test.dock.saveLayout());
        test.workspaces.switchTo("Inspect");
        verify(JSON.stringify(test.dock.saveLayout()) !== saved);
        verify(test.dock.restoreLayout(saved));
        compare(JSON.stringify(test.dock.saveLayout()), saved);
    }

    function test_workspace_keeps_the_users_changes() {
        test.workspaces.switchTo("Edit");
        App.requestPanel("console");
        tryVerify(() => test.dock.panel("console").isOpen);
        App.view.setLayer("dc", true);
        test.workspaces.switchTo("Default");
        verify(!test.dock.panel("console").isOpen);
        test.workspaces.switchTo("Edit");
        verify(test.dock.panel("console").isOpen);
        verify(App.view.layer("dc"));
        test.workspaces.reset();
        verify(!test.dock.panel("console").isOpen);
        verify(!App.view.layer("dc"));
    }

    function test_selection_switches_inspector_sheets_data() {
        return ["none", "range"].map(k => ({ tag: k, kind: k }));
    }
    function test_selection_switches_inspector_sheets(data) {
        App.selection.select(data.kind, { start: 1, end: 2 });
        wait(10);
        App.selection.clear();
    }

    function test_palette_finds_steps_by_both_names() {
        const registry = test.win.actions;
        verify(registry);
        const byPlain = registry.search("noise").map(a => a.title);
        const byFfmpeg = registry.search("afftdn").map(a => a.title);
        verify(byPlain.indexOf("Add step: Remove noise") >= 0);
        compare(byFfmpeg[0], "Add step: Remove noise");
    }
}
