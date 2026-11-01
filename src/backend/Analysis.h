#pragma once

#include <QQmlEngine>
#include <QUrl>
#include <QVariantMap>

// The background analysis of the current input (levels, loudness, silence,
// clipping, phase, spectrum) and its results. Media View layers appear as
// each part finishes.
//
// results: { loudness: { integrated, lra, truePeak, maxShortTerm, curve },
//            levels: { peak, rms, crest, dc: [..], clipSamples, clips: [t..] },
//            silence: [{ start, end }], phase: { mean, curve }, spectrum: { centroid, rolloff, flatness },
//            decode: { errors, warnings, messages } }
class Analysis : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by App")

    // idle | running | done | failed
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString stage READ stage NOTIFY progressChanged)
    Q_PROPERTY(QString subject READ subject NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap results READ results NOTIFY resultsChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    // Stats of the selected time range: { start, end, peak, rms, lufs, dc: [..],
    // correlation, measuring }. Empty when nothing has been measured.
    Q_PROPERTY(QVariantMap rangeStats READ rangeStats NOTIFY rangeStatsChanged)

public:
    explicit Analysis(QObject *parent = nullptr);

    QString state() const { return m_state; }
    bool running() const { return m_state == u"running"; }
    double progress() const { return m_progress; }
    QString stage() const { return m_stage; }
    QString subject() const { return m_subject; }
    QVariantMap results() const { return m_results; }
    QString error() const { return m_error; }
    QVariantMap rangeStats() const { return m_rangeStats; }

    // Whether the part a layer needs ("levels", "loudness", "silence",
    // "clipping", "phase", "spectrum") has finished.
    Q_INVOKABLE bool ready(const QString &part) const { return m_results.contains(part); }
    Q_INVOKABLE bool exportJson(const QUrl &file) const;
    Q_INVOKABLE bool exportCsv(const QUrl &file) const;
    Q_INVOKABLE void restart();
    // Measures [start, end] of the current input; results land in rangeStats.
    Q_INVOKABLE void measureRange(double start, double end);

    void setState(const QString &state, const QString &subject = {}, const QString &error = {});
    void setProgress(double progress, const QString &stage);
    void setPart(const QString &part, const QVariant &value);
    void reset();
    void setRangeStats(const QVariantMap &stats);

signals:
    void stateChanged();
    void progressChanged();
    void resultsChanged();
    void restartRequested();
    void rangeStatsChanged();
    void rangeRequested(double start, double end);

private:
    QString m_state = QStringLiteral("idle");
    double m_progress = 0;
    QString m_stage;
    QString m_subject;
    QVariantMap m_results;
    QString m_error;
    QVariantMap m_rangeStats;
};
