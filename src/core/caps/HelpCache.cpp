#include "HelpCache.h"
#include <tools/FfTools.h>
#include <tools/ProcessJob.h>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace ffgui
{
HelpCache::HelpCache(LogStore *log, QObject *parent) : QObject(parent), m_log(log) {}

HelpCache::~HelpCache() { reset(); }

void HelpCache::reset()
{
    ++m_generation;
    m_pending.clear();
    m_requested.clear();
    if (m_job) {
        disconnect(m_job, nullptr, this, nullptr);
        m_job->kill();
        m_job->deleteLater();
        m_job = nullptr;
    }
}

void HelpCache::request(const QString &kind, const QString &name)
{
    if (name.isEmpty() || !QStringList{"filter", "encoder", "muxer"}.contains(kind))
        return;
    const QString key = kind + u':' + name;
    if (m_requested.contains(key) || FfTools::paths().ffmpeg.isEmpty())
        return;
    m_requested.insert(key);
    m_pending.enqueue(key);
    next();
}

void HelpCache::next()
{
    if (m_job || m_pending.isEmpty())
        return;
    const QString key = m_pending.dequeue();
    const int generation = m_generation;
    const QFileInfo binary(FfTools::paths().ffmpeg);
    const QByteArray identity = (binary.canonicalFilePath() + u'|' + QString::number(binary.size()) + u'|' +
                                 QString::number(binary.lastModified().toMSecsSinceEpoch()) + u'|' + key)
                                    .toUtf8();
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/help");
    const QString path =
        dir + u'/' + QString::fromLatin1(QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex());
    QFile cached(path);
    if (cached.open(QIODevice::ReadOnly)) {
        const QString text = QString::fromUtf8(cached.readAll());
        QMetaObject::invokeMethod(
            this,
            [this, key, text, generation] {
                if (generation != m_generation)
                    return;
                emit ready(key, HelpParser::parse(text));
                next();
            },
            Qt::QueuedConnection);
        return;
    }
    QString query = key;
    query[query.indexOf(u':')] = u'=';
    m_job = new ProcessJob(binary.absoluteFilePath(), {"-hide_banner", "-h", query}, this);
    m_job->setLog(m_log, QStringLiteral("help"), QStringLiteral("error"));
    auto output = std::make_shared<QByteArray>();
    auto *job = m_job;
    connect(job, &ProcessJob::stdoutReady, this, [job, output] { *output += job->readStdout(); });
    connect(job, &ProcessJob::finished, this, [this, job, output, key, dir, path](bool ok) {
        *output += job->readStdout();
        const auto options = HelpParser::parse(QString::fromUtf8(*output));
        if (ok) {
            QDir().mkpath(dir);
            QSaveFile file(path);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(*output);
                file.commit();
            }
            emit ready(key, options);
        } else
            m_requested.remove(key);
        m_job = nullptr;
        job->deleteLater();
        next();
    });
    job->start();
}
} // namespace ffgui
