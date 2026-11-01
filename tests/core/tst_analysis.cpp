#include "fixtures.h"

#include <analysis/AnalysisParsers.h>

#include <QTest>

using namespace ffgui;
using namespace ffgui::analysis;

// The parsers against the log and ametadata files this ffmpeg wrote for a
// 5 s test file: 2 s of quiet 1 kHz, 2 s of silence, 1 s of louder 440 Hz.
class TestAnalysis : public QObject
{
    Q_OBJECT

private:
    QList<LogLine> m_lines;

private slots:
    void initTestCase()
    {
        LogLine previous;
        for (const QByteArray &raw : fixture("analysis_log.txt").split('\n')) {
            const QString text = QString::fromUtf8(raw);
            if (text.trimmed().isEmpty())
                continue;
            previous = LogLine::parse(text, previous);
            m_lines << previous;
        }
        QVERIFY(m_lines.size() > 50);
    }

    void astats()
    {
        const AstatsSummary s = parseAstats(linesOf(m_lines, QStringLiteral("astats")));
        QVERIFY(s.valid());
        QCOMPARE(s.channels.size(), 2);
        QCOMPARE(s.channels[0].peakDb, -8.497713);
        QCOMPARE(s.channels[1].rmsDb, -24.534522);
        QCOMPARE(s.channels[0].dcOffset, -0.000001);
        QCOMPARE(s.channels[0].bitDepth, QStringLiteral("30/32/32/32"));
        QVERIFY(std::isinf(s.channels[0].noiseFloorDb) && s.channels[0].noiseFloorDb < 0);
        QCOMPARE(s.overall.peakDb, -8.497713);
        QCOMPARE(s.overall.rmsDb, -20.552897);
        QCOMPARE(s.samples, 240000);
    }

    void ebur128()
    {
        const Ebur128Summary s = parseEbur128(linesOf(m_lines, QStringLiteral("ebur128")));
        QVERIFY(s.valid());
        QCOMPARE(s.integrated, -12.0);
        QCOMPARE(s.threshold, -26.7);
        QCOMPARE(s.lra, 25.3);
        QCOMPARE(s.lraLow, -41.8);
        QCOMPARE(s.lraHigh, -16.5);
        QCOMPARE(s.truePeak, -8.5);
    }

    void silence()
    {
        const QList<Interval> s = parseSilence(linesOf(m_lines, QStringLiteral("silencedetect")), 5.0);
        QCOMPARE(s.size(), 1);
        QVERIFY(qAbs(s[0].start - 2.0) < 0.01);
        QVERIFY(qAbs(s[0].end - 4.0) < 0.01);

        // A silence still open at the end runs to the end of the input.
        const QList<LogLine> open { LogLine::parse(QStringLiteral("[Parsed_silencedetect_0 @ 0x1] [info] silence_start: 3.5")) };
        const QList<Interval> tail = parseSilence(open, 6.0);
        QCOMPARE(tail.size(), 1);
        QCOMPARE(tail[0].end, 6.0);
    }

    void metadataFiles()
    {
        const QList<MetadataFrame> loud = parseMetadata(fixture("loudness.txt"));
        QCOMPARE(loud.size(), 4);
        QCOMPARE(loud[1].time, 0.1);
        QCOMPARE(loud[0].values.value(QStringLiteral("lavfi.r128.M")), -120.691);
        QVERIFY(loud[0].values.contains(QStringLiteral("lavfi.r128.true_peaks_ch1")));

        const QList<MetadataFrame> phase = parseMetadata(fixture("phase.txt"));
        QCOMPARE(phase.size(), 10);
        QCOMPARE(phase[0].values.value(QStringLiteral("lavfi.aphasemeter.phase")), 0.782196);

        const QList<MetadataFrame> spec = parseMetadata(fixture("spectrum.txt"));
        QCOMPARE(spec[0].values.value(QStringLiteral("lavfi.aspectralstats.2.rolloff")), 1312.5);
    }
};

QTEST_GUILESS_MAIN(TestAnalysis)
#include "tst_analysis.moc"
