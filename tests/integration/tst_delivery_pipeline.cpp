#include "App.h"
#include <analysis/AnalysisPass.h>
#include <caps/CapabilityProbe.h>
#include <command/GraphProbe.h>
#include <command/CommandBuilder.h>
#include <media/PcmReader.h>
#include <playback/PlaybackEngine.h>
#include <probe/Prober.h>
#include <render/RenderQueue.h>
#include <tools/FfTools.h>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QTest>
#include <QStandardPaths>
#include <cmath>

using namespace ffgui;
class TestDeliveryPipeline : public QObject
{
    Q_OBJECT
    QTemporaryDir dir;
    QString input;
    BuildInfo caps;
    QVariantMap track{{"codec", "pcm_s16le"}, {"rate", 48000}, {"depth", 16}, {"channels", 2}, {"audioIndex", 0}};
    Catalog catalog = Catalog::builtIn();
    static bool run(const QStringList &args)
    {
        QProcess p;
        p.start(FfTools::paths().ffmpeg, args);
        return p.waitForFinished(20000) && p.exitCode() == 0;
    }
    static ProbeResult probe(const QString &path)
    {
        Prober p(path);
        QSignalSpy done(&p, &Prober::finished);
        p.start();
        if (!done.wait(10000) || !done[0][0].toBool())
            return {};
        return done[0][1].value<ProbeResult>();
    }
    QVariantMap output(const PresetSpec &p, const QString &pattern)
    {
        return {{"codec", p.codec}, {"container", p.container}, {"quality", p.quality}, {"rate", p.rate},
                {"depth", p.depth}, {"channels", p.channels},   {"folder", dir.path()}, {"pattern", pattern}};
    }
    bool render(const BuiltCommand &command)
    {
        RenderJob job(command, nullptr);
        QSignalSpy done(&job, &RenderJob::finished);
        job.start();
        if (done.isEmpty() && !done.wait(20000))
            return false;
        if (!done[0][0].toBool())
            qWarning().noquote() << done[0][2].toString();
        return done[0][0].toBool();
    }
private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        if (FfTools::paths().ffmpeg.isEmpty())
            QSKIP("ffmpeg unavailable");
        QString error;
        QVERIFY2(CapabilityProbe::detect(FfTools::paths(), &caps, &error), qPrintable(error));
        input = dir.filePath("source.wav");
        QVERIFY(run({"-v", "error", "-f", "lavfi", "-i", "anoisesrc=color=pink:amplitude=0.25:r=48000:d=4:seed=7",
                     "-ac", "2", "-c:a", "pcm_s16le", input}));
    }
    void presetsRender()
    {
        for (const auto &p : catalog.presets) {
            if (p.kind != u"output")
                continue;
            if (!caps.audioEncoders.contains(p.codec))
                continue;
            const auto command = CommandBuilder::build(input, 0, track, 4, {}, {output(p, p.name)}, catalog, caps);
            QVERIFY2(command.error.isEmpty(), qPrintable(command.error));
            QVERIFY2(render(command), qPrintable(p.name));
            const auto result = probe(command.paths.first());
            QVERIFY2(!result.tracks.isEmpty(), qPrintable(p.name));
            QCOMPARE(result.tracks[0].codec, catalog.codec(p.codec)->codec);
            QCOMPARE(result.tracks[0].sampleRate, p.rate > 0 ? p.rate : 48000);
            QCOMPARE(result.tracks[0].channels, p.channels > 0 ? p.channels : 2);
        }
    }
    void compositePresetsAndMultiOutput()
    {
        for (const auto &p : catalog.presets) {
            if (p.kind != u"chain")
                continue;
            QList<QVariantMap> nodes;
            for (const auto &s : p.steps)
                nodes << QVariantMap{
                    {"uid", QStringLiteral("s%1").arg(nodes.size())}, {"type", s.type}, {"params", s.params}};
            auto out = output(*catalog.preset("Archive FLAC"), p.name);
            const auto command = CommandBuilder::build(input, 0, track, 4, nodes, {out}, catalog, caps);
            QVERIFY2(render(command), qPrintable(p.name));
        }
        auto a = output(*catalog.preset("Archive FLAC"), "multi-flac"),
             b = output(*catalog.preset("Podcast MP3"), "multi-mp3");
        auto command =
            CommandBuilder::build(input, 0, track, 4, {{{"uid", "gain"}, {"type", "gain"}}}, {a, b}, catalog, caps);
        QVERIFY(render(command));
        QCOMPARE(command.paths.size(), 2);
        for (const auto &path : command.paths)
            QVERIFY(!probe(path).tracks.isEmpty());
    }
    void loudnorm()
    {
        const QList<QVariantMap> nodes{{{"uid", "norm"}, {"type", "loudnorm"}, {"params", QVariantMap{{"I", -16}}}}};
        const auto command = CommandBuilder::build(
            input, 0, track, 4, nodes, {output(*catalog.preset("Archive FLAC"), "normalized")}, catalog, caps);
        QVERIFY(render(command));
        AnalysisPass::Options o;
        o.input = command.paths.first();
        o.duration = 4;
        o.channels = 2;
        AnalysisPass pass(o);
        QSignalSpy done(&pass, &AnalysisPass::finished);
        pass.start();
        QVERIFY(done.wait(20000));
        const auto result = done[0][0].value<AnalysisResult>();
        QVERIFY(result.ok);
        QVERIFY2(std::abs(result.loudness.integrated + 16) <= 1,
                 qPrintable(QString::number(result.loudness.integrated)));
    }
    void stoppedRenderIsValid()
    {
        auto command = CommandBuilder::build(input, 0, track, 4, {},
                                             {output(*catalog.preset("Archive FLAC"), "stopped")}, catalog, caps);
        command.args.insert(command.args.indexOf("-i"), "-re");
        RenderJob job(command, nullptr);
        QSignalSpy progress(&job, &RenderJob::progress), done(&job, &RenderJob::finished);
        job.start();
        QTRY_VERIFY_WITH_TIMEOUT(progress.count() > 1, 10000);
        job.stop();
        if (done.isEmpty())
            QVERIFY(done.wait(10000));
        QVERIFY(done[0][1].toBool());
        const auto result = probe(command.paths.first());
        QVERIFY(!result.tracks.isEmpty());
        QVERIFY(result.duration > 0);
        QVERIFY(result.duration < 4);
    }
    void graph()
    {
        GraphProbe graph;
        QSignalSpy done(&graph, &GraphProbe::finished);
        graph.run(input, 0, "volume@live=0.5", {{"rate", 44100}, {"codec", "flac"}});
        QVERIFY(done.wait(15000));
        const auto result = done[0][0].value<NegotiatedGraph>();
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        bool converter = false;
        for (const auto &n : result.nodes)
            converter |= n.value("kind").toString() == u"auto";
        QVERIFY(converter);
    }
    void liveCommand()
    {
        ProcessJob job(FfTools::paths().ffmpeg, {"-hide_banner", "-loglevel", "level+error", "-re", "-f", "lavfi", "-i",
                                                 "sine=f=1000:r=48000:d=3", "-af", "volume@live=0.5", "-c:a",
                                                 "pcm_f32le", "-f", "f32le", "pipe:1"});
        QByteArray data;
        connect(&job, &ProcessJob::stdoutReady, this, [&] { data += job.readStdout(); });
        QSignalSpy done(&job, &ProcessJob::finished);
        job.start();
        QTRY_VERIFY_WITH_TIMEOUT(data.size() >= 48000 * 4, 5000);
        QVERIFY(job.writeStdin("cvolume@live -1 volume 0.05\n") > 0);
        if (done.isEmpty())
            QVERIFY(done.wait(7000));
        QVERIFY(job.succeeded());
        QVERIFY(data.size() >= 48000 * 4 * 2);
        const float *samples = reinterpret_cast<const float *>(data.constData());
        double before = 0, after = 0;
        for (int i = 10000; i < 30000; ++i)
            before = std::max(before, double(std::abs(samples[i])));
        for (int i = 100000; i < 120000; ++i)
            after = std::max(after, double(std::abs(samples[i])));
        QVERIFY2(std::abs(20 * std::log10(after / before) + 20) < 0.1,
                 qPrintable(QString::number(20 * std::log10(after / before))));
    }
    void playbackAbAndRestart()
    {
        if (!PlaybackEngine::hasOutputDevice())
            QSKIP("No audio output device");
        PlaybackEngine player;
        player.setVolume(0); // Every audio integration test is silent.
        player.setSource({input, 0, 2, 48000, {"FL", "FR"}, 4});
        player.setProcessedFilter("volume@ab=0.1");
        player.setProcessed(false);
        QVERIFY(player.play(0));
        QTRY_VERIFY_WITH_TIMEOUT(player.framesPlayed() > 4800, 5000);
        std::vector<float> tap(48000 * 2);
        player.takeTap(tap.data(), 48000);
        QTest::qWait(150);
        auto peak = [&] {
            const qint64 frames = player.takeTap(tap.data(), 48000);
            double value = 0;
            for (qint64 i = 0; i < frames * 2; ++i)
                value = std::max(value, double(std::abs(tap[size_t(i)])));
            return value;
        };
        const double original = peak();
        QVERIFY(original > 0);
        const qint64 before = player.framesPlayed();
        player.setProcessed(true);
        QTest::qWait(200);
        QVERIFY(player.framesPlayed() > before);
        const double processed = peak();
        QVERIFY(processed > 0 && processed < original * 0.2);
        QSignalSpy playing(&player, &PlaybackEngine::playingChanged);
        const double at = player.position();
        player.setProcessedFilter("volume@ab=0.05");
        QVERIFY(player.playing());
        QCOMPARE(playing.size(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(player.position() > at + 0.1, 3000);
        QVERIFY(player.sendLiveCommand("volume@ab", "volume", "0.01"));
        QTest::qWait(1200);
        peak();
        QTest::qWait(100);
        const double live = peak();
        QVERIFY2(live > 0 && live < original * 0.03, qPrintable(QString::number(live / original)));
        player.stop();
    }
    void selectionAndDepth()
    {
        auto out = output(*catalog.preset("Archive FLAC"), "selection");
        out.insert("depth", 24);
        auto command = CommandBuilder::build(input, 0, track, 4, {}, {out}, catalog, caps, 1, 2);
        QVERIFY(render(command));
        const auto selected = probe(command.paths.first());
        QVERIFY(!selected.tracks.isEmpty());
        QVERIFY(std::abs(selected.duration - 1) < 0.01);
        QCOMPARE(selected.tracks[0].bitsPerSample, 24);
        out.insert("pattern", "cut");
        QList<QVariantMap> cut{
            {{"uid", "cut"}, {"type", "trim"}, {"params", QVariantMap{{"start", 1}, {"end", 2}, {"cut", true}}}}};
        command = CommandBuilder::build(input, 0, track, 4, cut, {out}, catalog, caps);
        QVERIFY(render(command));
        QVERIFY(std::abs(probe(command.paths.first()).duration - 3) < 0.06);
    }
    void coverCopyAndStrip()
    {
        if (!caps.audioEncoders.contains("libmp3lame")) QSKIP("MP3 encoder unavailable");
        const QString source = dir.filePath("with-cover.mp3");
        QVERIFY(run({ "-v", "error", "-i", input, "-f", "lavfi", "-i", "color=c=blue:s=16x16:d=0.04",
                      "-map", "0:a", "-map", "1:v", "-c:a", "libmp3lame", "-c:v", "png", "-frames:v", "1",
                      "-disposition:v", "attached_pic", source }));
        const auto metadata = probe(source);
        QVERIFY(metadata.coverStream >= 0);
        auto format = track;
        format.insert("codec", "mp3");
        format.insert("coverStream", metadata.coverStream);
        auto out = output(*catalog.preset("Archive FLAC"), "cover-copy");
        auto command = CommandBuilder::build(source, 0, format, metadata.duration, {}, {out}, catalog, caps);
        QVERIFY(render(command));
        QVERIFY(probe(command.paths.first()).coverStream >= 0);
        out.insert("pattern", "cover-strip"); out.insert("metadata", "strip");
        command = CommandBuilder::build(source, 0, format, metadata.duration, {}, {out}, catalog, caps);
        QVERIFY(render(command));
        QCOMPARE(probe(command.paths.first()).coverStream, -1);
    }
    void queueActions()
    {
        RenderQueue queue(nullptr);
        queue.setPaused(true);
        auto a = output(*catalog.preset("Archive FLAC"), "queued-a"),
             b = output(*catalog.preset("Podcast MP3"), "queued-b");
        auto tasks = BatchExpander::expand({input}, track, 4, {}, {a, b}, catalog, caps);
        QCOMPARE(tasks.size(), 2);
        queue.add(tasks);
        QCOMPARE(queue.tasks()[0].status, QStringLiteral("queued"));
        queue.action("down", 0);
        QCOMPARE(queue.tasks()[0].uid, tasks[1].uid);
        queue.action("cancel", 1);
        QCOMPARE(queue.tasks()[1].status, QStringLiteral("cancelled"));
        queue.setPaused(false);
        QTRY_COMPARE_WITH_TIMEOUT(queue.tasks()[0].status, QStringLiteral("done"), 10000);
        queue.action("clearFinished", -1);
        QVERIFY(queue.tasks().isEmpty());
    }
    void queueRetriesItsStoppedOutput()
    {
        RenderQueue queue(nullptr);
        auto tasks = BatchExpander::expand({input}, track, 4, {},
                                           {output(*catalog.preset("Archive FLAC"), "queue-retry")}, catalog, caps);
        tasks[0].command.args.insert(tasks[0].command.args.indexOf("-i"), "-re");
        queue.add(tasks);
        QTRY_VERIFY_WITH_TIMEOUT(queue.tasks()[0].progress > 0.1, 5000);
        queue.stopAll();
        QTRY_COMPARE_WITH_TIMEOUT(queue.tasks()[0].status, QStringLiteral("stopped"), 5000);
        QVERIFY(!probe(queue.tasks()[0].output).tracks.isEmpty());
        queue.action("retry", 0);
        QTRY_COMPARE_WITH_TIMEOUT(queue.tasks()[0].status, QStringLiteral("done"), 10000);
        QVERIFY(std::abs(probe(queue.tasks()[0].output).duration - 4) < 0.01);
    }
    void closingQueueFinishesItsFiles()
    {
        auto queue = std::make_unique<RenderQueue>(nullptr);
        auto tasks = BatchExpander::expand({input}, track, 4, {}, {output(*catalog.preset("Archive FLAC"), "queue-close")}, catalog, caps);
        tasks[0].command.args.insert(tasks[0].command.args.indexOf("-i"), "-re");
        queue->add(tasks);
        QTRY_VERIFY_WITH_TIMEOUT(queue->tasks()[0].progress > 0.1, 5000);
        const QString path = queue->tasks()[0].output;
        queue.reset();
        QVERIFY(!probe(path).tracks.isEmpty());
    }
    void closingAppWithBackgroundWork()
    {
        auto app = std::make_unique<App>(nullptr);
        app->project()->addFiles({QUrl::fromLocalFile(input)});
        QTRY_VERIFY_WITH_TIMEOUT(app->project()->probe(0) != nullptr, 10000);
        // Decoding, analysis and capability/help jobs may still be active.
        app.reset();
    }
    void backendWorkflow()
    {
        App app(nullptr);
        app.project()->addFiles({QUrl::fromLocalFile(input)});
        QTRY_VERIFY_WITH_TIMEOUT(app.project()->hasInput() && !app.project()->input().value("probing").toBool(), 15000);
        app.project()->addStep("gain", {{"volume", -6}});
        app.project()->addOutput("Archive FLAC");
        app.project()->setOutput(0, {{"folder", dir.path()}, {"pattern", "backend"}});
        QTRY_VERIFY_WITH_TIMEOUT(app.command()->valid(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(app.media()->processed()->ready() && !app.media()->processed()->loading(), 10000);
        QVERIFY(app.media()->processed()->error().isEmpty());
        const double original = app.media()->levelAt(0, 1, 0.1),
                     processed = app.media()->processed()->levelAt(0, 1, 0.1);
        QVERIFY(std::abs(original - processed - 6) < 0.1);
        app.render()->start();
        QTRY_VERIFY_WITH_TIMEOUT(app.queue()->count() > 0, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(app.queue()->value(0, "status").toString(), QStringLiteral("done"), 20000);
        QVERIFY(!probe(dir.filePath("backend.flac")).tracks.isEmpty());
        const auto projectFile = QUrl::fromLocalFile(dir.filePath("workflow.ffgui"));
        QVERIFY(app.project()->save(projectFile));
        app.project()->newProject();
        QVERIFY(!app.project()->hasInput());
        QCOMPARE(app.project()->graph()->count(), 0);
        QVERIFY(app.project()->converter().isEmpty());
        QVERIFY(app.project()->open(projectFile));
        QTRY_VERIFY_WITH_TIMEOUT(app.project()->probe(0) != nullptr && app.command()->valid(), 10000);
        QVERIFY(!app.project()->modified());
    }
};
QTEST_MAIN(TestDeliveryPipeline)
#include "tst_delivery_pipeline.moc"
