# ffgui_core: the pure C++ side (QtCore/Gui/Multimedia), no QML. Each folder
# is small and has its own unit tests under tests/core.
qt_add_library(ffgui_core STATIC
    util/Format.cpp util/Format.h
    catalog/Catalog.cpp catalog/Catalog.h
)
target_include_directories(ffgui_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ffgui_core PUBLIC Qt6::Core Qt6::Gui)

qt_add_resources(ffgui_core catalog_data
    PREFIX /ffgui/catalog
    BASE catalog/data
    FILES
        catalog/data/steps.json
        catalog/data/codecs.json
        catalog/data/presets.json
)
