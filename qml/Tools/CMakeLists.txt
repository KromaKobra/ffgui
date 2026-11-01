# The thirteen toggleable panels, one QML module each.
foreach(panel Analysis Scopes Metadata Routing Compare Console Queue Presets Record Generate StreamOut Transcript Capabilities)
    add_subdirectory(${panel})
endforeach()
