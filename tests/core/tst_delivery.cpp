#include "Project.h"
#include "Selection.h"
#include "CatalogFacade.h"
#include <caps/HelpParser.h>
#include <command/CommandBuilder.h>
#include <command/GraphProbe.h>
#include <project/Recipe.h>
#include <QTest>
#include <QFile>
#include <QTemporaryDir>
#include <QStandardPaths>
#include "fixtures.h"

using namespace ffgui;
class TestDelivery : public QObject
{
    Q_OBJECT
    const Catalog &catalog = Catalog::builtIn();
    QVariantMap node(const QString &type, const QVariantMap &params = {})
    {
        return {{"uid", "s1"}, {"type", type}, {"params", params}};
    }
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void help()
    {
        const auto options = HelpParser::parse(fixture("help_filter_volume.txt"));
        QCOMPARE(options.size(), 6);
        QCOMPARE(options[0].key, QStringLiteral("volume"));
        QVERIFY(options[0].runtime);
        QCOMPARE(options[0].defaultValue.toString(), QStringLiteral("1.0"));
        QCOMPARE(options[1].type, QStringLiteral("choice"));
        QCOMPARE(options[1].choices, QStringList({"fixed", "float", "double"}));
        QCOMPARE(options[1].defaultValue.toString(), QStringLiteral("float"));
        QVERIFY(options[5].defaultValue.toBool());
        const auto loudnorm = HelpParser::parse(fixture("help_filter_loudnorm.txt"));
        const auto i = std::find_if(loudnorm.begin(), loudnorm.end(), [](const auto &o) { return o.key == u"I"; });
        QVERIFY(i != loudnorm.end());
        QCOMPARE(i->min, -70);
        QCOMPARE(i->max, -5);
        QVERIFY(!HelpParser::parse(fixture("help_encoder_libopus.txt")).isEmpty());
        QVERIFY(!HelpParser::parse(fixture("help_muxer_mp3.txt")).isEmpty());
    }
    void builders()
    {
        QString error;
        QCOMPARE(StepBuilders::build(*catalog.step("gain"), node("gain", {{"volume", -6}}), 2),
                 QStringLiteral("volume@s1=-6dB"));
        QCOMPARE(StepBuilders::build(*catalog.step("trim"), node("trim", {{"start", 1}, {"end", 3}}), 2),
                 QStringLiteral("atrim@s1=start=1:end=3,asetpts@s1_1=PTS-STARTPTS"));
        QCOMPARE(StepBuilders::build(*catalog.step("mono"), node("mono"), 1), QStringLiteral("pan@s1=mono|c0=1*c0"));
        QCOMPARE(StepBuilders::build(*catalog.step("mono"), node("mono"), 2),
                 QStringLiteral("pan@s1=mono|c0=0.5*c0+0.5*c1"));
        QCOMPARE(StepBuilders::build(*catalog.step("pitch"), node("pitch", {{"semitones", 12}}), 2),
                 QStringLiteral("rubberband@s1=pitch=2"));
        QCOMPARE(StepBuilders::build(*catalog.step("dehum"), node("dehum", {{"harmonics", 2}}), 2),
                 QStringLiteral("bandreject@s1=f=60:width_type=h:w=4,bandreject@s1_1=f=120:width_type=h:w=4"));
        QCOMPARE(StepBuilders::build(*catalog.step("eq"), node("eq", {{"poles", 4}}), 2),
                 QStringLiteral("highpass@s1=f=80:poles=4,equalizer@s1_1=f=3200:t=q:w=1.2:g=2.5"));
        QCOMPARE(StepBuilders::build(*catalog.step("pitch"), node("pitch", {{"formant", "preserved"}}), 2),
                 QStringLiteral("rubberband@s1=pitch=1:formant='preserved'"));
        StepBuilders::build(*catalog.step("swap"), node("swap"), 1, &error);
        QVERIFY(!error.isEmpty());
    }
    void rangesAndPreview()
    {
        BuildInfo build;
        build.filters["volume"].timeline = true;
        auto n = node("gain", {{"volume", 3}});
        n.insert("hasRange", true);
        n.insert("rangeStart", 1);
        n.insert("rangeEnd", 2);
        auto chain = ChainCompiler::compile({n}, catalog, build, 2);
        QCOMPARE(chain.filter, QStringLiteral("volume@s1=3dB:enable='between(t,1,2)'"));
        build.filters["volume"].timeline = false;
        QVERIFY(!ChainCompiler::compile({n}, catalog, build, 2).error.isEmpty());
        chain = ChainCompiler::compile({node("trim", {{"start", 1}, {"end", 3}}), node("speed")}, catalog, {}, 2, true);
        QVERIFY(chain.filter.isEmpty());
        QVERIFY(!chain.alignment);
        QCOMPARE(chain.start, 1);
        QCOMPARE(chain.end, 3);
    }
    void commandGoldenAndCopy()
    {
        QVariantMap track{{"codec", "pcm_s16le"}, {"rate", 48000}, {"depth", 16}, {"channels", 2}};
        QVariantMap out{{"codec", "pcm_s16le"}, {"container", "wav"}, {"folder", "/tmp"}, {"pattern", "{name}-out"}};
        QVERIFY(CopyAdvisor::advise(track, out, {}, catalog).copy);
        auto built = CommandBuilder::build("/tmp/in.wav", 0, track, 4, {}, {out}, catalog, {});
        QVERIFY2(built.error.isEmpty(), qPrintable(built.error));
        QCOMPARE(built.args, QStringList({"-hide_banner",
                                          "-loglevel",
                                          "level+info",
                                          "-n",
                                          "-progress",
                                          "pipe:1",
                                          "-nostats",
                                          "-i",
                                          "/tmp/in.wav",
                                          "-map",
                                          "0:a:0",
                                          "-c:a",
                                          "copy",
                                          "-map_metadata",
                                          "0",
                                          "-map_chapters",
                                          "0",
                                          "-f",
                                          "wav",
                                          "/tmp/in-out.wav"}));
        const auto n = node("gain");
        QVERIFY(!CopyAdvisor::advise(track, out, {n}, catalog).copy);
        auto second = out;
        second.insert("pattern", "second");
        built = CommandBuilder::build("/tmp/in.wav", 0, track, 4, {n}, {out, second}, catalog, {});
        QVERIFY(built.error.isEmpty());
        QCOMPARE(built.args[built.args.indexOf("-filter_complex") + 1],
                 QStringLiteral("[0:a:0]volume@s1=3dB,asplit=2[a0][a1]"));
        QCOMPARE(built.args.count("-map"), 2);
        QCOMPARE(SizeEstimator::bytes(1536, 4), 768000);
        QCOMPARE(SizeEstimator::kbps(*catalog.codec("pcm_s16le"), out, track), 1536);
        auto mismatched = out;
        mismatched.insert("depth", 24);
        QVERIFY(CommandBuilder::build("/tmp/in.wav", 0, track, 4, {}, {mismatched}, catalog, {}).error.contains("bit depth"));
        out.insert("pattern", "in");
        QVERIFY(!CommandBuilder::build("/tmp/in.wav", 0, track, 4, {}, {out}, catalog, {}).error.isEmpty());
    }
    void graphFixture()
    {
        const auto graph = GraphProbe::parse(fixture("graph.txt"));
        QVERIFY2(graph.error.isEmpty(), qPrintable(graph.error));
        int converters = 0;
        for (const auto &n : graph.nodes)
            if (n.value("kind").toString() == u"auto") {
                ++converters;
                QVERIFY(n.value("from").toMap().value("rate").toInt() > 0);
                QVERIFY(!n.value("from").toMap().value("fmt").toString().isEmpty());
            }
        QCOMPARE(converters, 2);
        QVERIFY(!GraphProbe::parse("not a graph").error.isEmpty());
    }
    void recipe()
    {
        Recipe r;
        r.name = "Example";
        r.inputs << QVariantMap{{"uid", "i1"}, {"path", "/tmp/a file.wav"}, {"kind", "file"}};
        r.nodes << node("gain", {{"volume", 0.125}});
        r.outputs << QVariantMap{{"uid", "o1"}, {"codec", "flac"}};
        r.metadata = {{"title", "Title"}};
        Recipe loaded;
        QString error;
        const auto json = RecipeJson::encode(r);
        QVERIFY(RecipeJson::decode(json, &loaded, &error));
        QCOMPARE(RecipeJson::encode(loaded), json);
        auto broken = json;
        broken.insert("version", 2);
        QVERIFY(!RecipeJson::decode(broken, &loaded, &error));
    }
    void undoAndProjects()
    {
        Project project;
        project.addStep("gain");
        project.addStep("highpass");
        QCOMPARE(project.steps()->count(), 2);
        project.setParam(0, "volume", 4);
        project.setParam(0, "volume", 5);
        project.setParam(0, "volume", 6);
        QCOMPARE(project.undoStack()->count(), 3);
        project.undoStack()->undo();
        QCOMPARE(project.steps()->value(0, "params").toMap().value("volume").toInt(), 3);
        project.undoStack()->redo();
        QCOMPARE(project.steps()->value(0, "params").toMap().value("volume").toInt(), 6);
        const auto uid = project.steps()->value(0, "uid");
        project.moveStep(0, 1);
        QCOMPARE(project.steps()->value(1, "uid"), uid);
        project.undoStack()->undo();
        QCOMPARE(project.steps()->value(0, "uid"), uid);
        project.addOutput("Podcast MP3");
        QCOMPARE(project.outputs()->value(0, "codec").toString(), QStringLiteral("libmp3lame"));
        QTemporaryDir dir;
        const auto url = QUrl::fromLocalFile(dir.filePath("test.ffgui"));
        QVERIFY(project.save(url));
        QVERIFY(!project.modified());
        const auto saved = RecipeJson::encode(project.recipe());
        project.newProject();
        QCOMPARE(project.steps()->count(), 0);
        QVERIFY(project.open(url));
        QCOMPARE(RecipeJson::encode(project.recipe()), saved);
        QVERIFY(!project.modified());
    }
    void presetsRoundTrip()
    {
        auto user = Catalog::presetsFromJson(catalog.presetsJson());
        QCOMPARE(user.presets.size(), catalog.presets.size());
        for (const auto &p : user.presets)
            QVERIFY(!p.builtIn);
        user.presets[0].settings = {{"metadata", "strip"}, {"encoderOptions", QVariantMap{{"foo", 3}}}};
        auto &chain = user.presets.last();
        chain.steps[0].settings = {{"bypass", true}, {"hasRange", true}, {"rangeStart", 1}, {"rangeEnd", 2}};
        const auto json = user.presetsJson();
        QCOMPARE(Catalog::presetsFromJson(json).presetsJson(), json);
    }
    void gating()
    {
        CatalogFacade facade;
        BuildInfo build;
        build.version = "test";
        build.filters["volume"].timeline = true;
        facade.setCapabilities(build);
        const int gain = facade.steps()->find("key", "gain"), pitch = facade.steps()->find("key", "pitch");
        QVERIFY(facade.steps()->value(gain, "available").toBool());
        QVERIFY(!facade.steps()->value(pitch, "available").toBool());
        QVERIFY(facade.steps()->value(pitch, "reason").toString().contains("librubberband"));
        QVERIFY(facade.containers().isEmpty());
    }
};
QTEST_MAIN(TestDelivery)
#include "tst_delivery.moc"
