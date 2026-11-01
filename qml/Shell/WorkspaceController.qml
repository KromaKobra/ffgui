import QtCore
import QtQuick
import FFgui.Backend
import "Workspaces.js" as Workspaces

// Workspaces: saved arrangements for common jobs. Each is a dock layout plus
// the view settings that belong to it (layers, spectrogram share, Chain graph
// mode). The user's changes to a workspace are kept per workspace in its own
// Settings category; Reset brings back the built-in version.
QtObject {
    id: controller

    required property DockShell dock

    property string current: "Default"
    readonly property var names: Workspaces.names
    readonly property var descriptions: Workspaces.descriptions
    property var saved: ({})            // name -> { layout, view }: the user's version
    property bool started: false        // nothing is saved before the first apply

    function apply(name) {
        const mine = controller.saved[name];
        if (!mine || !controller.dock.restoreLayout(mine.layout))
            controller.dock.restoreLayout(Workspaces.layout(name));
        App.view.apply(mine ? mine.view : Workspaces.view(name));
    }

    function switchTo(name) {
        if (name === controller.current || Workspaces.names.indexOf(name) < 0)
            return;
        controller.remember();
        controller.current = name;
        controller.apply(name);
    }

    function reset() {
        const s = Object.assign({}, controller.saved);
        delete s[controller.current];
        controller.saved = s;
        controller.apply(controller.current);
        controller.store();
    }

    function resetAll() {
        controller.saved = {};
        controller.apply(controller.current);
        controller.store();
    }

    function remember() {
        if (!controller.started)
            return;
        const s = Object.assign({}, controller.saved);
        s[controller.current] = { layout: controller.dock.saveLayout(), view: App.view.save() };
        controller.saved = s;
        controller.store();
    }

    function store() {
        controller._settings.current = controller.current;
        controller._settings.saved = JSON.stringify(controller.saved);
    }

    // Call once the dock has built its first layout (after its
    // Component.onCompleted), e.g. with Qt.callLater from the window's.
    function start() {
        try {
            controller.saved = JSON.parse(controller._settings.saved) || {};
        } catch (e) {
            controller.saved = {};
        }
        controller.current = Workspaces.names.indexOf(controller._settings.current) >= 0 ? controller._settings.current : "Default";
        controller.apply(controller.current);
        controller.started = true;
    }

    component WorkspaceSettings: Settings {
        property string current: "Default"
        property string saved: "{}"
    }
    property WorkspaceSettings _settings: WorkspaceSettings { category: "workspaces" }

    property Timer _saveTimer: Timer {
        interval: 400
        onTriggered: controller.remember()
    }
    property Connections _dockChanges: Connections {
        target: controller.dock
        function onLayoutChanged() { controller._saveTimer.restart(); }
    }
    property Connections _viewChanges: Connections {
        target: App.view
        function onLayersChanged() { controller._saveTimer.restart(); }
        function onSpectrogramShareChanged() { controller._saveTimer.restart(); }
        function onChainGraphChanged() { controller._saveTimer.restart(); }
    }
}
