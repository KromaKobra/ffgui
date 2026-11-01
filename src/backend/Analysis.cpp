#include "Analysis.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

Analysis::Analysis(QObject *parent)
    : QObject(parent)
{
}

void Analysis::setState(const QString &state, const QString &subject, const QString &error)
{
    if (m_state == state && m_subject == subject && m_error == error)
        return;
    m_state = state;
    m_subject = subject;
    m_error = error;
    emit stateChanged();
}

void Analysis::setProgress(double progress, const QString &stage)
{
    if (m_progress == progress && m_stage == stage)
        return;
    m_progress = progress;
    m_stage = stage;
    emit progressChanged();
}

void Analysis::setPart(const QString &part, const QVariant &value)
{
    m_results.insert(part, value);
    emit resultsChanged();
}

void Analysis::reset()
{
    m_results.clear();
    emit resultsChanged();
    setProgress(0, {});
    setState(QStringLiteral("idle"));
}

void Analysis::restart()
{
    emit restartRequested();
}

void Analysis::measureRange(double start, double end)
{
    emit rangeRequested(start, end);
}

void Analysis::setRangeStats(const QVariantMap &stats)
{
    if (m_rangeStats == stats)
        return;
    m_rangeStats = stats;
    emit rangeStatsChanged();
}

bool Analysis::exportJson(const QUrl &file) const
{
    QFile out(file.isLocalFile() ? file.toLocalFile() : file.toString());
    if (m_results.isEmpty() || !out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    QJsonObject root = QJsonObject::fromVariantMap(m_results);
    root.insert(QStringLiteral("file"), m_subject);
    return out.write(QJsonDocument(root).toJson()) > 0;
}

bool Analysis::exportCsv(const QUrl &file) const
{
    QFile out(file.isLocalFile() ? file.toLocalFile() : file.toString());
    if (m_results.isEmpty() || !out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    // One "section,key,value" line per scalar result; lists are left to JSON.
    QByteArray csv = "section,key,value\n";
    for (auto s = m_results.begin(); s != m_results.end(); ++s) {
        const QVariantMap section = s.value().toMap();
        for (auto k = section.begin(); k != section.end(); ++k) {
            if (k.value().canConvert<double>() && k.value().typeId() != QMetaType::QVariantList)
                csv += s.key().toUtf8() + ',' + k.key().toUtf8() + ',' + k.value().toString().toUtf8() + '\n';
        }
    }
    return out.write(csv) > 0;
}
