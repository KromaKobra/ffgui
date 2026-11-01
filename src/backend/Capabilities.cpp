#include "Capabilities.h"

Capabilities::Capabilities(QObject *parent)
    : QObject(parent)
    , m_libraries(new LibrariesModel(this))
{
}

QString Capabilities::summary() const
{
    if (m_state == u"missing")
        return QStringLiteral("FFmpeg not found");
    if (m_state != u"ready")
        return QStringLiteral("Checking FFmpeg…");
    QString s = QStringLiteral("FFmpeg ") + m_version;
    if (m_missingCount > 0)
        s += QStringLiteral(" · %1 %2 unavailable").arg(m_missingCount)
                 .arg(m_missingCount == 1 ? QStringLiteral("feature") : QStringLiteral("features"));
    return s;
}

void Capabilities::refresh()
{
    emit refreshRequested();
}

void Capabilities::apply(const Snapshot &s)
{
    m_state = s.state;
    m_version = s.version;
    m_ffmpegPath = s.ffmpegPath;
    m_ffprobePath = s.ffprobePath;
    m_ffplayPath = s.ffplayPath;
    m_configuration = s.configuration;
    m_filters = QSet<QString>(s.filters.begin(), s.filters.end());
    m_encoders = QSet<QString>(s.encoders.begin(), s.encoders.end());
    m_muxers = QSet<QString>(s.muxers.begin(), s.muxers.end());
    m_filterCount = int(m_filters.size());
    m_encoderCount = int(m_encoders.size());
    m_muxerCount = int(m_muxers.size());
    m_missingCount = 0;
    for (const QVariantMap &lib : s.libraries)
        m_missingCount += lib.value(QStringLiteral("found")).toBool() ? 0 : 1;
    m_libraries->setRows(s.libraries);
    emit changed();
}
