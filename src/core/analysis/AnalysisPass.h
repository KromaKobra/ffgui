#pragma once

#include "AnalysisParsers.h"

#include <QObject>
#include <QPointer>
#include <QVector>

#include <memory>

class QTemporaryDir;

// One ffmpeg run that measures an input: the decoded audio is split into
// ebur128 (loudness, true peak, the short-term curve), silencedetect,
// aphasemeter (stereo phase), aspectralstats and astats branches. Per-frame
// values go to ametadata files, summaries come from the log, and the decoder's
// own warnings and errors become the input's health.
namespace ffgui {

class LogStore;
class ProcessJob;

struct AnalysisResult
{
    bool ok = false;
    QString error;
    bool range = false;
    double start = 0;
    double length = 0;

    analysis::Ebur128Summary loudness;
    double maxMomentary = NAN, maxShortTerm = NAN;
    QVector<double> times;          // per 100 ms block
    QVector<float> momentary;       // LUFS
    QVector<float> shortTerm;       // LUFS
    QVector<float> phase;           // −1..+1, stereo only
    double phaseMean = NAN;
    analysis::AstatsSummary levels;
    QList<analysis::Interval> silence;
    double centroid = NAN, rolloff = NAN, flatness = NAN;
    int decodeErrors = 0;
    int decodeWarnings = 0;
    QStringList decodeMessages;
};

class AnalysisPass : public QObject
{
    Q_OBJECT

public:
    struct Options
    {
        QString input;
        int audioIndex = 0;
        int channels = 2;
        int sampleRate = 48000;
        double duration = 0;        // of the input, for progress
        double start = -1;          // >= 0: measure [start, start + length) only
        double length = 0;
        double silenceDb = -50;
        double silenceMin = 2;
    };

    AnalysisPass(const Options &options, LogStore *log = nullptr, QObject *parent = nullptr);
    ~AnalysisPass() override;

    void start();
    void cancel();
    const Options &options() const { return m_options; }

    static QStringList args(const Options &options);

signals:
    void progress(double fraction);
    void finished(const ffgui::AnalysisResult &result);

private:
    void collect(bool ok);

    Options m_options;
    QPointer<LogStore> m_log;
    std::unique_ptr<QTemporaryDir> m_dir;
    ProcessJob *m_job = nullptr;
};

} // namespace ffgui
