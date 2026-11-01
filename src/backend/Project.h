#pragma once

#include "Models.h"

#include <probe/ProbeResult.h>
#include <project/Recipe.h>
#include <functional>

#include <QHash>
#include <QPoint>
#include <QQmlEngine>
#include <QUndoStack>
#include <QUrl>

#include <memory>

class CoverStore;
class CatalogFacade;
class Selection;
class ProjectEdit;
namespace ffgui { class LogStore; }

// The open project: inputs, the processing chain and the output targets
// (design: "a project = inputs + chain + outputs, saved as a recipe file").
// Nothing touches a source file; every change is undoable.
class Project : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by App")

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QUrl fileUrl READ fileUrl NOTIFY fileUrlChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY modifiedChanged)

    Q_PROPERTY(InputsModel *inputs READ inputs CONSTANT)
    Q_PROPERTY(StepsModel *steps READ steps CONSTANT)
    Q_PROPERTY(GraphModel *graph READ graph CONSTANT)
    Q_PROPERTY(GraphModel *graphNodes READ graphNodes CONSTANT)
    Q_PROPERTY(RowModel *graphLinks READ graphLinks CONSTANT)
    Q_PROPERTY(bool graphActive READ graphActive NOTIFY graphChanged)
    Q_PROPERTY(bool canUseRow READ canUseRow NOTIFY graphChanged)
    Q_PROPERTY(QString graphError READ graphError NOTIFY graphChanged)
    Q_PROPERTY(OutputsModel *outputs READ outputs CONSTANT)

    Q_PROPERTY(int currentInput READ currentInput WRITE setCurrentInput NOTIFY currentInputChanged)
    Q_PROPERTY(int currentOutput READ currentOutput WRITE setCurrentOutput NOTIFY currentOutputChanged)
    // The track of the current input that the Media View shows and plays
    // (index into its tracks); -1 = its default track.
    Q_PROPERTY(int currentTrack READ currentTrack WRITE setCurrentTrack NOTIFY currentTrackChanged)
    // The current input's row and the track shown (empty maps when none).
    Q_PROPERTY(QVariantMap input READ input NOTIFY inputChanged)
    Q_PROPERTY(QVariantMap track READ track NOTIFY inputChanged)
    Q_PROPERTY(bool hasInput READ hasInput NOTIFY inputChanged)
    Q_PROPERTY(double duration READ duration NOTIFY chainChanged)

    Q_PROPERTY(int activeSteps READ activeSteps NOTIFY chainChanged)
    // The part of the timeline a Trim step keeps (the whole input otherwise).
    Q_PROPERTY(double keptStart READ keptStart NOTIFY chainChanged)
    Q_PROPERTY(double keptEnd READ keptEnd NOTIFY chainChanged)
    // The converter ffmpeg inserts before the current output's encoder, if
    // any: { filter, from, to, reason }.
    Q_PROPERTY(QVariantMap converter READ converter NOTIFY negotiatedChanged)
    Q_PROPERTY(QString totalSizeText READ totalSizeText NOTIFY outputsChanged)
    Q_PROPERTY(QVariantMap tags READ tags NOTIFY tagsChanged)
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY tagsChanged)
    Q_PROPERTY(QVariantList editableTracks READ editableTracks NOTIFY tagsChanged)
    Q_PROPERTY(QString cover READ cover NOTIFY tagsChanged)
    Q_PROPERTY(bool generating READ generating NOTIFY generationChanged)

public:
    explicit Project(QObject *parent = nullptr);

    ~Project() override;

    QString name() const { return m_name; }
    void setName(const QString &name);
    QUrl fileUrl() const { return m_fileUrl; }
    bool modified() const { return !m_undo.isClean(); }

    InputsModel *inputs() const { return m_inputs; }
    StepsModel *steps() const { return m_steps; }
    GraphModel *graph() const { return m_graph; }
    OutputsModel *outputs() const { return m_outputs; }
    GraphModel *graphNodes() const { return m_graphNodes; }
    RowModel *graphLinks() const { return m_graphLinks; }
    bool graphActive() const { return m_graphState.value(QStringLiteral("active")).toBool(); }
    bool canUseRow() const { return m_canUseRow; }
    QString graphError() const { return m_graphError; }
    // The compiled graph (GraphPlan::variant()); onlyOutput sinks the other outputs.
    QVariantMap graphPlan(const QString &onlyOutput = QString()) const;
    // The input whose name, folder, tags and cover outputs take: the selected
    // one, or in graph mode the first graph source unless the selected input
    // is one. renderTrack() is its track (a graph source's track in graph mode).
    int renderInput() const;
    QVariantMap renderTrack() const;
    Q_INVOKABLE QString addGraphSource(int inputIndex, int audioIndex = -1);
    Q_INVOKABLE QString addGraphOperation(const QString &type);
    Q_INVOKABLE bool connectGraph(const QString &from, int fromPort, const QString &to, int toPort);
    Q_INVOKABLE void disconnectGraph(const QString &linkUid);
    Q_INVOKABLE void moveGraphNode(const QString &id, double x, double y);
    Q_INVOKABLE void removeGraphNode(const QString &id);
    Q_INVOKABLE bool useRow();
    // A track dropped on the graph: a source at (x, y), wired into the first
    // open input of `target` (an output's feed is replaced). One undo step.
    Q_INVOKABLE QString routeTrack(int inputIndex, int audioIndex, double x, double y, const QString &target = {});
    QUndoStack *undoStack() { return &m_undo; }

    int currentInput() const { return m_currentInput; }
    void setCurrentInput(int index);
    int currentOutput() const { return m_currentOutput; }
    void setCurrentOutput(int index);
    int currentTrack() const { return m_currentTrack; }
    void setCurrentTrack(int index);
    QVariantMap input() const;
    QVariantMap track() const;
    bool hasInput() const { return m_currentInput >= 0 && m_currentInput < m_inputs->count(); }
    double duration() const;

    int activeSteps() const;
    double keptStart() const { return m_keptStart; }
    double keptEnd() const { return m_keptEnd; }
    QVariantMap converter() const { return m_converter; }
    QString totalSizeText() const { return m_totalSizeText; }
    QVariantMap tags() const;
    QVariantList chapters() const;
    QVariantList editableTracks() const;
    QString cover() const;
    QVariantMap outputMetadata(int inputIndex = -1) const;
    bool generating() const { return m_generating; }
    Q_INVOKABLE void extractCover(const QUrl &file);
    Q_INVOKABLE void removeCover();
    Q_INVOKABLE void setChapter(int index, double start, double end, const QString &title);
    Q_INVOKABLE void removeChapter(int index);
    Q_INVOKABLE void addChapter(double start, double end, const QString &title);
    Q_INVOKABLE void setTrackMetadata(int audioIndex, const QVariantMap &changes);

    // ----- For the engine (C++ only)
    void setServices(ffgui::LogStore *log, std::shared_ptr<CoverStore> covers);
    // What ffprobe said about an input (null while it is being read or when
    // it failed). For a folder, its first readable file.
    const ffgui::ProbeResult *probe(int index) const;
    QString uid(int index) const { return m_inputs->value(index, QStringLiteral("uid")).toString(); }
    int indexOf(const QString &uid) const { return m_inputs->find(QStringLiteral("uid"), uid); }
    // The audio track the Media View shows (index into probe()->tracks), or -1.
    int shownTrack() const;
    // Health after a full decode check.
    void setHealth(const QString &uid, const QString &health, const QString &text);

    void setCatalog(CatalogFacade *catalog);
    void setSelection(Selection *selection) { m_selection = selection; }
    ffgui::Recipe recipe() const;
    QStringList inputFiles(int index) const;
    void refreshDerived();
    QList<QVariantMap> markers() const { return m_markers; }
    void setNegotiated(const QList<QVariantMap> &nodes, const QVariantMap &converter);

    // Inputs
    Q_INVOKABLE void addFiles(const QList<QUrl> &files);
    Q_INVOKABLE void addFolder(const QUrl &folder);
    Q_INVOKABLE void addUrl(const QString &url);
    // A lavfi source ("sine=frequency=1000:duration=5") as a new input.
    Q_INVOKABLE void addGenerator(const QString &source, const QString &name);
    Q_INVOKABLE void removeInput(int index);
    // Swaps an input's source and keeps everything else (chain, outputs).
    Q_INVOKABLE void replaceInput(int index, const QUrl &file);
    // Uses an image as the cover art written to outputs.
    Q_INVOKABLE void setCoverArt(const QUrl &image);
    // Adds a convolution step that uses this file as its impulse response.
    Q_INVOKABLE void useImpulseResponse(const QUrl &file);
    Q_INVOKABLE void reprobe(int index);

    // Chain
    Q_INVOKABLE void insertStep(int index, const QString &type);
    // Appends a step with some parameters set (e.g. a Channel mix from Routing).
    Q_INVOKABLE void addStep(const QString &type, const QVariantMap &params = {});
    Q_INVOKABLE void removeStep(int index);
    Q_INVOKABLE void cancelPendingRange() { m_pendingRangeStart = m_pendingRangeEnd = 0; }
    void addRangedStep(const QString &type, const QVariantMap &params, double start, double end);
    Q_INVOKABLE void moveStep(int from, int to);
    Q_INVOKABLE void setBypass(int index, bool bypass);
    Q_INVOKABLE void setParam(int index, const QString &key, const QVariant &value);
    Q_INVOKABLE void setRange(int index, double start, double end);
    Q_INVOKABLE void clearRange(int index);
    Q_INVOKABLE void applyChainPreset(const QString &name);
    // The selection toolbar: trim, cut, fadeIn, fadeOut, silence, normalize,
    // step, export, marker, applied to [start, end].
    Q_INVOKABLE void applySelectionAction(const QString &action, double start, double end);

    // Outputs
    Q_INVOKABLE void addOutput(const QString &preset = {});
    Q_INVOKABLE void removeOutput(int index);
    Q_INVOKABLE void setOutput(int index, const QVariantMap &changes);
    Q_INVOKABLE void applyPreset(int index, const QString &preset);

    // Metadata written to outputs whose policy is "edit". Starts from the
    // current input's tags.
    Q_INVOKABLE void setTag(const QString &key, const QString &value);
    Q_INVOKABLE void removeTag(const QString &key);

    Q_INVOKABLE bool savePreset(const QString &name, bool chain = false);

    // Files
    Q_INVOKABLE void newProject();
    Q_INVOKABLE bool open(const QUrl &file);
    Q_INVOKABLE bool save(const QUrl &file = {});

signals:
    void graphChanged();
    void nameChanged();
    void fileUrlChanged();
    void modifiedChanged();
    void currentInputChanged();
    void currentOutputChanged();
    void currentTrackChanged();
    // An input finished probing (ok or not).
    void inputProbed(const QString &uid, bool ok);
    void inputChanged();
    void chainChanged();
    void negotiatedChanged();
    void paramEdited(int index, const QString &key, const QVariant &value);
    void outputsChanged();
    void tagsChanged();
    void generationChanged();
    // For things the project can't do yet, or can't do with this build.
    void notice(const QString &text);
    void selectionActionRequested(const QString &action, double start, double end);

private:
    friend class ProjectEdit;
    QVariantMap snapshot() const;
    void restoreSnapshot(const QVariantMap &snapshot);
    void applyRecipe(const ffgui::Recipe &recipe);
    void editStep(int index, const QVariantMap &changes, const QString &label, const QString &mergeKey = {});
    QVariantMap makeStep(const QString &type, const QVariantMap &params = {}) const;
    CatalogFacade *m_catalog = nullptr;
    Selection *m_selection = nullptr;
    bool m_editing = false;
    bool m_derived = false;
    QList<QVariantMap> m_markers;
    double m_pendingRangeStart = 0, m_pendingRangeEnd = 0;

    struct InputInfo
    {
        std::shared_ptr<ffgui::ProbeResult> probe;
        QStringList files;          // a folder's audio files
        int epoch = 0;
        int pending = 0;            // probes still running for it
        int failed = 0;
        QString firstError;
        double duration = 0;
        qint64 size = 0;
        QStringList warnings;
    };

    QString addRow(const QVariantMap &row);
    void probeNext();
    void startProbe(const QString &uid, const QString &path, bool primary);
    void probeFinished(const QString &uid, const QString &path, bool primary, bool ok, const ffgui::ProbeResult &result,
                       const QString &error, const QStringList &warnings);
    void fetchCover(const QString &uid, const ffgui::ProbeResult &result);
    void updateRow(const QString &uid, const QVariantMap &changes);

    ffgui::LogStore *m_log = nullptr;
    std::shared_ptr<CoverStore> m_covers;
    QHash<QString, InputInfo> m_info;
    struct ProbeTask { QString uid, path; bool primary; };
    QList<ProbeTask> m_probeQueue;
    int m_probesRunning = 0;
    int m_probeEpoch = 0;
    int m_coverVersion = 0;
    int m_currentTrack = -1;
    QString m_name = QStringLiteral("Untitled project");
    QUrl m_fileUrl;
    InputsModel *m_inputs;
    StepsModel *m_steps;
    GraphModel *m_graph;
    GraphModel *m_graphNodes;
    RowModel *m_graphLinks;
    QVariantMap m_graphState, m_graphPlan;
    bool m_canUseRow = true;
    QString m_graphError;
    void syncGraphState();
    void refreshGraph();
    void promoteGraph();
    bool graphFull();
    QList<QVariantMap> graphDescriptors() const;
    QList<QVariantMap> graphDefinition() const;
    QHash<QString, QPoint> graphLayout(const QList<QVariantMap> &nodes, const QList<QVariantMap> &links) const;
    QList<QVariantMap> graphConnections() const;
    QVariantMap compileGraph(const QList<QVariantMap> &nodes, const QList<QVariantMap> &links, const QString &onlyOutput) const;
    OutputsModel *m_outputs;
    QUndoStack m_undo;
    int m_currentInput = -1;
    int m_currentOutput = -1;
    double m_keptStart = 0;
    double m_keptEnd = 0;
    QVariantMap m_converter;
    QString m_totalSizeText;
    QVariantMap m_tags, m_edits;
    bool m_generating = false;
    QObject *m_generator = nullptr;
    void storeChapters(const QVariantList &chapters);
};
