import QtQuick

// One dockable panel. Its children are the panel's content and fill it:
//
//     DockPanel { panelId: "inspector"; title: "Inspector"; area: Dock.Right
//                 Inspector { } }
//
// A panel is created once and never re-created: docking, tabbing, popping
// out and closing only move it to a new parent, so its content keeps all of
// its state. Quilt drives `visible` and the geometry; don't bind them.
Item {
    id: panel

    // Unique, stable id. Saved layouts refer to panels by it.
    required property string panelId

    property string title: panelId
    property url icon

    // Placement used when no saved/default layout or previous position
    // applies. Panels with the same area share one tab group; Center panels
    // form the middle of the layout and edge panels dock around it in
    // declaration order. areaSize is the fraction of the window it takes.
    property int area: Dock.Center
    property real areaSize: 0.25
    property bool openOnStart: true

    // Restrictions. The defaults allow everything.
    property bool movable: true                 // can be dragged to a new place
    property bool floatable: true               // can live in a pop-out window
    property bool closable: true                // shows a close button
    property bool tabbable: true                // can share a tab group; false gives it its
                                                // own group with a plain title bar
    property real minimumWidth: 120
    property real minimumHeight: 80
    property real maximumWidth: Number.POSITIVE_INFINITY
    property real maximumHeight: Number.POSITIVE_INFINITY

    // Fill behind the content. Set "transparent" to let whatever is behind
    // the dock area show through.
    property color background: _manager && _manager.style ? _manager.style.panel : "#1e2127"

    // State (read-only for applications)
    readonly property bool isOpen: _area !== null
    readonly property bool isCurrent: _current      // the visible tab of its group
    readonly property bool isPoppedOut: _area !== null && !_area.isMain
    readonly property var dockArea: _area           // the DockArea showing it, or null

    function open() { if (_manager) _manager.openPanel(panelId); }
    function close() { if (_manager) _manager.closePanel(panelId); }
    function popOut() { if (_manager) _manager.popOut(panelId); }
    // Opens the panel if needed, makes it the current tab and raises its window.
    function raise() { open(); }

    // Internal: maintained by DockManager / DockArea.
    property var _manager: null
    property var _area: null
    property bool _current: false
    property string _stackId: ""
    property string _lastStackId: ""

    visible: false
}
