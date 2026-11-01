cmake_minimum_required(VERSION 3.24)
project(ffgui VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.12 REQUIRED COMPONENTS
    Core Gui Qml Quick QuickControls2 Multimedia Concurrent Test QuickTest)
qt_standard_project_setup(REQUIRES 6.12)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(FfguiQml)

enable_testing()

# Quilt, vendored by scripts/sync-quilt.sh. Never edited here.
add_subdirectory(third_party/Quilt)

add_subdirectory(src/core)
add_subdirectory(src/backend)

# QML modules, in dependency order (see CLAUDE.md for the layering rules).
add_subdirectory(qml/Theme)
add_subdirectory(qml/Controls)
add_subdirectory(qml/Sources)
add_subdirectory(qml/Media)
add_subdirectory(qml/Meters)
add_subdirectory(qml/Chain)
add_subdirectory(qml/Inspector)
add_subdirectory(qml/Output)
add_subdirectory(qml/Tools)
add_subdirectory(qml/Shell)

add_subdirectory(src/app)
add_subdirectory(tests)
