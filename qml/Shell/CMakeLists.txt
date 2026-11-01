# FFgui.Shell: the window, the dock and everything around it. The only module
# that knows about Quilt (DockShell.qml), and the only one that imports the
# feature modules.
ffgui_add_qml_module(ffgui_shell
    URI FFgui.Shell
    QML_FILES
        Main.qml
        DockShell.qml
        WorkspaceController.qml
        PanelsMenu.qml
        HeaderBar.qml
        StatusBar.qml
        ProjectMenu.qml
        ActionRegistry.qml
        CommandPalette.qml
        ToastHost.qml
        DropOverlay.qml
        FileDialogs.qml
        Shortcuts.qml
        PanelRegistry.js
        Workspaces.js
    IMPORTS FFgui.Backend FFgui.Theme FFgui.Controls
)
