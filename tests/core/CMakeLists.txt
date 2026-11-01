# C++ unit tests (QtTest). One executable per area.
function(ffgui_add_test name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "LIBRARIES")
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBRARIES})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS core)
endfunction()

ffgui_add_test(tst_format LIBRARIES ffgui_core)
ffgui_add_test(tst_catalog LIBRARIES ffgui_core)
ffgui_add_test(tst_models LIBRARIES ffgui_backend)
