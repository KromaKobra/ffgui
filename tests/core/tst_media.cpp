#include <media/Fft.h>
#include <media/MediaData.h>
#include <media/PeakPyramid.h>
#include <media/Spectrogram.h>

#include <QTest>

#include <cmath>
#include <numbers>

using namespace ffgui;

namespace {

std::vector<float> sine(int frames, int channels, double hz, double amplitude, int rate = 48000)
{
    std::vector<float> s(size_t(frames) * channels);
    for (int f = 0; f < frames; ++f)
        for (int c = 0; c < channels; ++c)
            s[size_t(f) * channels + c] = float(amplitude * std::sin(2 * std::numbers::pi * hz * f / rate));
    return s;
}

} // namespace

class TestMedia : public QObject
{
    Q_OBJECT

private slots:
    void fftFindsTheBin()
    {
        Fft fft(1024);
        std::vector<std::complex<float>> d(1024);
        for (int i = 0; i < 1024; ++i)
            d[size_t(i)] = { float(std::cos(2 * std::numbers::pi * 37 * i / 1024)), 0.0f };
        fft.transform(d.data());
        int best = 0;
        for (int k = 1; k < 512; ++k)
            if (std::abs(d[size_t(k)]) > std::abs(d[size_t(best)]))
                best = k;
        QCOMPARE(best, 37);
        QVERIFY(qAbs(std::abs(d[37]) - 512.0f) < 0.5f);    // N/2 for a unit cosine
    }

    void pyramidAtEveryLevel()
    {
        // A ramp from -1 to 1 over 100k frames, channel 1 at half amplitude.
        const int n = 100000;
        std::vector<float> s(size_t(n) * 2);
        for (int f = 0; f < n; ++f) {
            s[size_t(f) * 2] = -1.0f + 2.0f * f / (n - 1);
            s[size_t(f) * 2 + 1] = 0.5f * s[size_t(f) * 2];
        }
        PeakPyramid p;
        p.reset(2, n);
        // Fed in odd chunks.
        for (int f = 0; f < n; f += 777)
            p.append(s.data() + size_t(f) * 2, std::min(777, n - f));
        p.finish();
        QCOMPARE(p.frames(), n);
        const PeakColumn all = p.range(0, 0, n);
        QVERIFY(all.valid);
        QVERIFY(qAbs(all.min + 1.0f) < 1e-3f && qAbs(all.max - 1.0f) < 1e-3f);
        QVERIFY(qAbs(all.rms - float(1.0 / std::sqrt(3.0))) < 0.01f);
        const PeakColumn half = p.range(1, 0, n);
        QVERIFY(qAbs(half.max - 0.5f) < 1e-3f);
        const PeakColumn both = p.range(-1, 0, n);
        QVERIFY(qAbs(both.max - 1.0f) < 1e-3f);

        // Columns: the first column of 10 holds the most negative values.
        std::vector<PeakColumn> cols(10);
        p.columns(0, 0, n, 10, cols.data());
        QVERIFY(cols[0].min < -0.99f && cols[0].max < -0.7f);
        QVERIFY(cols[9].max > 0.99f);
        // Zoomed in to a few blocks, still sensible.
        p.columns(0, 50000, 50000 + 2048, 4, cols.data());
        QVERIFY(cols[0].valid && cols[3].valid);
    }

    void spectrogramOfAFullScaleSine()
    {
        Spectrogram s;
        s.reset(1, 48000, 48000);
        const std::vector<float> data = sine(48000, 1, 3000, 1.0);
        s.append(data.data(), 48000);
        QVERIFY(s.columns() > 5);
        const int bin = int(std::lround(3000.0 / 24000.0 * Spectrogram::kBins));
        const uint8_t *col = s.column(0, s.columns() / 2);
        QVERIFY2(Spectrogram::toDb(col[bin]) > -1.0, qPrintable(QString::number(Spectrogram::toDb(col[bin]))));
        QVERIFY(Spectrogram::toDb(col[bin + 40]) < -60.0);
    }

    void mediaDataClipsAndWindows()
    {
        MediaData d(2, 48000, 48000);
        std::vector<float> s = sine(48000, 2, 100, 0.5);
        // Clip at 0.25 s and 0.75 s.
        s[size_t(12000) * 2] = 1.0f;
        s[size_t(36000) * 2 + 1] = -1.0f;
        d.append(s.data(), 48000);
        d.finish(true);
        QCOMPARE(d.clipSamples(), 2);
        const QList<double> times = d.clipTimes();
        QCOMPARE(times.size(), 2);
        QVERIFY(qAbs(times[0] - 0.25) < 1e-6);
        QVERIFY(qAbs(times[1] - 0.75) < 1e-6);

        const PeakColumn c = d.peakRange(0, 0.0, 0.5);
        QVERIFY(qAbs(c.max - 1.0f) < 1e-6f);

        // Raw samples for deep zoom.
        QVERIFY(!d.windowCovers(0.1, 0.2));
        d.setWindow(4800, std::vector<float>(s.begin() + 4800 * 2, s.begin() + 9600 * 2));
        QVERIFY(d.windowCovers(0.1, 0.2));
        std::vector<float> w;
        qint64 first = 0;
        QVERIFY(d.windowSamples(0, 0.1, 0.11, &w, &first));
        QCOMPARE(first, 4800);
        QCOMPARE(w[0], s[size_t(4800) * 2]);
        QVERIFY(!std::isnan(d.spectrumDb(0, 0.5, 100)));
    }

    void spectrogramRendering()
    {
        MediaData d(1, 48000, 48000);
        const std::vector<float> s = sine(48000, 1, 1000, 0.9);
        d.append(s.data(), 48000);
        d.finish(true);
        QRgb lut[256];
        for (int i = 0; i < 256; ++i)
            lut[i] = qRgb(i, i, i);
        QImage image(64, 100, QImage::Format_ARGB32_Premultiplied);
        d.renderSpectrogram(0, 0, 1, 20, 0, true, lut, &image);
        // The brightest row is where 1 kHz falls on a 20 Hz..24 kHz log axis.
        int brightest = 0, value = -1;
        for (int y = 0; y < 100; ++y)
            if (qRed(image.pixel(32, y)) > value) {
                value = qRed(image.pixel(32, y));
                brightest = y;
            }
        const int expected = int((1.0 - std::log(1000.0 / 20) / std::log(24000.0 / 20)) * 100);
        QVERIFY2(qAbs(brightest - expected) <= 2, qPrintable(QStringLiteral("%1 vs %2").arg(brightest).arg(expected)));
        QVERIFY(value > 240);
    }
};

QTEST_GUILESS_MAIN(TestMedia)
#include "tst_media.moc"
