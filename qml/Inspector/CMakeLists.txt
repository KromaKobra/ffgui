ffgui_add_qml_module(ffgui_inspector
    URI FFgui.Inspector
    QML_FILES
        InspectorPanel.qml
        ProjectSheet.qml
        InputSheet.qml
        TrackSheet.qml
        StepSheet.qml
        RangeSheet.qml
        OutputSheet.qml
        StepFooter.qml
        ChannelLayoutDiagram.qml
        TimeRangeEditor.qml
    IMPORTS FFgui.Backend FFgui.Theme FFgui.Controls
)
