#include "AnalysisPass.h"

#include <log/LogStore.h>
#include <tools/FfTools.h>
#include <tools/ProcessJob.h>

#include <QTemporaryDir>

#include <algorithm>

namespace ffgui {

using namespace analysis;

AnalysisPass::AnalysisPass(const Options &options, LogStore *log, QObject *parent)
    : QObject(parent)
    , m_options(options)
    , m_log(log)
{
}

AnalysisPass::~AnalysisPass() = default;

QStringList AnalysisPass::args(const Options &o)
{
    const bool range = o.start >= 0;
    const bool stereo = o.channels == 2;
    // 100 ms frames: the loudness and phase curves get one value per block.
    QString graph = QStringLiteral("[0:a:%1]asetnsamples=n=%2:p=0,").arg(o.audioIndex).arg(std::max(1, o.sampleRate / 10));
    QStringList branches;
    branches << QStringLiteral("ebur128=peak=true:metadata=1,ametadata=mode=print:file=loudness.txt,anullsink");
    if (stereo)
        branches << QStringLiteral("aphasemeter=video=0,ametadata=mode=print:file=phase.txt,anullsink");
    if (!range) {
        branches << QStringLiteral("silencedetect=n=%1dB:d=%2,anullsink").arg(o.silenceDb).arg(o.silenceMin);
        branches << QStringLiteral("aspectralstats=win_size=8192:overlap=0:measure=centroid+rolloff+flatness,"
                                   "ametadata=mode=print:file=spectrum.txt,anullsink");
    }
    graph += QStringLiteral("asplit=%1").arg(branches.size() + 1);
    for (int i = 0; i <= branches.size(); ++i)
        graph += QStringLiteral("[b%1]").arg(i);
    for (int i = 0; i < branches.size(); ++i)
        graph += QStringLiteral(";[b%1]").arg(i) + branches[i];
    graph += QStringLiteral(";[b%1]astats=metadata=0[out]").arg(branches.size());

    QStringList a { QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"), QStringLiteral("-nostats"),
                    QStringLiteral("-loglevel"), QStringLiteral("level+info"),
                    QStringLiteral("-progress"), QStringLiteral("pipe:1") };
    if (range)
        a << QStringLiteral("-ss") << QString::number(o.start, 'f', 6) << QStringLiteral("-t") << QString::number(o.length, 'f', 6);
    a << QStringLiteral("-i") << o.input << QStringLiteral("-filter_complex") << graph
      << QStringLiteral("-map") << QStringLiteral("[out]") << QStringLiteral("-f") << QStringLiteral("null") << QStringLiteral("-");
    return a;
}

void AnalysisPass::start()
{
    m_dir = std::make_unique<QTemporaryDir>();
    m_job = new ProcessJob(FfTools::paths().ffmpeg, args(m_options), this);
    m_job->setWorkingDirectory(m_dir->path());
    m_job->setLog(m_log, m_options.start >= 0 ? QStringLiteral("range") : QStringLiteral("analysis"));
    m_job->setProgress(true);
    m_job->setBackground(true);
    const double total = m_options.start >= 0 ? m_options.length : m_options.duration;
    connect(m_job, &ProcessJob::progress, this, [this, total](const ProgressParser::Snapshot &s) {
        if (total > 0)
            emit progress(std::clamp(s.outTime / total, 0.0, 1.0));
    });
    connect(m_job, &ProcessJob::finished, this, &AnalysisPass::collect);
    m_job->start();
}

void AnalysisPass::cancel()
{
    if (m_job)
        m_job->kill();
}

void AnalysisPass::collect(bool ok)
{
    AnalysisResult r;
    r.range = m_options.start >= 0;
    r.start = std::max(0.0, m_options.start);
    r.length = m_options.length;
    const QList<LogLine> &lines = m_job->stderrLines();

    // The decoder's and demuxer's complaints (not the filters' output).
    for (const LogLine &l : lines) {
        if (l.context.startsWith(QStringLiteral("Parsed_")) || LogLine::rank(l.level) > 1 || LogLine::benign(l.text))
            continue;
        if (l.level == u"error")
            ++r.decodeErrors;
        else
            ++r.decodeWarnings;
        if (r.decodeMessages.size() < 20)
            r.decodeMessages << l.text.trimmed();
    }

    if (!ok || m_job->stopped()) {
        r.ok = false;
        r.error = m_job->stopped() ? QStringLiteral("cancelled")
                  : r.decodeMessages.isEmpty() ? QStringLiteral("ffmpeg failed") : r.decodeMessages.last();
        m_job->deleteLater();
        m_job = nullptr;
        emit finished(r);
        return;
    }

    r.levels = parseAstats(linesOf(lines, QStringLiteral("astats")));
    r.loudness = parseEbur128(linesOf(lines, QStringLiteral("ebur128")));
    r.silence = parseSilence(linesOf(lines, QStringLiteral("silencedetect")), m_options.duration);

    const QString dir = m_dir->path() + u'/';
    for (const MetadataFrame &f : parseMetadataFile(dir + QStringLiteral("loudness.txt"))) {
        r.times << f.time + r.start;
        const float m = float(f.values.value(QStringLiteral("lavfi.r128.M"), -120.7));
        const float s = float(f.values.value(QStringLiteral("lavfi.r128.S"), -120.7));
        r.momentary << m;
        r.shortTerm << s;
        if (m > -120 && (std::isnan(r.maxMomentary) || m > r.maxMomentary))
            r.maxMomentary = m;
        if (s > -120 && (std::isnan(r.maxShortTerm) || s > r.maxShortTerm))
            r.maxShortTerm = s;
    }

    double phaseSum = 0;
    int phaseCount = 0;
    for (const MetadataFrame &f : parseMetadataFile(dir + QStringLiteral("phase.txt"))) {
        const double p = f.values.value(QStringLiteral("lavfi.aphasemeter.phase"), NAN);
        r.phase << float(std::isnan(p) ? 0 : p);
        if (!std::isnan(p)) {
            phaseSum += p;
            ++phaseCount;
        }
    }
    if (phaseCount)
        r.phaseMean = phaseSum / phaseCount;

    double centroid = 0, rolloff = 0, flatness = 0;
    int spectral = 0;
    for (const MetadataFrame &f : parseMetadataFile(dir + QStringLiteral("spectrum.txt"))) {
        for (int c = 1; c <= m_options.channels; ++c) {
            const double ce = f.values.value(QStringLiteral("lavfi.aspectralstats.%1.centroid").arg(c), NAN);
            const double ro = f.values.value(QStringLiteral("lavfi.aspectralstats.%1.rolloff").arg(c), NAN);
            const double fl = f.values.value(QStringLiteral("lavfi.aspectralstats.%1.flatness").arg(c), NAN);
            // Silent windows report nothing useful.
            if (std::isnan(ce) || std::isnan(ro) || std::isnan(fl) || ce <= 0)
                continue;
            centroid += ce;
            rolloff += ro;
            flatness += fl;
            ++spectral;
        }
    }
    if (spectral) {
        r.centroid = centroid / spectral;
        r.rolloff = rolloff / spectral;
        r.flatness = flatness / spectral;
    }

    r.ok = r.levels.valid() || r.loudness.valid();
    if (!r.ok)
        r.error = QStringLiteral("ffmpeg printed no measurements");
    m_job->deleteLater();
    m_job = nullptr;
    emit finished(r);
}

} // namespace ffgui
