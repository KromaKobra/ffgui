#include "CatalogFacade.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>

using namespace ffgui;

namespace {

QVariantList optionList(const QList<OptionSpec> &options)
{
    QVariantList list;
    for (const OptionSpec &o : options)
        list << CatalogFacade::optionMap(o);
    return list;
}

QVariantMap codecMap(const CodecSpec &c)
{
    QVariantList rates;
    for (int r : c.sampleRates)
        rates << r;
    QVariantMap quality;
    if (c.hasQuality)
        quality = { { "kind", c.quality.kind }, { "label", c.quality.label }, { "flag", c.quality.flag },
                    { "min", c.quality.min }, { "max", c.quality.max }, { "step", c.quality.step },
                    { "default", c.quality.defaultValue } };
    return { { "key", c.key }, { "label", c.label }, { "codec", c.codec }, { "lossy", c.lossy },
             { "sampleFormats", c.sampleFormats }, { "sampleRates", rates }, { "depth", c.depth },
             { "quality", quality }, { "hasQuality", c.hasQuality } };
}

QVariantMap containerMap(const ContainerSpec &c)
{
    return { { "key", c.key }, { "label", c.label }, { "extension", c.extension }, { "muxer", c.muxer },
             { "codecs", c.codecs } };
}

} // namespace

CatalogFacade::CatalogFacade(QObject *parent)
    : QObject(parent)
    , m_cache(new HelpCache(nullptr, this))
    , m_catalog(Catalog::builtIn())
    , m_steps(new StepCatalogModel(this))
    , m_presets(new PresetsModel(this))
    , m_chainPresets(new PresetsModel(this))
{
    m_catalog.merge(Catalog::load(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/catalog")));
    m_catalog.merge(Catalog::load(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/presets")));
    connect(m_cache, &HelpCache::ready, this, [this](const QString &key, const QList<OptionSpec> &options) {
        m_help.insert(key, optionList(options));
        ++m_revision;
        emit changed();
    });
    rebuild();
}

void CatalogFacade::setCatalog(const Catalog &catalog)
{
    m_catalog = catalog;
    rebuild();
    ++m_revision;
    emit changed();
}

QVariantMap CatalogFacade::optionMap(const OptionSpec &o)
{
    return { { "key", o.key }, { "label", o.label }, { "unit", o.unit }, { "type", o.type }, { "min", o.min },
             { "max", o.max }, { "step", o.step }, { "default", o.defaultValue }, { "choices", o.choices },
             { "runtime", o.runtime }, { "help", o.help } };
}

void CatalogFacade::rebuild()
{
    QList<QVariantMap> steps;
    for (const StepType &s : std::as_const(m_catalog.steps)) {
        steps << QVariantMap { { "key", s.key }, { "name", s.name }, { "category", s.category },
                               { "glyph", s.glyph }, { "blurb", s.blurb }, { "filter", s.filters.join(" + ") },
                               { "keywords", s.keywords }, { "timeline", s.timeline }, { "available", stepProblem(s).isEmpty() },
                               { "reason", stepProblem(s) } };
    }
    m_steps->setRows(steps);

    QList<QVariantMap> output, chain;
    for (const PresetSpec &p : std::as_const(m_catalog.presets)) {
        if (p.kind == u"output") {
            const CodecSpec *codec = m_catalog.codec(p.codec);
            const auto *container = m_catalog.container(p.container);
            const bool available = encoderAvailable(p.codec) && container && muxerAvailable(container->muxer);
            output << QVariantMap { { "name", p.name }, { "kind", p.kind }, { "description", p.description },
                                    { "builtIn", p.builtIn }, { "container", p.container }, { "codec", p.codec },
                                    { "codecLabel", codec ? codec->label : p.codec }, { "quality", p.quality },
                                    { "rate", p.rate }, { "depth", p.depth }, { "channels", p.channels }, { "available", available }, { "reason", available ? QString() : tr("Encoder or container unavailable in this build") } };
        } else {
            QVariantList steps;
            for (const ChainStepPreset &st : p.steps)
                steps << QVariantMap { { "type", st.type }, { "params", st.params } };
            QString problem;
            for (const auto &st : p.steps) {
                const auto *s = m_catalog.step(st.type);
                if (s && !stepProblem(*s).isEmpty()) { problem = stepProblem(*s); break; }
            }
            chain << QVariantMap { { "name", p.name }, { "kind", p.kind }, { "description", p.description },
                                   { "builtIn", p.builtIn }, { "steps", steps }, { "available", problem.isEmpty() }, { "reason", problem } };
        }
    }
    m_presets->setRows(output);
    m_chainPresets->setRows(chain);
}

QVariantList CatalogFacade::containers() const
{
    QVariantList list;
    for (const ContainerSpec &c : m_catalog.containers)
        if (muxerAvailable(c.muxer)) list << containerMap(c);
    return list;
}

QVariantMap CatalogFacade::step(const QString &key) const
{
    const StepType *s = m_catalog.step(key);
    if (!s)
        return {};
    const auto help = m_help.value(QStringLiteral("filter:") + s->primaryFilter());
    auto curated = [&](const QList<OptionSpec> &options) {
        auto list = optionList(options);
        for (auto &option : list) {
            auto map = option.toMap();
            for (const auto &raw : help)
                if (raw.toMap().value(QStringLiteral("key")) == map.value(QStringLiteral("key")))
                    map.insert(QStringLiteral("runtime"), s->builder.isEmpty() && raw.toMap().value(QStringLiteral("runtime")).toBool());
            option = map;
        }
        return list;
    };
    return { { "key", s->key }, { "name", s->name }, { "category", s->category }, { "glyph", s->glyph },
             { "blurb", s->blurb }, { "filter", s->filters.join(" + ") }, { "filters", s->filters },
             { "primaryFilter", s->primaryFilter() }, { "timeline", s->timeline }, { "builder", s->builder },
             { "template", s->tmpl }, { "library", s->library }, { "common", curated(s->common) },
             { "advanced", curated(s->advanced) }, { "fixed", s->fixed } };
}

QVariantMap CatalogFacade::codec(const QString &key) const
{
    const CodecSpec *c = m_catalog.codec(key);
    return c ? codecMap(*c) : QVariantMap();
}

QVariantMap CatalogFacade::container(const QString &key) const
{
    const ContainerSpec *c = m_catalog.container(key);
    return c ? containerMap(*c) : QVariantMap();
}

QVariantList CatalogFacade::codecsFor(const QString &container) const
{
    QVariantList list;
    if (const ContainerSpec *c = m_catalog.container(container))
        for (const QString &key : c->codecs)
            if (const CodecSpec *codec = m_catalog.codec(key))
                if (encoderAvailable(codec->key)) list << codecMap(*codec);
    return list;
}

QVariantMap CatalogFacade::preset(const QString &name) const
{
    const int row = m_presets->find(QStringLiteral("name"), name);
    if (row >= 0)
        return m_presets->get(row);
    return m_chainPresets->get(m_chainPresets->find(QStringLiteral("name"), name));
}

void CatalogFacade::setCapabilities(const BuildInfo &info)
{
    m_build = info;
    m_detected = true;
    m_cache->reset();
    m_help.clear();
    rebuild();
    ++m_revision;
    emit changed();
}

bool CatalogFacade::encoderAvailable(const QString &name) const
{
    return !m_detected || m_build.audioEncoders.contains(name);
}

bool CatalogFacade::muxerAvailable(const QString &name) const
{
    return !m_detected || m_build.muxers.contains(name);
}

QString CatalogFacade::stepProblem(const StepType &step) const
{
    if (!m_detected) return {};
    for (const QString &filter : step.filters)
        if (!m_build.filters.contains(filter))
            return tr("Needs %1, which this build lacks").arg(step.library.isEmpty() ? filter : step.library);
    return {};
}

QVariantList CatalogFacade::options(const QString &kind, const QString &name) const
{
    m_cache->request(kind, name);
    return m_help.value(kind + u':' + name);
}

bool CatalogFacade::persist(const Catalog &catalog)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/presets");
    QDir().mkpath(dir);
    QSaveFile file(dir + QStringLiteral("/presets.json"));
    const QByteArray data = QJsonDocument(catalog.presetsJson(true)).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) return false;
    setCatalog(catalog);
    return true;
}

bool CatalogFacade::savePreset(const QString &name, const QString &kind, const QVariantMap &output, const QList<QVariantMap> &nodes)
{
    if (name.trimmed().isEmpty() || (kind == u"output" && output.isEmpty()) || (kind == u"chain" && nodes.isEmpty())) return false;
    PresetSpec p;
    p.name = name.trimmed(); p.kind = kind; p.builtIn = false;
    if (kind == u"output") {
        p.container = output.value(QStringLiteral("container")).toString(); p.codec = output.value(QStringLiteral("codec")).toString();
        p.quality = output.value(QStringLiteral("quality")).toDouble(); p.rate = output.value(QStringLiteral("rate")).toInt();
        p.depth = output.value(QStringLiteral("depth")).toInt(); p.channels = output.value(QStringLiteral("channels")).toInt();
        for (const auto &key : { QStringLiteral("encoderOptions"), QStringLiteral("muxerOptions"), QStringLiteral("metadata") })
            p.settings.insert(key, output.value(key));
    } else for (const auto &node : nodes) {
        QVariantMap settings;
        for (const auto &key : { QStringLiteral("bypass"), QStringLiteral("hasRange"), QStringLiteral("rangeStart"), QStringLiteral("rangeEnd") })
            settings.insert(key, node.value(key));
        p.steps << ChainStepPreset { node.value(QStringLiteral("type")).toString(), node.value(QStringLiteral("params")).toMap(), settings };
    }
    Catalog additions; additions.presets << p;
    Catalog next = m_catalog; next.merge(additions);
    return persist(next);
}

bool CatalogFacade::importPresets(const QUrl &url)
{
    if (!url.isLocalFile()) return false;
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) return false;
    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return false;
    const auto imported = Catalog::presetsFromJson(doc.object());
    if (imported.presets.isEmpty()) return false;
    // Validate everything before modifying the user's library.
    for (const auto &p : imported.presets) {
        if (p.kind == u"output") {
            const auto *c = m_catalog.container(p.container);
            if (!c || !m_catalog.codec(p.codec) || !c->codecs.contains(p.codec)) return false;
        } else for (const auto &s : p.steps) if (!m_catalog.step(s.type)) return false;
    }
    auto next = m_catalog; next.merge(imported);
    return persist(next);
}

bool CatalogFacade::exportPresets(const QUrl &url) const
{
    if (!url.isLocalFile()) return false;
    QSaveFile file(url.toLocalFile());
    const auto data = QJsonDocument(m_catalog.presetsJson()).toJson();
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}
