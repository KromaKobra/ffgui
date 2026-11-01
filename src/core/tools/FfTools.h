#pragma once

#include <QString>

// Where ffmpeg, ffprobe and ffplay are. Looked up once: a Settings override
// ("tools/ffmpeg", "tools/ffprobe", "tools/ffplay"), then FFGUI_FFMPEG /
// FFGUI_FFPROBE / FFGUI_FFPLAY, then the PATH.
namespace ffgui {

struct ToolPaths
{
    QString ffmpeg;
    QString ffprobe;
    QString ffplay;
    bool complete() const { return !ffmpeg.isEmpty() && !ffprobe.isEmpty(); }
};

class FfTools
{
public:
    static ToolPaths locate();
    // The paths every job uses; locate()d on first use.
    static const ToolPaths &paths();
    static void setPaths(const ToolPaths &paths);
};

} // namespace ffgui
