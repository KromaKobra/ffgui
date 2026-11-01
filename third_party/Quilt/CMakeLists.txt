# Optional: build Quilt as a compiled QML module, with qmlcachegen and the
# files embedded in your binary. You don't need this when you load Quilt from
# an import path (`qml -I`, QQmlEngine::addImportPath).
#
#   add_subdirectory(path/to/Quilt)
#   target_link_libraries(myapp PRIVATE quiltplugin)
#
# and in main.cpp:  Q_IMPORT_QML_PLUGIN(QuiltPlugin)

cmake_minimum_required(VERSION 3.21)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2)

if(QT_KNOWN_POLICY_QTP0001)
    qt_policy(SET QTP0001 NEW)      # resources under :/qt/qml, on the default import path
endif()

set_source_files_properties(DockManager.qml DockStack.qml SplitHandles.qml
    PROPERTIES QT_QML_INTERNAL_TYPE TRUE)

qt_add_library(quilt STATIC)
qt_add_qml_module(quilt
    URI Quilt
    VERSION 1.0
    QML_FILES
        Dock.qml
        DockArea.qml
        DockPanel.qml
        DockStyle.qml
        DockWindow.qml
        DockManager.qml
        DockStack.qml
        SplitHandles.qml
        LayoutTree.js
)

target_link_libraries(quilt PRIVATE Qt6::Quick)
