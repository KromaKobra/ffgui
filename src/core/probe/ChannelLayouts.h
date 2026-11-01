#pragma once

#include <QStringList>

// Channel names for ffmpeg's channel layouts ("5.1(side)" -> FL FR FC LFE SL SR).
namespace ffgui::layouts {

// Names for a layout name as ffprobe prints it; when the layout is empty or
// unknown, ffmpeg's default layout for `channels`.
QStringList channelNames(const QString &layout, int channels);
// ffmpeg's default layout name for a channel count ("stereo", "5.1", ...).
QString defaultLayout(int channels);

} // namespace ffgui::layouts
