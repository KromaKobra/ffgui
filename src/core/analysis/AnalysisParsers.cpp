#include "AnalysisParsers.h"

#include <QFile>
#include <QRegularExpression>

namespace ffgui::analysis {

namespace {

double number(const QString &text)
{
    const QString t = text.trimmed();
    if (t == u"-inf")
        return -INFINITY;
    if (t == u"inf")
        return INFINITY;
    bool ok = false;
    const double v = t.toDouble(&ok);
    return ok ? v : NAN;
}

} // namespace

QList<LogLine> linesOf(const QList<LogLine> &lines, const QString &filter)
{
    const QString prefix = QStringLiteral("Parsed_") + filter + u'_';
    QList<LogLine> out;
    for (const LogLine &l : lines)
        if (l.context.startsWith(prefix))
            out << l;
    return out;
}

AstatsSummary parseAstats(const QList<LogLine> &lines)
{
    AstatsSummary s;
    ChannelStats *current = nullptr;
    for (const LogLine &l : lines) {
        const QString text = l.text.trimmed();
        if (text.startsWith(QStringLiteral("Channel:"))) {
            s.channels.append(ChannelStats());
            current = &s.channels.last();
            continue;
        }
        if (text == u"Overall") {
            current = &s.overall;
            continue;
        }
        const qsizetype colon = text.indexOf(u':');
        if (!current || colon < 0)
            continue;
        const QString key = text.left(colon);
        const QString value = text.mid(colon + 1).trimmed();
        if (key == u"DC offset")
            current->dcOffset = number(value);
        else if (key == u"Min level")
            current->minLevel = number(value);
        else if (key == u"Max level")
            current->maxLevel = number(value);
        else if (key == u"Peak level dB")
            current->peakDb = number(value);
        else if (key == u"RMS level dB")
            current->rmsDb = number(value);
        else if (key == u"RMS peak dB")
            current->rmsPeakDb = number(value);
        else if (key == u"Crest factor")
            current->crest = number(value);
        else if (key == u"Noise floor dB")
            current->noiseFloorDb = number(value);
        else if (key == u"Dynamic range")
            current->dynamicRange = number(value);
        else if (key == u"Bit depth")
            current->bitDepth = value;
        else if (key == u"Zero crossings rate")
            current->zeroCrossingRate = number(value);
        else if (key == u"Number of samples")
            s.samples = qint64(number(value));
    }
    return s;
}

Ebur128Summary parseEbur128(const QList<LogLine> &lines)
{
    // "Summary:" then indented continuation lines:
    //   Integrated loudness:  I: -12.0 LUFS  Threshold: -26.7 LUFS
    //   Loudness range:  LRA: 25.3 LU  Threshold: ...  LRA low: ...  LRA high: ...
    //   True peak:  Peak: -8.5 dBFS
    static const QRegularExpression value(QStringLiteral(R"(^\s*([A-Za-z ]+):\s*(-?inf|-?[0-9.]+)\s*(LUFS|LU|dBFS)?\s*$)"));
    Ebur128Summary s;
    bool inSummary = false;
    QString section;
    for (const LogLine &l : lines) {
        if (l.text.trimmed() == u"Summary:") {
            inSummary = true;
            s = {};
            continue;
        }
        if (!inSummary)
            continue;
        const QString t = l.text.trimmed();
        if (t.endsWith(u':')) {
            section = t;
            continue;
        }
        const auto m = value.match(l.text);
        if (!m.hasMatch())
            continue;
        const QString key = m.captured(1).trimmed();
        const double v = number(m.captured(2));
        if (section.startsWith(QStringLiteral("Integrated"))) {
            if (key == u"I")
                s.integrated = v;
            else if (key == u"Threshold")
                s.threshold = v;
        } else if (section.startsWith(QStringLiteral("Loudness range"))) {
            if (key == u"LRA")
                s.lra = v;
            else if (key == u"LRA low")
                s.lraLow = v;
            else if (key == u"LRA high")
                s.lraHigh = v;
        } else if (section.startsWith(QStringLiteral("True peak")) && key == u"Peak") {
            s.truePeak = v;
        }
    }
    return s;
}

QList<Interval> parseSilence(const QList<LogLine> &lines, double duration)
{
    static const QRegularExpression start(QStringLiteral(R"(silence_start:\s*(-?[0-9.]+))"));
    static const QRegularExpression end(QStringLiteral(R"(silence_end:\s*(-?[0-9.]+))"));
    QList<Interval> out;
    double open = NAN;
    for (const LogLine &l : lines) {
        if (const auto m = start.match(l.text); m.hasMatch()) {
            open = std::max(0.0, m.captured(1).toDouble());
        } else if (const auto m2 = end.match(l.text); m2.hasMatch() && !std::isnan(open)) {
            out << Interval { open, m2.captured(1).toDouble() };
            open = NAN;
        }
    }
    if (!std::isnan(open) && duration > open)
        out << Interval { open, duration };
    return out;
}

QList<MetadataFrame> parseMetadata(const QByteArray &text)
{
    QList<MetadataFrame> frames;
    qsizetype pos = 0;
    while (pos < text.size()) {
        qsizetype nl = text.indexOf('\n', pos);
        if (nl < 0)
            nl = text.size();
        const QByteArray line = text.mid(pos, nl - pos).trimmed();
        pos = nl + 1;
        if (line.startsWith("frame:")) {
            MetadataFrame f;
            const qsizetype t = line.indexOf("pts_time:");
            if (t >= 0)
                f.time = line.mid(t + 9).trimmed().toDouble();
            frames << f;
        } else if (!frames.isEmpty()) {
            const qsizetype eq = line.indexOf('=');
            if (eq > 0) {
                bool ok = false;
                const double v = line.mid(eq + 1).toDouble(&ok);
                if (ok)
                    frames.last().values.insert(QString::fromLatin1(line.left(eq)), v);
            }
        }
    }
    return frames;
}

QList<MetadataFrame> parseMetadataFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return parseMetadata(f.readAll());
}

} // namespace ffgui::analysis
