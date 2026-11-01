#pragma once

#include "ProbeResult.h"

#include <QByteArray>

namespace ffgui {

// Parses `ffprobe -of json -show_format -show_streams -show_chapters`.
// Returns false (with `error`) for output that isn't ffprobe JSON.
bool parseProbe(const QByteArray &json, ProbeResult *result, QString *error = nullptr);

// ffprobe's arguments for a path or URL.
QStringList probeArgs(const QString &input);

} // namespace ffgui
