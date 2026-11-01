#pragma once

#include <QQmlEngine>

#include <cmath>

// What the Meters strip shows: live levels of the frames being played, or the
// values at the playhead when stopped. Levels are in dBFS / LUFS; -inf is
// clamped to `floor`.
class Meters : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by App")

    Q_PROPERTY(QStringList channelNames READ channelNames NOTIFY channelsChanged)
    Q_PROPERTY(QList<double> peak READ peak NOTIFY levelsChanged)
    Q_PROPERTY(QList<double> rms READ rms NOTIFY levelsChanged)
    Q_PROPERTY(QList<double> hold READ hold NOTIFY levelsChanged)
    Q_PROPERTY(QList<bool> clip READ clip NOTIFY levelsChanged)
    Q_PROPERTY(double momentary READ momentary NOTIFY levelsChanged)
    Q_PROPERTY(double shortTerm READ shortTerm NOTIFY levelsChanged)
    Q_PROPERTY(double integrated READ integrated NOTIFY levelsChanged)
    Q_PROPERTY(double truePeak READ truePeak NOTIFY levelsChanged)
    Q_PROPERTY(double correlation READ correlation NOTIFY levelsChanged)
    // Loudness target from a Normalize step in the chain; NaN when there is none.
    Q_PROPERTY(double target READ target NOTIFY targetChanged)
    Q_PROPERTY(bool live READ live NOTIFY levelsChanged)
    Q_PROPERTY(double floor READ floor CONSTANT)

public:
    explicit Meters(QObject *parent = nullptr);

    QStringList channelNames() const { return m_channelNames; }
    QList<double> peak() const { return m_peak; }
    QList<double> rms() const { return m_rms; }
    QList<double> hold() const { return m_hold; }
    QList<bool> clip() const { return m_clip; }
    double momentary() const { return m_momentary; }
    double shortTerm() const { return m_shortTerm; }
    double integrated() const { return m_integrated; }
    double truePeak() const { return m_truePeak; }
    double correlation() const { return m_correlation; }
    double target() const { return m_target; }
    bool live() const { return m_live; }
    double floor() const { return -60; }

    Q_INVOKABLE void resetClip(int channel);
    Q_INVOKABLE void resetAll();

    void setChannelNames(const QStringList &names);
    void setTarget(double target);

    struct Reading
    {
        QList<double> peak, rms;
        QList<bool> clipped;
        double momentary = -INFINITY, shortTerm = -INFINITY, integrated = -INFINITY;
        double truePeak = -INFINITY, correlation = 0;
        bool live = false;
    };
    // Applies a reading: peak hold decays, clip lights latch until reset.
    void apply(const Reading &reading);

signals:
    void channelsChanged();
    void levelsChanged();
    void targetChanged();

private:
    double clamp(double v) const { return std::isfinite(v) ? std::max(v, floor()) : floor(); }

    QStringList m_channelNames;
    QList<double> m_peak, m_rms, m_hold;
    QList<bool> m_clip;
    double m_momentary = -60, m_shortTerm = -60, m_integrated = -60, m_truePeak = -60;
    double m_correlation = 0;
    double m_target = NAN;
    bool m_live = false;
};
