#pragma once

#include "Models.h"

#include <catalog/Catalog.h>
#include <caps/BuildInfo.h>
#include <caps/HelpCache.h>

#include <QQmlEngine>

// The step types, encoders, containers and presets ffgui knows, for the step
// picker, the Inspector and the Output bar. Options come back as maps:
// { key, label, unit, type, min, max, step, default, choices, runtime, help }.
class CatalogFacade : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Catalog)
    QML_UNCREATABLE("Owned by App")

    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(QStringList categories READ categories NOTIFY changed)
    Q_PROPERTY(StepCatalogModel *steps READ steps CONSTANT)
    Q_PROPERTY(PresetsModel *presets READ presets CONSTANT)
    Q_PROPERTY(PresetsModel *chainPresets READ chainPresets CONSTANT)
    // [{ key, label, extension, muxer, codecs }]
    Q_PROPERTY(QVariantList containers READ containers NOTIFY changed)

public:
    explicit CatalogFacade(QObject *parent = nullptr);

    int revision() const { return m_revision; }
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
    Q_INVOKABLE QVariantList filterOptions(const QString &name) const { return options(QStringLiteral("filter"), name); }
    Q_INVOKABLE QVariantList encoderOptions(const QString &name) const { return options(QStringLiteral("encoder"), name); }
    Q_INVOKABLE QVariantList muxerOptions(const QString &name) const { return options(QStringLiteral("muxer"), name); }

    const ffgui::Catalog &catalog() const { return m_catalog; }
    void setCatalog(const ffgui::Catalog &catalog);

    void setLog(ffgui::LogStore *log) { m_cache->setLog(log); }
    void setCapabilities(const ffgui::BuildInfo &info);
    bool filterTimeline(const QString &name) const { return m_build.filters.value(name).timeline; }
    QString stepProblem(const ffgui::StepType &step) const;

    bool savePreset(const QString &name, const QString &kind, const QVariantMap &output, const QList<QVariantMap> &nodes);
    Q_INVOKABLE bool importPresets(const QUrl &file);
    Q_INVOKABLE bool exportPresets(const QUrl &file) const;

    static QVariantMap optionMap(const ffgui::OptionSpec &option);

signals:
    void changed();

private:
    void rebuild();
    QVariantList options(const QString &kind, const QString &name) const;
    bool persist(const ffgui::Catalog &catalog);
    bool encoderAvailable(const QString &name) const;
    bool muxerAvailable(const QString &name) const;
    int m_revision = 0;
    ffgui::BuildInfo m_build;
    bool m_detected = false;
    ffgui::HelpCache *m_cache;

    ffgui::Catalog m_catalog;
    QHash<QString, QVariantList> m_help;
    StepCatalogModel *m_steps;
    PresetsModel *m_presets;
    PresetsModel *m_chainPresets;
};
