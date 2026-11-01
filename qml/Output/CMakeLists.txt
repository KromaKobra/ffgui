ffgui_add_qml_module(ffgui_output
    URI FFgui.Output
    QML_FILES
        OutputBar.qml
        TargetChip.qml
        FormatControls.qml
        QualityControl.qml
        SignalFormat.qml
        MetadataPolicySelector.qml
        RenderButton.qml
        CommandPreview.qml
    IMPORTS FFgui.Backend FFgui.Theme FFgui.Controls
)
