#pragma once

#include "Analysis.h"
#include "CatalogFacade.h"
#include "Capabilities.h"
#include "Command.h"
#include "Meters.h"
#include "Models.h"
#include "Project.h"
#include "Render.h"
#include "Selection.h"
#include "Transport.h"
#include "Undo.h"
#include "ViewState.h"

#include <QQmlEngine>

// The application as QML sees it: one singleton that owns the project and
// everything the panels share. Panels read and call these objects and never
// talk to each other; to open another panel they call requestPanel(), which
// the Shell turns into a dock action.
class App : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(Project *project READ project CONSTANT)
    Q_PROPERTY(Selection *selection READ selection CONSTANT)
    Q_PROPERTY(Transport *transport READ transport CONSTANT)
    Q_PROPERTY(Meters *meters READ meters CONSTANT)
    Q_PROPERTY(Analysis *analysis READ analysis CONSTANT)
    Q_PROPERTY(Render *render READ render CONSTANT)
    Q_PROPERTY(QueueModel *queue READ queue CONSTANT)
    Q_PROPERTY(Capabilities *caps READ caps CONSTANT)
    Q_PROPERTY(LogModel *log READ log CONSTANT)
    Q_PROPERTY(Undo *undo READ undo CONSTANT)
    Q_PROPERTY(ViewState *view READ view CONSTANT)
    Q_PROPERTY(Command *command READ command CONSTANT)
    Q_PROPERTY(CatalogFacade *catalog READ catalog CONSTANT)
    Q_PROPERTY(TranscriptModel *transcript READ transcript CONSTANT)
    Q_PROPERTY(DevicesModel *devices READ devices CONSTANT)
    Q_PROPERTY(DestinationsModel *destinations READ destinations CONSTANT)

    Q_PROPERTY(int queuePending READ queuePending NOTIFY queuePendingChanged)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY logCountsChanged)
    Q_PROPERTY(int errorCount READ errorCount NOTIFY logCountsChanged)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    explicit App(QObject *parent = nullptr);

    Project *project() const { return m_project; }
    Selection *selection() const { return m_selection; }
    Transport *transport() const { return m_transport; }
    Meters *meters() const { return m_meters; }
    Analysis *analysis() const { return m_analysis; }
    Render *render() const { return m_render; }
    QueueModel *queue() const { return m_queue; }
    Capabilities *caps() const { return m_caps; }
    LogModel *log() const { return m_log; }
    Undo *undo() const { return m_undo; }
    ViewState *view() const { return m_view; }
    Command *command() const { return m_command; }
    CatalogFacade *catalog() const { return m_catalog; }
    TranscriptModel *transcript() const { return m_transcript; }
    DevicesModel *devices() const { return m_devices; }
    DestinationsModel *destinations() const { return m_destinations; }

    int queuePending() const;
    int warningCount() const { return m_warningCount; }
    int errorCount() const { return m_errorCount; }
    QString version() const;

    // Asks the Shell to open (and raise) a panel, or to pop it out.
    Q_INVOKABLE void requestPanel(const QString &panelId, bool popOut = false);
    // Asks the Shell for a file dialog: addFiles, addFolder, addUrl,
    // openProject, saveProject, exportAnalysis, replaceCover, ...
    Q_INVOKABLE void requestDialog(const QString &dialogId, const QVariantMap &options = {});
    // A brief message with an optional action ("Open file", "View log").
    // kind: info | success | warning | error
    Q_INVOKABLE void toast(const QString &text, const QString &kind = QStringLiteral("info"),
                           const QString &actionText = {}, const QString &actionId = {});
    // Runs a toast's action: "open:<path>", "reveal:<path>", "panel:<id>".
    Q_INVOKABLE void runAction(const QString &actionId);
    Q_INVOKABLE void copyText(const QString &text);
    // Runs a command line typed or edited in the Console, as it is.
    Q_INVOKABLE void runCommand(const QString &commandLine);
    Q_INVOKABLE void revealInFolder(const QString &path);
    // Plays the current input (from the playhead) in real ffplay, for
    // reference listening.
    Q_INVOKABLE void openInFfplay();

    // ffgui's own log lines (ffmpeg's arrive through the jobs).
    void logLine(const QString &level, const QString &text, const QString &source = QStringLiteral("ffgui"),
                 const QString &jobId = {});

signals:
    void panelRequested(const QString &panelId, bool popOut);
    void dialogRequested(const QString &dialogId, const QVariantMap &options);
    void toastRequested(const QString &text, const QString &kind, const QString &actionText,
                        const QString &actionId);
    void queuePendingChanged();
    void logCountsChanged();

private:
    void recount();

    Project *m_project;
    Selection *m_selection;
    Transport *m_transport;
    Meters *m_meters;
    Analysis *m_analysis;
    Render *m_render;
    QueueModel *m_queue;
    Capabilities *m_caps;
    LogModel *m_log;
    Undo *m_undo;
    ViewState *m_view;
    Command *m_command;
    CatalogFacade *m_catalog;
    TranscriptModel *m_transcript;
    DevicesModel *m_devices;
    DestinationsModel *m_destinations;
    int m_warningCount = 0;
    int m_errorCount = 0;
};
