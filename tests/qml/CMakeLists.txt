# QML tests (QuickTest), run offscreen. One executable with every ffgui QML
# module linked in; each tst_*.qml file gets its own engine (and App).
qt_add_executable(tst_qml main.cpp)
target_compile_definitions(tst_qml PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tst_qml PRIVATE Qt6::QuickTest Qt6::Quick Qt6::QuickControls2 ffgui_backend)
ffgui_link_qml_plugins(tst_qml)

add_test(NAME qml COMMAND tst_qml)
set_tests_properties(qml PROPERTIES LABELS qml ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
