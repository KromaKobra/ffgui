#pragma once

#include "RowModel.h"

#include <QSortFilterProxyModel>

// The list models App exposes. Each is a RowModel with a fixed set of roles;
// the roles are the contract with the QML delegates (see Models.cpp).

#define FFGUI_ROW_MODEL(Name)                                   \
    class Name : public RowModel                                \
    {                                                           \
        Q_OBJECT                                                \
        QML_ELEMENT                                             \
        QML_UNCREATABLE("Models are owned by App")              \
    public:                                                     \
        explicit Name(QObject *parent = nullptr);               \
    };

FFGUI_ROW_MODEL(InputsModel)        // one row per input (file, folder, URL, device, generator)
FFGUI_ROW_MODEL(StepsModel)         // the chain as a row: the graph's single path
FFGUI_ROW_MODEL(GraphModel)         // nodes of the chain graph (graph mode)
FFGUI_ROW_MODEL(OutputsModel)       // output targets
FFGUI_ROW_MODEL(LogModel)           // ffmpeg/ffprobe output and ffgui's own messages
FFGUI_ROW_MODEL(StepCatalogModel)   // every step type, with availability in this build
FFGUI_ROW_MODEL(PresetsModel)       // output and chain presets, built-in and user
FFGUI_ROW_MODEL(LibrariesModel)     // optional libraries and what each unlocks
FFGUI_ROW_MODEL(TranscriptModel)    // transcript segments
FFGUI_ROW_MODEL(DevicesModel)       // capture devices
FFGUI_ROW_MODEL(DestinationsModel)  // stream-out destinations

#undef FFGUI_ROW_MODEL

// Render jobs, one per file x output. The actions are requests; the render
// queue carries them out.
class QueueModel : public RowModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Models are owned by App")

    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)

public:
    explicit QueueModel(QObject *parent = nullptr);

    bool paused() const { return m_paused; }
    void setPaused(bool paused);

    Q_INVOKABLE void pauseAll() { emit actionRequested(QStringLiteral("pause"), -1); }
    Q_INVOKABLE void resumeAll() { emit actionRequested(QStringLiteral("resume"), -1); }
    Q_INVOKABLE void clearFinished() { emit actionRequested(QStringLiteral("clearFinished"), -1); }
    Q_INVOKABLE void retry(int row) { emit actionRequested(QStringLiteral("retry"), row); }
    Q_INVOKABLE void cancel(int row) { emit actionRequested(QStringLiteral("cancel"), row); }
    Q_INVOKABLE void moveUp(int row) { emit actionRequested(QStringLiteral("up"), row); }
    Q_INVOKABLE void moveDown(int row) { emit actionRequested(QStringLiteral("down"), row); }

signals:
    void pausedChanged();
    void actionRequested(const QString &action, int row);

private:
    bool m_paused = false;
};

// Filters a model by `category` and a free-text `search` over plain and
// ffmpeg names, so "noise" and "afftdn" both find Remove noise.
class StepFilter : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString category READ category WRITE setCategory NOTIFY categoryChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY searchChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit StepFilter(QObject *parent = nullptr);

    QString category() const { return m_category; }
    void setCategory(const QString &category);
    QString search() const { return m_search; }
    void setSearch(const QString &search);
    int count() const { return rowCount(); }

    Q_INVOKABLE QVariantMap get(int row) const;

signals:
    void categoryChanged();
    void searchChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_category;
    QString m_search;
};
