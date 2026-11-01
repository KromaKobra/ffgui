#include "fixtures.h"

#include <caps/BuildInfo.h>

#include <QJsonDocument>
#include <QTest>

using namespace ffgui;

class TestCaps : public QObject
{
    Q_OBJECT

private slots:
    void version()
    {
        BuildInfo info;
        caps::parseVersion(QString::fromUtf8(fixture("version.txt")), &info);
        QCOMPARE(info.version, QStringLiteral("9.0.2"));
        QVERIFY(info.configuration.startsWith(QStringLiteral("--prefix=/usr")));
        QVERIFY(info.hasConfigFlag(QStringLiteral("--enable-libmp3lame")));
    }

    void filters()
    {
        const auto f = caps::parseFilters(QString::fromUtf8(fixture("filters.txt")));
        QVERIFY(f.size() > 500);
        QVERIFY(f.value(QStringLiteral("afftdn")).timeline);
        QVERIFY(f.value(QStringLiteral("afftdn")).slice);
        QVERIFY(f.value(QStringLiteral("volume")).timeline);
        QVERIFY(!f.value(QStringLiteral("loudnorm")).timeline);
        QCOMPARE(f.value(QStringLiteral("afftdn")).io, QStringLiteral("A->A"));
        QVERIFY(f.contains(QStringLiteral("rubberband")));
        QVERIFY(!f.contains(QStringLiteral("whisper")));
    }

    void encodersAndMuxers()
    {
        const auto e = caps::parseEncoders(QString::fromUtf8(fixture("encoders.txt")));
        QVERIFY(e.contains(QStringLiteral("libmp3lame")));
        QVERIFY(e.contains(QStringLiteral("flac")));
        QVERIFY(!e.contains(QStringLiteral("libx264")));   // video: not listed
        const auto m = caps::parseMuxers(QString::fromUtf8(fixture("muxers.txt")));
        for (const char *name : { "mp3", "ipod", "wav", "matroska", "flac", "ogg", "opus" })
            QVERIFY2(m.contains(QString::fromLatin1(name)), name);
    }

    void librariesAndCacheRoundTrip()
    {
        BuildInfo info;
        caps::parseVersion(QString::fromUtf8(fixture("version.txt")), &info);
        info.filters = caps::parseFilters(QString::fromUtf8(fixture("filters.txt")));
        info.audioEncoders = caps::parseEncoders(QString::fromUtf8(fixture("encoders.txt")));
        info.muxers = caps::parseMuxers(QString::fromUtf8(fixture("muxers.txt")));
        info.libraries = caps::evaluateLibraries(info);
        auto found = [&](const char *name) {
            for (const LibraryInfo &l : info.libraries)
                if (l.name == QLatin1String(name))
                    return l.found;
            return false;
        };
        QVERIFY(found("libmp3lame"));
        QVERIFY(found("librubberband"));
        QVERIFY(!found("whisper"));
        QVERIFY(!found("libfdk_aac"));

        const BuildInfo back = BuildInfo::fromJson(QJsonDocument::fromJson(QJsonDocument(info.toJson()).toJson()).object());
        QCOMPARE(back.version, info.version);
        QCOMPARE(back.filters.size(), info.filters.size());
        QCOMPARE(back.filters.value(QStringLiteral("afftdn")).timeline, true);
        QCOMPARE(back.libraries.size(), info.libraries.size());
    }
};

QTEST_GUILESS_MAIN(TestCaps)
#include "tst_caps.moc"
