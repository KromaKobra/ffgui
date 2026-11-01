# ffgui_core: the pure C++ side (QtCore/Gui/Multimedia), no QML. Each folder
# is small and has its own unit tests under tests/core.
qt_add_library(ffgui_core STATIC
    project/Recipe.cpp project/Recipe.h project/SnapshotCommand.h
    command/CommandBuilder.cpp command/CommandBuilder.h
    command/GraphProbe.cpp command/GraphProbe.h
    render/RenderQueue.cpp render/RenderQueue.h
    util/Format.cpp util/Format.h
    catalog/Catalog.cpp catalog/Catalog.h
    tools/FfTools.cpp tools/FfTools.h
    tools/ProcessJob.cpp tools/ProcessJob.h
    tools/ProgressParser.cpp tools/ProgressParser.h
    tools/ShellQuote.cpp tools/ShellQuote.h
    log/LogStore.cpp log/LogStore.h
    caps/BuildInfo.cpp caps/BuildInfo.h
    caps/CapabilityProbe.cpp caps/CapabilityProbe.h
    caps/HelpParser.cpp caps/HelpParser.h
    caps/HelpCache.cpp caps/HelpCache.h
    probe/ChannelLayouts.cpp probe/ChannelLayouts.h
    probe/ProbeResult.h
    probe/ProbeParser.cpp probe/ProbeParser.h
    probe/Prober.cpp probe/Prober.h
    media/Fft.cpp media/Fft.h
    media/PeakPyramid.cpp media/PeakPyramid.h
    media/Spectrogram.cpp media/Spectrogram.h
    media/MediaData.cpp media/MediaData.h
    media/PcmReader.cpp media/PcmReader.h
    analysis/AnalysisParsers.cpp analysis/AnalysisParsers.h
    analysis/AnalysisPass.cpp analysis/AnalysisPass.h
    dsp/LoudnessMeter.cpp dsp/LoudnessMeter.h
    playback/RingBuffer.h
    playback/PlaybackEngine.cpp playback/PlaybackEngine.h
    metadata/CoverArt.cpp metadata/CoverArt.h
)
target_include_directories(ffgui_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# The per-sample DSP is ~20x slower unoptimized; keep it fast in Debug too.
set_source_files_properties(
    media/Fft.cpp media/PeakPyramid.cpp media/Spectrogram.cpp media/MediaData.cpp dsp/LoudnessMeter.cpp
    PROPERTIES COMPILE_OPTIONS "$<$<CONFIG:Debug>:-O2>")
target_link_libraries(ffgui_core PUBLIC Qt6::Core Qt6::Gui Qt6::Multimedia)

qt_add_resources(ffgui_core catalog_data
    PREFIX /ffgui/catalog
    BASE catalog/data
    FILES
        catalog/data/steps.json
        catalog/data/codecs.json
        catalog/data/presets.json
)
