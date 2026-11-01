pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Output
import FFgui.Theme

// ffgui's window. Signal flows left to right through four stages (Load,
// Inspect, Shape, Deliver):
//
//   Header bar, Output bar, Status bar   fixed, outside the dock: they can't
//                                        close, move or pop out, and keeping
//                                        the Output bar outside puts the
//                                        dock's bottom zone above it
//   everything else                      dock panels (DockShell), arranged by
//                                        the current workspace
ApplicationWindow {
    id: window

    width: 1600
    height: 960
    minimumWidth: Math.max(960, dockShell.minimumWidth)
    minimumHeight: dockShell.minimumHeight + headerBar.implicitHeight + outputBar.implicitHeight + statusBar.implicitHeight
    visible: true
    title: "ffgui — " + App.project.name + (App.project.modified ? " •" : "") + " — " + workspaceController.current
    color: Theme.window

    // For tests and screenshots.
    readonly property alias dock: dockShell
    readonly property alias workspaces: workspaceController
    readonly property alias actions: actionRegistry

    // Fusion takes its colours from the palette.
    palette {
        window: Theme.window
        windowText: Theme.text
        base: Theme.base
        alternateBase: Theme.stripe
        text: Theme.text
        button: Theme.control
        buttonText: Theme.text
        light: Theme.borderStrong
        midlight: Theme.border
        mid: Theme.line
        dark: Theme.well
        shadow: Theme.statusBar
        highlight: Theme.accent
        highlightedText: Theme.textOnAccent
        placeholderText: Theme.textDim
        toolTipBase: Theme.control
        toolTipText: Theme.text
        link: Theme.accentSoft
        disabled {
            text: Theme.textFaint
            buttonText: Theme.textFaint
            windowText: Theme.textFaint
            highlight: Theme.neutral
            highlightedText: Theme.textFaint
        }
    }

    WorkspaceController {
        id: workspaceController
        dock: dockShell
    }

    // The dock builds its first layout in its own Component.onCompleted, so
    // apply the workspace after that has run.
    Component.onCompleted: Qt.callLater(workspaceController.start)
    onClosing: workspaceController.remember()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        HeaderBar {
            id: headerBar
            Layout.fillWidth: true
            dock: dockShell
            workspaces: workspaceController
            onPaletteRequested: text => commandPalette.show(text)
        }

        DockShell {
            id: dockShell
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        OutputBar {
            id: outputBar
            Layout.fillWidth: true
        }

        StatusBar {
            id: statusBar
            Layout.fillWidth: true
        }
    }

    ToastHost {
        anchors.fill: parent
        anchors.bottomMargin: outputBar.height + statusBar.height
    }

    DropOverlay { anchors.fill: parent }

    ActionRegistry {
        id: actionRegistry
        workspaces: workspaceController
    }
    CommandPalette {
        id: commandPalette
        registry: actionRegistry
    }

    FileDialogs { }
    Shortcuts {
        dock: dockShell
        palettePopup: commandPalette
    }
}
