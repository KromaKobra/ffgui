#pragma once

#include "Models.h"

#include <catalog/Catalog.h>

#include <QQmlEngine>

// The step types, encoders, containers and presets ffgui knows, for the step
// picker, the Inspector and the Output bar. Options come back as maps:
// { key, label, unit, type, min, max, step, default, choices, runtime, help }.
class CatalogFacade : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Catalog)
    QML_UNCREATABLE("Owned by App")

    Q_PROPERTY(QStringList categories READ categories CONSTANT)
    Q_PROPERTY(StepCatalogModel *steps READ steps CONSTANT)
    Q_PROPERTY(PresetsModel *presets READ presets CONSTANT)
    Q_PROPERTY(PresetsModel *chainPresets READ chainPresets CONSTANT)
    // [{ key, label, extension, muxer, codecs }]
    Q_PROPERTY(QVariantList containers READ containers NOTIFY changed)

public:
    explicit CatalogFacade(QObject *parent = nullptr);

    QStringList categories() const { return m_catalog.categories; }
    StepCatalogModel *steps() const { return m_steps; }
    PresetsModel *presets() const { return m_presets; }
    PresetsModel *chainPresets() const { return m_chainPresets; }
    QVariantList containers() const;

    // { key, name, category, glyph, blurb, filter, filters, timeline, builder,
    //   template, common: [option], advanced: [option], library }
    Q_INVOKABLE QVariantMap step(const QString &key) const;
    Q_INVOKABLE QVariantMap codec(const QString &key) const;
    Q_INVOKABLE QVariantMap container(const QString &key) const;
    // The codecs a container accepts, as codec maps.
    Q_INVOKABLE QVariantList codecsFor(const QString &container) const;
    Q_INVOKABLE QVariantMap preset(const QString &name) const;
    // Every option of a filter, encoder or muxer, from `ffmpeg -h ...=name`.
    // Empty until the help output has been read.
    Q_INVOKABLE QVariantList filterOptions(const QString &name) const { return m_help.value(u"filter:" + name); }
    Q_INVOKABLE QVariantList encoderOptions(const QString &name) const { return m_help.value(u"encoder:" + name); }
    Q_INVOKABLE QVariantList muxerOptions(const QString &name) const { return m_help.value(u"muxer:" + name); }

    const ffgui::Catalog &catalog() const { return m_catalog; }
    void setCatalog(const ffgui::Catalog &catalog);

    static QVariantMap optionMap(const ffgui::OptionSpec &option);

signals:
    void changed();

private:
    void rebuild();

    ffgui::Catalog m_catalog;
    QHash<QString, QVariantList> m_help;
    StepCatalogModel *m_steps;
    PresetsModel *m_presets;
    PresetsModel *m_chainPresets;
};
