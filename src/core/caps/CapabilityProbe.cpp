#include "CapabilityProbe.h"

#include <log/LogStore.h>
#include <tools/ProcessJob.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QPointer>
#include <QStandardPaths>
#include <QThreadPool>

namespace ffgui {

namespace {

QString cachePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/capabilities.json");
}

QString cacheKey(const QString &binary)
{
    const QFileInfo info(binary);
    return QStringLiteral("%1|%2|%3").arg(info.canonicalFilePath()).arg(info.size()).arg(info.lastModified().toMSecsSinceEpoch());
}

bool runTool(const QString &program, const QStringList &args, QString *output)
{
    QProcess p;
    p.start(program, args);
    if (!p.waitForFinished(15000) || p.exitStatus() != QProcess::NormalExit)
        return false;
    *output = QString::fromUtf8(p.readAllStandardOutput());
    return true;
}

} // namespace

CapabilityProbe::CapabilityProbe(LogStore *log, QObject *parent)
    : QObject(parent)
    , m_log(log)
{
}

bool CapabilityProbe::detect(const ToolPaths &paths, BuildInfo *info, QString *error)
{
    if (paths.ffmpeg.isEmpty()) {
        *error = QStringLiteral("ffmpeg was not found on the PATH");
        return false;
    }
    if (paths.ffprobe.isEmpty()) {
        *error = QStringLiteral("ffprobe was not found on the PATH");
        return false;
    }
    QString version, filters, encoders, muxers;
    const QStringList quiet { QStringLiteral("-hide_banner") };
    if (!runTool(paths.ffmpeg, { QStringLiteral("-version") }, &version)
        || !runTool(paths.ffmpeg, quiet + QStringList { QStringLiteral("-filters") }, &filters)
        || !runTool(paths.ffmpeg, quiet + QStringList { QStringLiteral("-encoders") }, &encoders)
        || !runTool(paths.ffmpeg, quiet + QStringList { QStringLiteral("-muxers") }, &muxers)) {
        *error = QStringLiteral("%1 didn't answer -version / -filters").arg(paths.ffmpeg);
        return false;
    }
    caps::parseVersion(version, info);
    info->filters = caps::parseFilters(filters);
    info->audioEncoders = caps::parseEncoders(encoders);
    info->muxers = caps::parseMuxers(muxers);
    info->libraries = caps::evaluateLibraries(*info);
    if (info->version.isEmpty() || info->filters.isEmpty()) {
        *error = QStringLiteral("Couldn't read %1's version and filters").arg(paths.ffmpeg);
        return false;
    }
    return true;
}

void CapabilityProbe::run(bool useCache)
{
    if (m_running)
        return;
    m_running = true;
    const ToolPaths paths = FfTools::locate();
    FfTools::setPaths(paths);

    if (useCache && !paths.ffmpeg.isEmpty()) {
        QFile f(cachePath());
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
            if (root.value(u"key").toString() == cacheKey(paths.ffmpeg)) {
                const BuildInfo info = BuildInfo::fromJson(root.value(u"info").toObject());
                if (!info.version.isEmpty()) {
                    m_running = false;
                    QMetaObject::invokeMethod(this, [this, paths, info] { emit finished(true, paths, info, {}); },
                                              Qt::QueuedConnection);
                    return;
                }
            }
        }
    }

    QPointer<CapabilityProbe> self(this);
    QThreadPool::globalInstance()->start([self, paths] {
        BuildInfo info;
        QString error;
        const bool ok = detect(paths, &info, &error);
        if (ok) {
            QDir().mkpath(QFileInfo(cachePath()).absolutePath());
            QFile f(cachePath());
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
                f.write(QJsonDocument(QJsonObject { { "key", cacheKey(paths.ffmpeg) }, { "info", info.toJson() } }).toJson(QJsonDocument::Compact));
        }
        QMetaObject::invokeMethod(qApp, [self, ok, paths, info, error] {
            if (!self)
                return;
            self->m_running = false;
            if (self->m_log)
                self->m_log->append({}, ok ? QStringLiteral("info") : QStringLiteral("error"),
                                    ok ? QStringLiteral("FFmpeg %1 at %2: %3 filters, %4 audio encoders, %5 muxers")
                                             .arg(info.version, paths.ffmpeg).arg(info.filters.size())
                                             .arg(info.audioEncoders.size()).arg(info.muxers.size())
                                       : error,
                                    QStringLiteral("caps"));
            emit self->finished(ok, paths, info, error);
        });
    });
}

} // namespace ffgui
