#include "MediaState.h"

#include <cmath>

using namespace ffgui;

namespace {
constexpr int kCacheSize = 4;

double toDb(double v)
{
    return v > 0 ? 20.0 * std::log10(v) : -INFINITY;
}
}

MediaState::MediaState(LogStore *log, QObject *parent)
    : QObject(parent)
    , m_log(log)
{
}

MediaState::~MediaState() = default;

bool MediaState::loading() const
{
    const auto it = m_cache.find(m_key);
    return it != m_cache.end() && !it->second.done;
}

void MediaState::bump()
{
    ++m_revision;
    emit updated();
}

void MediaState::show(const QString &key, const PcmReader::Source &source)
{
    if (key == m_key && m_data)
        return;
    // An unfinished decode of something no longer shown is dropped.
    if (!m_key.isEmpty() && m_key != key) {
        const auto old = m_cache.find(m_key);
        if (old != m_cache.end() && !old->second.done) {
            m_cache.erase(old);
            m_order.removeAll(m_key);
        }
    }
    m_key = key;
    m_source = source;
    m_live = false;
    m_error.clear();
    m_window.reset();

    auto it = m_cache.find(key);
    if (it == m_cache.end()) {
        Entry entry;
        entry.reader = std::make_unique<PcmReader>(source, m_log);
        entry.data = entry.reader->data();
        PcmReader *reader = entry.reader.get();
        connect(reader, &PcmReader::progress, this, [this, key](double fraction) {
            if (key != m_key)
                return;
            m_progress = fraction;
            emit progressChanged();
            // The waveform and spectrogram redraw at most four times a second
            // while the rest decodes.
            if (!m_redraw.isValid() || m_redraw.elapsed() >= 250) {
                m_redraw.start();
                bump();
            }
        });
        connect(reader, &PcmReader::finished, this, [this, key](bool ok, const QString &error) {
            auto e = m_cache.find(key);
            if (e == m_cache.end())
                return;
            e->second.done = true;
            e->second.ok = ok;
            e->second.error = error;
            if (key == m_key) {
                m_error = ok ? QString() : error;
                emit changed();
                bump();
            }
            emit decoded(key, ok);
        });
        it = m_cache.emplace(key, std::move(entry)).first;
        m_order << key;
        while (m_order.size() > kCacheSize) {
            const QString oldest = m_order.takeFirst();
            m_cache.erase(oldest);
        }
        it->second.reader->start();
        m_progress = 0;
    } else {
        m_order.removeAll(key);
        m_order << key;
        m_progress = it->second.done ? 1.0 : m_progress;
        m_error = it->second.done && !it->second.ok ? it->second.error : QString();
    }
    m_data = it->second.data;
    m_window = std::make_unique<WindowDecoder>(source, m_data);
    connect(m_window.get(), &WindowDecoder::ready, this, &MediaState::bump);
    emit changed();
    emit progressChanged();
    bump();
    if (it->second.done)
        emit decoded(key, it->second.ok);
}

void MediaState::clear(bool live)
{
    if (!m_key.isEmpty()) {
        const auto old = m_cache.find(m_key);
        if (old != m_cache.end() && !old->second.done) {
            m_cache.erase(old);
            m_order.removeAll(m_key);
        }
    }
    m_key.clear();
    m_data.reset();
    m_window.reset();
    m_source = {};
    m_live = live;
    m_error.clear();
    m_progress = 0;
    emit changed();
    emit progressChanged();
    bump();
}

void MediaState::forget(const QString &keyPrefix)
{
    for (auto it = m_cache.begin(); it != m_cache.end();) {
        if (it->first.startsWith(keyPrefix) && it->first != m_key) {
            m_order.removeAll(it->first);
            it = m_cache.erase(it);
        } else {
            ++it;
        }
    }
}

double MediaState::levelAt(int channel, double t, double halfWindow) const
{
    if (!m_data)
        return NAN;
    const PeakColumn c = m_data->peakRange(channel, std::max(0.0, t - halfWindow), t + halfWindow);
    return c.valid ? toDb(std::max(std::abs(c.min), std::abs(c.max))) : NAN;
}

double MediaState::rmsAt(int channel, double t, double halfWindow) const
{
    if (!m_data)
        return NAN;
    const PeakColumn c = m_data->peakRange(channel, std::max(0.0, t - halfWindow), t + halfWindow);
    return c.valid ? toDb(c.rms) : NAN;
}

double MediaState::spectrumAt(int channel, double t, double hz) const
{
    return m_data ? m_data->spectrumDb(channel, t, hz) : NAN;
}

void MediaState::requestWindow(double t0, double t1)
{
    if (m_window && m_data && !m_data->windowCovers(t0, t1))
        m_window->request(t0, t1);
}
