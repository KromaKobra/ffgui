import QtQuick
import QtQuick.Controls

// The window a popped-out panel lives in. It hosts a full DockArea, so more
// panels can be docked into it, and it closes itself when its last panel
// leaves. Closing it returns its panels to the main window (see
// DockArea.windowClosePolicy).
//
// To give pop-outs your app's own chrome, set DockArea.windowComponent to a
// Component whose root is a DockWindow; header, menuBar, footer and title
// all work as on any ApplicationWindow.
ApplicationWindow {
    id: window

    required property var manager
    readonly property DockArea dockArea: area

    // Panel content inherits palette and font from the window it is in, so
    // a pop-out copies them from the main window to look the same.
    readonly property var mainWindow: manager.mainArea ? manager.mainArea.Window.window : null

    width: 480
    height: 360
    minimumWidth: Math.max(160, area.minimumWidth)
    minimumHeight: Math.max(120, area.minimumHeight)
    title: area.title
    color: manager.style.background

    Binding on palette { when: !!window.mainWindow; value: window.mainWindow.palette }
    Binding on font { when: !!window.mainWindow; value: window.mainWindow.font }

    DockArea {
        id: area
        anchors.fill: parent
        manager: window.manager
        style: window.manager.style
    }

    onClosing: window.manager.windowClosing(window)
}
