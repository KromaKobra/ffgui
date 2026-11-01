#include "Catalog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>
#include <cmath>

namespace ffgui {

namespace {

QStringList strings(const QJsonValue &value)
{
    QStringList out;
    for (const QJsonValue &v : value.toArray())
        out << v.toString();
    return out;
}

OptionSpec parseOption(const QJsonObject &o)
{
    OptionSpec spec;
    spec.key = o.value(u"key").toString();
    spec.label = o.value(u"label").toString(spec.key);
    spec.unit = o.value(u"unit").toString();
    spec.type = o.value(u"type").toString(QStringLiteral("number"));
    spec.min = o.value(u"min").toDouble();
    spec.max = o.value(u"max").toDouble();
    spec.step = o.value(u"step").toDouble(spec.type == u"number" ? 1 : 0);
    spec.defaultValue = o.value(u"default").toVariant();
    spec.choices = strings(o.value(u"choices"));
    spec.runtime = o.value(u"runtime").toBool();
    spec.help = o.value(u"help").toString();
    return spec;
}

QList<OptionSpec> parseOptions(const QJsonValue &value)
{
    QList<OptionSpec> out;
    for (const QJsonValue &v : value.toArray())
        out << parseOption(v.toObject());
    return out;
}

bool readJson(const QString &path, QJsonObject *out, QString *error)
{
    QFile file(path);
    if (!file.exists())
        return false;
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("%1: %2").arg(path, file.errorString());
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!doc.isObject()) {
        if (error)
            *error = QStringLiteral("%1: %2 at offset %3").arg(path, parseError.errorString()).arg(parseError.offset);
        return false;
    }
    *out = doc.object();
    return true;
}

template <typename T, typename Key>
void mergeBy(QList<T> &into, const QList<T> &from, Key key)
{
    for (const T &item : from) {
        const auto it = std::find_if(into.begin(), into.end(), [&](const T &x) { return key(x) == key(item); });
        if (it != into.end())
            *it = item;
        else
            into << item;
    }
}

template <typename T, typename Key>
const T *findBy(const QList<T> &list, const QString &value, Key key)
{
    const auto it = std::find_if(list.begin(), list.end(), [&](const T &x) { return key(x) == value; });
    return it == list.end() ? nullptr : &*it;
}

} // namespace

QVariantMap StepType::defaults() const
{
    QVariantMap p;
    for (const OptionSpec &o : common)
        p.insert(o.key, o.defaultValue);
    for (const OptionSpec &o : advanced)
        p.insert(o.key, o.defaultValue);
    return p;
}

double CodecSpec::estimateKbps(double qualityValue, double pcmKbps) const
{
    if (sizeModel == u"table" && !sizeTable.isEmpty()) {
        const qsizetype i = std::clamp<qsizetype>(std::lround(qualityValue), 0, sizeTable.size() - 1);
        return sizeTable.at(i);
    }
    if (sizeModel == u"bitrate")
        return qualityValue;
    if (sizeModel == u"pcmRatio")
        return pcmKbps * (ratioBase + qualityValue * ratioPerLevel);
    return pcmKbps;
}

Catalog Catalog::load(const QString &dir, QString *error)
{
    Catalog c;
    QJsonObject root;

    if (readJson(dir + QStringLiteral("/steps.json"), &root, error)) {
        c.categories = strings(root.value(u"categories"));
        for (const QJsonValue &v : root.value(u"steps").toArray()) {
            const QJsonObject o = v.toObject();
            StepType s;
            s.key = o.value(u"key").toString();
            s.name = o.value(u"name").toString(s.key);
            s.category = o.value(u"category").toString();
            s.glyph = o.value(u"glyph").toString();
            s.blurb = o.value(u"blurb").toString();
            s.filters = strings(o.value(u"requires"));
            s.library = o.value(u"library").toString();
            s.timeline = o.value(u"timeline").toString();
            s.builder = o.value(u"builder").toString();
            s.tmpl = o.value(u"template").toString();
            s.keywords = strings(o.value(u"keywords"));
            s.common = parseOptions(o.value(u"common"));
            s.advanced = parseOptions(o.value(u"advanced"));
            s.out = o.value(u"out").toObject();
            if (!s.key.isEmpty())
                c.steps << s;
        }
    }

    if (readJson(dir + QStringLiteral("/codecs.json"), &root, error)) {
        for (const QJsonValue &v : root.value(u"codecs").toArray()) {
            const QJsonObject o = v.toObject();
            CodecSpec s;
            s.key = o.value(u"key").toString();
            s.label = o.value(u"label").toString(s.key);
            s.codec = o.value(u"codec").toString(s.key);
            s.lossy = o.value(u"lossy").toBool();
            s.sampleFormats = strings(o.value(u"sampleFormats"));
            for (const QJsonValue &r : o.value(u"sampleRates").toArray())
                s.sampleRates << r.toInt();
            s.depth = o.value(u"depth").toInt();
            const QJsonObject q = o.value(u"quality").toObject();
            s.hasQuality = !q.isEmpty();
            s.quality = { q.value(u"kind").toString(), q.value(u"label").toString(), q.value(u"flag").toString(),
                          q.value(u"min").toDouble(), q.value(u"max").toDouble(), q.value(u"step").toDouble(1),
                          q.value(u"default").toDouble() };
            const QJsonObject size = o.value(u"size").toObject();
            s.sizeModel = size.value(u"model").toString(QStringLiteral("pcm"));
            for (const QJsonValue &t : size.value(u"table").toArray())
                s.sizeTable << t.toDouble();
            s.ratioBase = size.value(u"base").toDouble(1);
            s.ratioPerLevel = size.value(u"perLevel").toDouble();
            if (!s.key.isEmpty())
                c.codecs << s;
        }
        for (const QJsonValue &v : root.value(u"containers").toArray()) {
            const QJsonObject o = v.toObject();
            ContainerSpec s;
            s.key = o.value(u"key").toString();
            s.label = o.value(u"label").toString(s.key);
            s.muxer = o.value(u"muxer").toString(s.key);
            s.extension = o.value(u"extension").toString(s.key);
            s.codecs = strings(o.value(u"codecs"));
            if (!s.key.isEmpty())
                c.containers << s;
        }
    }

    if (readJson(dir + QStringLiteral("/presets.json"), &root, error))
        c.merge(presetsFromJson(root, dir.startsWith(u':')));

    return c;
}

const Catalog &Catalog::builtIn()
{
    static const Catalog catalog = load(QStringLiteral(":/ffgui/catalog"));
    return catalog;
}

void Catalog::merge(const Catalog &other)
{
    for (const QString &cat : other.categories)
        if (!categories.contains(cat))
            categories << cat;
    mergeBy(steps, other.steps, [](const StepType &s) { return s.key; });
    mergeBy(codecs, other.codecs, [](const CodecSpec &s) { return s.key; });
    mergeBy(containers, other.containers, [](const ContainerSpec &s) { return s.key; });
    mergeBy(presets, other.presets, [](const PresetSpec &s) { return s.kind + u'/' + s.name; });
}

const StepType *Catalog::step(const QString &key) const
{
    return findBy(steps, key, [](const StepType &s) { return s.key; });
}

const CodecSpec *Catalog::codec(const QString &key) const
{
    return findBy(codecs, key, [](const CodecSpec &s) { return s.key; });
}

const ContainerSpec *Catalog::container(const QString &key) const
{
    return findBy(containers, key, [](const ContainerSpec &s) { return s.key; });
}

const PresetSpec *Catalog::preset(const QString &name) const
{
    return findBy(presets, name, [](const PresetSpec &s) { return s.name; });
}

} // namespace ffgui

namespace ffgui {
Catalog Catalog::presetsFromJson(const QJsonObject &root, bool builtIn)
{
    Catalog c;
        for (const QJsonValue &v : root.value(u"output").toArray()) {
            const QJsonObject o = v.toObject();
            PresetSpec p;
            p.kind = QStringLiteral("output");
            p.name = o.value(u"name").toString();
            p.description = o.value(u"description").toString();
            p.container = o.value(u"container").toString();
            p.codec = o.value(u"codec").toString();
            p.quality = o.value(u"quality").toDouble();
            p.rate = o.value(u"rate").toInt();
            p.depth = o.value(u"depth").toInt();
            p.channels = o.value(u"channels").toInt();
            p.settings = o.value(u"settings").toObject().toVariantMap();
            if (!p.name.isEmpty())
                c.presets << p;
        }
        for (const QJsonValue &v : root.value(u"chain").toArray()) {
            const QJsonObject o = v.toObject();
            PresetSpec p;
            p.kind = QStringLiteral("chain");
            p.name = o.value(u"name").toString();
            p.description = o.value(u"description").toString();
            for (const QJsonValue &st : o.value(u"steps").toArray()) {
                const QJsonObject so = st.toObject();
                p.steps << ChainStepPreset { so.value(u"type").toString(), so.value(u"params").toObject().toVariantMap(), so.value(u"settings").toObject().toVariantMap() };
            }
            if (!p.name.isEmpty())
                c.presets << p;
        }
    for (auto &p : c.presets) p.builtIn = builtIn;
    return c;
}
QJsonObject Catalog::presetsJson(bool userOnly) const
{
    QJsonArray outputs, chains;
    for (const auto &p : presets) {
        if (userOnly && p.builtIn) continue;
        QJsonObject o { { "name", p.name }, { "description", p.description } };
        if (p.kind == u"output") {
            o.insert(QStringLiteral("container"), p.container); o.insert(QStringLiteral("codec"), p.codec);
            o.insert(QStringLiteral("quality"), p.quality); o.insert(QStringLiteral("rate"), p.rate);
            o.insert(QStringLiteral("depth"), p.depth); o.insert(QStringLiteral("channels"), p.channels);
            if (!p.settings.isEmpty()) o.insert(QStringLiteral("settings"), QJsonObject::fromVariantMap(p.settings));
            outputs << o;
        } else {
            QJsonArray steps;
            for (const auto &step : p.steps) steps << QJsonObject { { "type", step.type }, { "params", QJsonObject::fromVariantMap(step.params) }, { "settings", QJsonObject::fromVariantMap(step.settings) } };
            o.insert(QStringLiteral("steps"), steps); chains << o;
        }
    }
    return { { "output", outputs }, { "chain", chains } };
}
} // namespace ffgui
