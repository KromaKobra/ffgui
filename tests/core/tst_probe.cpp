#include "fixtures.h"

#include <probe/ChannelLayouts.h>
#include <probe/ProbeParser.h>

#include <QTest>

using namespace ffgui;

class TestProbe : public QObject
{
    Q_OBJECT

private slots:
    void mp3WithCoverAndChapters()
    {
        ProbeResult r;
        r.path = QStringLiteral("/music/test.mp3");
        QString error;
        QVERIFY2(parseProbe(fixture("probe_mp3.json"), &r, &error), qPrintable(error));
        QCOMPARE(r.formatName, QStringLiteral("mp3"));
        QCOMPARE(r.duration, 5.0);
        QCOMPARE(r.size, 25984);
        QCOMPARE(r.containerLabel(), QStringLiteral("MP3"));
        QCOMPARE(r.tags.value(QStringLiteral("title")).toString(), QStringLiteral("Test Title"));
        QCOMPARE(r.tracks.size(), 1);
        const AudioTrack &t = r.tracks.first();
        QCOMPARE(t.codec, QStringLiteral("mp3"));
        QCOMPARE(t.sampleRate, 48000);
        QCOMPARE(t.channels, 2);
        QCOMPARE(t.channelNames, QStringList({ QStringLiteral("FL"), QStringLiteral("FR") }));
        QCOMPARE(t.bitsPerSample, 0);
        QVERIFY(t.isDefault);           // the only track, though not flagged default
        QCOMPARE(r.coverStream, 1);
        QCOMPARE(r.coverWidth, 64);
        QCOMPARE(r.chapters.size(), 2);
        QCOMPARE(r.chapters[1].title, QStringLiteral("Rest"));
        QCOMPARE(r.chapters[1].start, 2.0);
    }

    void wavWithoutLayout()
    {
        ProbeResult r;
        r.path = QStringLiteral("/x/test.wav");
        QVERIFY(parseProbe(fixture("probe_wav.json"), &r));
        QCOMPARE(r.containerLabel(), QStringLiteral("WAV"));
        const AudioTrack &t = r.tracks.first();
        QCOMPARE(t.codec, QStringLiteral("pcm_s16le"));
        QCOMPARE(t.bitsPerSample, 16);
        QCOMPARE(t.layout, QStringLiteral("stereo"));     // ffmpeg's default for 2 channels
        QCOMPARE(t.bitRate, 1536000);
        QCOMPARE(r.coverStream, -1);
    }

    void garbage()
    {
        ProbeResult r;
        QString error;
        QVERIFY(!parseProbe("{\n\n}", &r, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!parseProbe("not json", &r, &error));
    }

    void layouts()
    {
        QCOMPARE(layouts::channelNames(QStringLiteral("5.1(side)"), 6),
                 QStringList({ "FL", "FR", "FC", "LFE", "SL", "SR" }));
        QCOMPARE(layouts::channelNames(QString(), 1), QStringList({ "FC" }));
        QCOMPARE(layouts::channelNames(QStringLiteral("FL+FR+LFE"), 3), QStringList({ "FL", "FR", "LFE" }));
        // A layout that doesn't match the count falls back to the default.
        QCOMPARE(layouts::channelNames(QStringLiteral("stereo"), 6).size(), 6);
        QCOMPARE(layouts::channelNames(QString(), 11).first(), QStringLiteral("c0"));
        QCOMPARE(layouts::defaultLayout(6), QStringLiteral("5.1"));
    }
};

QTEST_GUILESS_MAIN(TestProbe)
#include "tst_probe.moc"
