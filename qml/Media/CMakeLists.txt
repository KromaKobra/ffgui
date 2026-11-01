ffgui_add_qml_module(ffgui_media
    URI FFgui.Media
    QML_FILES
        MediaView.qml
        QuickConvert.qml
        Lane.qml
        TimeRuler.qml
        ChapterFlag.qml
        OverviewStrip.qml
        OverlayBand.qml
        Crosshair.qml
        SelectionToolbar.qml
        LayersMenu.qml
        TransportBar.qml
    IMPORTS FFgui.Backend FFgui.Theme FFgui.Controls
)
