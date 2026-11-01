#pragma once

#include <QQmlEngine>

// Playback state for the Transport and the playhead. The playback engine
// drives position while playing; the UI calls the methods.
class Transport : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by App")

    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool canPlay READ canPlay NOTIFY durationChanged)
    Q_PROPERTY(bool loop READ loop WRITE setLoop NOTIFY loopChanged)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    // A/B switch: "original" or "processed".
    Q_PROPERTY(QString ab READ ab WRITE setAb NOTIFY abChanged)
    Q_PROPERTY(bool abEnabled READ abEnabled NOTIFY abAvailabilityChanged)
    Q_PROPERTY(QString abReason READ abReason NOTIFY abAvailabilityChanged)
    // Bit n set = channel n soloed. 0 = everything plays.
    Q_PROPERTY(int soloMask READ soloMask NOTIFY soloMaskChanged)
    Q_PROPERTY(QList<double> markers READ markers NOTIFY markersChanged)

public:
    explicit Transport(QObject *parent = nullptr);

    bool playing() const { return m_playing; }
    double position() const { return m_position; }
    double duration() const { return m_duration; }
    bool canPlay() const { return m_duration > 0; }
    bool loop() const { return m_loop; }
    void setLoop(bool loop);
    double speed() const { return m_speed; }
    void setSpeed(double speed);
    double volume() const { return m_volume; }
    void setVolume(double volume);
    QString ab() const { return m_ab; }
    void setAb(const QString &ab);
    bool abEnabled() const { return m_abEnabled; }
    QString abReason() const { return m_abReason; }
    int soloMask() const { return m_soloMask; }
    QList<double> markers() const { return m_markers; }

    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void toggle();
    // Stops and returns to `to` (the selection start, or 0).
    Q_INVOKABLE void stop(double to = 0);
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void nudge(double seconds) { seek(m_position + seconds); }
    Q_INVOKABLE void previousMarker();
    Q_INVOKABLE void nextMarker();
    Q_INVOKABLE void toggleSolo(int channel);
    Q_INVOKABLE bool isSoloed(int channel) const { return m_soloMask & (1 << channel); }

    // For the engine and the project.
    void setDuration(double duration);
    void setPosition(double position);
    void setPlaying(bool playing);
    void setMarkers(const QList<double> &markers);
    void setAbAvailability(bool enabled, const QString &reason);

signals:
    void playingChanged();
    void positionChanged();
    void durationChanged();
    void loopChanged();
    void speedChanged();
    void volumeChanged();
    void abChanged();
    void abAvailabilityChanged();
    void soloMaskChanged();
    void markersChanged();
    // The user moved the playhead (not playback advancing it).
    void seeked(double position);

private:
    bool m_playing = false;
    double m_position = 0;
    double m_duration = 0;
    bool m_loop = false;
    double m_speed = 1;
    double m_volume = 0.8;
    QString m_ab = QStringLiteral("processed");
    bool m_abEnabled = false;
    QString m_abReason = QStringLiteral("Add a step to the chain to compare");
    int m_soloMask = 0;
    QList<double> m_markers;
};
