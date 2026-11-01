qt_add_executable(ffgui main.cpp)
target_compile_definitions(ffgui PRIVATE FFGUI_VERSION="${PROJECT_VERSION}")
target_link_libraries(ffgui PRIVATE Qt6::Gui Qt6::Quick Qt6::QuickControls2 ffgui_backend)
ffgui_link_qml_plugins(ffgui)
