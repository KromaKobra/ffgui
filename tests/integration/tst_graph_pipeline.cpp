#include "App.h"
#include <QSignalSpy>
#include <QStandardPaths>
#include <QImage>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>
#include <cmath>
#include <command/CommandBuilder.h>
#include <graph/GraphCompiler.h>
#include <media/PcmReader.h>
#include <playback/PlaybackEngine.h>
#include <probe/Prober.h>
#include <render/RenderQueue.h>
#include <tools/FfTools.h>
using namespace ffgui;
class TestGraphPipeline : public QObject
{
    Q_OBJECT
    QTemporaryDir dir;
    QString a, b, ir;
    Catalog catalog = Catalog::builtIn();
    QList<QVariantMap> nodes(const QString &type)
    {
        return {{{"uid", "a"}, {"kind", "source"}, {"path", a}, {"duration", 2.}, {"channels", 2}},
                {{"uid", "b"},
                 {"kind", "source"},
                 {"path", type == u"convolve" ? ir : b},
                 {"duration", type == u"convolve" ? .1 : 2.},
                 {"channels", 2}},
                {{"uid", "op"}, {"kind", "step"}, {"type", type}, {"params", catalog.step(type)->defaults()}},
                {{"uid", "out"}, {"kind", "output"}, {"outputUid", "o1"}}};
    }
    QList<QVariantMap> links(bool two = true)
    {
        QList<QVariantMap> links{{{"from", "a"}, {"to", "op"}, {"fromPort", 0}, {"toPort", 0}},
                                 {{"from", "op"}, {"to", "out"}, {"fromPort", 0}, {"toPort", 0}}};
        if (two)
            links << QVariantMap{{"from", "b"}, {"to", "op"}, {"fromPort", 0}, {"toPort", 1}};
        return links;
    }
    bool run(const QStringList &args)
    {
        QProcess p;
        p.start(FfTools::paths().ffmpeg, args);
        bool ok = p.waitForFinished(20000) && p.exitCode() == 0;
        if (!ok)
            qWarning().noquote() << p.readAllStandardError();
        return ok;
    }
    ProbeResult probe(const QString &file)
    {
        Prober p(file);
        QSignalSpy done(&p, &Prober::finished);
        p.start();
        if (!done.wait(10000) || !done[0][0].toBool())
            return {};
        return done[0][1].value<ProbeResult>();
    }
    bool render(const BuiltCommand &command)
    {
        RenderJob job(command, nullptr);
        QSignalSpy done(&job, &RenderJob::finished);
        job.start();
        if (done.isEmpty() && !done.wait(10000))
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
        a = dir.filePath("a.wav");
        b = dir.filePath("b.wav");
        ir = dir.filePath("ir.wav");
        QVERIFY(run({"-v", "error", "-f", "lavfi", "-i", "sine=f=440:r=48000:d=2", "-ac", "2", a}));
        QVERIFY(run({"-v", "error", "-f", "lavfi", "-i", "sine=f=880:r=48000:d=2", "-ac", "2", b}));
        QVERIFY(run({"-v", "error", "-f", "lavfi", "-i", "aevalsrc=if(eq(n\\,0)\\,1\\,0):s=48000:d=0.1",
                     "-ac", "2", ir}));
    }
    void validation()
    {
        auto n = nodes("amix");
        auto l = links();
        auto p = GraphCompiler::compile(n, l, catalog);
        QVERIFY2(p.error.isEmpty(), qPrintable(p.error));
        QVERIFY(p.filter.contains("amix=inputs=2"));
        QCOMPARE(p.inputArgs.count("-i"), 2);
        QVERIFY(!GraphCompiler::linear(n, l));
        l << QVariantMap{{"from", "op"}, {"to", "op"}, {"fromPort", 0}, {"toPort", 0}};
        QVERIFY(!GraphCompiler::compile(n, l, catalog).error.isEmpty());
        l = links();
        l[0]["fromPort"] = 8;
        QVERIFY(GraphCompiler::compile(n, l, catalog).error.contains("port"));
        l = links();
        l.removeLast();
        QVERIFY(GraphCompiler::compile(n, l, catalog).error.contains("Connect input"));
        n = nodes("split");
        l = links(false);
        l << QVariantMap{{"from", "op"}, {"to", "out"}, {"fromPort", 1}, {"toPort", 0}};
        QVERIFY(GraphCompiler::compile(n, l, catalog).error.contains("one connection"));
    }
    void operations_data()
    {
        QTest::addColumn<QString>("type");
        for (const QString &t :
             {"amix", "duck", "crossfade", "merge", "split", "channelsplit", "crossover", "convolve"})
            QTest::newRow(qPrintable(t)) << t;
    }
    void operations()
    {
        QFETCH(QString, type);
        auto n = nodes(type);
        auto l = links(GraphCompiler::inputs(n[2]) > 1);
        auto p = GraphCompiler::compile(n, l, catalog);
        QVERIFY2(p.error.isEmpty(), qPrintable(p.error));
        const QString out = dir.filePath(type + ".wav");
        QVariantMap format{
            {"codec", "pcm_s16le"}, {"rate", 48000}, {"channels", 2}, {"graphPlan", p.variant()}};
        QVariantMap output{{"uid", "o1"},
                           {"codec", "pcm_s16le"},
                           {"container", "wav"},
                           {"folder", dir.path()},
                           {"pattern", type}};
        auto command = CommandBuilder::build(a, 0, format, 2, {}, {output}, catalog, {});
        QVERIFY2(command.error.isEmpty(), qPrintable(command.error));
        QVERIFY(render(command));
        const auto info = probe(out);
        QVERIFY(!info.tracks.isEmpty());
        const double expected = type == u"crossfade" ? 3.5 : 2.;
        QVERIFY2(std::abs(info.duration - expected) < .05,
                 qPrintable(QStringLiteral("%1 s from %2").arg(info.duration).arg(command.groups.join(u' '))));
        QCOMPARE(info.tracks[0].channels, type == u"merge" ? 4 : type == u"channelsplit" ? 1 : 2);
        PcmReader::Source source{a, 0, 2, 48000, p.duration};
        source.graphInputs = p.inputArgs;
        source.graphFilter = p.filter;
        source.graphOutput = p.outputs.first();
        PcmReader pcm(source);
        QSignalSpy done(&pcm, &PcmReader::finished);
        pcm.start();
        QVERIFY(done.wait(10000));
        QVERIFY2(done[0][0].toBool(), qPrintable(done[0][1].toString()));
        QVERIFY(pcm.data()->frames() > 1000);
        QVERIFY(pcm.data()->peakRange(0, .05, .09).max > 0);
    }
    void splitBranchesAndFanout()
    {
        auto n = nodes("split");
        n << QVariantMap{{"uid", "out2"}, {"kind", "output"}, {"outputUid", "o2"}};
        auto l = links(false);
        l << QVariantMap{{"from", "op"}, {"to", "out2"}, {"fromPort", 1}, {"toPort", 0}};
        auto p = GraphCompiler::compile(n, l, catalog);
        QVERIFY(p.error.isEmpty());
        QCOMPARE(p.outputs.size(), 2);
        QVariantMap format{
            {"codec", "pcm_s16le"}, {"rate", 48000}, {"channels", 2}, {"graphPlan", p.variant()}};
        QVariantMap first{{"uid", "o1"},
                          {"codec", "pcm_s16le"},
                          {"container", "wav"},
                          {"folder", dir.path()},
                          {"pattern", "branch-one"}};
        auto second = first;
        second["uid"] = "o2";
        second["pattern"] = "branch-two";
        auto command = CommandBuilder::build(a, 0, format, 2, {}, {first, second}, catalog, {});
        QVERIFY(render(command));
        QVERIFY(!probe(command.paths[0]).tracks.isEmpty());
        QVERIFY(!probe(command.paths[1]).tracks.isEmpty());
        auto preview = GraphCompiler::compile(n, l, catalog, {}, "o1");
        QCOMPARE(preview.outputs.size(), 1);
        QVERIFY(preview.filter.contains("anullsink"));
        n[2]["type"] = "gain";
        n[2]["params"] = QVariantMap{{"volume", -6}};
        l.last()["fromPort"] = 0;
        p = GraphCompiler::compile(n, l, catalog);
        QVERIFY(p.filter.contains("asplit=2"));
    }
    // A seeked window of an aligned graph matches the same window cut from a
    // decode that starts at zero.
    void seekableWindow()
    {
        const auto plan = GraphCompiler::compile(nodes("duck"), links(), catalog);
        QVERIFY(plan.seekable);
        auto window = [&](bool seekable) {
            PcmReader::Source s{a, 0, 2, 48000, .5};
            s.graphInputs = plan.inputArgs;
            s.graphFilter = plan.filter;
            s.graphOutput = plan.outputs.first();
            s.graphSeekable = seekable;
            QProcess p;
            p.start(FfTools::paths().ffmpeg, PcmReader::decodeArgs(s, 1.25, .5));
            p.waitForFinished(10000);
            return p.readAllStandardOutput();
        };
        const auto seeked = window(true), cut = window(false);
        QCOMPARE(seeked.size(), qsizetype(48000 / 2 * 2 * sizeof(float)));
        QCOMPARE(seeked.size(), cut.size());
        const auto *x = reinterpret_cast<const float *>(seeked.constData()),
                   *y = reinterpret_cast<const float *>(cut.constData());
        float diff = 0;
        for (qsizetype i = 0; i < seeked.size() / qsizetype(sizeof(float)); ++i)
            diff = std::max(diff, std::abs(x[i] - y[i]));
        // The compressor's envelope starts fresh at the seek point.
        QVERIFY2(diff < .02, qPrintable(QString::number(diff)));
    }

    void graphPlaybackAndSeek()
    {
        if (!PlaybackEngine::hasOutputDevice())
            QSKIP("No output device");
        const auto plan = GraphCompiler::compile(nodes("crossfade"), links(), catalog);
        PcmReader::Source graph{a, 0, 2, 48000, plan.duration};
        graph.graphInputs = plan.inputArgs;
        graph.graphFilter = plan.filter;
        graph.graphOutput = plan.outputs.first();
        PlaybackEngine player;
        player.setVolume(0);
        player.setSource({a, 0, 2, 48000, {"FL", "FR"}, 2});
        player.setProcessedGraph(graph);
        QVERIFY(player.play(0));
        QTRY_VERIFY_WITH_TIMEOUT(player.position() > 2.15, 6000);
        QVERIFY(player.playing());
        player.stop();
        QVERIFY(player.play(2.8));
        QTRY_VERIFY_WITH_TIMEOUT(player.framesPlayed() > 2400, 3000);
        QVERIFY(player.lastError().isEmpty());
        player.stop();
    }
    // Project topology rules: port counts, removals, positions, track and
    // cycle validation, hand-edited recipes, and undo restoring exactly.
    void projectTopology()
    {
        App app(nullptr);
        auto *p = app.project();
        QSignalSpy notices(p, &Project::notice);
        p->addFiles({QUrl::fromLocalFile(a), QUrl::fromLocalFile(b), QUrl::fromLocalFile(ir)});
        QTRY_VERIFY_WITH_TIMEOUT(p->probe(0) && p->probe(1) && p->probe(2), 10000);
        p->setCurrentInput(0);
        p->addOutput("Archive FLAC");
        QVERIFY(p->addGraphSource(1, 7).isEmpty());
        QVERIFY(notices.last()[0].toString().contains("no audio track 8"));
        QVERIFY(!p->graphActive());

        // A three-input mix fed by b and the IR; dropping to two inputs drops the third link.
        const auto second = p->addGraphSource(1), third = p->addGraphSource(2);
        const auto mix = p->addGraphOperation("amix");
        const int row = p->steps()->find("uid", mix);
        p->setParam(row, "inputs", 3);
        QCOMPARE(p->graphNodes()->value(p->graphNodes()->find("uid", mix), "inputs").toInt(), 3);
        QVERIFY(p->connectGraph(second, 0, mix, 1));
        QVERIFY(p->connectGraph(third, 0, mix, 2));
        QCOMPARE(p->graphError(), QString());
        const int links = p->graphLinks()->count();
        p->setParam(row, "inputs", 2);
        QCOMPARE(p->graphLinks()->count(), links - 1);
        QCOMPARE(p->graphError(), QString());
        QCOMPARE(p->graphNodes()->value(p->graphNodes()->find("uid", mix), "inputNames").toStringList(),
                 QStringList({"1", "2"}));

        // A wire back into the chain is refused and leaves the graph as it was.
        const auto output = "out_" + p->outputs()->value(0, "uid").toString();
        const auto gain = (p->addStep("gain"), p->steps()->value(p->steps()->count() - 1, "uid").toString());
        const auto before = p->recipe().graph;
        QVERIFY(!p->connectGraph(gain, 0, mix, 0));
        QVERIFY(notices.last()[0].toString().contains("cycle"));
        QCOMPARE(p->recipe().graph, before);
        QVERIFY(!p->connectGraph(output, 0, gain, 0)); // outputs have no output port

        // Undo and redo restore the same topology, link ids included.
        p->undoStack()->undo();
        p->undoStack()->redo();
        QCOMPARE(p->recipe().graph, before);

        // Removing an input removes its source node; removing a node drops its position.
        p->moveGraphNode(third, 400, 300);
        QVERIFY(p->recipe().graph.value("positions").toMap().contains(third));
        p->removeInput(2);
        QCOMPARE(p->graphNodes()->find("uid", third), -1);
        QVERIFY(!p->recipe().graph.value("positions").toMap().contains(third));
        p->undoStack()->undo();
        QVERIFY(p->graphNodes()->find("uid", third) >= 0);
        p->removeGraphNode(gain);
        QCOMPARE(p->steps()->find("uid", gain), -1);
        QCOMPARE(p->graphError(), QString());

        // A hand-edited recipe: a missing track and duplicate entries.
        auto recipe = RecipeJson::encode(p->recipe()).toVariantMap();
        auto graph = recipe.value("graph").toMap();
        auto nodes = graph.value("nodes").toList();
        for (auto &v : nodes) {
            auto n = v.toMap();
            if (n.value("uid") == second)
                n.insert("audioIndex", 5);
            v = n;
        }
        nodes << nodes.first();
        graph.insert("nodes", nodes);
        recipe.insert("graph", graph);
        const auto file = dir.filePath("edited.ffgui");
        QFile f(file);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument::fromVariant(recipe).toJson());
        f.close();
        QVERIFY(p->open(QUrl::fromLocalFile(file)));
        QTRY_VERIFY_WITH_TIMEOUT(p->graphError().contains("no audio track 6"), 10000);
        QCOMPARE(p->graphNodes()->count(), nodes.size() - 1);

        // A batch folder can't be a graph source.
        QVERIFY(QDir(dir.path()).mkpath("batch"));
        QVERIFY(QFile::copy(a, dir.filePath("batch/one.wav")));
        const int count = p->inputs()->count();
        p->addFolder(QUrl::fromLocalFile(dir.filePath("batch")));
        QTRY_VERIFY_WITH_TIMEOUT(p->inputs()->count() == count + 1 && p->probe(count), 10000);
        QVERIFY(p->addGraphSource(count).isEmpty());
        QVERIFY(notices.last()[0].toString().contains("folder"));
    }

    // The window's "Use as impulse response" drop zone: one undoable edit that
    // adds the file and convolves the selected input with it.
    void impulseResponse()
    {
        App app(nullptr);
        auto *p = app.project();
        p->addFiles({QUrl::fromLocalFile(a)});
        QTRY_VERIFY_WITH_TIMEOUT(p->probe(0), 10000);
        p->addOutput("Archive FLAC");
        p->setOutput(0, {{"folder", dir.path()}, {"pattern", "convolved"}});
        const int undo = p->undoStack()->count();
        p->useImpulseResponse(QUrl::fromLocalFile(ir));
        QCOMPARE(p->undoStack()->count(), undo + 1);
        QCOMPARE(p->currentInput(), 0);
        QCOMPARE(p->inputs()->count(), 2);
        QTRY_VERIFY2_WITH_TIMEOUT(p->probe(1) && p->graphError().isEmpty(), qPrintable(p->graphError()), 10000);
        QVERIFY(p->graphPlan().value("filter").toString().contains("afir=dry=1:wet=1"));
        QTRY_VERIFY_WITH_TIMEOUT(app.command()->valid(), 5000);
        app.render()->start();
        QTRY_COMPARE_WITH_TIMEOUT(app.queue()->value(0, "status").toString(), QString("done"), 10000);
        QVERIFY(std::abs(probe(dir.filePath("convolved.flac")).duration - 2.) < .05);
        p->undoStack()->undo();
        QCOMPARE(p->inputs()->count(), 1);
        QVERIFY(!p->graphActive());
    }

    // Two outputs on separate branches, one with an effect: different codecs,
    // rates and channel counts, metadata Edit (tags and a new cover) and Strip,
    // then a selection export of both.
    void multiOutputDelivery()
    {
        const auto tagged = dir.filePath("delivery.flac"), image = dir.filePath("graph-cover.png");
        QVERIFY(run({"-v", "error", "-f", "lavfi", "-i", "sine=f=440:r=48000:d=2", "-ac", "2", "-metadata",
                     "title=Original", tagged}));
        QImage cover(16, 16, QImage::Format_RGB32);
        cover.fill(Qt::blue);
        QVERIFY(cover.save(image));
        App app(nullptr);
        auto *p = app.project();
        p->addFiles({QUrl::fromLocalFile(tagged)});
        QTRY_VERIFY_WITH_TIMEOUT(p->probe(0), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(app.caps()->state(), QString("ready"), 10000);
        p->addOutput("Archive FLAC");
        p->setOutput(0, {{"folder", dir.path()}, {"pattern", "{name}-edit"}, {"metadata", "edit"}});
        p->addOutput("Podcast MP3");
        p->setOutput(1, {{"folder", dir.path()}, {"pattern", "{name}-strip"}, {"metadata", "strip"}, {"channels", 1}});
        // input → split → FLAC, and split → gain → MP3.
        p->setCurrentOutput(0);
        const auto split = p->addGraphOperation("split");
        p->setCurrentOutput(1);
        p->addStep("gain", {{"volume", -6}});
        const auto gain = p->steps()->value(p->steps()->count() - 1, "uid").toString();
        QVERIFY(p->connectGraph(split, 1, gain, 0));
        QCOMPARE(p->graphError(), QString());
        QVERIFY(!p->canUseRow());
        QCOMPARE(p->outputs()->value(0, "reencodeReason").toString(), QString("Graph processing requires encoding"));
        p->setTag("title", "Edited");
        p->setCoverArt(QUrl::fromLocalFile(image));
        QTRY_VERIFY_WITH_TIMEOUT(app.command()->valid(), 5000);
        app.render()->start();
        QTRY_COMPARE_WITH_TIMEOUT(app.queue()->value(0, "status").toString(), QString("done"), 10000);
        const auto edited = probe(dir.filePath("delivery-edit.flac")), stripped = probe(dir.filePath("delivery-strip.mp3"));
        QCOMPARE(edited.tags.value("title").toString(), QString("Edited"));
        QVERIFY(edited.coverStream >= 0);
        QCOMPARE(edited.tracks[0].sampleRate, 48000);
        QCOMPARE(edited.tracks[0].channels, 2);
        QVERIFY(std::abs(edited.duration - 2.) < .05);
        QVERIFY(!stripped.tags.contains("title"));
        QCOMPARE(stripped.coverStream, -1);
        QCOMPARE(stripped.tracks[0].sampleRate, 44100);
        QCOMPARE(stripped.tracks[0].channels, 1);

        app.selection()->select("range", {{"start", .5}, {"end", 1.25}});
        p->setOutput(0, {{"pattern", "{name}-selection"}});
        p->setOutput(1, {{"pattern", "{name}-selection"}});
        app.render()->start("selection");
        QTRY_COMPARE_WITH_TIMEOUT(app.queue()->count(), 2, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(app.queue()->value(1, "status").toString(), QString("done"), 10000);
        QVERIFY(std::abs(probe(dir.filePath("delivery-selection.flac")).duration - .75) < .02);
        QVERIFY(std::abs(probe(dir.filePath("delivery-selection.mp3")).duration - .75) < .1);
    }

    // Silent playback through the Engine: a crossfade runs past the end of the
    // selected file, survives a graph edit and a speed change mid-play, and
    // the project returns to a row.
    void appPlaybackTimeline()
    {
        if (!PlaybackEngine::hasOutputDevice())
            QSKIP("No output device");
        App app(nullptr);
        auto *p = app.project();
        auto *t = app.transport();
        t->setVolume(0);
        p->addFiles({QUrl::fromLocalFile(a), QUrl::fromLocalFile(b), QUrl::fromLocalFile(ir)});
        QTRY_VERIFY_WITH_TIMEOUT(p->probe(0) && p->probe(1) && p->probe(2), 10000);
        p->setCurrentInput(0);
        p->addOutput("Archive FLAC");

        // An aligned mix of the selected input compares A/B; another input doesn't.
        const auto source = p->addGraphSource(1);
        const auto mix = p->addGraphOperation("amix");
        QVERIFY(p->connectGraph(source, 0, mix, 1));
        QTRY_VERIFY_WITH_TIMEOUT(t->abEnabled(), 5000);
        QCOMPARE(t->duration(), 2.);
        p->setCurrentInput(2);
        QTRY_VERIFY_WITH_TIMEOUT(!t->abEnabled() && t->abReason().contains("doesn't feed"), 5000);
        QCOMPARE(p->renderInput(), 0);
        p->setCurrentInput(0);

        // Swap the mix for a crossfade: 2 + 2 − 0.5 s, so no aligned A/B.
        p->removeGraphNode(mix);
        const auto fade = p->addGraphOperation("crossfade");
        QVERIFY(p->connectGraph(source, 0, fade, 1));
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(t->duration() - 3.5) < .01, 5000);
        QVERIFY(!t->abEnabled());
        QCOMPARE(t->ab(), QString("processed"));
        QTRY_VERIFY_WITH_TIMEOUT(app.media()->processed()->ready() && !app.media()->processed()->loading(), 10000);
        QCOMPARE(p->keptEnd(), p->duration());

        t->seek(1.7);
        t->play();
        QTRY_VERIFY_WITH_TIMEOUT(t->position() > 2.2, 4000);
        QVERIFY(t->playing());
        // A graph edit restarts processing at the playhead.
        p->setParam(p->steps()->find("uid", fade), "d", 1.0);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(t->duration() - 3.) < .01, 5000);
        QVERIFY(t->playing());
        QVERIFY(t->position() > 2.);
        t->setSpeed(2);
        QTRY_VERIFY_WITH_TIMEOUT(!t->playing(), 6000);
        QVERIFY(std::abs(t->position() - 3.) < .05);
        t->setSpeed(1);

        // Back to one path and a row: the input's own timeline again.
        p->removeGraphNode(fade);
        p->removeGraphNode(source);
        QVERIFY(p->canUseRow());
        QVERIFY(p->useRow());
        QTRY_COMPARE_WITH_TIMEOUT(t->duration(), 2., 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!t->abEnabled(), 5000);
        t->seek(0);
        t->play();
        QTRY_VERIFY_WITH_TIMEOUT(t->position() > .3, 4000);
        t->pause();
    }

    // Output names, tags and chapters come from the selected input when it
    // feeds the graph, otherwise from the graph's first source.
    void metadataSource()
    {
        const auto first = dir.filePath("tagged-first.flac"), second = dir.filePath("tagged-second.flac");
        QVERIFY(run({"-v", "error", "-f", "lavfi", "-i", "sine=f=330:r=48000:d=1", "-metadata", "title=First",
                     first}));
        QVERIFY(run({"-v", "error", "-f", "lavfi", "-i", "sine=f=660:r=48000:d=1", "-metadata", "title=Second",
                     second}));
        App app(nullptr);
        auto *p = app.project();
        p->addFiles({QUrl::fromLocalFile(first), QUrl::fromLocalFile(second), QUrl::fromLocalFile(ir)});
        QTRY_VERIFY_WITH_TIMEOUT(p->probe(0) && p->probe(1) && p->probe(2), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(app.caps()->state(), QString("ready"), 10000);
        p->setCurrentInput(0);
        p->addOutput("Archive FLAC");
        p->setOutput(0, {{"folder", dir.path()}, {"pattern", "{name}-graph"}});
        const auto source = p->addGraphSource(1);
        const auto mix = p->addGraphOperation("amix");
        QVERIFY(p->connectGraph(source, 0, mix, 1));
        auto check = [&](int current, const QString &name, const QString &mapped) {
            p->setCurrentInput(current);
            QTRY_VERIFY_WITH_TIMEOUT(app.command()->valid(), 5000);
            const auto args = app.command()->args();
            QCOMPARE(QFileInfo(args.last()).fileName(), name);
            QCOMPARE(args.value(args.indexOf("-map_metadata") + 1), mapped);
            QCOMPARE(p->outputs()->value(0, "fileName").toString(), name);
        };
        check(2, "tagged-first-graph.flac", "0");  // the IR doesn't feed the graph
        check(1, "tagged-second-graph.flac", "1");
        app.render()->start();
        QTRY_COMPARE_WITH_TIMEOUT(app.queue()->value(0, "status").toString(), QString("done"), 10000);
        QCOMPARE(probe(dir.filePath("tagged-second-graph.flac")).tags.value("title").toString(), QString("Second"));
    }

    void backendUndoRecipeAndRender()
    {
        App app(nullptr);
        auto *p = app.project();
        p->addFiles({QUrl::fromLocalFile(a), QUrl::fromLocalFile(b)});
        QTRY_VERIFY_WITH_TIMEOUT(p->probe(0) && p->probe(1), 10000);
        p->setCurrentInput(0);
        p->addOutput("Archive FLAC");
        p->setOutput(0, {{"folder", dir.path()}, {"pattern", "backend-graph"}});
        const auto source = p->addGraphSource(1);
        const auto op = p->addGraphOperation("amix");
        QVERIFY(!p->canUseRow());
        QVERIFY(p->connectGraph(source, 0, op, 1));
        QTRY_VERIFY_WITH_TIMEOUT(app.command()->valid(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(app.media()->processed()->ready() && !app.media()->processed()->loading(),
                                 10000);
        QVERIFY2(app.media()->processed()->error().isEmpty(), qPrintable(app.media()->processed()->error()));
        const auto graph = p->recipe().graph;
        p->moveGraphNode(op, 333, 177);
        QCOMPARE(p->graphNodes()->value(p->graphNodes()->find("uid", op), "x").toDouble(), 333.);
        p->undoStack()->undo();
        QCOMPARE(p->recipe().graph, graph);
        p->undoStack()->redo();
        const auto file = QUrl::fromLocalFile(dir.filePath("graph.ffgui"));
        QVERIFY(p->save(file));
        const auto saved = p->recipe().graph;
        p->newProject();
        QVERIFY(p->open(file));
        QTRY_VERIFY_WITH_TIMEOUT(p->probe(0) && p->probe(1), 10000);
        QCOMPARE(p->recipe().graph, saved);
        QVERIFY(!p->canUseRow());
        app.render()->start();
        QTRY_COMPARE_WITH_TIMEOUT(app.queue()->value(0, "status").toString(), QString("done"), 10000);
        QCOMPARE(probe(dir.filePath("backend-graph.flac")).tracks[0].channels, 2);
        app.stream()->add("Graph HLS", "hls", dir.filePath("graph.m3u8"), 96);
        app.stream()->start(0);
        QTRY_COMPARE_WITH_TIMEOUT(app.destinations()->value(0, "status").toString(), QString("stopped"),
                                  7000);
        QVERIFY(!probe(dir.filePath("graph.m3u8")).tracks.isEmpty());
        app.compare()->run();
        QTRY_VERIFY_WITH_TIMEOUT(!app.compare()->active(), 10000);
        QVERIFY2(app.compare()->error().isEmpty(), qPrintable(app.compare()->error()));
        QVERIFY(!app.compare()->result().value("equal").toBool());
        p->removeGraphNode(op);
        p->removeGraphNode(source);
        const auto input = p->graphNodes()->value(0, "uid").toString();
        const auto output = "out_" + p->outputs()->value(0, "uid").toString();
        QVERIFY(p->connectGraph(input, 0, output, 0));
        QVERIFY(p->canUseRow());
        QVERIFY(p->useRow());
        QVERIFY(!p->graphActive());
    }
};
QTEST_MAIN(TestGraphPipeline)
#include "tst_graph_pipeline.moc"
