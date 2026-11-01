#include <QJsonArray>
#include <QTest>
#include <graph/GraphCompiler.h>
#include <media/PcmReader.h>
#include <project/Recipe.h>

using namespace ffgui;

// GraphCompiler without FFmpeg: shape validation, the labelled-pad filter
// text of each operation, option rules, durations and the linear check.
// tst_graph_pipeline renders the same operations through FFmpeg.
class TestGraph : public QObject
{
    Q_OBJECT
    const Catalog &catalog = Catalog::builtIn();

    static QVariantMap source(const QString &uid, double duration = 2, int channels = 2)
    {
        return {{"uid", uid},           {"kind", "source"},   {"path", "/media/" + uid + ".wav"},
                {"audioIndex", 0},      {"duration", duration}, {"channels", channels}};
    }
    QVariantMap step(const QString &uid, const QString &type, const QVariantMap &params = {}) const
    {
        auto p = catalog.step(type)->defaults();
        for (auto it = params.begin(); it != params.end(); ++it)
            p.insert(it.key(), it.value());
        return {{"uid", uid}, {"kind", "step"}, {"type", type}, {"params", p}, {"label", type}};
    }
    static QVariantMap output(const QString &uid, const QString &outputUid)
    {
        return {{"uid", uid}, {"kind", "output"}, {"outputUid", outputUid}};
    }
    static QVariantMap link(const QString &from, const QString &to, int fromPort = 0, int toPort = 0)
    {
        return {{"from", from}, {"to", to}, {"fromPort", fromPort}, {"toPort", toPort}};
    }
    // a, b → op → out
    GraphPlan twoInputs(const QString &type, const QVariantMap &params = {}, double da = 2, double db = 2) const
    {
        return GraphCompiler::compile({source("a", da), source("b", db), step("op", type, params), output("out", "o1")},
                                      {link("a", "op"), link("b", "op", 0, 1), link("op", "out")}, catalog);
    }

private slots:
    void shapeValidation()
    {
        const QList<QVariantMap> nodes{source("a"), step("g", "gain"), output("out", "o1")};
        QCOMPARE(GraphCompiler::validate(nodes, {link("a", "g"), link("g", "out")}), QString());
        QVERIFY(GraphCompiler::validate({source("a"), step("g", "gain"), step("h", "gain")}, {link("g", "h"), link("h", "g")})
                    .contains("cycle"));
        QVERIFY(GraphCompiler::validate(nodes, {link("a", "g", 1)}).contains("port"));
        QVERIFY(GraphCompiler::validate(nodes, {link("a", "nowhere")}).contains("missing node"));
        QVERIFY(GraphCompiler::validate(nodes, {link("a", "g"), link("a", "g")}).contains("one connection"));
        QVERIFY(GraphCompiler::validate({source("a b")}, {}).contains("id"));
        QVERIFY(GraphCompiler::validate({source("a"), source("a")}, {}).contains("duplicate"));
        QList<QVariantMap> many;
        for (int i = 0; i <= GraphCompiler::kMaxNodes; ++i)
            many << source(QStringLiteral("s%1").arg(i));
        QVERIFY(GraphCompiler::validate(many, {}).contains("at most"));
        const auto open = GraphCompiler::compile(nodes, {link("g", "out")}, catalog);
        QCOMPARE(open.error, QString("Connect input 1 of gain"));
        QCOMPARE(GraphCompiler::compile({source("a")}, {}, catalog).error, QString("Connect at least one output"));
    }

    void linearChain()
    {
        const auto plan = GraphCompiler::compile({source("a"), step("g", "gain", {{"volume", -6}}), output("out", "o1")},
                                                 {link("a", "g"), link("g", "out")}, catalog);
        QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
        QCOMPARE(plan.inputArgs, QStringList({"-i", "/media/a.wav"}));
        QVERIFY(plan.filter.startsWith("[0:a:0]anull[g_a_0];[g_a_0]volume@g="));
        QVERIFY(plan.filter.endsWith(";[g_g_0]anull[g_out_out]"));
        QCOMPARE(plan.outputs.value("o1"), QString("g_out_out"));
        QCOMPARE(plan.duration, 2.);
        QVERIFY(plan.aligned);
    }

    void operations()
    {
        auto mix = twoInputs("amix", {{"normalize", false}}, 2, 3);
        QVERIFY(mix.filter.contains("[g_a_0][g_b_0]amix=inputs=2:duration=longest:normalize=0[g_op_0]"));
        QCOMPARE(mix.duration, 3.);
        QCOMPARE(twoInputs("amix", {{"duration", "shortest"}}, 2, 3).duration, 2.);
        QVERIFY(twoInputs("amix", {{"duration", "sometimes"}}).error.contains("longest"));

        // Duck pads both sides and cuts to the main input (see the comment in GraphCompiler).
        const auto duck = twoInputs("duck", {}, 2, .5);
        QVERIFY2(duck.error.isEmpty(), qPrintable(duck.error));
        QVERIFY(duck.filter.contains("[g_a_0]asplit=2[g_op_main][g_op_length];[g_b_0]apad[g_op_side];"
                                     "[g_op_main]apad[g_op_padded];[g_op_padded][g_op_side]sidechaincompress="
                                     "threshold=0.125:ratio=4:attack=20:release=250[g_op_ducked];"
                                     "[g_op_length][g_op_ducked]amix=inputs=2:duration=first:weights='0 1':"
                                     "normalize=0[g_op_0]"));
        QCOMPARE(duck.duration, 2.);

        const auto fade = twoInputs("crossfade", {{"d", 1}}, 2, 3);
        QVERIFY(fade.filter.contains("acrossfade=d=1:overlap=1:curve1='tri':curve2='tri'"));
        QCOMPARE(fade.duration, 4.);
        QVERIFY(!fade.aligned);
        QCOMPARE(twoInputs("crossfade", {{"d", 1}, {"overlap", false}}, 2, 3).duration, 5.);
        QVERIFY(twoInputs("crossfade", {{"d", 2.5}}).error.contains("shorter"));
        QVERIFY(twoInputs("crossfade", {{"d", 0}}).error.contains("positive"));

        const auto merge = GraphCompiler::compile(
            {source("a", 2, 2), source("b", 1, 1), step("op", "merge"), step("m", "mono"), output("out", "o1")},
            {link("a", "op"), link("b", "op", 0, 1), link("op", "m"), link("m", "out")}, catalog);
        QVERIFY2(merge.error.isEmpty(), qPrintable(merge.error));
        QVERIFY(merge.filter.contains("amerge=inputs=2"));
        // The mono step after the merge sums all three merged channels.
        QVERIFY(merge.filter.contains("mono|c0=0.333333333333*c0+0.333333333333*c1+0.333333333333*c2"));
        QCOMPARE(merge.duration, 1.);

        const auto convolve = twoInputs("convolve", {{"wet", .5}}, 2, .1);
        QVERIFY(convolve.filter.contains("[g_a_0][g_b_0]afir=dry=1:wet=0.5[g_op_0]"));
        QCOMPARE(convolve.duration, 2.);
    }

    void splitsAndFanOut()
    {
        // Two outputs of a crossover; the third band is unused and sinks.
        auto bands = step("x", "crossover", {{"split", "200 2000"}});
        QCOMPARE(GraphCompiler::outputs(bands), 3);
        QCOMPARE(GraphCompiler::portNames(bands, true), QStringList({"< 200", "200–2000", "> 2000"}));
        const auto plan = GraphCompiler::compile(
            {source("a"), bands, output("lo", "o1"), output("mid", "o2")},
            {link("a", "x"), link("x", "lo"), link("x", "mid", 1)}, catalog);
        QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
        QVERIFY(plan.filter.contains("acrossover=split='200 2000'[g_x_0][g_x_1][g_x_2]"));
        QVERIFY(plan.filter.contains("[g_x_2]anullsink"));
        QCOMPARE(plan.outputs.size(), 2);
        // Preview of one output sinks the other.
        const auto preview = GraphCompiler::compile(
            {source("a"), bands, output("lo", "o1"), output("mid", "o2")},
            {link("a", "x"), link("x", "lo"), link("x", "mid", 1)}, catalog, {}, "o2");
        QCOMPARE(preview.outputs.keys(), QStringList({"o2"}));
        QVERIFY(preview.filter.contains("[g_x_0]anullsink"));
        for (const auto &bad : {"2000 200", "200 abc", "", "1 2 3 4 5 6 7 8"})
            QVERIFY2(!GraphCompiler::compile({source("a"), step("x", "crossover", {{"split", bad}}), output("o", "o1")},
                                             {link("a", "x"), link("x", "o")}, catalog)
                          .error.isEmpty(),
                     bad);

        // One port feeding two inputs is split explicitly.
        const auto fan = GraphCompiler::compile({source("a"), step("g", "gain"), output("o", "o1"), output("p", "o2")},
                                                {link("a", "g"), link("g", "o"), link("g", "p")}, catalog);
        QVERIFY(fan.filter.contains("[g_g_0]asplit=2[g_fan_0][g_fan_1]"));

        auto channels = step("c", "channelsplit", {{"channel_layout", "5.1"}});
        QCOMPARE(GraphCompiler::outputs(channels), 6);
        QCOMPARE(GraphCompiler::portNames(channels, true), QStringList({"FL", "FR", "FC", "LFE", "BL", "BR"}));
        const auto split = GraphCompiler::compile({source("a", 2, 6), channels, output("o", "o1")},
                                                  {link("a", "c"), link("c", "o", 3)}, catalog);
        QVERIFY(split.filter.contains("channelsplit=channel_layout=5.1[g_c_0][g_c_1][g_c_2][g_c_3][g_c_4][g_c_5]"));
        QVERIFY(GraphCompiler::compile({source("a"), step("c", "channelsplit", {{"channel_layout", "surround"}}),
                                        output("o", "o1")},
                                       {link("a", "c"), link("c", "o")}, catalog)
                    .error.contains("layout"));
        QCOMPARE(GraphCompiler::layoutChannels("22.2"), 24);
        QCOMPARE(GraphCompiler::layoutChannels("FL+FR+LFE"), 3);
        QCOMPARE(GraphCompiler::layoutChannels("fl+fr"), 0);
        QCOMPARE(GraphCompiler::portNames(step("d", "duck"), false), QStringList({"main", "sidechain"}));
        QVERIFY(GraphCompiler::portNames(step("g", "gain"), false).isEmpty());
        QCOMPARE(GraphCompiler::portNames(step("s", "split", {{"outputs", 3}}), true), QStringList({"1", "2", "3"}));
    }

    void optionsAndBypass()
    {
        // Advanced options pass through, quoted; port-shaping ones are refused.
        const auto weights = twoInputs("amix", {{"weights", "1 0.5"}});
        QVERIFY(weights.filter.contains(":weights='1 0.5'"));
        QVERIFY(twoInputs("convolve", {{"nbirs", 2}}).error.contains("nbirs"));
        QVERIFY(twoInputs("crossfade", {{"ns", 4800}}).error.contains("ns"));
        QVERIFY(twoInputs("amix", {{"bad-name", 1}}).error.contains("option name"));
        QVERIFY(catalog.step("channelsplit")->fixed.contains("channels"));

        // A bypassed two-input node passes its first input and sinks the other.
        auto off = step("op", "duck");
        off.insert("bypass", true);
        const auto plan = GraphCompiler::compile({source("a"), source("b"), off, output("out", "o1")},
                                                 {link("a", "op"), link("b", "op", 0, 1), link("op", "out")}, catalog);
        QVERIFY(plan.filter.contains("[g_b_0]anullsink"));
        QVERIFY(plan.filter.contains("[g_a_0]anull[g_op_0]"));
        QVERIFY(!plan.filter.contains("sidechaincompress"));
    }

    void sources()
    {
        auto broken = source("a");
        broken.insert("error", "a.wav has no audio track 3");
        QCOMPARE(GraphCompiler::compile({broken, output("o", "o1")}, {link("a", "o")}, catalog).error,
                 QString("a.wav has no audio track 3"));
        auto reading = source("a");
        reading.insert("probing", true);
        reading.insert("label", "a.wav");
        QCOMPARE(GraphCompiler::compile({reading, output("o", "o1")}, {link("a", "o")}, catalog).error,
                 QString("Waiting for a.wav"));
        // Sources are numbered in topological order; the second gets FFmpeg input 1.
        const auto plan = twoInputs("amix");
        QVERIFY(plan.filter.contains("[1:a:0]anull[g_b_0]"));
        QList<QVariantMap> nodes{step("m", "amix", {{"inputs", 8}}), output("o", "o1")};
        QList<QVariantMap> links{link("m", "o")};
        for (int i = 0; i < GraphCompiler::kMaxSources + 1; ++i) {
            nodes << source(QStringLiteral("s%1").arg(i));
            if (i < 8)
                links << link(QStringLiteral("s%1").arg(i), "m", 0, i);
        }
        QVERIFY(GraphCompiler::compile(nodes, links, catalog).error.contains("at most"));
    }

    void seekableWindows()
    {
        QVERIFY(twoInputs("amix").seekable);
        QVERIFY(twoInputs("duck").seekable);
        QVERIFY(!twoInputs("crossfade").seekable);
        QVERIFY(!twoInputs("convolve").seekable);
        QVERIFY(twoInputs("convolve").aligned);
        const auto trimmed = GraphCompiler::compile({source("a"), step("t", "trim", {{"start", .5}, {"end", 1}}), output("o", "o1")},
                                                    {link("a", "t"), link("t", "o")}, catalog);
        QVERIFY(!trimmed.seekable && !trimmed.aligned);

        // Seekable graphs seek every input and keep timestamps; others cut the
        // window from the output.
        const auto plan = twoInputs("amix");
        PcmReader::Source s{"/media/a.wav", 0, 2, 48000, 2};
        s.graphInputs = plan.inputArgs;
        s.graphFilter = plan.filter;
        s.graphOutput = plan.outputs.value("o1");
        s.graphSeekable = true;
        auto args = PcmReader::decodeArgs(s, 1.25, .5);
        QCOMPARE(args.mid(4, 9), QStringList({"-copyts", "-ss", "1.250000", "-i", "/media/a.wav", "-ss", "1.250000", "-i",
                                              "/media/b.wav"}));
        QVERIFY(args.value(args.indexOf("-filter_complex") + 1).endsWith(";[g_out_out]atrim=duration=0.500000[pcm_out]"));
        s.graphSeekable = false;
        args = PcmReader::decodeArgs(s, 1.25, .5);
        QVERIFY(!args.contains("-ss") && !args.contains("-copyts"));
        QVERIFY(args.value(args.indexOf("-filter_complex") + 1)
                    .endsWith(";[g_out_out]atrim=start=1.250000:duration=0.500000,asetpts=PTS-STARTPTS[pcm_out]"));
    }

    void linearCheck()
    {
        QList<QVariantMap> nodes{source("a"), step("g", "gain"), step("h", "highpass"), output("o", "o1"),
                                 output("p", "o2")};
        QStringList order;
        QVERIFY(GraphCompiler::linear(nodes, {link("a", "h"), link("h", "g"), link("g", "o"), link("g", "p")}, &order));
        QCOMPARE(order, QStringList({"h", "g"}));
        // An output hanging off the middle of the path is a branch.
        QVERIFY(!GraphCompiler::linear(nodes, {link("a", "h"), link("h", "g"), link("g", "o"), link("h", "p")}));
        QVERIFY(!GraphCompiler::linear(nodes << source("b"), {link("a", "h"), link("h", "g"), link("g", "o")}));
        QVERIFY(!GraphCompiler::linear({source("a"), step("m", "amix"), output("o", "o1")},
                                       {link("a", "m"), link("m", "o")}));
    }

    void recipeGraphField()
    {
        Recipe r;
        r.graph = {{"active", true},
                   {"nodes", QVariantList{QVariantMap{{"uid", "a"}, {"kind", "source"}, {"inputUid", "x"}}}},
                   {"links", QVariantList()},
                   {"positions", QVariantMap{{"a", QVariantMap{{"x", 10}, {"y", 20}}}}}};
        Recipe back;
        QString error;
        QVERIFY(RecipeJson::decode(RecipeJson::encode(r), &back, &error));
        QCOMPARE(back.graph, r.graph);
        // Phase 3 recipes have no graph field.
        auto old = RecipeJson::encode(Recipe());
        old.remove("graph");
        QVERIFY(RecipeJson::decode(old, &back, &error));
        QVERIFY(back.graph.isEmpty());
        QVariantList nodes;
        for (int i = 0; i <= GraphCompiler::kMaxNodes; ++i)
            nodes << QVariantMap{{"uid", QStringLiteral("n%1").arg(i)}};
        r.graph.insert("nodes", nodes);
        QVERIFY(!RecipeJson::decode(RecipeJson::encode(r), &back, &error));
    }
};

QTEST_GUILESS_MAIN(TestGraph)
#include "tst_graph.moc"
