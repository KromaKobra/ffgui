#include <util/Format.h>

#include <QTest>

#include <cmath>

using namespace ffgui;

class TestFormat : public QObject
{
    Q_OBJECT

private slots:
    void timecode_data()
    {
        QTest::addColumn<double>("seconds");
        QTest::addColumn<bool>("ms");
        QTest::addColumn<QString>("expected");
        QTest::newRow("zero") << 0.0 << false << "00:00";
        QTest::newRow("minutes") << 75.4 << false << "01:15";
        QTest::newRow("hours") << 3725.0 << false << "1:02:05";
        QTest::newRow("ms") << 75.25 << true << "01:15.250";
        QTest::newRow("ms rounding up") << 59.9996 << true << "01:00.000";
        QTest::newRow("negative clamps") << -3.0 << false << "00:00";
        QTest::newRow("nan") << double(NAN) << true << "00:00.000";
    }
    void timecode()
    {
        QFETCH(double, seconds);
        QFETCH(bool, ms);
        QFETCH(QString, expected);
        QCOMPARE(format::timecode(seconds, ms), expected);
    }

    void parseTimecode_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<double>("expected");
        QTest::newRow("seconds") << "75.2" << 75.2;
        QTest::newRow("mm:ss") << "02:03" << 123.0;
        QTest::newRow("h:mm:ss.ms") << "1:02:03.5" << 3723.5;
        QTest::newRow("spaces") << "  10 " << 10.0;
        QTest::newRow("bad") << "abc" << double(NAN);
        QTest::newRow("sixty seconds") << "1:60" << double(NAN);
        QTest::newRow("empty") << "" << double(NAN);
        QTest::newRow("too many parts") << "1:2:3:4" << double(NAN);
    }
    void parseTimecode()
    {
        QFETCH(QString, text);
        QFETCH(double, expected);
        const double got = format::parseTimecode(text);
        if (std::isnan(expected))
            QVERIFY(std::isnan(got));
        else
            QCOMPARE(got, expected);
    }

    void numbers()
    {
        QCOMPARE(format::number(-12.34), QStringLiteral("−12.3"));
        QCOMPARE(format::number(-0.04), QStringLiteral("0.0"));     // no "−0.0"
        QCOMPARE(format::signedNumber(2.5), QStringLiteral("+2.5"));
        QCOMPARE(format::signedNumber(-2.5), QStringLiteral("−2.5"));
        QCOMPARE(format::signedNumber(0), QStringLiteral("0.0"));
        QCOMPARE(format::decibels(1.0), QStringLiteral("0.0"));
        QCOMPARE(format::decibels(0.5), QStringLiteral("−6.0"));
        QCOMPARE(format::decibels(0), QStringLiteral("−∞"));
    }

    void units()
    {
        QCOMPARE(format::bytes(417), QStringLiteral("417 B"));
        QCOMPARE(format::bytes(731'000'000), QStringLiteral("731 MB"));
        QCOMPARE(format::bytes(1'700'000), QStringLiteral("1.7 MB"));
        QCOMPARE(format::bytes(1'400'000'000), QStringLiteral("1.40 GB"));
        QCOMPARE(format::sampleRate(48000), QStringLiteral("48 kHz"));
        QCOMPARE(format::sampleRate(44100), QStringLiteral("44.1 kHz"));
        QCOMPARE(format::frequency(440), QStringLiteral("440 Hz"));
        QCOMPARE(format::frequency(1200), QStringLiteral("1.20 kHz"));
        QCOMPARE(format::bitrate(2304000), QStringLiteral("2304 kb/s"));
    }

    void notes()
    {
        QCOMPARE(format::noteName(440), QStringLiteral("A4"));
        QCOMPARE(format::noteName(261.63), QStringLiteral("C4"));
        QCOMPARE(format::noteName(55), QStringLiteral("A1"));
        QCOMPARE(format::noteName(0), QStringLiteral("—"));
    }
};

QTEST_GUILESS_MAIN(TestFormat)
#include "tst_format.moc"
