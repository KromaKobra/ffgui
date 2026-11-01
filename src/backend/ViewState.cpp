#include "ViewState.h"

#include <algorithm>

namespace {

struct LayerInfo
{
    const char *key;
    const char *label;
    bool on;
};

// The Media View layers, as in the design's layer table.
constexpr LayerInfo kLayers[] = {
    { "waveform", "Waveform", true },
    { "spectrogram", "Spectrogram", true },
    { "processed", "Processed overlay", true },
    { "clipping", "Clipping marks", true },
    { "silence", "Silence regions", true },
    { "loudness", "Loudness curve", false },
    { "phase", "Phase / correlation lane", false },
    { "chapters", "Chapters and markers", true },
    { "ranges", "Step time ranges", true },
    { "cut", "Cut preview", true },
    { "transcript", "Transcript lane", false },
    { "dc", "DC offset line", false },
    { "grid", "Grid", false },
};

} // namespace

ViewState::ViewState(QObject *parent)
    : QObject(parent)
    , m_layers(defaultLayers())
{
}

QVariantMap ViewState::defaultLayers()
{
    QVariantMap m;
    for (const LayerInfo &l : kLayers)
        m.insert(QString::fromLatin1(l.key), l.on);
    return m;
}

QVariantList ViewState::layerList() const
{
    QVariantList list;
    for (const LayerInfo &l : kLayers)
        list << QVariantMap { { "key", QString::fromLatin1(l.key) }, { "label", QString::fromLatin1(l.label) } };
    return list;
}

void ViewState::setSpectrogramShare(double share)
{
    share = std::clamp(share, 0.15, 0.85);
    if (m_spectrogramShare == share)
        return;
    m_spectrogramShare = share;
    emit spectrogramShareChanged();
}

void ViewState::setChainGraph(bool graph)
{
    if (m_chainGraph == graph)
        return;
    m_chainGraph = graph;
    emit chainGraphChanged();
}

void ViewState::setLayer(const QString &key, bool on)
{
    if (!m_layers.contains(key) || m_layers.value(key).toBool() == on)
        return;
    m_layers.insert(key, on);
    emit layersChanged();
}

QVariantMap ViewState::save() const
{
    return { { "layers", m_layers }, { "spectrogramShare", m_spectrogramShare }, { "chainGraph", m_chainGraph } };
}

void ViewState::apply(const QVariantMap &view)
{
    QVariantMap layers = defaultLayers();
    const QVariantMap given = view.value(QStringLiteral("layers")).toMap();
    for (auto it = given.begin(); it != given.end(); ++it)
        if (layers.contains(it.key()))
            layers.insert(it.key(), it.value().toBool());
    if (layers != m_layers) {
        m_layers = layers;
        emit layersChanged();
    }
    setSpectrogramShare(view.value(QStringLiteral("spectrogramShare"), 0.45).toDouble());
    setChainGraph(view.value(QStringLiteral("chainGraph"), false).toBool());
}
