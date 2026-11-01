#include "fixtures.h"

#include <tools/ProcessJob.h>
#include <tools/ProgressParser.h>
#include <tools/ShellQuote.h>

#include <QTest>

using namespace ffgui;

class TestTools : public QObject
{
    Q_OBJECT

private slots:
    void logLines()
    {
        const LogLine a = LogLine::parse(QStringLiteral("[Parsed_silencedetect_5 @ 0x7f2944004980] [info] silence_start: 1.999958"));
        QCOMPARE(a.level, QStringLiteral("info"));
        QCOMPARE(a.context, QStringLiteral("Parsed_silencedetect_5"));
        QCOMPARE(a.text, QStringLiteral("silence_start: 1.999958"));
        QVERIFY(!a.continuation);

        const LogLine b = LogLine::parse(QStringLiteral("[info] Input #0, mp3, from 'test.mp3':"));
        QCOMPARE(b.context, QString());
        QCOMPARE(b.level, QStringLiteral("info"));

        // An untagged line carries on from the one before (ebur128's summary).
        const LogLine c = LogLine::parse(QStringLiteral("  Integrated loudness:"), a);
        QVERIFY(c.continuation);
        QCOMPARE(c.context, a.context);
        QCOMPARE(c.level, QStringLiteral("info"));

        const LogLine d = LogLine::parse(QStringLiteral("[in#0 @ 0x561e9825bc40] [warning] filesize and duration do not match"));
        QCOMPARE(d.level, QStringLiteral("warning"));
        QCOMPARE(d.context, QStringLiteral("in#0"));

        const LogLine e = LogLine::parse(QString::fromUtf8(fixture("probe_junk_stderr.txt")).trimmed());
        QCOMPARE(e.level, QStringLiteral("error"));
        QCOMPARE(e.text, QStringLiteral("junk.wav: Invalid data found when processing input"));

        QCOMPARE(LogLine::parse(QStringLiteral("[fatal] boom")).level, QStringLiteral("error"));
        QVERIFY(LogLine::rank(QStringLiteral("error")) < LogLine::rank(QStringLiteral("warning")));
        QVERIFY(LogLine::benign(QStringLiteral("Guessed Channel Layout: stereo")));
        QVERIFY(!LogLine::benign(QStringLiteral("Header missing")));
    }

    void progress()
    {
        ProgressParser p;
        ProgressParser::Snapshot s;
        const QByteArray data = fixture("progress.txt");
        // Fed in two halves, as pipes deliver it.
        QVERIFY(!p.feed(data.left(20), &s));
        QVERIFY(p.feed(data.mid(20), &s));
        QVERIFY(s.end);
        QCOMPARE(s.outTime, 5.0);
        QVERIFY(s.speed > 100);

        ProgressParser q;
        QVERIFY(q.feed("out_time_us=1500000\nspeed=N/A\nprogress=continue\n", &s));
        QCOMPARE(s.outTime, 1.5);
        QCOMPARE(s.speed, 0.0);
        QVERIFY(!s.end);
    }

    void shellQuoting()
    {
        QCOMPARE(shellQuote(QStringLiteral("plain.wav")), QStringLiteral("plain.wav"));
        QCOMPARE(shellQuote(QStringLiteral("my file.wav")), QStringLiteral("\"my file.wav\""));
        QCOMPARE(shellQuote(QStringLiteral("it's $5.wav")), QStringLiteral("'it'\\''s $5.wav'"));
        QCOMPARE(shellQuote(QString()), QStringLiteral("''"));
        QCOMPARE(shellJoin({ QStringLiteral("ffmpeg"), QStringLiteral("-af"), QStringLiteral("volume=3dB,afade=t=in") }),
                 QStringLiteral("ffmpeg -af volume=3dB,afade=t=in"));
        QCOMPARE(shellJoin({ QStringLiteral("-af"), QStringLiteral("pan=mono|c0=c0") }), QStringLiteral("-af \"pan=mono|c0=c0\""));
    }
};

QTEST_GUILESS_MAIN(TestTools)
#include "tst_tools.moc"
