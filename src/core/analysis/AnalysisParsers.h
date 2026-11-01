#pragma once

#include <tools/ProcessJob.h>

#include <QHash>
#include <QList>
#include <QString>
#include <cmath>

// Parsers for what ffmpeg's analysis filters print, in the formats this build
// writes (captured fixtures in tests/core/fixtures).
namespace ffgui::analysis {

struct ChannelStats
{
    double dcOffset = NAN;      // linear
    double minLevel = NAN, maxLevel = NAN;
    double peakDb = NAN, rmsDb = NAN, rmsPeakDb = NAN;
    double crest = NAN;         // linear (peak / RMS)
    double noiseFloorDb = NAN;
    double dynamicRange = NAN;
    QString bitDepth;           // "24/24/24/24"
    double zeroCrossingRate = NAN;
};

struct AstatsSummary
{
    QList<ChannelStats> channels;
    ChannelStats overall;
    qint64 samples = 0;
    bool valid() const { return !channels.isEmpty(); }
};

struct Ebur128Summary
{
    double integrated = NAN, threshold = NAN;
    double lra = NAN, lraLow = NAN, lraHigh = NAN;
    double truePeak = NAN;      // dBFS
    bool valid() const { return !std::isnan(integrated); }
};

struct Interval
{
    double start = 0;
    double end = 0;
};

// One block of `ametadata=print:file=...`: "frame:N pts:P pts_time:T" then key=value lines.
struct MetadataFrame
{
    double time = 0;
    QHash<QString, double> values;
};

// Lines of the filter whose context starts with Parsed_<filter>_.
QList<LogLine> linesOf(const QList<LogLine> &lines, const QString &filter);

AstatsSummary parseAstats(const QList<LogLine> &filterLines);
Ebur128Summary parseEbur128(const QList<LogLine> &filterLines);
// silence_start / silence_end pairs; an unterminated silence ends at `duration`.
QList<Interval> parseSilence(const QList<LogLine> &filterLines, double duration);
QList<MetadataFrame> parseMetadata(const QByteArray &text);
QList<MetadataFrame> parseMetadataFile(const QString &path);

} // namespace ffgui::analysis
