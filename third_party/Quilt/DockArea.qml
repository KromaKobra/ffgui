pragma ComponentBehavior: Bound
import QtQuick
import "LayoutTree.js" as Tree

// A docking surface. Declare one in your main window and put DockPanels in it:
//
//     DockArea {
//         anchors.fill: parent
//         DockPanel { panelId: "viewport"; title: "Viewport"; Viewport { } }
//         DockPanel { panelId: "outliner"; title: "Outliner"; area: Dock.Left; Outliner { } }
//     }
//
// That first area is the *main* area: it owns the panels, the configuration
// below and the shared DockManager. Every pop-out window hosts a secondary
// DockArea that shares the same manager, so panels move freely between them.
Item {
    id: area

    // ---------------------------------------------------------------------
    //  Configuration (read from the main area)

    default property list<DockPanel> panels

    // Optional starting layout in LayoutTree's compact JSON form, e.g.
    // { row: [ { tabs: ["outliner"], size: 0.2 }, { tabs: ["viewport"] } ] }.
    // Panels it doesn't mention are placed by their `area` hint.
    property var defaultLayout: null

    property DockStyle style: DockStyle { }

    // Creates pop-out windows. Defaults to DockWindow; supply your own
    // DockWindow { ... } to add menus, toolbars or a different title.
    property Component windowComponent: null

    property bool allowPopOut: true
    property int windowClosePolicy: Dock.ReturnPanels

    // When set, the layout (including pop-out windows) is saved under this
    // key with QtCore Settings and restored on the next start.
    property string autoSaveKey: ""

    // ---------------------------------------------------------------------
    //  State

    // Shared with every area of this docking system. Only set it yourself
    // when writing a custom pop-out window (see DockWindow).
    property var manager: null
    readonly property bool isMain: !!manager && manager.mainArea === area
    readonly property var activePanel: manager ? manager.activePanel : null

    // Smallest size that fits every docked panel's minimum; bind your
    // window's minimumWidth/minimumHeight to these.
    readonly property real minimumWidth: _minimum.width
    readonly property real minimumHeight: _minimum.height

    // Titles of the visible tabs, e.g. for a window title.
    readonly property string title: {
        return Tree.stacks(_tree(revision)).map(s => {
            const p = area.panel(s.current);
            return p ? p.title : s.current;
        }).join(" · ");
    }

    readonly property int stackCount: Tree.stackIds(_tree(revision)).length
    readonly property int panelCount: Tree.panelIds(_tree(revision)).length

    // Main area: emitted after any change to any area's layout.
    signal layoutChanged()

    // ---------------------------------------------------------------------
    //  API (forwards to the shared manager)

    function panel(panelId) { return manager ? manager.panel(panelId) : null; }
    function openPanel(panelId) { if (manager) manager.openPanel(panelId); }
    function closePanel(panelId) { if (manager) manager.closePanel(panelId); }
    function popOut(panelId) { return manager ? manager.popOut(panelId) : null; }
    function toggleMaximized(panelId) { if (manager) manager.toggleMaximized(panelId); }
    function saveLayout() { return manager ? manager.saveLayout() : null; }
    function restoreLayout(state) { return manager ? manager.restoreLayout(state) : false; }
    function resetLayout() { if (manager) manager.resetLayout(); }

    // Registers a panel created at runtime (e.g. with Component.createObject).
    // It opens unless `open` is false (default: the panel's openOnStart).
    function addPanel(p, open) {
        if (!manager || !manager.addPanel(p))
            return false;
        if (open === undefined ? p.openOnStart : open)
            manager.openPanel(p.panelId);
        return true;
    }

    // Closes and unregisters a panel. The panel object is not destroyed.
    function removePanel(panelId) { if (manager) manager.removePanel(panelId); }

    // ---------------------------------------------------------------------
    //  Internals: the layout tree of this area and its rendering

    property var root: null
    property int revision: 0                    // bumped on every layout change
    property string maximizedStackId: ""
    property bool resizing: false
    property bool _animate: false
    property int _geometryRevision: 0
    property var _stackItems: ({})              // stackId -> DockStack
    property var _placed: ({})                  // panelIds placed here last time
    property var _resizeState: null
    property var _target: null                  // current drop target while dragging
    property bool _dragOver: false

    function _constraints(panelId) {
        const p = panel(panelId);
        return p ? { minimumWidth: p.minimumWidth, minimumHeight: p.minimumHeight,
                     maximumWidth: p.maximumWidth, maximumHeight: p.maximumHeight } : null;
    }

    function _options() {
        return { gap: style.gap, tabHeight: style.tabHeight, constraints: _constraints };
    }

    readonly property var geometry: {
        const o = _options();
        o.maximized = maximizedStackId;
        return Tree.computeLayout(_tree(revision + _geometryRevision), width, height, o);
    }

    readonly property var _minimum: Tree.minimumSize(_tree(revision), _options())

    // The tree is plain JS data, so bindings can't see it change. They read
    // it through this, passing a revision counter as the dependency.
    function _tree(rev) {
        return root;
    }

    // Replaces the tree with fn(tree) and brings the view up to date.
    // Geometry changes animate unless `animate` is false.
    function mutate(fn, animate) {
        root = fn(root);
        if (maximizedStackId && !Tree.findStack(root, maximizedStackId))
            maximizedStackId = "";
        if (animate !== false && style.animationDuration > 0) {
            _animate = true;
            animationTimer.restart();
        }
        revision += 1;
        _sync();
        if (!manager)
            return;
        if (!isMain && !root)
            manager.scheduleCleanup();
        manager.layoutChanged();
    }

    function setCurrent(panelId) {
        const s = Tree.findStackOf(root, panelId);
        if (!s)
            return;
        if (s.current !== panelId)
            mutate(r => { s.current = panelId; return r; }, false);
        if (manager)
            manager.activePanel = panel(panelId);
    }

    // A fresh snapshot of one stack, for DockStack's bindings.
    function stackInfo(stackId, rev) {
        const s = Tree.findStack(root, stackId);
        return s ? { panels: s.panels.slice(), current: s.current } : null;
    }

    // Rough geometry of a panel, used to size (and, where allowed, place) a
    // pop-out window where the panel was.
    function panelGeometry(panelId) {
        const s = Tree.findStackOf(root, panelId);
        const r = s ? geometry.rects[s.id] : null;
        if (!r)
            return null;
        const g = mapToGlobal(r.x, r.y);
        return { x: g.x + 32, y: g.y + 32, width: r.width, height: r.height };
    }

    function _sync() {
        const ids = Tree.stackIds(root);
        const want = {};
        ids.forEach(id => { want[id] = true; });
        const have = {};
        for (let i = 0; i < stackModel.count; ++i)
            have[stackModel.get(i).stackId] = true;
        // New stacks first, then move panels in, then drop old stacks, so a
        // panel is never left inside a delegate that is being destroyed.
        ids.forEach(id => { if (!have[id]) stackModel.append({ stackId: id }); });
        _placePanels();
        for (let i = stackModel.count - 1; i >= 0; --i)
            if (!want[stackModel.get(i).stackId])
                stackModel.remove(i);
    }

    function _placePanels() {
        const now = {};
        Tree.stacks(root).forEach(s => {
            const item = _stackItems[s.id];
            if (!item)
                return;
            s.panels.forEach(pid => {
                const p = panel(pid);
                if (!p)
                    return;
                now[pid] = true;
                if (p.parent !== item.contentHost) {
                    p.anchors.fill = undefined;
                    p.parent = item.contentHost;
                    p.anchors.fill = item.contentHost;
                }
                p._area = area;
                p._stackId = s.id;
                p._current = pid === s.current;
                p.visible = p._current;
            });
        });
        for (const pid in _placed) {
            const p = panel(pid);
            if (!now[pid] && p && p._area === area)
                manager.parkPanel(p);
        }
        _placed = now;
    }

    // ---- split handles ----
    //
    // Handles that continue each other in a straight line move together (see
    // LayoutTree "Lines of split handles"). `independent` (Ctrl) moves only
    // the segment under the cursor and turns snapping off.

    // The handle under (x, y), or "".
    function handleAt(x, y) {
        return Tree.handleAt(geometry, x, y, style.handleGrabSize);
    }

    // Rects to highlight for a hover at (x, y) over handle `key`.
    function handleHighlight(key, independent, x, y) {
        const h = key ? geometry.handles[key] : null;
        if (!h)
            return [];
        if (independent)
            return [Tree.segmentAt(root, geometry, key, h.horizontal ? y : x)];
        return Tree.linkedHandles(geometry, key, { gap: style.gap }).map(k => geometry.handles[k]);
    }

    // Starts dragging the handle line at (x, y). Returns false if there is
    // no handle there.
    function beginResize(x, y, independent) {
        let key = handleAt(x, y);
        if (!key)
            return false;
        if (independent) {
            const isolated = Tree.isolateSegment(root, geometry, key);
            if (isolated) {
                mutate(() => isolated, false);
                key = handleAt(x, y);
            }
        }
        const keys = independent ? [key] : Tree.linkedHandles(geometry, key, { gap: style.gap });
        const opts = _options();
        const states = keys.map(k => {
            const h = geometry.handles[k];
            return Tree.beginResize(root, geometry, h.splitId, h.index, opts);
        }).filter(st => !!st);
        if (!states.length)
            return false;
        const h = geometry.handles[key];
        _resizeState = {
            keys: keys,
            states: states,
            horizontal: h.horizontal,
            origin: h.horizontal ? h.x + h.width / 2 : h.y + h.height / 2,
            candidates: Tree.snapCandidates(geometry, keys, { gap: style.gap }),
            snapKey: ""
        };
        resizing = true;
        return true;
    }

    // `translation` is how far the pointer moved since the press. With
    // `snap`, the line jumps onto a handle it is about to continue.
    function resize(translation, snap) {
        const r = _resizeState;
        if (!r)
            return;
        const range = Tree.lineRange(r.states);
        let d = Math.min(range[1], Math.max(range[0], r.horizontal ? translation.x : translation.y));
        r.snapKey = "";
        if (snap) {
            const raw = d;
            let best = style.snapDistance + 0.001;
            r.candidates.forEach(c => {
                const to = c.pos - r.origin;
                if (Math.abs(to - raw) < best && to >= range[0] && to <= range[1]) {
                    best = Math.abs(to - raw);
                    d = to;
                    r.snapKey = c.key;
                }
            });
        }
        Tree.applyLineResize(r.states, d);
        _geometryRevision += 1;
    }

    function endResize() {
        _resizeState = null;
        resizing = false;
        if (manager)
            manager.layoutChanged();
    }

    // ---- drag and drop ----

    // Starts the platform drag for a panel; returns when the drag is over.
    function startDrag(p, imageUrl, hotSpot) {
        if (!manager || !p || !p.movable || manager.dragPanel)
            return;
        manager.beginDrag(p, area);
        dragProxy.Drag.mimeData = { "application/x-quilt-panel": p.panelId };
        dragProxy.Drag.imageSource = imageUrl || "";
        dragProxy.Drag.hotSpot = hotSpot || Qt.point(0, 0);
        dragProxy.Drag.active = true;
    }

    function _ownsDrag(drag) {
        return !!manager && !!manager.dragPanel
            && (!drag.source || drag.source.quiltManager === manager);
    }

    // The drag moved to (x, y) in this area's coordinates.
    function dragMoved(x, y) {
        const p = manager ? manager.dragPanel : null;
        if (!p)
            return;
        const w = Window.window;
        if (w) {
            const pt = mapToItem(null, x, y);
            manager.trackDrag(w, pt.x, pt.y);
        }
        _dragOver = true;
        let t = Tree.hitTest(root, geometry, width, height, x, y, {
            tabHeight: style.tabHeight,
            indicatorSize: style.dropIndicatorSize,
            indicatorInset: 8,
            edgeFraction: p.areaSize,
            tabIndexAt: (id, lx) => _stackItems[id] ? _stackItems[id].tabIndexAt(lx) : -1,
            canTab: id => Tree.canJoin(Tree.findStack(root, id), p.panelId, manager.canShare)
        });
        if (t === null)
            return;                             // over a gap: keep the current target
        if ((!isMain && !p.floatable) || Tree.isNoop(root, p.panelId, t))
            t = null;
        _target = t;
    }

    // The drag was released over this area. Returns whether it was accepted.
    function dropped() {
        const t = _target;
        clearDragState();
        if (!manager || !manager.dragPanel)
            return false;
        if (!t) {
            manager.cancelDrop();
            return false;
        }
        manager.proposeDrop(area, t);
        return true;
    }

    function clearDragState() {
        _target = null;
        _dragOver = false;
    }

    readonly property var _previewRect: {
        const t = _target;
        if (!t)
            return null;
        if (t.kind === "tab" && t.inTabBar) {
            const s = _stackItems[t.stackId];
            const r = t.rect;
            const x = s ? s.tabInsertX(t.index) : 0;
            return { x: r.x + x - 2, y: r.y + 4, width: 4, height: style.tabHeight - 8 };
        }
        return t.rect;
    }

    // ---------------------------------------------------------------------
    //  Lifecycle

    Component.onCompleted: {
        if (manager) {
            manager.registerArea(area);
            return;
        }
        manager = managerComponent.createObject(area, { mainArea: area });
        manager.registerArea(area);
        for (let i = 0; i < panels.length; ++i)
            manager.addPanel(panels[i]);
        if (!manager.loadSaved())
            manager.resetLayout();
    }

    Component.onDestruction: {
        if (!manager)
            return;
        if (isMain)
            manager.shutdown();
        manager.unregisterArea(area);
    }

    function _mainWindowClosed() {
        const w = Window.window;
        if (manager && (!w || !w.visible))
            manager.shutdown();
    }

    Connections {
        target: area.isMain ? area.Window.window : null
        function onClosing() { Qt.callLater(area._mainWindowClosed); }
    }

    Connections {
        target: area.isMain ? area.manager : null
        function onLayoutChanged() { area.layoutChanged(); }
    }

    Component {
        id: managerComponent
        DockManager { }
    }

    Timer {
        id: animationTimer
        interval: area.style.animationDuration + 50
        onTriggered: area._animate = false
    }

    // ---------------------------------------------------------------------
    //  Visuals

    Rectangle {
        anchors.fill: parent
        color: area.style.background
    }

    Text {
        anchors.centerIn: parent
        visible: area.root === null
        text: area._dragOver ? "Drop to dock here" : "No panels open"
        color: area.style.textDim
        font.pixelSize: area.style.fontPixelSize + 1
    }

    Repeater {
        model: ListModel { id: stackModel }
        delegate: DockStack {
            area: area
        }
        onItemAdded: (index, item) => { area._stackItems[(item as DockStack).stackId] = item; }
        onItemRemoved: (index, item) => {
            const id = (item as DockStack).stackId;
            if (area._stackItems[id] === item)
                delete area._stackItems[id];
        }
    }

    SplitHandles {
        area: area
        anchors.fill: parent
        z: 2
    }

    // Drop preview: where the dragged panel will land.
    Rectangle {
        id: preview
        readonly property var r: area._previewRect
        property var shown: ({ x: 0, y: 0, width: 0, height: 0 })
        onRChanged: {
            if (r) {
                fade.enabled = preview.opacity > 0;
                shown = r;
            }
        }
        z: 10
        x: shown.x
        y: shown.y
        width: shown.width
        height: shown.height
        radius: area.style.radius
        color: area.style.dropFill
        border.width: 2
        border.color: area.style.accent
        opacity: r ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 90 } }
        Behavior on x { id: fade; NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
        Behavior on y { enabled: fade.enabled; NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
        Behavior on width { enabled: fade.enabled; NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
        Behavior on height { enabled: fade.enabled; NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
    }

    // "Dock along the whole edge" indicators, shown while a drag is over us.
    Repeater {
        model: ["left", "right", "top", "bottom"]
        delegate: Rectangle {
            id: indicator
            required property string modelData
            readonly property var r: Tree.edgeIndicators(area.width, area.height, area.style.dropIndicatorSize, 8)[modelData]
            readonly property bool hot: !!area._target && area._target.kind === "edge" && area._target.side === modelData
            readonly property bool vertical: modelData === "left" || modelData === "right"
            z: 11
            visible: area._dragOver && area.root !== null
            x: r.x
            y: r.y
            width: r.width
            height: r.height
            radius: 6
            color: hot ? area.style.accent : area.style.tabBar
            border.width: 1
            border.color: area.style.accent
            opacity: 0.95

            // A bar on the matching side of the square shows the edge.
            Rectangle {
                readonly property real inset: 7
                x: indicator.modelData === "right" ? indicator.width - inset - width : inset
                y: indicator.modelData === "bottom" ? indicator.height - inset - height : inset
                width: indicator.vertical ? indicator.width * 0.3 : indicator.width - 2 * inset
                height: indicator.vertical ? indicator.height - 2 * inset : indicator.height * 0.3
                radius: 2
                color: indicator.hot ? area.style.text : area.style.accent
            }
        }
    }

    DropArea {
        anchors.fill: parent
        keys: ["application/x-quilt-panel"]
        onEntered: drag => {
            if (!area._ownsDrag(drag)) {
                drag.accepted = false;
                return;
            }
            area.dragMoved(drag.x, drag.y);
        }
        onPositionChanged: drag => area.dragMoved(drag.x, drag.y)
        onExited: area.clearDragState()
        onDropped: drop => {
            if (area.dropped())
                drop.accept(Qt.MoveAction);
            else
                drop.accepted = false;
        }
    }

    // Covers the rest of the window (toolbars, menus) beneath everything
    // else, so a release there counts as "cancel", not "torn off".
    DropArea {
        parent: area.Window.contentItem
        anchors.fill: parent
        z: -1
        keys: ["application/x-quilt-panel"]
        onEntered: drag => {
            if (!area._ownsDrag(drag)) {
                drag.accepted = false;
                return;
            }
            area.manager.trackDrag(area.Window.window, drag.x, drag.y);
        }
        onPositionChanged: drag => {
            if (area.manager)
                area.manager.trackDrag(area.Window.window, drag.x, drag.y);
        }
        onDropped: drop => {
            if (area.manager)
                area.manager.cancelDrop();
            drop.accepted = false;
        }
    }

    // Runs the platform drag. It lives as long as the area, so rebuilding
    // tabs never destroys the object that owns an active drag.
    Item {
        id: dragProxy
        readonly property var quiltManager: area.manager
        Drag.dragType: Drag.Automatic
        Drag.supportedActions: Qt.MoveAction
        Drag.proposedAction: Qt.MoveAction
        Drag.onDragFinished: dropAction => {
            if (area.manager)
                area.manager.finishDrag(dropAction);
        }
    }
}
