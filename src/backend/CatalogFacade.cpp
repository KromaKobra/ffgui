#include "CatalogFacade.h"

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
    , m_catalog(Catalog::builtIn())
    , m_steps(new StepCatalogModel(this))
    , m_presets(new PresetsModel(this))
    , m_chainPresets(new PresetsModel(this))
{
    rebuild();
}

void CatalogFacade::setCatalog(const Catalog &catalog)
{
    m_catalog = catalog;
    rebuild();
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
                               { "keywords", s.keywords }, { "timeline", s.timeline }, { "available", true },
                               { "reason", QString() } };
    }
    m_steps->setRows(steps);

    QList<QVariantMap> output, chain;
    for (const PresetSpec &p : std::as_const(m_catalog.presets)) {
        if (p.kind == u"output") {
            const CodecSpec *codec = m_catalog.codec(p.codec);
            output << QVariantMap { { "name", p.name }, { "kind", p.kind }, { "description", p.description },
                                    { "builtIn", p.builtIn }, { "container", p.container }, { "codec", p.codec },
                                    { "codecLabel", codec ? codec->label : p.codec }, { "quality", p.quality },
                                    { "rate", p.rate }, { "depth", p.depth }, { "channels", p.channels } };
        } else {
            QVariantList steps;
            for (const ChainStepPreset &st : p.steps)
                steps << QVariantMap { { "type", st.type }, { "params", st.params } };
            chain << QVariantMap { { "name", p.name }, { "kind", p.kind }, { "description", p.description },
                                   { "builtIn", p.builtIn }, { "steps", steps } };
        }
    }
    m_presets->setRows(output);
    m_chainPresets->setRows(chain);
}

QVariantList CatalogFacade::containers() const
{
    QVariantList list;
    for (const ContainerSpec &c : m_catalog.containers)
        list << containerMap(c);
    return list;
}

QVariantMap CatalogFacade::step(const QString &key) const
{
    const StepType *s = m_catalog.step(key);
    if (!s)
        return {};
    return { { "key", s->key }, { "name", s->name }, { "category", s->category }, { "glyph", s->glyph },
             { "blurb", s->blurb }, { "filter", s->filters.join(" + ") }, { "filters", s->filters },
             { "primaryFilter", s->primaryFilter() }, { "timeline", s->timeline }, { "builder", s->builder },
             { "template", s->tmpl }, { "library", s->library }, { "common", optionList(s->common) },
             { "advanced", optionList(s->advanced) } };
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
                list << codecMap(*codec);
    return list;
}

QVariantMap CatalogFacade::preset(const QString &name) const
{
    const int row = m_presets->find(QStringLiteral("name"), name);
    if (row >= 0)
        return m_presets->get(row);
    return m_chainPresets->get(m_chainPresets->find(QStringLiteral("name"), name));
}
