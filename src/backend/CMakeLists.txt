# FFgui.Backend: the C++ objects QML binds to. QML_ELEMENT/QML_SINGLETON
# facades over ffgui_core, list models, and (later) scene-graph items.
ffgui_add_qml_module(ffgui_backend
    URI FFgui.Backend
    SOURCES
        App.cpp App.h
        Analysis.cpp Analysis.h
        Capabilities.cpp Capabilities.h
        CatalogFacade.cpp CatalogFacade.h
        CoverStore.h
        Engine.cpp Engine.h
        MediaState.cpp MediaState.h
        SceneItems.cpp SceneItems.h
        Command.cpp Command.h
        FormatApi.cpp FormatApi.h
        Meters.cpp Meters.h
        ModelRow.cpp ModelRow.h
        Models.cpp Models.h
        Project.cpp Project.h
        Render.cpp Render.h
        RowModel.cpp RowModel.h
        Selection.cpp Selection.h
        Transport.cpp Transport.h
        Undo.cpp Undo.h
        ViewState.cpp ViewState.h
    LIBRARIES ffgui_core Qt6::Gui Qt6::Quick Qt6::Multimedia
)
target_include_directories(ffgui_backend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ffgui_backend PUBLIC ffgui_core Qt6::Qml Qt6::Gui)
