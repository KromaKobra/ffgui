# C++ unit tests (QtTest). One executable per area; parsers run against
# output captured from this machine's ffmpeg (fixtures/).
function(ffgui_add_test name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "LIBRARIES")
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBRARIES})
    target_compile_definitions(${name} PRIVATE FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS core TIMEOUT 60)
endfunction()

ffgui_add_test(tst_format LIBRARIES ffgui_core)
ffgui_add_test(tst_catalog LIBRARIES ffgui_core)
ffgui_add_test(tst_tools LIBRARIES ffgui_core)
ffgui_add_test(tst_caps LIBRARIES ffgui_core)
ffgui_add_test(tst_probe LIBRARIES ffgui_core)
ffgui_add_test(tst_analysis LIBRARIES ffgui_core)
ffgui_add_test(tst_media LIBRARIES ffgui_core)
ffgui_add_test(tst_dsp LIBRARIES ffgui_core)
ffgui_add_test(tst_models LIBRARIES ffgui_backend)

ffgui_add_test(tst_delivery LIBRARIES ffgui_backend)
ffgui_add_test(tst_tooling LIBRARIES ffgui_backend)
