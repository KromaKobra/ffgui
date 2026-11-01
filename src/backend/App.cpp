#include "App.h"
#include "CoverStore.h"
#include "Engine.h"

#include <log/LogStore.h>

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QTime>
#include <QTimer>
#include <QUrl>

namespace {
constexpr int kMaxLogRows = 5000;
}

App::App(QQmlEngine *engine, QObject *parent)
    : QObject(parent), m_logStore(new ffgui::LogStore(this)), m_covers(std::make_shared<CoverStore>()),
      m_project(new Project(this)), m_selection(new Selection(this)), m_transport(new Transport(this)),
      m_meters(new Meters(this)), m_analysis(new Analysis(this)), m_media(new MediaState(m_logStore, this)),
      m_render(new Render(this)), m_queue(new QueueModel(this)), m_caps(new Capabilities(this)),
      m_log(new LogModel(this)), m_undo(new Undo(m_project->undoStack(), this)), m_view(new ViewState(this)),
      m_command(new Command(this)), m_catalog(new CatalogFacade(this)),
      m_transcript(new TranscriptDocument(this)), m_devices(new DevicesModel(this)),
      m_destinations(new DestinationsModel(this)), m_console(new ConsoleSession(m_logStore, this)),
      m_compare(new CompareState(this)), m_scopes(new ScopeState(this)), m_capture(new CaptureState(this)),
      m_stream(new StreamState(this)), m_settings(new ToolSettings(this)), m_engine(nullptr)
{
    if (engine)
        engine->addImageProvider(QStringLiteral("cover"), new CoverProvider(m_covers));
    m_project->setServices(m_logStore, m_covers);
    m_catalog->setLog(m_logStore);
    m_project->setCatalog(m_catalog);
    m_project->setSelection(m_selection);
    for (auto entry : { std::pair { static_cast<RowModel *>(m_project->steps()), QStringLiteral("step") },
                        { static_cast<RowModel *>(m_project->outputs()), QStringLiteral("output") },
                        { static_cast<RowModel *>(m_project->inputs()), QStringLiteral("input") } }) {
        connect(entry.first, &QAbstractItemModel::rowsInserted, this, [this, kind = entry.second](const QModelIndex &, int first, int last) {
            for (int i = first; i <= last; ++i) m_selection->itemInserted(kind, i);
        });
        connect(entry.first, &QAbstractItemModel::rowsRemoved, this, [this, kind = entry.second](const QModelIndex &, int first, int last) {
            for (int i = last; i >= first; --i) m_selection->itemRemoved(kind, i);
        });
        connect(entry.first, &QAbstractItemModel::rowsMoved, this, [this, kind = entry.second](const QModelIndex &, int first, int, const QModelIndex &, int destination) {
            m_selection->itemMoved(kind, first, destination > first ? destination-1 : destination);
        });
    }
    connect(m_project, &Project::selectionActionRequested, this, [this](const QString &action, double, double) {
        if (action == u"export") m_render->start(QStringLiteral("selection"));
        if (action == u"step") { requestPanel(QStringLiteral("chain")); emit selectionStepRequested(); }
    });
    connect(m_queue, &RowModel::revisionChanged, this, &App::queuePendingChanged);
    connect(m_log, &RowModel::revisionChanged, this, &App::recount);
    connect(m_project, &Project::notice, this, [this](const QString &text) { toast(text); });
    connect(m_logStore, &ffgui::LogStore::appended, this, [this](const ffgui::LogStore::Entry &e) {
        if (m_log->count() >= kMaxLogRows)
            m_log->removeAt(0);
        m_log->append({ { "time", e.time.toString(QStringLiteral("HH:mm:ss")) }, { "jobId", e.jobId },
                        { "level", e.level }, { "text", e.text }, { "source", e.source } });
    });
    m_engine = new Engine(this);
    m_engine->start();
    connect(m_settings, &ToolSettings::changed, this, [this] {
        if (m_settings->error().isEmpty()) {
            m_transport->pause();
            m_caps->refresh();
        }
    });
    connect(m_project, &Project::currentInputChanged, this, [this] {
        m_transcript->showInput(m_project->uid(m_project->currentInput()));
        m_compare->cancel();
    });
    // An active graph always shows as one, whatever the workspace asks for;
    // the Chain's Row button turns a linear graph back into rows first.
    connect(m_project, &Project::graphChanged, this, [this] {
        if (m_project->graphActive())
            m_view->setChainGraph(true);
    });
    connect(m_view, &ViewState::chainGraphChanged, this, [this] {
        if (!m_view->chainGraph() && m_project->graphActive())
            m_view->setChainGraph(true);
    });
    QTimer::singleShot(0, m_capture, &CaptureState::refreshDevices);
}

App::~App()
{
    // The engine's jobs and threads go before the objects they report to.
    delete m_engine;
    m_engine = nullptr;
    delete m_stream;
    m_stream = nullptr;
    delete m_capture;
    m_capture = nullptr;
    delete m_compare;
    m_compare = nullptr;
    delete m_console;
    m_console = nullptr;
    delete m_transcript;
    m_transcript = nullptr;
    // These objects own background jobs too. Join them while the log store
    // and other services are still alive, rather than relying on child order.
    delete m_media;
    m_media = nullptr;
    delete m_undo;
    m_undo = nullptr;
    delete m_project;
    m_project = nullptr;
    delete m_catalog;
    m_catalog = nullptr;
}


int App::queuePending() const
{
    int n = 0;
    for (const QVariantMap &job : m_queue->rows()) {
        const QString status = job.value(QStringLiteral("status")).toString();
        n += status == u"queued" || status == u"running" || status == u"paused" ? 1 : 0;
    }
    return n;
}

QString App::version() const
{
    return QCoreApplication::applicationVersion();
}

void App::requestPanel(const QString &panelId, bool popOut)
{
    emit panelRequested(panelId, popOut);
}

void App::requestDialog(const QString &dialogId, const QVariantMap &options)
{
    emit dialogRequested(dialogId, options);
}

void App::toast(const QString &text, const QString &kind, const QString &actionText, const QString &actionId)
{
    emit toastRequested(text, kind, actionText, actionId);
}

void App::runAction(const QString &actionId)
{
    const qsizetype colon = actionId.indexOf(u':');
    const QString verb = actionId.left(colon);
    const QString arg = colon >= 0 ? actionId.mid(colon + 1) : QString();
    if (verb == u"open")
        QDesktopServices::openUrl(QUrl::fromLocalFile(arg));
    else if (verb == u"reveal")
        revealInFolder(arg);
    else if (verb == u"panel")
        requestPanel(arg);
}

void App::copyText(const QString &text)
{
    if (QClipboard *clipboard = QGuiApplication::clipboard())
        clipboard->setText(text);
    toast(tr("Copied to clipboard"));
}

void App::runCommand(const QString &text) { m_console->run(text); }

void App::openInFfplay()
{
    m_engine->openInFfplay();
}

void App::revealInFolder(const QString &path)
{
    const QFileInfo info(path);
    // The file manager's "show item" call selects the file; fall back to
    // opening its folder.
    const bool shown = QProcess::startDetached(
        QStringLiteral("dbus-send"),
        { QStringLiteral("--session"), QStringLiteral("--dest=org.freedesktop.FileManager1"),
          QStringLiteral("--type=method_call"), QStringLiteral("/org/freedesktop/FileManager1"),
          QStringLiteral("org.freedesktop.FileManager1.ShowItems"),
          QStringLiteral("array:string:") + QUrl::fromLocalFile(info.absoluteFilePath()).toString(),
          QStringLiteral("string:") });
    if (!shown)
        QDesktopServices::openUrl(QUrl::fromLocalFile(info.isDir() ? info.absoluteFilePath() : info.absolutePath()));
}

void App::logLine(const QString &level, const QString &text, const QString &source, const QString &jobId)
{
    m_logStore->append(jobId, level, text, source);
}

void App::recount()
{
    int warnings = 0, errors = 0;
    for (const QVariantMap &e : m_log->rows()) {
        const QString level = e.value(QStringLiteral("level")).toString();
        warnings += level == u"warning" ? 1 : 0;
        errors += level == u"error" ? 1 : 0;
    }
    if (warnings == m_warningCount && errors == m_errorCount)
        return;
    m_warningCount = warnings;
    m_errorCount = errors;
    emit logCountsChanged();
}
