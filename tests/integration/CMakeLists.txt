# Integration tests: the real ffmpeg, ffprobe and audio output on generated
# files. They skip themselves when ffmpeg is missing (ctest -LE ffmpeg leaves
# them out).
qt_add_executable(tst_pipeline tst_pipeline.cpp)
target_link_libraries(tst_pipeline PRIVATE Qt6::Test ffgui_core)
add_test(NAME tst_pipeline COMMAND tst_pipeline)
set_tests_properties(tst_pipeline PROPERTIES LABELS ffmpeg TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
