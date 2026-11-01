#include "App.h"
#include <QImage>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <caps/CapabilityProbe.h>
#include <capture/Capture.h>
#include <cmath>
#include <command/CommandBuilder.h>
#include <compare/Comparer.h>
#include <probe/Prober.h>
#include <render/RenderQueue.h>
#include <tools/FfTools.h>
using namespace ffgui;
class TestToolingPipeline : public QObject
{
    Q_OBJECT
    QTemporaryDir dir;
    QString input;
    BuildInfo caps;
    Catalog catalog = Catalog::builtIn();
    QVariantMap track{
        {"codec", "pcm_s16le"}, {"rate", 48000}, {"depth", 16}, {"channels", 2}, {"audioIndex", 0}};
    bool run(const QStringList &args)
    {
        QProcess p;
        p.start(FfTools::paths().ffmpeg, args);
        const bool ok = p.waitForFinished(20000) && p.exitCode() == 0;
        if (!ok)
            qWarning().noquote() << p.readAllStandardError();
        return ok;
    }
    ProbeResult probe(const QString &path)
    {
        Prober job(path);
        QSignalSpy done(&job, &Prober::finished);
        job.start();
        if (!done.wait(10000) || !done[0][0].toBool())
            return {};
        return done[0][1].value<ProbeResult>();
    }
    bool render(const BuiltCommand &command)
    {
        if (!command.error.isEmpty()) {
            qWarning().noquote() << command.error;
            return false;
        }
        RenderJob job(command, nullptr);
        QSignalSpy done(&job, &RenderJob::finished);
        job.start();
        if (done.isEmpty() && !done.wait(20000))
            return false;
        if (!done[0][0].toBool())
            qWarning().noquote() << done[0][2].toString();
        return done[0][0].toBool();
    }
    QVariantMap compare(const CompareSpec &spec)
    {
        Comparer job(nullptr);
        QSignalSpy done(&job, &Comparer::finished);
        job.start(spec);
        if (!done.wait(20000))
            return {{"error", "Timeout"}};
        return done[0][0].toMap();
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
        QVERIFY(run({"-v", "error", "-f", "lavfi", "-i",
                     "anoisesrc=r=48000:d=4:amplitude=0.2:seed=7,volume='0.1+0.9*mod(t,1)':eval=frame", "-ac",
                     "2", input}));
    }
    void metadataRenders()
    {
        const QVariantMap edits{
            {"tags",
             QVariantMap{{"title", "Edited = ; # title"}, {"artist", "Author"}, {"custom-key", "Custom"}}},
            {"chapters", QVariantList{QVariantMap{{"start", .25}, {"end", 1.5}, {"title", "One"}},
                                      QVariantMap{{"start", 1.5}, {"end", 3.8}, {"title", "Two"}}}},
            {"tracks",
             QVariantMap{{"0", QVariantMap{{"title", "Main track"}, {"lang", "fra"}, {"isDefault", true}}}}}};
        auto format = track;
        format.insert("metadataEdits", edits);
        QVariantMap out{{"container", "mka"},
                        {"codec", "flac"},
                        {"folder", dir.path()},
                        {"pattern", "edited"},
                        {"metadata", "edit"}};
        auto command = CommandBuilder::build(input, 0, format, 4, {}, {out}, catalog, caps);
        QVERIFY(render(command));
        const auto result = probe(command.paths.first());
        QCOMPARE(result.chapters.size(), 2);
        QVERIFY(std::abs(result.chapters[0].start - .25) < .001);
        QCOMPARE(result.chapters[1].title, QString("Two"));
        QCOMPARE(result.tracks[0].title, QString("Main track"));
        QCOMPARE(result.tracks[0].language, QString("fra"));
        QVERIFY(result.tracks[0].isDefault);
        auto tags = result.tags;
        QVERIFY(tags.value("title", tags.value("TITLE")).toString() == u"Edited = ; # title");
        QCOMPARE(tags.value("CUSTOM-KEY", tags.value("custom-key")).toString(), QString("Custom"));
        // Removing every tag and chapter must not let FFmpeg copy them back.
        format.insert("metadataEdits", QVariantMap{{"tags", QVariantMap()}, {"chapters", QVariantList()}});
        out.insert("pattern", "empty-tags");
        command = CommandBuilder::build(command.paths.first(), 0, format, 4, {}, {out}, catalog, caps);
        QVERIFY(render(command));
        const auto empty = probe(command.paths.first());
        QVERIFY(empty.chapters.isEmpty());
        QVERIFY(!empty.tags.contains("title") && !empty.tags.contains("TITLE"));
    }
    void coverAndProjectEdits()
    {
        App app(nullptr);
        app.project()->addFiles({QUrl::fromLocalFile(input)});
        QTRY_VERIFY_WITH_TIMEOUT(app.project()->probe(0), 10000);
        auto *project = app.project();
        project->setTag("title", "New title");
        project->setTag("custom", "Value");
        project->removeTag("custom");
        QVERIFY(!project->tags().contains("custom"));
        project->undoStack()->undo();
        QCOMPARE(project->tags().value("custom").toString(), QString("Value"));
        project->addChapter(.5, 2., "Chapter");
        project->setChapter(0, .6, 2.1, "Changed");
        project->setTrackMetadata(0, {{"title", "Voice"}, {"lang", "eng"}, {"isDefault", true}});
        const QString image = dir.filePath("cover.png");
        QImage cover(32, 32, QImage::Format_RGB32);
        cover.fill(Qt::red);
        QVERIFY(cover.save(image));
        project->setCoverArt(QUrl::fromLocalFile(image));
        QVERIFY(project->cover().startsWith("data:image/png;base64,"));
        const auto exported = QUrl::fromLocalFile(dir.filePath("extracted.png"));
        project->extractCover(exported);
        QCOMPARE(QImage(exported.toLocalFile()).pixelColor(0, 0), QColor(Qt::red));
        project->removeCover();
        QVERIFY(project->cover().isEmpty());
        project->undoStack()->undo();
        QVERIFY(!project->cover().isEmpty());
        const auto saved = QUrl::fromLocalFile(dir.filePath("edits.ffgui"));
        QVERIFY(project->save(saved));
        const auto expected = project->outputMetadata();
        project->newProject();
        QVERIFY(project->open(saved));
        QTRY_VERIFY_WITH_TIMEOUT(project->probe(0), 10000);
        QCOMPARE(project->outputMetadata(), expected);
        auto format = track;
        format.insert("metadataEdits", project->outputMetadata());
        QVariantMap out{{"container", "flac"},
                        {"codec", "flac"},
                        {"folder", dir.path()},
                        {"pattern", "cover-replaced"},
                        {"metadata", "edit"}};
        auto command = CommandBuilder::build(input, 0, format, 4, {}, {out}, catalog, caps);
        QVERIFY(render(command));
        const auto result = probe(command.paths.first());
        QVERIFY(result.coverStream >= 0);
        QCOMPARE(result.coverWidth, 32);
        project->removeCover();
        format.insert("coverStream", result.coverStream);
        format.insert("metadataEdits", project->outputMetadata());
        out.insert("pattern", "cover-removed");
        command = CommandBuilder::build(command.paths.first(), 0, format, 4, {}, {out}, catalog, caps);
        QVERIFY(render(command));
        QCOMPARE(probe(command.paths.first()).coverStream, -1);
    }
    void comparison()
    {
        auto result = compare({input, input});
        QVERIFY2(result.value("error").toString().isEmpty(), qPrintable(result.value("error").toString()));
        QVERIFY(result.value("equal").toBool());
        QVERIFY(std::isinf(result.value("sdr").toDouble()));
        QVERIFY(!result.value("traceA").toList().isEmpty());
        result = compare({input, input, {}, "volume=0.5"});
        QVERIFY2(result.value("error").toString().isEmpty(), qPrintable(result.value("error").toString()));
        QVERIFY(!result.value("equal").toBool());
        QVERIFY(std::abs(result.value("sdr").toDouble() - 6.0206) < .01);
        QVERIFY(std::abs(result.value("peakChange").toDouble() + 6.0206) < .01);
        const QString delayed = dir.filePath("delayed.wav");
        QVERIFY(run({"-v", "error", "-i", input, "-af", "adelay=200:all=1", delayed}));
        result = compare({input, delayed});
        QVERIFY2(result.value("error").toString().isEmpty(), qPrintable(result.value("error").toString()));
        QVERIFY(std::abs(result.value("suggestedOffset").toDouble() - .2) < .02);
        result = compare({input, delayed, {}, {}, 0, 0, .2});
        QVERIFY(result.value("equal").toBool());
    }
    void recordLimitsSplitAndStop()
    {
        for (int mode = 0; mode < 4; ++mode) {
            CaptureSpec spec;
            spec.backend = "lavfi";
            spec.device = "sine=f=440:r=48000";
            spec.output = dir.filePath(QString("record-%1.wav").arg(mode));
            spec.seconds = mode == 0 ? 1.2 : mode == 1 ? 1.4 : 0;
            if (mode == 1) {
                spec.output = dir.filePath("split_%03d.wav");
                spec.splitSeconds = .5;
            }
            if (mode == 2)
                spec.maxBytes = 48000;
            Recorder recorder(nullptr);
            QSignalSpy done(&recorder, &Recorder::finished), levels(&recorder, &Recorder::reading);
            recorder.start(spec);
            if (mode == 3) {
                QTRY_VERIFY_WITH_TIMEOUT(levels.count() > 3, 5000);
                recorder.stop();
            }
            if (done.isEmpty())
                QVERIFY2(done.wait(8000),qPrintable(QString("Recording mode %1 did not stop").arg(mode)));
            QVERIFY2(done[0][0].toBool(), qPrintable(done[0][2].toString()));
            const auto files = done[0][1].toStringList();
            QVERIFY(!files.isEmpty());
            if (mode == 1)
                QVERIFY(files.size() >= 2);
            for (const auto &file : files)
                QVERIFY(!probe(file).tracks.isEmpty());
            QVERIFY(levels.size() > 0);
            QVERIFY(levels.last()[0].toDouble() > 0);
            QVERIFY(levels.last()[1].toDouble() > -30);
            if (mode == 0) {
                QVERIFY(std::abs(probe(files.first()).duration - 1.2) < .02);
                QSignalSpy refused(&recorder, &Recorder::finished);
                recorder.start(spec);
                QCOMPARE(refused.count(), 1);
                QVERIFY(!refused[0][0].toBool());
            }
        }
    }
    void hlsStream()
    {
        const QString playlist = dir.filePath("local.m3u8");
        StreamOut stream({input, "volume=0.5", "hls", playlist, 0, 96}, nullptr);
        QSignalSpy done(&stream, &StreamOut::finished), rates(&stream, &StreamOut::reading);
        stream.start();
        QVERIFY(done.wait(12000));
        QVERIFY2(done[0][0].toBool(), qPrintable(done[0][2].toString()));
        QFile file(playlist);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.readAll().contains("#EXT-X-ENDLIST"));
        QVERIFY(!probe(playlist).tracks.isEmpty());
        double bitrate = 0;
        for (const auto &r : rates)
            bitrate = std::max(bitrate, r[0].toDouble());
        QVERIFY(bitrate > 0);
        StreamOut stop({input, {}, "hls", dir.filePath("stopped.m3u8"), 0, 128}, nullptr);
        QSignalSpy stopped(&stop, &StreamOut::finished), progress(&stop, &StreamOut::reading);
        stop.start();
        QTRY_VERIFY_WITH_TIMEOUT(progress.size() > 1, 5000);
        stop.stop();
        if (stopped.isEmpty())
            QVERIFY(stopped.wait(5000));
        QVERIFY(stopped[0][1].toBool());
    }
    void captureAndStreamFacades()
    {
        App app(nullptr);
        app.capture()->setOutput(dir.filePath("facade.wav"));
        app.capture()->start({{"backend", "lavfi"}, {"device", "sine=f=220:r=48000"}, {"seconds", 0.3}});
        QTRY_VERIFY_WITH_TIMEOUT(!app.capture()->active(), 5000);
        QVERIFY2(app.capture()->error().isEmpty(), qPrintable(app.capture()->error()));
        QTRY_VERIFY_WITH_TIMEOUT(app.project()->probe(0), 10000);
        QCOMPARE(app.project()->inputs()->count(), 1);
        app.transcript()->addSegment(0, .2, "First source");
        app.project()->setTag("title", "Recorded");
        app.project()->addFiles({QUrl::fromLocalFile(input)});
        QTRY_VERIFY_WITH_TIMEOUT(app.project()->probe(1), 10000);
        app.project()->setTag("title", "Other source");
        QCOMPARE(app.project()->outputMetadata(0).value("tags").toMap().value("title").toString(),
                 QString("Recorded"));
        QCOMPARE(app.transcript()->count(), 0);
        app.project()->setCurrentInput(0);
        QCOMPARE(app.transcript()->value(0, "text").toString(), QString("First source"));
        app.project()->setCurrentInput(1);
        app.stream()->add("Test HLS", "hls", dir.filePath("facade.m3u8"), 96);
        QCOMPARE(app.destinations()->value(0, "status").toString(), QString("idle"));
        app.stream()->start(0);
        QTRY_COMPARE_WITH_TIMEOUT(app.destinations()->value(0, "status").toString(), QString("live"), 5000);
        QVERIFY(!app.destinations()->value(0, "history").toList().isEmpty());
        app.stream()->stopAll();
        QTRY_COMPARE_WITH_TIMEOUT(app.destinations()->value(0, "status").toString(), QString("stopped"),
                                  5000);
        app.stream()->remove(0);
        QCOMPARE(app.destinations()->count(), 0);
        QSignalSpy stopped(app.console(), &ConsoleSession::finished);
        app.console()->run("ffmpeg -re -f lavfi -i anullsrc -f null -");
        QVERIFY(app.console()->active());
        QTest::qWait(150);
        app.console()->stop();
        if (stopped.isEmpty())
            QVERIFY(stopped.wait(5000));
        QVERIFY(!app.console()->active());
        DeviceList devices(nullptr);
        QSignalSpy found(&devices, &DeviceList::finished);
        devices.refresh();
        QVERIFY(found.wait(5000));
    }
    void consoleTranscriptGeneratorAndScopes()
    {
        App app(nullptr);
        app.transport()->setVolume(0);
        app.project()->addFiles({QUrl::fromLocalFile(input)});
        QTRY_VERIFY_WITH_TIMEOUT(app.project()->probe(0), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(app.scopes()->ready(), 15000);
        QCOMPARE(app.scopes()->values().value("spectrum").toList().size(), 64);
        // Scope refreshes must leave the Media View's deep-zoom PCM window intact.
        app.media()->requestWindow(1, 2);
        QTRY_VERIFY_WITH_TIMEOUT(app.media()->data()->windowCovers(1, 2), 5000);
        app.transport()->seek(2.4);
        QTest::qWait(150);
        QVERIFY(app.media()->data()->windowCovers(1, 2));
        const auto before = app.project()->recipe().nodes;
        QSignalSpy done(app.console(), &ConsoleSession::finished);
        app.runCommand("ffmpeg -version");
        QVERIFY(done.wait(5000));
        QVERIFY(done[0][0].toBool());
        QCOMPARE(app.project()->recipe().nodes, before);
        app.runCommand("ffmpeg -version | cat");
        QVERIFY(!app.console()->active());
        QVERIFY(!app.console()->error().isEmpty());
        auto *doc = app.transcript();
        doc->addSegment(.1, .5, "Hello");
        QVERIFY(doc->setSegment(0, .2, .7, "Edited"));
        for (const QString &format : {"srt", "json", "txt"}) {
            const auto file = QUrl::fromLocalFile(dir.filePath("transcript." + format));
            QVERIFY(doc->exportFile(file, format));
            doc->removeSegment(0);
            QVERIFY(doc->importFile(file));
            QCOMPARE(doc->value(0, "text").toString(), QString("Edited"));
        }
        if (!caps.filters.contains("whisper")) {
            doc->transcribe("missing.bin");
            QVERIFY(!doc->active());
            QVERIFY(doc->error().contains("whisper"));
        }
        app.project()->addGenerator("sine=f=880:r=48000:d=0.25", "Generated tone");
        QTRY_VERIFY_WITH_TIMEOUT(!app.project()->generating(), 10000);
        QCOMPARE(app.project()->inputs()->count(), 2);
        const QString generated = app.project()->inputs()->value(1, "path").toString();
        QTRY_VERIFY_WITH_TIMEOUT(app.project()->probe(1), 10000);
        QVERIFY(std::abs(app.project()->probe(1)->duration - .25) < .01);
        QCOMPARE(app.project()->inputs()->value(1, "kind").toString(), QString("generator"));
        app.project()->undoStack()->undo();
        QCOMPARE(app.project()->inputs()->count(), 1);
        QFile::remove(generated);
    }
};
QTEST_MAIN(TestToolingPipeline)
#include "tst_tooling_pipeline.moc"
