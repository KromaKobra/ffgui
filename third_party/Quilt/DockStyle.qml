import QtQuick

// Colours and metrics for every DockArea in one docking system. Override
// individual values on the main area, e.g. `style.accent: "#ff8a3d"`, or
// assign a whole DockStyle { }. Pop-out windows share the main area's style.
QtObject {
    // Surfaces
    property color background: "#111317"        // behind the panels, visible in the gaps
    property color panel: "#1e2127"             // default DockPanel.background
    property color tabBar: "#16181c"
    property color tabHover: "#23262d"

    // Text and highlights
    property color text: "#d7dae0"
    property color textDim: "#7f8591"
    property color accent: "#4c8dff"
    property color dropFill: Qt.rgba(accent.r, accent.g, accent.b, 0.22)

    // Metrics
    property real gap: 4                        // space between panels (0 for a seamless look)
    property real radius: 6                     // corner radius of each panel
    property real tabHeight: 30
    property real handleGrabSize: 8             // grab width of split handles, even when gap is 0
    property real snapDistance: 10              // a dragged split line snaps onto one this close
    property real dropIndicatorSize: 36         // the "dock at full edge" squares shown while dragging
    property int fontPixelSize: 12
    property int animationDuration: 160         // 0 disables layout animations
}
