#include <catalog/Catalog.h>

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

using namespace ffgui;

class TestCatalog : public QObject
{
    Q_OBJECT

private slots:
    void builtInLoads()
    {
        const Catalog &c = Catalog::builtIn();
        QCOMPARE(c.categories.size(), 8);
        QVERIFY(c.steps.size() >= 28);
        QVERIFY(c.codecs.size() >= 9);
        QVERIFY(c.containers.size() >= 7);
        QVERIFY(c.preset(QStringLiteral("Podcast MP3")));
        QVERIFY(c.preset(QStringLiteral("Podcast voice")));
    }

    void everyStepIsComplete()
    {
        const Catalog &c = Catalog::builtIn();
        for (const StepType &s : c.steps) {
            QVERIFY2(c.categories.contains(s.category), qPrintable(s.key));
            QVERIFY2(!s.filters.isEmpty(), qPrintable(s.key));
            QVERIFY2(!s.tmpl.isEmpty(), qPrintable(s.key));
            QVERIFY2(!s.name.isEmpty() && !s.glyph.isEmpty() && !s.blurb.isEmpty(), qPrintable(s.key));
            // Every {placeholder} in the template is an option, unless a named
            // builder fills it.
            static const QRegularExpression placeholder(QStringLiteral("\\{(\\w+)\\}"));
            const QVariantMap defaults = s.defaults();
            for (auto m = placeholder.globalMatch(s.tmpl); m.hasNext();) {
                const QString key = m.next().captured(1);
                QVERIFY2(defaults.contains(key) || !s.builder.isEmpty(),
                         qPrintable(s.key + QStringLiteral(": {") + key + u'}'));
            }
            for (const OptionSpec &o : s.common + s.advanced) {
                if (o.type == u"number") {
                    QVERIFY2(o.min <= o.max, qPrintable(s.key + u'.' + o.key));
                    const double d = o.defaultValue.toDouble();
                    QVERIFY2(d >= o.min && d <= o.max, qPrintable(s.key + u'.' + o.key));
                } else if (o.type == u"choice") {
                    QVERIFY2(o.choices.contains(o.defaultValue.toString()), qPrintable(s.key + u'.' + o.key));
                }
            }
        }
    }

    // The ranges and defaults this build reports (ffmpeg -h filter=...).
    void rangesMatchFfmpeg()
    {
        const Catalog &c = Catalog::builtIn();
        auto option = [&](const char *step, const char *key) -> OptionSpec {
            for (const OptionSpec &o : c.step(QString::fromLatin1(step))->common + c.step(QString::fromLatin1(step))->advanced)
                if (o.key == QLatin1String(key))
                    return o;
            return {};
        };
        QCOMPARE(option("loudnorm", "I").min, -70.0);
        QCOMPARE(option("loudnorm", "I").max, -5.0);
        QCOMPARE(option("loudnorm", "LRA").max, 50.0);
        QCOMPARE(option("loudnorm", "TP").defaultValue.toDouble(), -2.0);
        QCOMPARE(option("denoise", "tn").defaultValue.toBool(), false);
        QCOMPARE(option("deesser", "f").max, 1.0);
        QCOMPARE(option("compressor", "makeup").min, 0.0);   // dB: 1..64 linear is 0..36 dB
        QCOMPARE(option("compressor", "makeup").max, 36.0);
    }

    void containersReferenceKnownCodecs()
    {
        const Catalog &c = Catalog::builtIn();
        for (const ContainerSpec &k : c.containers)
            for (const QString &codec : k.codecs)
                QVERIFY2(c.codec(codec), qPrintable(k.key + u':' + codec));
        for (const PresetSpec &p : c.presets) {
            if (p.kind != u"output")
                continue;
            QVERIFY2(c.container(p.container), qPrintable(p.name));
            QVERIFY2(c.container(p.container)->codecs.contains(p.codec), qPrintable(p.name));
        }
    }

    void sizeEstimates()
    {
        const Catalog &c = Catalog::builtIn();
        QCOMPARE(c.codec(QStringLiteral("libmp3lame"))->estimateKbps(2, 1411.2), 190.0);
        QCOMPARE(c.codec(QStringLiteral("aac"))->estimateKbps(160, 1411.2), 160.0);
        QCOMPARE(c.codec(QStringLiteral("pcm_s16le"))->estimateKbps(0, 1411.2), 1411.2);
        QVERIFY(qAbs(c.codec(QStringLiteral("flac"))->estimateKbps(8, 1000) - 556) < 0.001);
    }

    void userCatalogOverridesByKey()
    {
        QTemporaryDir dir;
        QFile f(dir.filePath(QStringLiteral("steps.json")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({ "categories": ["Mine"], "steps": [
            { "key": "gain", "name": "Volume", "category": "Mine", "glyph": "!", "blurb": "b",
              "requires": ["volume"], "template": "volume={v}", "common": [{ "key": "v", "min": 0, "max": 2, "default": 1 }] },
            { "key": "custom", "name": "Custom", "category": "Mine", "glyph": "?", "blurb": "b",
              "requires": ["anull"], "template": "anull" } ] })");
        f.close();
        QString error;
        Catalog user = Catalog::load(dir.path(), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        Catalog merged = Catalog::builtIn();
        const qsizetype before = merged.steps.size();
        merged.merge(user);
        QCOMPARE(merged.steps.size(), before + 1);
        QCOMPARE(merged.step(QStringLiteral("gain"))->name, QStringLiteral("Volume"));
        QVERIFY(merged.categories.contains(QStringLiteral("Mine")));
    }

    void brokenFileReportsError()
    {
        QTemporaryDir dir;
        QFile f(dir.filePath(QStringLiteral("codecs.json")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{ not json");
        f.close();
        QString error;
        const Catalog c = Catalog::load(dir.path(), &error);
        QVERIFY(!error.isEmpty());
        QVERIFY(c.codecs.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestCatalog)
#include "tst_catalog.moc"
