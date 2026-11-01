#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

// The built-in step, encoder, container and preset definitions (JSON in qrc),
// plus a user directory that can add or override entries by key. Availability
// against the installed ffmpeg is resolved later, by the capabilities.
namespace ffgui {

// One option of a filter, encoder or muxer. Catalog entries fill the curated
// fields; HelpParser fills the same struct from `ffmpeg -h filter=...`.
struct OptionSpec
{
    QString key;
    QString label;              // plain name; empty for options straight from -h
    QString unit;
    QString type = QStringLiteral("number");   // number, choice, bool, string
    double min = 0;
    double max = 0;
    double step = 0;
    QVariant defaultValue;
    QStringList choices;
    bool runtime = false;       // accepts live commands while running (-h shows T)
    QString help;
};

struct StepType
{
    QString key;
    QString name;
    QString category;
    QString glyph;
    QString blurb;
    QStringList filters;        // filters it needs (JSON "requires")
    QString library;            // shown when one of them is missing
    QString timeline;           // "", "trim", "pad" or "speed"
    QString builder;            // "" means the template builder
    QString tmpl;
    QStringList keywords;
    QList<OptionSpec> common;
    QList<OptionSpec> advanced;
    QJsonObject out;            // known output format: rate, fmt ("planar" = make planar), layout

    // The first filter, the one the Inspector footer names and links to.
    QString primaryFilter() const { return filters.isEmpty() ? QString() : filters.first(); }
    QVariantMap defaults() const;
};

struct CodecSpec
{
    struct Quality
    {
        QString kind;           // vbr, bitrate or level
        QString label;
        QString flag;
        double min = 0;
        double max = 0;
        double step = 1;
        double defaultValue = 0;
    };

    QString key;                // encoder name
    QString label;
    QString codec;              // codec name ffprobe reports
    bool lossy = false;
    QStringList sampleFormats;
    QList<int> sampleRates;     // empty = any
    int depth = 0;              // PCM only
    bool hasQuality = false;
    Quality quality;
    QString sizeModel;          // table, bitrate, pcmRatio, pcm
    QList<double> sizeTable;
    double ratioBase = 1;
    double ratioPerLevel = 0;

    // Estimated output bitrate in kb/s for a quality value, given the PCM rate.
    double estimateKbps(double qualityValue, double pcmKbps) const;
};

struct ContainerSpec
{
    QString key;
    QString label;
    QString muxer;
    QString extension;
    QStringList codecs;
};

struct ChainStepPreset
{
    QString type;
    QVariantMap params;
    QVariantMap settings;
};

struct PresetSpec
{
    QString name;
    QString kind;               // output or chain
    QString description;
    bool builtIn = true;
    // output
    QString container;
    QString codec;
    double quality = 0;
    int rate = 0;
    int depth = 0;
    int channels = 0;
    QVariantMap settings;
    // chain
    QList<ChainStepPreset> steps;
};

class Catalog
{
public:
    // Loads steps.json, codecs.json and presets.json from `dir` (a path or a
    // ":/" resource directory). Missing files leave their part empty; a broken
    // file sets `error` and is skipped.
    static Catalog load(const QString &dir, QString *error = nullptr);

    // The catalog compiled into the binary.
    static const Catalog &builtIn();
    static Catalog presetsFromJson(const QJsonObject &json, bool builtIn = false);
    QJsonObject presetsJson(bool userOnly = false) const;

    // Adds or replaces entries by key/name, e.g. from a user directory.
    void merge(const Catalog &other);

    QStringList categories;
    QList<StepType> steps;
    QList<CodecSpec> codecs;
    QList<ContainerSpec> containers;
    QList<PresetSpec> presets;

    const StepType *step(const QString &key) const;
    const CodecSpec *codec(const QString &key) const;
    const ContainerSpec *container(const QString &key) const;
    const PresetSpec *preset(const QString &name) const;
};

} // namespace ffgui
