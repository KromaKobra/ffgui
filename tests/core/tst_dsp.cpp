#include <dsp/LoudnessMeter.h>
#include <playback/RingBuffer.h>

#include <QTest>

#include <cmath>
#include <numbers>

using namespace ffgui;

namespace {

std::vector<float> tone(double seconds, int channels, double hz, double amplitude, double phase = 0, int rate = 48000)
{
    const int n = int(seconds * rate);
    std::vector<float> s(size_t(n) * channels);
    for (int f = 0; f < n; ++f)
        for (int c = 0; c < channels; ++c)
            s[size_t(f) * channels + c] = float(amplitude * std::sin(2 * std::numbers::pi * hz * f / rate + phase));
    return s;
}

const QStringList kStereo { QStringLiteral("FL"), QStringLiteral("FR") };

} // namespace

class TestDsp : public QObject
{
    Q_OBJECT

private slots:
    // EBU Tech 3341: a 1 kHz sine at -23 dBFS on both channels reads -23.0 LUFS.
    void referenceTone()
    {
        LoudnessMeter m(48000, kStereo);
        const std::vector<float> s = tone(20, 2, 1000, std::pow(10.0, -23.0 / 20.0));
        m.process(s.data(), qint64(s.size() / 2));
        QVERIFY2(qAbs(m.integrated() + 23.0) < 0.1, qPrintable(QString::number(m.integrated())));
        QVERIFY(qAbs(m.momentary() + 23.0) < 0.1);
        QVERIFY(qAbs(m.shortTerm() + 23.0) < 0.1);
    }

    void monoIsThreeDecibelsQuieter()
    {
        LoudnessMeter m(44100, { QStringLiteral("FC") });
        const std::vector<float> s = tone(10, 1, 1000, std::pow(10.0, -20.0 / 20.0), 0, 44100);
        m.process(s.data(), qint64(s.size()));
        QVERIFY2(qAbs(m.integrated() + 23.01) < 0.1, qPrintable(QString::number(m.integrated())));
    }

    void silenceIsGatedOut()
    {
        LoudnessMeter m(48000, kStereo);
        const std::vector<float> loud = tone(10, 2, 1000, std::pow(10.0, -23.0 / 20.0));
        const std::vector<float> quiet(size_t(48000) * 20 * 2, 0.0f);
        m.process(quiet.data(), qint64(quiet.size() / 2));
        QVERIFY(std::isinf(m.integrated()));
        m.process(loud.data(), qint64(loud.size() / 2));
        m.process(quiet.data(), qint64(quiet.size() / 2));
        // The 400 ms blocks straddling each edge pass the gate and pull the
        // result down a little, as in any BS.1770 meter (ebur128 included).
        QVERIFY2(qAbs(m.integrated() + 23.0) < 0.2, qPrintable(QString::number(m.integrated())));
        QVERIFY(m.momentary() < -100 || std::isinf(m.momentary()));
    }

    void truePeakFindsIntersamplePeaks()
    {
        // fs/4 sine, 45° phase: every sample is at ±0.707, the waveform peaks at 1.0.
        LoudnessMeter m(48000, kStereo);
        const std::vector<float> s = tone(1, 2, 12000, 1.0, std::numbers::pi / 4);
        m.process(s.data(), qint64(s.size() / 2));
        QVERIFY(qAbs(m.samplePeak() + 3.01) < 0.05);
        QVERIFY2(qAbs(m.truePeak()) < 0.5, qPrintable(QString::number(m.truePeak())));
        QVERIFY(qAbs(m.takeTruePeak() - m.truePeak()) < 1e-9);
        QVERIFY(std::isinf(m.takeTruePeak()));
    }

    void lfeDoesNotCount()
    {
        LoudnessMeter a(48000, { QStringLiteral("FL"), QStringLiteral("FR"), QStringLiteral("FC"), QStringLiteral("LFE") });
        std::vector<float> s = tone(5, 4, 1000, 0.1);
        for (size_t f = 0; f < s.size() / 4; ++f)
            s[f * 4 + 3] = 0.9f;        // a loud LFE changes nothing
        a.process(s.data(), qint64(s.size() / 4));
        LoudnessMeter b(48000, { QStringLiteral("FL"), QStringLiteral("FR"), QStringLiteral("FC") });
        const std::vector<float> t = tone(5, 3, 1000, 0.1);
        b.process(t.data(), qint64(t.size() / 3));
        QVERIFY(qAbs(a.integrated() - b.integrated()) < 0.01);
    }

    void correlation()
    {
        Correlation c;
        std::vector<float> s = tone(0.5, 2, 440, 0.5);
        c.process(s.data(), qint64(s.size() / 2), 2);
        QVERIFY(qAbs(c.take() - 1.0) < 1e-6);
        for (size_t f = 0; f < s.size() / 2; ++f)
            s[f * 2 + 1] = -s[f * 2];
        c.process(s.data(), qint64(s.size() / 2), 2);
        QVERIFY(qAbs(c.take() + 1.0) < 1e-6);
        QCOMPARE(c.take(0.25), 0.25);   // nothing fed: the fallback
    }

    void ringBufferWraps()
    {
        RingBuffer r(10);
        float in[7] = { 1, 2, 3, 4, 5, 6, 7 }, out[10] = {};
        QCOMPARE(r.write(in, 7), size_t(7));
        QCOMPARE(r.read(out, 5), size_t(5));
        QCOMPARE(r.write(in, 7), size_t(7));     // wraps around
        QCOMPARE(r.write(in, 7), size_t(1));     // full: only room for one
        QCOMPARE(r.readable(), size_t(10));
        QCOMPARE(r.read(out, 10), size_t(10));
        QCOMPARE(out[0], 6.0f);
        QCOMPARE(out[2], 1.0f);
        QCOMPARE(out[9], 1.0f);
        QCOMPARE(r.readable(), size_t(0));
    }
};

QTEST_GUILESS_MAIN(TestDsp)
#include "tst_dsp.moc"
