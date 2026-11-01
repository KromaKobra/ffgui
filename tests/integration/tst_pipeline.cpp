#include <analysis/AnalysisPass.h>
#include <caps/CapabilityProbe.h>
#include <dsp/LoudnessMeter.h>
#include <media/PcmReader.h>
#include <playback/PlaybackEngine.h>
#include <probe/Prober.h>
#include <tools/FfTools.h>

#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>

using namespace ffgui;

// The real ffmpeg, ffprobe and audio output, on small generated files with
// known content. Skipped when ffmpeg isn't installed.
class TestPipeline : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_levels;       // 2 s 1 kHz at -20 dBFS, 3 s silence, 1 s clipped 440 Hz
    QString m_noise;        // 12 s of pink noise, for loudness

    static bool ffmpeg(const QStringList &args)
    {
        QProcess p;
        p.start(FfTools::paths().ffmpeg, QStringList { "-hide_banner", "-v", "error", "-y" } + args);
        return p.waitForFinished(30000) && p.exitCode() == 0;
    }

private slots:
    void initTestCase()
    {
        if (FfTools::paths().ffmpeg.isEmpty() || FfTools::paths().ffprobe.isEmpty())
            QSKIP("ffmpeg/ffprobe not found");
        QVERIFY(m_dir.isValid());
        m_levels = m_dir.filePath("levels.wav");
        m_noise = m_dir.filePath("noise.flac");
        QVERIFY(ffmpeg({ "-f", "lavfi", "-i", "sine=f=1000:r=48000:d=2,volume=0.8",          // 1/8 x 0.8 = 0.1 = -20 dBFS
                         "-f", "lavfi", "-i", "anullsrc=r=48000:cl=mono:d=3",
                         "-f", "lavfi", "-i", "sine=f=440:r=48000:d=1,volume=12",            // 1.5: clips
                         "-filter_complex", "[0][1][2]concat=n=3:v=0:a=1,pan=stereo|c0=c0|c1=c0",
                         "-c:a", "pcm_s16le", m_levels }));
        QVERIFY(ffmpeg({ "-f", "lavfi", "-i", "anoisesrc=color=pink:amplitude=0.25:r=48000:d=12:seed=7",
                         "-ac", "2", "-c:a", "flac", m_noise }));
    }

    void capabilities()
    {
        BuildInfo info;
        QString error;
        QVERIFY2(CapabilityProbe::detect(FfTools::paths(), &info, &error), qPrintable(error));
        QVERIFY(!info.version.isEmpty());
        for (const char *f : { "ebur128", "astats", "silencedetect", "aphasemeter", "aspectralstats", "asetnsamples" })
            QVERIFY2(info.filters.contains(QString::fromLatin1(f)), f);
    }

    void probe()
    {
        Prober p(m_levels);
        QSignalSpy done(&p, &Prober::finished);
        p.start();
        QVERIFY(done.wait(15000));
        QVERIFY(done.at(0).at(0).toBool());
        const ProbeResult r = done.at(0).at(1).value<ProbeResult>();
        QCOMPARE(r.duration, 6.0);
        QCOMPARE(r.tracks.size(), 1);
        QCOMPARE(r.tracks[0].channels, 2);
        QCOMPARE(r.tracks[0].sampleRate, 48000);
        QVERIFY(done.at(0).at(3).toStringList().isEmpty());     // no warnings worth reporting

        const QString junk = m_dir.filePath("junk.wav");
        QFile f(junk);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("definitely not audio");
        f.close();
        Prober bad(junk);
        QSignalSpy failed(&bad, &Prober::finished);
        bad.start();
        QVERIFY(failed.wait(15000));
        QVERIFY(!failed.at(0).at(0).toBool());
        QCOMPARE(failed.at(0).at(2).toString(), QStringLiteral("Invalid data found when processing input"));
    }

    void decode()
    {
        PcmReader reader({ m_levels, 0, 2, 48000, 6.0 });
        QSignalSpy done(&reader, &PcmReader::finished);
        reader.start();
        QVERIFY(done.wait(30000));
        QVERIFY(done.at(0).at(0).toBool());
        const auto data = reader.data();
        QCOMPARE(data->frames(), 6 * 48000);
        QVERIFY(data->complete());
        // The tone, the silence, the clipped part.
        QVERIFY(qAbs(20 * std::log10(data->peakRange(0, 0.5, 1.5).max) + 20.0) < 0.1);
        QVERIFY(data->peakRange(0, 2.5, 4.5).max < 1e-4f);
        QVERIFY(data->clipSamples() > 1000);
        const QList<double> clips = data->clipTimes();
        QVERIFY(!clips.isEmpty());
        QVERIFY(clips.first() >= 5.0 - 1e-3 && clips.last() <= 6.0);
    }

    void analysis()
    {
        AnalysisPass::Options o;
        o.input = m_levels;
        o.duration = 6.0;
        AnalysisPass pass(o);
        QSignalSpy done(&pass, &AnalysisPass::finished);
        pass.start();
        QVERIFY(done.wait(30000));
        const AnalysisResult r = done.at(0).at(0).value<AnalysisResult>();
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.decodeErrors, 0);
        QCOMPARE(r.silence.size(), 1);
        QVERIFY(qAbs(r.silence[0].start - 2.0) < 0.05);
        QVERIFY(qAbs(r.silence[0].end - 5.0) < 0.05);
        QVERIFY(qAbs(r.levels.overall.peakDb) < 0.1);           // the clipped second
        QVERIFY(r.loudness.valid());
        QVERIFY(!r.shortTerm.isEmpty() && r.times.size() == r.shortTerm.size());
        QVERIFY(qAbs(r.times.last() - 5.9) < 0.11);
        QVERIFY(qAbs(r.phaseMean - 1.0) < 0.05);                // identical channels
    }

    // Our meter (used live during playback) agrees with ffmpeg's ebur128.
    void loudnessMatchesEbur128()
    {
        QProcess decode;
        decode.start(FfTools::paths().ffmpeg, { "-v", "error", "-i", m_noise, "-f", "f32le", "-c:a", "pcm_f32le", "pipe:1" });
        QVERIFY(decode.waitForFinished(30000));
        const QByteArray pcm = decode.readAllStandardOutput();
        LoudnessMeter meter(48000, { "FL", "FR" });
        meter.process(reinterpret_cast<const float *>(pcm.constData()), pcm.size() / 8);

        AnalysisPass::Options o;
        o.input = m_noise;
        o.duration = 12.0;
        AnalysisPass pass(o);
        QSignalSpy done(&pass, &AnalysisPass::finished);
        pass.start();
        QVERIFY(done.wait(30000));
        const AnalysisResult r = done.at(0).at(0).value<AnalysisResult>();
        QVERIFY(r.ok);
        QVERIFY2(qAbs(meter.integrated() - r.loudness.integrated) <= 0.1,
                 qPrintable(QStringLiteral("ours %1, ebur128 %2").arg(meter.integrated()).arg(r.loudness.integrated)));
        QVERIFY2(qAbs(meter.truePeak() - r.loudness.truePeak) <= 0.5,
                 qPrintable(QStringLiteral("ours %1, ebur128 %2").arg(meter.truePeak()).arg(r.loudness.truePeak)));
    }

    void selectionStats()
    {
        AnalysisPass::Options o;
        o.input = m_levels;
        o.duration = 6.0;
        o.start = 0.5;
        o.length = 1.0;
        AnalysisPass pass(o);
        QSignalSpy done(&pass, &AnalysisPass::finished);
        pass.start();
        QVERIFY(done.wait(30000));
        const AnalysisResult r = done.at(0).at(0).value<AnalysisResult>();
        QVERIFY(r.ok && r.range);
        QVERIFY(qAbs(r.levels.overall.peakDb + 20.0) < 0.1);
        QVERIFY(qAbs(r.levels.overall.rmsDb + 23.01) < 0.1);
        QVERIFY(r.silence.isEmpty());
    }

    // Plays at zero volume: checks frames reach the device and the playhead moves.
    void playbackAtZeroVolume()
    {
        if (!PlaybackEngine::hasOutputDevice())
            QSKIP("no audio output device");
        PlaybackEngine engine;
        engine.setSource({ m_levels, 0, 2, 48000, { "FL", "FR" }, 6.0 });
        engine.setVolume(0);
        QVERIFY2(engine.play(1.0), qPrintable(engine.lastError()));
        QTRY_VERIFY_WITH_TIMEOUT(engine.framesPlayed() > 4800, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(engine.position() > 1.2, 5000);
        std::vector<float> tap(48000 * 2);
        QVERIFY(engine.takeTap(tap.data(), 48000) > 0);
        engine.stop();
        QVERIFY(!engine.playing());
        const double at = engine.position();
        QVERIFY(at > 1.0 && at < 6.0);

        // The end of the input stops it.
        QSignalSpy ended(&engine, &PlaybackEngine::ended);
        QVERIFY(engine.play(5.7));
        QVERIFY(ended.wait(5000));
        QVERIFY(!engine.playing());
    }
};

QTEST_MAIN(TestPipeline)
#include "tst_pipeline.moc"
