#include "Transport.h"

#include <algorithm>

Transport::Transport(QObject *parent)
    : QObject(parent)
{
}

void Transport::setLoop(bool loop)
{
    if (m_loop == loop)
        return;
    m_loop = loop;
    emit loopChanged();
}

void Transport::setSpeed(double speed)
{
    speed = std::clamp(speed, 0.25, 4.0);
    if (m_speed == speed)
        return;
    m_speed = speed;
    emit speedChanged();
}

void Transport::setVolume(double volume)
{
    volume = std::clamp(volume, 0.0, 1.0);
    if (m_volume == volume)
        return;
    m_volume = volume;
    emit volumeChanged();
}

void Transport::setAb(const QString &ab)
{
    const QString v = ab == u"original" ? ab : QStringLiteral("processed");
    if (m_ab == v)
        return;
    m_ab = v;
    emit abChanged();
}

void Transport::play()
{
    if (canPlay())
        setPlaying(true);
}

void Transport::pause()
{
    setPlaying(false);
}

void Transport::toggle()
{
    if (m_playing)
        pause();
    else
        play();
}

void Transport::stop(double to)
{
    setPlaying(false);
    seek(to);
}

void Transport::seek(double seconds)
{
    setPosition(seconds);
    emit seeked(m_position);
}

void Transport::previousMarker()
{
    double target = 0;
    for (double t : std::as_const(m_markers))
        if (t < m_position - 1)
            target = t;
    seek(target);
}

void Transport::nextMarker()
{
    // Markers within 50 ms count as "here", so repeated presses move on.
    for (double t : std::as_const(m_markers)) {
        if (t > m_position + 0.05) {
            seek(t);
            return;
        }
    }
    seek(m_duration);
}

void Transport::toggleSolo(int channel)
{
    if (channel < 0 || channel > 30)
        return;
    m_soloMask ^= 1 << channel;
    emit soloMaskChanged();
}

void Transport::setDuration(double duration)
{
    duration = std::max(0.0, duration);
    if (m_duration == duration)
        return;
    m_duration = duration;
    emit durationChanged();
    if (m_position > duration)
        setPosition(duration);
    if (!canPlay())
        setPlaying(false);
}

void Transport::setPosition(double position)
{
    position = std::clamp(position, 0.0, m_duration);
    if (m_position == position)
        return;
    m_position = position;
    emit positionChanged();
}

void Transport::setPlaying(bool playing)
{
    if (m_playing == playing)
        return;
    m_playing = playing;
    emit playingChanged();
}

void Transport::setMarkers(const QList<double> &markers)
{
    QList<double> sorted = markers;
    std::sort(sorted.begin(), sorted.end());
    if (m_markers == sorted)
        return;
    m_markers = sorted;
    emit markersChanged();
}

void Transport::setAbAvailability(bool enabled, const QString &reason)
{
    if (m_abEnabled == enabled && m_abReason == reason)
        return;
    m_abEnabled = enabled;
    m_abReason = reason;
    emit abAvailabilityChanged();
}
