import QtQuick
import FFgui.Backend

// Application-wide keys. They work in pop-out windows too. Space and the
// arrows are left to text fields while one has focus, as in ffplay otherwise.
Item {
    id: keys

    required property DockShell dock
    required property CommandPalette palettePopup

    function typing() {
        const f = keys.Window.activeFocusItem;
        return !!f && (f instanceof TextInput || f instanceof TextEdit);
    }

    Shortcut {
        sequence: "Space"
        context: Qt.ApplicationShortcut
        onActivated: if (!keys.typing()) App.transport.toggle()
    }
    Shortcut {
        sequence: "Left"
        context: Qt.ApplicationShortcut
        onActivated: if (!keys.typing()) App.transport.nudge(-5)
    }
    Shortcut {
        sequence: "Right"
        context: Qt.ApplicationShortcut
        onActivated: if (!keys.typing()) App.transport.nudge(5)
    }
    Shortcut {
        sequence: "Home"
        context: Qt.ApplicationShortcut
        onActivated: if (!keys.typing()) App.transport.seek(0)
    }
    Shortcut {
        sequence: "End"
        context: Qt.ApplicationShortcut
        onActivated: if (!keys.typing()) App.transport.seek(App.transport.duration)
    }
    Shortcut {
        sequences: ["Ctrl+Return", "Ctrl+Enter"]
        context: Qt.ApplicationShortcut
        onActivated: App.render.start("all")
    }
    Shortcut {
        sequence: "Ctrl+K"
        context: Qt.ApplicationShortcut
        onActivated: keys.palettePopup.show("")
    }
    Shortcut {
        sequences: [StandardKey.Undo]
        context: Qt.ApplicationShortcut
        onActivated: if (!keys.typing()) App.undo.undo()
    }
    Shortcut {
        sequences: [StandardKey.Redo, "Ctrl+Shift+Z"]
        context: Qt.ApplicationShortcut
        onActivated: if (!keys.typing()) App.undo.redo()
    }
    Shortcut {
        sequences: [StandardKey.Open]
        context: Qt.ApplicationShortcut
        onActivated: App.requestDialog("openProject")
    }
    Shortcut {
        sequences: [StandardKey.Save]
        context: Qt.ApplicationShortcut
        onActivated: App.project.fileUrl.toString() !== "" ? App.project.save() : App.requestDialog("saveProject")
    }
    Shortcut {
        sequence: "Ctrl+Shift+S"
        context: Qt.ApplicationShortcut
        onActivated: App.requestDialog("saveProject")
    }
    Shortcut {
        sequence: "Ctrl+Space"
        context: Qt.ApplicationShortcut
        onActivated: if (keys.dock.activePanel) keys.dock.toggleMaximized(keys.dock.activePanel.panelId)
    }
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        onActivated: if (!keys.typing()) App.selection.clear()
    }
    Shortcut {
        sequences: [StandardKey.Delete]
        context: Qt.ApplicationShortcut
        onActivated: {
            if (keys.typing())
                return;
            if (App.selection.kind === "step")
                App.project.removeStep(App.selection.index);
            else if (App.selection.kind === "output")
                App.project.removeOutput(App.selection.index);
        }
    }
}
