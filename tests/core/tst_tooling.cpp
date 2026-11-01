#include "App.h"
#include "fixtures.h"
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <capture/Capture.h>
#include <cmath>
#include <compare/Comparer.h>
#include <dsp/Scopes.h>
#include <metadata/Metadata.h>
#include <tools/CommandLine.h>
#include <tools/ShellQuote.h>
#include <transcript/Transcript.h>
using namespace ffgui;
class TestTooling : public QObject
{
    Q_OBJECT
  private slots:
    void initTestCase() { QCoreApplication::setOrganizationName("ffgui-tooling-tests"); }
    void commandText()
    {
        const QStringList args{"ffmpeg", "-i",   "a file's \\\"name.wav", "-af", "pan=stereo|c0=c1|c1=c0",
                               "",       "$HOME"};
        QString error;
        QCOMPARE(splitCommand(shellJoin(args), &error), args);
        QVERIFY(error.isEmpty());
        QCOMPARE(splitCommand("ffmpeg \\\n -i \"input file\" -metadata 'title=a;b'"),
                 QStringList({"ffmpeg", "-i", "input file", "-metadata", "title=a;b"}));
        for (const auto &bad :
             {"ffmpeg -i a | cat", "ffmpeg ; true", "ffmpeg > out", "ffmpeg 'oops", "ffmpeg -i a &"}) {
            error.clear();
            QVERIFY(splitCommand(bad, &error).isEmpty());
            QVERIFY(!error.isEmpty());
        }
    }
    void transcripts()
    {
        const QList<QVariantMap> entries{{{"start", 1.234}, {"end", 3.5}, {"text", "Hello\nworld"}},
                                         {{"start", 5.}, {"end", 6.}, {"text", "café"}}};
        for (const QString &format : {"srt", "json"}) {
            QList<QVariantMap> parsed;
            QString error;
            QVERIFY2(Transcript::parse(Transcript::encode(entries, format), format, &parsed, &error),
                     qPrintable(error));
            QCOMPARE(parsed, entries);
        }
        QList<QVariantMap> parsed;
        QString error;
        QVERIFY(Transcript::parse("first\r\n\r\nsecond", "txt", &parsed, &error));
        QCOMPARE(parsed.size(), 2);
        QCOMPARE(parsed[1].value("start").toDouble(), 5.);
        QVERIFY(!Transcript::parse("[{\"start\":2,\"end\":1,\"text\":\"bad\"}]", "json", &parsed, &error));
        QVERIFY(!Transcript::parse("1\n00:00:10,000 --> 00:00:01,000\nbad", "srt", &parsed, &error));
    }
    void devices()
    {
        const auto pulse = DeviceList::parse(fixture("sources_pulse.txt"), "pulse");
        QVERIFY(pulse.size() >= 2);
        bool monitor = false, def = false;
        for (const auto &d : pulse) {
            monitor |= d.value("isMonitor").toBool();
            def |= d.value("isDefault").toBool();
            QCOMPARE(d.value("backend").toString(), QString("pulse"));
        }
        QVERIFY(monitor);
        QVERIFY(def);
        QVERIFY(DeviceList::parse("Could not list sources", "pulse").isEmpty());
        QVERIFY(DeviceList::parse(fixture("sources_alsa.txt"), "alsa").size() >= 2);
    }
    void outputArgs()
    {
        CaptureSpec capture;
        capture.output = "/tmp/rec_%03d.wav";
        capture.seconds = 2;
        capture.splitSeconds = .5;
        capture.maxBytes = 100000;
        auto args = Recorder::args(capture);
        QVERIFY(args.contains("-n"));
        QVERIFY(args.contains("segment"));
        QVERIFY(args.contains("pcm_f32le"));
        QVERIFY(args.contains("100000"));
        for (const QString &protocol : {"rtmp", "icecast", "srt", "hls"}) {
            QString error;
            const auto target =
                protocol == u"hls" ? "/tmp/ffgui-unit-absent.m3u8" : protocol + "://127.0.0.1/example";
            args = StreamOut::args({"source.wav", "volume=0.5", protocol, target, 1, 96}, &error);
            QVERIFY2(!args.isEmpty(), qPrintable(error));
            QVERIFY(args.contains("0:a:1"));
            QVERIFY(args.contains("96k"));
            QVERIFY(args.contains("volume=0.5"));
        }
        QString error;
        QVERIFY(StreamOut::args({"a.wav", {}, "rtmp", "https://example.org", 0, 128}, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }
    void scoresAndOffset()
    {
        const auto scores = Comparer::parseScores(fixture("compare_scores.txt"));
        QVERIFY(std::isinf(scores.value("sdr").toDouble()));
        QVERIFY(std::isinf(scores.value("psnr").toDouble()));
        std::vector<float> a(4000), b(4000);
        for (int i = 0; i < 3500; ++i) {
            a[i] = float((i / 100 % 5 + 1) * .1 * std::sin(i * .19));
            b[i + 200] = a[i];
        }
        double confidence = 0;
        QVERIFY(std::abs(Comparer::findOffset(a, b, 1000, &confidence) - .2) < .011);
        QVERIFY(confidence > .95);
        std::fill(a.begin(), a.end(), 0);
        std::fill(b.begin(), b.end(), 0);
        QCOMPARE(Comparer::findOffset(a, b, 1000), 0.);
    }
    void scopeSignal()
    {
        std::vector<float> samples(4096 * 2);
        for (int i = 0; i < 4096; ++i)
            samples[2 * i] = samples[2 * i + 1] = float(.5 * std::sin(2 * M_PI * 1000 * i / 48000));
        auto data = ScopeAnalysis::analyze(samples.data(), 4096, 2, 48000);
        QCOMPARE(data.value("spectrum").toList().size(), 64);
        QCOMPARE(data.value("cqt").toList().size(), 96);
        QVERIFY(std::abs(data.value("phase").toDouble() - 1) < 1e-6);
        const auto spectrum = data.value("spectrum").toList();
        double max = -100;
        int at = 0;
        for (int i = 0; i < spectrum.size(); ++i)
            if (spectrum[i].toDouble() > max) {
                max = spectrum[i].toDouble();
                at = i;
            }
        QVERIFY(max > -7 && max < -5);
        QVERIFY(at >= 33 && at <= 37);
        for (int i = 0; i < 4096; ++i)
            samples[2 * i + 1] = -samples[2 * i];
        data = ScopeAnalysis::analyze(samples.data(), 4096, 2, 48000);
        QVERIFY(data.value("phase").toDouble() < -.999);
        QVERIFY(data.value("vectorscope").toList().size() <= 512);
        std::fill(samples.begin(), samples.end(), 0);
        for (double bit : BitScope::activity(samples.data(), int(samples.size())))
            QCOMPARE(bit, 0.);
        ScopeState scope;
        scope.consume(samples.data(), 4096, 2, 48000);
        scope.publish();
        QVERIFY(scope.ready());
        scope.clear();
        QVERIFY(!scope.ready());
    }
    void metadataEscapes()
    {
        const auto data = ffmetadata({{"title", "a=b;#\nc\\d"}},
                                     {{QVariantMap{{"start", 1.25}, {"end", 2.}, {"title", "Chapter"}}}});
        QVERIFY(data.startsWith(";FFMETADATA1\n"));
        QVERIFY(data.contains("title=a\\=b\\;\\#\\\nc\\\\d"));
        QVERIFY(data.contains("START=1250\nEND=2000"));
    }
    void settingsPaths()
    {
        QTemporaryDir dir;
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        ToolSettings settings;
        QVERIFY(!settings.save("/does/not/exist", {}, {}));
        QVERIFY(!settings.error().isEmpty());
        QVERIFY(settings.save("sh", {}, {}));
        QVERIFY(QFileInfo(settings.ffmpeg()).isAbsolute());
        QVERIFY(settings.save({}, {}, {}));
    }
};
QTEST_MAIN(TestTooling)
#include "tst_tooling.moc"
