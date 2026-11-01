#include "Meters.h"

Meters::Meters(QObject *parent)
    : QObject(parent)
{
    setChannelNames({ QStringLiteral("L"), QStringLiteral("R") });
}

void Meters::setChannelNames(const QStringList &names)
{
    if (m_channelNames == names)
        return;
    m_channelNames = names;
    const qsizetype n = names.size();
    m_peak = m_rms = m_hold = QList<double>(n, floor());
    m_clip = QList<bool>(n, false);
    emit channelsChanged();
    emit levelsChanged();
}

void Meters::setTarget(double target)
{
    if ((std::isnan(m_target) && std::isnan(target)) || m_target == target)
        return;
    m_target = target;
    emit targetChanged();
}

void Meters::resetClip(int channel)
{
    if (channel < 0 || channel >= m_clip.size() || !m_clip[channel])
        return;
    m_clip[channel] = false;
    emit levelsChanged();
}

void Meters::resetAll()
{
    const qsizetype n = m_channelNames.size();
    m_peak = m_rms = m_hold = QList<double>(n, floor());
    m_clip = QList<bool>(n, false);
    m_momentary = m_shortTerm = m_integrated = m_truePeak = floor();
    m_correlation = 0;
    m_live = false;
    emit levelsChanged();
}

void Meters::apply(const Reading &r)
{
    const qsizetype n = m_channelNames.size();
    for (qsizetype c = 0; c < n; ++c) {
        const double p = clamp(c < r.peak.size() ? r.peak[c] : -INFINITY);
        m_peak[c] = p;
        m_rms[c] = clamp(c < r.rms.size() ? r.rms[c] : -INFINITY);
        // Hold the highest peak, then fall at 0.6 dB per reading.
        m_hold[c] = p >= m_hold[c] ? p : std::max(p, m_hold[c] - 0.6);
        if (c < r.clipped.size() && r.clipped[c])
            m_clip[c] = true;
    }
    m_momentary = clamp(r.momentary);
    m_shortTerm = clamp(r.shortTerm);
    m_integrated = clamp(r.integrated);
    m_truePeak = clamp(r.truePeak);
    m_correlation = std::isfinite(r.correlation) ? std::clamp(r.correlation, -1.0, 1.0) : 0;
    m_live = r.live;
    emit levelsChanged();
}
