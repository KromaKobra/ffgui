#pragma once

#include <QQmlEngine>
#include <QVariantMap>

// View settings that belong to a workspace rather than to the project: which
// Media View layers are on, how much of each lane the spectrogram takes, and
// whether the Chain shows a row or a graph. Workspaces save and apply them.
class ViewState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by App")

    Q_PROPERTY(QVariantMap layers READ layers NOTIFY layersChanged)
    Q_PROPERTY(double spectrogramShare READ spectrogramShare WRITE setSpectrogramShare NOTIFY spectrogramShareChanged)
    Q_PROPERTY(bool chainGraph READ chainGraph WRITE setChainGraph NOTIFY chainGraphChanged)
    // [{ key, label }] in menu order.
    Q_PROPERTY(QVariantList layerList READ layerList CONSTANT)

public:
    explicit ViewState(QObject *parent = nullptr);

    QVariantMap layers() const { return m_layers; }
    double spectrogramShare() const { return m_spectrogramShare; }
    void setSpectrogramShare(double share);
    bool chainGraph() const { return m_chainGraph; }
    void setChainGraph(bool graph);
    QVariantList layerList() const;

    Q_INVOKABLE bool layer(const QString &key) const { return m_layers.value(key).toBool(); }
    Q_INVOKABLE void setLayer(const QString &key, bool on);
    // { layers, spectrogramShare, chainGraph }; missing keys take defaults.
    Q_INVOKABLE QVariantMap save() const;
    Q_INVOKABLE void apply(const QVariantMap &view);

    static QVariantMap defaultLayers();

signals:
    void layersChanged();
    void spectrogramShareChanged();
    void chainGraphChanged();

private:
    QVariantMap m_layers;
    double m_spectrogramShare = 0.45;
    bool m_chainGraph = false;
};
