ffgui_add_qml_module(ffgui_tools_console
    URI FFgui.Tools.Console
    QML_FILES
        ConsolePanel.qml
        CommandTab.qml
        LogTab.qml
        GraphTab.qml
    IMPORTS FFgui.Backend FFgui.Theme FFgui.Controls
)
