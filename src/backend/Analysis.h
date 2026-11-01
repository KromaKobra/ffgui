#pragma once

#include <QQmlEngine>
#include <QUrl>
#include <QVariantMap>
#include <QVector>

#include <memory>

// The background analysis of the current input (levels, loudness, silence,
// clipping, phase, spectrum) and its results. Media View layers appear as
// each part finishes.
//
// results: { loudness: { integrated, lra, truePeak, maxShortTerm, curve },
//            levels: { peak, rms, crest, dc: [..], clipSamples, clips: [t..] },
//            silence: [{ start, end }], phase: { mean, curve }, spectrum: { centroid, rolloff, flatness },
//            decode: { errors, warnings, messages }, clipping: { samples, times } }
//
// Per-100 ms series (momentary, shortTerm, phase) are kept in C++ for the
// curve items; seriesAt() reads one value.
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
    // The value of a series at `t` (nearest block), NaN when there is none.
    Q_INVOKABLE double seriesAt(const QString &name, double t) const;

    struct Series
    {
        QVector<double> times;
        QVector<float> values;
    };
    std::shared_ptr<const Series> series(const QString &name) const { return m_series.value(name); }
    void setSeries(const QString &name, std::shared_ptr<const Series> series);
    void clearSeries();

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
    void seriesChanged();

private:
    QString m_state = QStringLiteral("idle");
    double m_progress = 0;
    QString m_stage;
    QString m_subject;
    QVariantMap m_results;
    QString m_error;
    QVariantMap m_rangeStats;
    QHash<QString, std::shared_ptr<const Series>> m_series;
};
