import QtQuick
import QtCore
import "LayoutTree.js" as Tree

// Shared state behind all DockAreas of one docking system: the panel
// registry, pop-out windows, the drag session and persistence. The main
// DockArea creates it. Applications reach it as DockArea.manager, but the
// DockArea and DockPanel methods cover normal use.
QtObject {
    id: manager

    required property var mainArea
    readonly property DockStyle style: mainArea ? mainArea.style : null

    property var panels: ({})          // panelId -> DockPanel
    property var panelOrder: []        // declaration order; drives hint placement
    property var areas: []             // every live DockArea, main first
    property var windows: []           // pop-out windows (DockWindow)
    property var activePanel: null     // last panel the user interacted with

    // The panel being dragged, or null. Only one platform drag can run at a
    // time, so a non-null value also identifies the drag as ours.
    property var dragPanel: null

    // Emitted after any change to any area's layout.
    signal layoutChanged()

    readonly property bool _canPlaceWindows: !Qt.platform.pluginName.startsWith("wayland")

    // Drag decisions are logged here; enable with
    // QT_LOGGING_RULES="quilt.drag.debug=true".
    property LoggingCategory _log: LoggingCategory {
        name: "quilt.drag"
        defaultLogLevel: LoggingCategory.Warning
    }
    property var _drop: null            // { area, target } or { cancel: true }
    property var _track: null           // last drag position over one of our windows
    property bool _quitting: false
    property Component _windowComponent: null

    // ---------------------------------------------------------------------
    //  Registry

    function panel(panelId) {
        return panels[panelId] || null;
    }

    function addPanel(p) {
        if (!p || !p.panelId) {
            console.warn("Quilt: a DockPanel needs a panelId");
            return false;
        }
        const existing = panels[p.panelId];
        if (existing === p)
            return true;
        if (existing) {
            console.warn("Quilt: duplicate panelId", p.panelId);
            return false;
        }
        const next = Object.assign({}, panels);
        next[p.panelId] = p;
        panels = next;
        panelOrder = panelOrder.concat([p.panelId]);
        p._manager = manager;
        return true;
    }

    function removePanel(panelId) {
        const p = panel(panelId);
        if (!p)
            return;
        closePanel(panelId);
        const next = Object.assign({}, panels);
        delete next[panelId];
        panels = next;
        panelOrder = panelOrder.filter(id => id !== panelId);
        p._lastStackId = "";
        p._manager = null;
    }

    function registerArea(area) {
        if (areas.indexOf(area) < 0)
            areas = areas.concat([area]);
    }

    function unregisterArea(area) {
        areas = areas.filter(a => a !== area);
    }

    function areaOf(panelId) {
        for (const a of areas)
            if (Tree.findStackOf(a.root, panelId))
                return a;
        return null;
    }

    // Called by an area for a panel that left its layout.
    function parkPanel(p) {
        p._lastStackId = p._stackId;
        p.anchors.fill = undefined;
        p.parent = null;
        p.visible = false;
        p._area = null;
        p._current = false;
        p._stackId = "";
        if (activePanel === p)
            activePanel = null;
    }

    function hintFor(p, activate) {
        const sides = {};
        sides[Dock.Center] = "center";
        sides[Dock.Left] = "left";
        sides[Dock.Right] = "right";
        sides[Dock.Top] = "top";
        sides[Dock.Bottom] = "bottom";
        return {
            side: sides[p.area] || "center",
            fraction: p.areaSize,
            group: panelOrder.filter(id => panels[id].area === p.area),
            activate: activate,
            canShare: canShare
        };
    }

    // Whether a panel may share a tab group (DockPanel.tabbable).
    function canShare(panelId) {
        return !panels[panelId] || panels[panelId].tabbable;
    }

    function canFloat(p) {
        return !!p && p.floatable && mainArea.allowPopOut;
    }

    // ---------------------------------------------------------------------
    //  Panel operations

    function openPanel(panelId) {
        const p = panel(panelId);
        if (!p)
            return;
        let area = areaOf(panelId);
        if (area) {
            area.setCurrent(panelId);
        } else {
            area = areas.find(a => Tree.canJoin(Tree.findStack(a.root, p._lastStackId), panelId, canShare)) || null;
            if (area)
                area.mutate(root => Tree.insertTab(root, p._lastStackId, panelId, -1, true));
            else {
                area = mainArea;
                area.mutate(root => Tree.placeByHint(root, panelId, hintFor(p, true)));
            }
        }
        activePanel = p;
        raiseArea(area);
    }

    function closePanel(panelId) {
        const area = areaOf(panelId);
        if (area)
            area.mutate(root => Tree.removePanel(root, panelId));
    }

    // Moves a panel into its own window. Returns the window's DockArea.
    function popOut(panelId) {
        const p = panel(panelId);
        if (!canFloat(p))
            return null;
        const src = areaOf(panelId);
        if (src && !src.isMain && Tree.panelIds(src.root).length === 1) {
            raiseArea(src);
            return src;
        }
        const win = createWindow(src ? src.panelGeometry(panelId) : null);
        if (!win)
            return null;
        if (src)
            src.mutate(root => Tree.removePanel(root, panelId));
        win.dockArea.mutate(() => Tree.makeStack([panelId], panelId, 1));
        activePanel = p;
        return win.dockArea;
    }

    // Moves a panel onto a drop target (see LayoutTree.applyDrop) in `area`.
    function movePanel(panelId, area, target) {
        const p = panel(panelId);
        if (!p || areas.indexOf(area) < 0 || (!area.isMain && !p.floatable))
            return;
        const src = areaOf(panelId);
        if (src && src !== area)
            src.mutate(root => Tree.removePanel(root, panelId));
        area.mutate(root => Tree.applyDrop(root, panelId, target, canShare));
        activePanel = p;
        raiseArea(area);
    }

    function toggleMaximized(panelId) {
        const area = areaOf(panelId);
        if (!area)
            return;
        const s = Tree.findStackOf(area.root, panelId);
        area.maximizedStackId = area.maximizedStackId === s.id ? "" : s.id;
    }

    function raiseArea(area) {
        const w = area ? area.Window.window : null;
        if (!w || area.isMain)
            return;
        w.raise();
        if (_canPlaceWindows)
            w.requestActivate();
    }

    // ---------------------------------------------------------------------
    //  Windows

    function createWindow(geometry) {
        let component = mainArea.windowComponent;
        if (!component) {
            if (!_windowComponent)
                _windowComponent = Qt.createComponent(Qt.resolvedUrl("DockWindow.qml"));
            component = _windowComponent;
        }
        if (component.status !== Component.Ready) {
            console.warn("Quilt: cannot create a pop-out window:", component.errorString());
            return null;
        }
        const g = geometry || {};
        const props = {
            manager: manager,
            width: Math.round(Math.max(g.width || 0, 320)),
            height: Math.round(Math.max(g.height || 0, 240))
        };
        if (_canPlaceWindows && g.x !== undefined) {
            props.x = Math.round(g.x);
            props.y = Math.round(g.y);
        }
        const win = component.createObject(null, props);
        if (!win || !win.dockArea) {
            console.warn("Quilt: windowComponent must create a window with `manager` and `dockArea` properties (see DockWindow)");
            if (win)
                win.destroy();
            return null;
        }
        windows = windows.concat([win]);
        win.show();
        if (_canPlaceWindows)
            win.requestActivate();
        return win;
    }

    // The user closed a pop-out window.
    function windowClosing(win) {
        if (_quitting || windows.indexOf(win) < 0)
            return;
        windows = windows.filter(w => w !== win);
        const ids = Tree.panelIds(win.dockArea.root);
        win.dockArea.mutate(() => null);
        for (const id of ids) {
            const p = panel(id);
            if (!p || (mainArea.windowClosePolicy === Dock.ClosePanels && p.closable))
                continue;
            mainArea.mutate(root => Tree.placeByHint(root, id, hintFor(p, true)));
        }
        Qt.callLater(() => win.destroy());
    }

    // Pop-out windows close themselves once their last panel leaves.
    function scheduleCleanup() {
        Qt.callLater(_closeEmptyWindows);
    }

    function _closeEmptyWindows() {
        const empty = windows.filter(w => !w.dockArea.root);
        if (!empty.length)
            return;
        windows = windows.filter(w => !!w.dockArea.root);
        empty.forEach(w => { w.visible = false; w.destroy(); });
    }

    function _closeAllWindows() {
        const list = windows;
        windows = [];
        list.forEach(w => {
            w.dockArea.mutate(() => null);
            w.visible = false;
            w.destroy();
        });
    }

    // The main window closed or the main area is going away.
    function shutdown() {
        if (_quitting)
            return;
        saveNow();
        _quitting = true;
        _saveTimer.stop();
        const list = windows;
        windows = [];
        list.forEach(w => { w.visible = false; w.destroy(); });
    }

    // ---------------------------------------------------------------------
    //  Drag session. A drag is one platform drag-and-drop operation, started
    //  by DockArea.startDrag; every DockArea's DropArea reports into it.

    function beginDrag(p, sourceArea) {
        console.debug(_log, "begin", p.panelId, "from", sourceArea.isMain ? "main window" : "pop-out");
        dragPanel = p;
        _drop = null;
        _track = null;
        activePanel = p;
        sourceArea.maximizedStackId = "";
    }

    // (x, y) in window coordinates.
    function trackDrag(window, x, y) {
        _track = { window: window, x: x, y: y };
    }

    function proposeDrop(area, target) {
        console.debug(_log, "drop accepted:", target.kind, target.side || target.index, "in", area.isMain ? "main window" : "pop-out");
        _drop = { area: area, target: target };
    }

    function cancelDrop() {
        console.debug(_log, "drop refused (no valid target there)");
        _drop = { cancel: true };
    }

    // Called once the platform drag has ended. The layout is only changed
    // afterwards, from the event loop, never inside the drag loop itself.
    function finishDrag(dropAction) {
        const p = dragPanel, drop = _drop, track = _track;
        console.debug(_log, "finished, action", dropAction, "last seen",
                      track ? Math.round(track.x) + "," + Math.round(track.y) + " in " + track.window.width + "x" + track.window.height : "nowhere",
                      "->", drop && drop.area ? "move" : (!drop && dropAction === Qt.IgnoreAction && _leftWindow(track) ? "tear off" : "cancel"));
        dragPanel = null;
        _drop = null;
        _track = null;
        areas.forEach(a => a.clearDragState());
        if (!p)
            return;
        if (drop && drop.area) {
            Qt.callLater(() => movePanel(p.panelId, drop.area, drop.target));
            return;
        }
        // No window accepted the drop. Released outside every window means
        // "tear off"; anything else (Esc, an invalid spot) cancels. The
        // platform reports both the same way, so look at where the cursor
        // was last seen: near a window border means it left.
        if (!drop && dropAction === Qt.IgnoreAction && _leftWindow(track))
            Qt.callLater(() => popOut(p.panelId));
    }

    function _leftWindow(t) {
        if (!t || !t.window)
            return false;
        const margin = 48;
        return t.x < margin || t.y < margin
            || t.x > t.window.width - margin || t.y > t.window.height - margin;
    }

    // ---------------------------------------------------------------------
    //  Persistence

    function saveLayout() {
        const open = {};
        areas.forEach(a => Tree.panelIds(a.root).forEach(id => { open[id] = true; }));
        return {
            version: 1,
            main: Tree.serialize(mainArea.root),
            windows: windows.map(w => ({
                x: w.x, y: w.y, width: w.width, height: w.height,
                layout: Tree.serialize(w.dockArea.root)
            })),
            closed: panelOrder.filter(id => !open[id])
        };
    }

    function restoreLayout(state) {
        let s = state;
        if (typeof s === "string") {
            try {
                s = JSON.parse(s);
            } catch (e) {
                console.warn("Quilt: restoreLayout: invalid JSON:", e);
                return false;
            }
        }
        if (!s || s.version !== 1) {
            console.warn("Quilt: restoreLayout: unsupported layout");
            return false;
        }
        _closeAllWindows();
        const claimed = {};
        const claim = id => {
            if (!panels[id] || claimed[id])
                return false;
            claimed[id] = true;
            return true;
        };
        let main = Tree.separateSolo(Tree.parse(s.main, claim), canShare);
        (s.windows || []).forEach(w => {
            const root = Tree.separateSolo(Tree.parse(w.layout, id => !!panels[id] && panels[id].floatable && claim(id)), canShare);
            if (!root)
                return;
            const win = mainArea.allowPopOut ? createWindow(w) : null;
            if (win)
                win.dockArea.mutate(() => root);
            else
                Tree.panelIds(root).forEach(id => { delete claimed[id]; });
        });
        const closed = {};
        (s.closed || []).forEach(id => { closed[id] = true; });
        panelOrder.forEach(id => {
            if (!claimed[id] && !closed[id] && panels[id].openOnStart)
                main = Tree.placeByHint(main, id, hintFor(panels[id], false));
        });
        mainArea.mutate(() => main);
        return true;
    }

    // Rebuilds the starting layout: DockArea.defaultLayout, then the panels'
    // area hints (centre panels first, so edge panels wrap around them).
    function resetLayout() {
        _closeAllWindows();
        const claimed = {};
        const claim = id => {
            if (!panels[id] || claimed[id])
                return false;
            claimed[id] = true;
            return true;
        };
        let root = Tree.separateSolo(mainArea.defaultLayout ? Tree.parse(mainArea.defaultLayout, claim) : null, canShare);
        const centre = panelOrder.filter(id => panels[id].area === Dock.Center);
        const edges = panelOrder.filter(id => panels[id].area !== Dock.Center);
        centre.concat(edges).forEach(id => {
            if (!claimed[id] && panels[id].openOnStart)
                root = Tree.placeByHint(root, id, hintFor(panels[id], false));
        });
        mainArea.mutate(() => root);
    }

    // Autosave, enabled by DockArea.autoSaveKey. The Settings object is only
    // created when used: QSettings needs the application's organization name
    // (or domain) and complains without one.
    property var _settings: null
    property Component _settingsComponent: Component {
        Settings { category: "QuiltLayouts" }
    }

    function _store() {
        if (!_settings)
            _settings = _settingsComponent.createObject(manager);
        return _settings;
    }

    property Timer _saveTimer: Timer {
        interval: 500
        onTriggered: manager.saveNow()
    }

    function saveNow() {
        if (_quitting || !mainArea || !mainArea.autoSaveKey)
            return;
        _store().setValue(mainArea.autoSaveKey, JSON.stringify(saveLayout()));
        _store().sync();
    }

    function loadSaved() {
        if (!mainArea.autoSaveKey)
            return false;
        const saved = _store().value(mainArea.autoSaveKey, "");
        return saved ? restoreLayout(String(saved)) : false;
    }

    onLayoutChanged: {
        if (!_quitting && mainArea && mainArea.autoSaveKey)
            _saveTimer.restart();
    }

    property Connections _quitHook: Connections {
        target: Qt.application
        function onAboutToQuit() { manager.saveNow(); }
    }
}
