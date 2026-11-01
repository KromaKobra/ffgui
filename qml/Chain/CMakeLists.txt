ffgui_add_qml_module(ffgui_chain
    URI FFgui.Chain
    QML_FILES
        ChainPanel.qml
        ChainRowView.qml
        ChainGraphView.qml
        StepBlock.qml
        NodeCard.qml
        Connector.qml
        GhostBlock.qml
        EndBlock.qml
        StepPicker.qml
        MiniVisual.qml
    IMPORTS FFgui.Backend FFgui.Theme FFgui.Controls
)
