#pragma once

#include "BuildInfo.h"

#include <tools/FfTools.h>

#include <QObject>

// Finds out what the installed ffmpeg can do: runs -version, -filters,
// -encoders and -muxers once and caches the result on disk, keyed by the
// binary's path, size and modification time.
namespace ffgui {

class LogStore;

class CapabilityProbe : public QObject
{
    Q_OBJECT

public:
    explicit CapabilityProbe(LogStore *log = nullptr, QObject *parent = nullptr);

    // Asynchronous; emits finished(). `useCache: false` always asks ffmpeg.
    void run(bool useCache = true);
    bool running() const { return m_running; }

    // Blocking: for tests and tools.
    static bool detect(const ToolPaths &paths, BuildInfo *info, QString *error);

signals:
    void finished(bool ok, const ffgui::ToolPaths &paths, const ffgui::BuildInfo &info, const QString &error);

private:
    LogStore *m_log;
    bool m_running = false;
};

} // namespace ffgui
