ffgui_add_qml_module(ffgui_sources
    URI FFgui.Sources
    QML_FILES
        SourcesPanel.qml
        AddInputMenu.qml
        InputCard.qml
        TrackRow.qml
        ChapterRow.qml
        CoverTile.qml
        LayoutIcon.qml
    IMPORTS FFgui.Backend FFgui.Theme FFgui.Controls
)
