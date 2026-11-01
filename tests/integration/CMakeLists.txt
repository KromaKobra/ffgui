# Integration tests: the real ffmpeg, ffprobe and audio output on generated
# files. They skip themselves when ffmpeg is missing (ctest -LE ffmpeg leaves
# them out).
qt_add_executable(tst_pipeline tst_pipeline.cpp)
target_link_libraries(tst_pipeline PRIVATE Qt6::Test ffgui_core)
add_test(NAME tst_pipeline COMMAND tst_pipeline)
set_tests_properties(tst_pipeline PROPERTIES LABELS ffmpeg TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_delivery_pipeline tst_delivery_pipeline.cpp)
target_link_libraries(tst_delivery_pipeline PRIVATE Qt6::Test ffgui_backend)
add_test(NAME tst_delivery_pipeline COMMAND tst_delivery_pipeline)
set_tests_properties(tst_delivery_pipeline PROPERTIES LABELS ffmpeg TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_tooling_pipeline tst_tooling_pipeline.cpp)
target_link_libraries(tst_tooling_pipeline PRIVATE Qt6::Test ffgui_backend)
add_test(NAME tst_tooling_pipeline COMMAND tst_tooling_pipeline)
set_tests_properties(tst_tooling_pipeline PROPERTIES LABELS ffmpeg TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_graph_pipeline tst_graph_pipeline.cpp)
target_link_libraries(tst_graph_pipeline PRIVATE Qt6::Test ffgui_backend)
add_test(NAME tst_graph_pipeline COMMAND tst_graph_pipeline)
set_tests_properties(tst_graph_pipeline PROPERTIES LABELS ffmpeg TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
