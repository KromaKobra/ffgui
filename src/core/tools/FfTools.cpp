#include "FfTools.h"

#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

#include <mutex>
#include <optional>

namespace ffgui {

namespace {

QString find(const char *name, const char *env)
{
    const QString override = QSettings().value(QStringLiteral("tools/") + QString::fromLatin1(name)).toString();
    if (!override.isEmpty() && QFileInfo(override).isExecutable())
        return override;
    const QString fromEnv = qEnvironmentVariable(env);
    if (!fromEnv.isEmpty() && QFileInfo(fromEnv).isExecutable())
        return fromEnv;
    return QStandardPaths::findExecutable(QString::fromLatin1(name));
}

std::mutex g_mutex;
std::optional<ToolPaths> g_paths;

} // namespace

ToolPaths FfTools::locate()
{
    return { find("ffmpeg", "FFGUI_FFMPEG"), find("ffprobe", "FFGUI_FFPROBE"), find("ffplay", "FFGUI_FFPLAY") };
}

const ToolPaths &FfTools::paths()
{
    std::lock_guard lock(g_mutex);
    if (!g_paths)
        g_paths = locate();
    return *g_paths;
}

void FfTools::setPaths(const ToolPaths &paths)
{
    std::lock_guard lock(g_mutex);
    g_paths = paths;
}

} // namespace ffgui
