#include "Tools.h"
#include "App.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <cmath>
#include <command/CommandBuilder.h>
#include <dsp/Scopes.h>
#include <log/LogStore.h>
#include <tools/CommandLine.h>
#include <tools/FfTools.h>
#include <transcript/Transcript.h>
using namespace ffgui;
ConsoleSession::ConsoleSession(LogStore *log, QObject *parent) : QObject(parent), m_log(log) {}
ConsoleSession::~ConsoleSession()
{
    if (m_job) {
        disconnect(m_job, nullptr, this, nullptr);
        m_job->stop();
        m_job->waitForFinished(1500);
        delete m_job;
    }
}
void ConsoleSession::run(const QString &text)
{
    if (m_job)
        return;
    m_error.clear();
    QString error;
    auto args = splitCommand(text, &error);
    if (args.isEmpty()) {
        m_error = error.isEmpty() ? tr("Enter an FFmpeg command") : error;
        emit changed();
        return;
    }
    QString program = FfTools::paths().ffmpeg;
    if (!args.first().startsWith(u'-')) {
        const QString executable = args.takeFirst();
        if (executable != u"ffmpeg" &&
            (QFileInfo(program).canonicalFilePath().isEmpty() ||
             QFileInfo(executable).canonicalFilePath() != QFileInfo(program).canonicalFilePath())) {
            m_error = tr("The Console runs FFmpeg commands only");
            emit changed();
            return;
        }
    }
    auto *job = new ProcessJob(program, args, this);
    m_job = job;
    m_stdout.clear();
    job->setLog(m_log, "console", "info");
    job->setKeepStderr(false);
    connect(job, &ProcessJob::stdoutReady, this, &ConsoleSession::read);
    connect(job, &ProcessJob::finished, this, [this, job](bool ok) {
        read();
        if (!m_stdout.isEmpty())
            m_log->append(job->jobId(), "info", QString::fromUtf8(m_stdout.left(4096)), "console");
        m_stdout.clear();
        m_job = nullptr;
        if (!ok)
            m_error = tr("FFmpeg exited with code %1; see the Console log").arg(job->exitCode());
        job->deleteLater();
        emit changed();
        emit finished(ok);
    });
    emit changed();
    job->start();
}
void ConsoleSession::read()
{
    if (!m_job)
        return;
    QByteArray bytes = m_job->readStdout();
    if (bytes.contains('\0')) {
        m_log->append(m_job->jobId(), "info", tr("%1 bytes of binary output").arg(bytes.size()), "console");
        return;
    }
    m_stdout += bytes;
    qsizetype at;
    while ((at = m_stdout.indexOf('\n')) >= 0) {
        m_log->append(m_job->jobId(), "info", QString::fromUtf8(m_stdout.left(std::min<qsizetype>(at, 4096))),
                      "console");
        m_stdout.remove(0, at + 1);
    }
    if (m_stdout.size() > 8192) {
        m_stdout = m_stdout.right(4096);
    }
}
void ConsoleSession::stop()
{
    if (m_job)
        m_job->stop();
}
TranscriptDocument::TranscriptDocument(App *app) : RowModel({"start", "end", "text"}, app), m_app(app) {}
TranscriptDocument::~TranscriptDocument() { cancel(); }
void TranscriptDocument::cancel()
{
    if (m_job) {
        disconnect(m_job, nullptr, this, nullptr);
        m_job->kill();
        delete m_job;
        m_job = nullptr;
        emit errorChanged();
    }
}
void TranscriptDocument::transcribe(const QString &model, const QString &language)
{
    if (m_job)
        return;
    m_error.clear();
    const auto *source = m_app->project()->probe(m_app->project()->currentInput());
    if (!m_app->caps()->hasFilter("whisper"))
        m_error = tr("This FFmpeg build has no whisper filter");
    else if (!source)
        m_error = tr("Choose a readable audio input");
    else if (!QFileInfo(model).isFile() || !QFileInfo(model).isReadable())
        m_error = tr("Choose a readable whisper.cpp model file");
    else if (!QRegularExpression("^[a-z]{2,3}$|^auto$").match(language).hasMatch())
        m_error = tr("Use a language code or auto");
    if (!m_error.isEmpty()) {
        emit errorChanged();
        return;
    }
    auto dir = std::make_shared<QTemporaryDir>();
    if (!dir->isValid()) {
        m_error = tr("Couldn't create a transcript file");
        emit errorChanged();
        return;
    }
    // Escape both filter-option and filtergraph parsing layers (no shell).
    auto escape = [](const QString &value) {
        QString out;
        for (QChar c : value) {
            if (QStringLiteral("\\':").contains(c))
                out += u'\\';
            out += c;
        }
        QString graph;
        for (QChar c : out) {
            if (QStringLiteral("\\'[],;").contains(c))
                graph += u'\\';
            graph += c;
        }
        return graph;
    };
    const QString destination = dir->filePath("transcript.srt");
    const QString filter = "whisper=model=" + escape(QFileInfo(model).absoluteFilePath()) +
                           ":language=" + language + ":destination=" + escape(destination) +
                           ":format=srt:use_gpu=false";
    auto *job =
        new ProcessJob(FfTools::paths().ffmpeg,
                       {"-hide_banner", "-nostdin", "-loglevel", "level+error", "-i", source->path, "-map",
                        QStringLiteral("0:a:%1").arg(m_app->project()->track().value("audioIndex").toInt()),
                        "-af", filter, "-f", "null", "-"},
                       this);
    m_job = job;
    job->setLog(m_app->logStore(), "transcribe", "error");
    job->setBackground(true);
    connect(job, &ProcessJob::finished, this, [this, job, dir, destination](bool ok) {
        m_job = nullptr;
        if (ok)
            importFile(QUrl::fromLocalFile(destination));
        else
            m_error = job->problems("error").join(u'\n');
        job->deleteLater();
        emit errorChanged();
    });
    emit errorChanged();
    job->start();
}
bool TranscriptDocument::importFile(const QUrl &url)
{
    QFile f(url.toLocalFile());
    QList<QVariantMap> entries;
    m_error.clear();
    if (!url.isLocalFile() || !f.open(QIODevice::ReadOnly) || f.size() > 16 * 1024 * 1024) {
        m_error = tr("Choose a readable transcript smaller than 16 MB");
        emit errorChanged();
        return false;
    }
    if (!Transcript::parse(f.readAll(), QFileInfo(f.fileName()).suffix().toLower(), &entries, &m_error)) {
        emit errorChanged();
        return false;
    }
    setRows(entries);
    emit errorChanged();
    return true;
}
bool TranscriptDocument::exportFile(const QUrl &url, const QString &format) const
{
    if (!url.isLocalFile() || !QStringList{"srt", "txt", "json"}.contains(format))
        return false;
    QSaveFile f(url.toLocalFile());
    const auto data = Transcript::encode(rows(), format);
    return f.open(QIODevice::WriteOnly) && f.write(data) == data.size() && f.commit();
}
bool TranscriptDocument::setSegment(int row, double start, double end, const QString &text)
{
    QVariantMap segment{{"start", start}, {"end", end}, {"text", text}};
    if (row < 0 || row >= count() || !Transcript::valid(segment))
        return false;
    update(row, segment);
    return true;
}
void TranscriptDocument::addSegment(double start, double end, const QString &text)
{
    QVariantMap s{{"start", start}, {"end", end}, {"text", text}};
    if (Transcript::valid(s))
        append(s);
}
void TranscriptDocument::showInput(const QString &uid)
{
    if (uid == m_uid)
        return;
    cancel();
    m_documents.insert(m_uid, rows());
    m_uid = uid;
    setRows(m_documents.value(uid));
}
QVariantMap ScopeState::values() const
{
    const QMutexLocker lock(&m_mutex);
    return m_values;
}
void ScopeState::consume(const float *samples, int frames, int channels, int rate)
{
    if (channels < 1 || frames < 1)
        return;
    if (channels != m_channels || rate != m_rate) {
        m_samples.clear();
        m_channels = channels;
        m_rate = rate;
    }
    const int keep = std::min(frames, 4096);
    m_samples.insert(m_samples.end(), samples + (frames - keep) * channels, samples + frames * channels);
    if (m_samples.size() > size_t(4096 * channels))
        m_samples.erase(m_samples.begin(), m_samples.end() - 4096 * channels);
}
void ScopeState::publish()
{
    if (m_samples.empty())
        return;
    auto value =
        ScopeAnalysis::analyze(m_samples.data(), int(m_samples.size()) / m_channels, m_channels, m_rate);
    {
        const QMutexLocker lock(&m_mutex);
        m_values = value;
    }
    emit updated();
}
void ScopeState::clear()
{
    m_samples.clear();
    {
        const QMutexLocker lock(&m_mutex);
        m_values.clear();
    }
    emit updated();
}
CompareState::CompareState(App *app) : QObject(app), m_app(app), m_comparer(app->logStore())
{
    connect(&m_comparer, &Comparer::finished, this, [this](const QVariantMap &result) {
        m_active = false;
        m_error = result.value("error").toString();
        m_result = result;
        emit changed();
    });
}
void CompareState::setOffset(double value)
{
    if (!std::isfinite(value) || std::abs(value) > 3600 || value == m_offset)
        return;
    m_offset = value;
    emit changed();
}
void CompareState::run(int mode, int a, int b)
{
    auto *p = m_app->project();
    if (a < 0)
        a = p->currentInput();
    if (b < 0)
        b = a == 0 ? 1 : 0;
    const auto *first = p->probe(a);
    const auto *second = mode == 0 ? first : p->probe(b);
    if (!first || !second || first->tracks.isEmpty() || second->tracks.isEmpty()) {
        m_error = tr("Choose two readable audio sources");
        emit changed();
        return;
    }
    CompareSpec spec;
    spec.a = first->path;
    spec.b = second->path;
    spec.trackA = (a == p->currentInput() ? p->track().value("audioIndex").toInt()
                                          : first->tracks[first->defaultTrack()].audioIndex);
    spec.trackB = mode == 0 ? spec.trackA : second->tracks[second->defaultTrack()].audioIndex;
    spec.offset = m_offset;
    if (mode == 0 && p->graphActive()) {
        // The selected output's branch of the graph is B.
        spec.graphB = p->graphPlan(p->outputs()->value(std::max(0, p->currentOutput()), "uid").toString());
        if (!spec.graphB.value("error").toString().isEmpty()) {
            m_error = spec.graphB.value("error").toString();
            emit changed();
            return;
        }
    } else if (mode == 0) {
        const auto chain = ChainCompiler::compile(p->steps()->rows(), m_app->catalog()->catalog(), {},
                                                  first->tracks[first->defaultTrack()].channels, true);
        if (!chain.error.isEmpty()) {
            m_error = chain.error;
            emit changed();
            return;
        }
        spec.filterB = chain.filter;
    }
    m_mode = mode;
    m_a = a;
    m_b = b;
    m_active = true;
    m_error.clear();
    m_result.clear();
    emit changed();
    m_comparer.start(spec);
}
void CompareState::findOffset()
{
    if (m_active)
        return;
    setOffset(m_result.value("suggestedOffset", m_offset).toDouble());
    run(m_mode, m_a, m_b);
}
void CompareState::cancel()
{
    m_comparer.cancel();
    m_active = false;
    m_result.clear();
    m_error.clear();
    emit changed();
}
CaptureState::CaptureState(App *app)
    : QObject(app), m_app(app), m_devices(app->logStore()), m_recorder(app->logStore())
{
    m_output = QStandardPaths::writableLocation(QStandardPaths::MusicLocation) + "/recording-" +
               QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ".wav";
    connect(&m_devices, &DeviceList::finished, this,
            [this](const QList<QVariantMap> &devices, const QString &error) {
                m_app->devices()->setRows(devices);
                m_refreshing = false;
                m_error = error;
                emit changed();
            });
    connect(&m_recorder, &Recorder::reading, this, [this](double t, double db) {
        m_elapsed = t;
        m_level = db;
        emit changed();
    });
    connect(&m_recorder, &Recorder::finished, this,
            [this](bool ok, const QStringList &files, const QString &error) {
                m_active = false;
                m_error = error;
                emit changed();
                if (ok && !files.isEmpty()) {
                    QList<QUrl> urls;
                    for (const auto &f : files)
                        urls << QUrl::fromLocalFile(f);
                    m_app->project()->addFiles(urls);
                    m_app->toast(tr("Recording added as input"), "success");
                } else if (!ok)
                    m_app->toast(error, "error");
            });
}
void CaptureState::setOutput(const QString &value)
{
    QUrl url(value);
    m_output = url.isLocalFile() ? url.toLocalFile() : value;
    emit changed();
}
void CaptureState::refreshDevices()
{
    m_refreshing = true;
    emit changed();
    m_devices.refresh();
}
void CaptureState::start(const QVariantMap &options)
{
    if (m_active)
        return;
    CaptureSpec spec;
    spec.output = m_output;
    spec.backend = options.value("backend", "pulse").toString();
    spec.device = options.value("device", "default").toString();
    spec.codec = options.value("codec", "pcm_s24le").toString();
    spec.rate = options.value("rate", 48000).toInt();
    spec.depth = options.value("depth", 24).toInt();
    spec.channels = options.value("channels", 2).toInt();
    spec.seconds = options.value("seconds", 0).toDouble();
    spec.maxBytes = options.value("maxBytes", 0).toLongLong();
    spec.splitSeconds = options.value("splitSeconds", 0).toDouble();
    spec.monitor = options.value("monitor", true).toBool();
    m_error.clear();
    m_elapsed = 0;
    m_level = -100;
    m_active = true;
    emit changed();
    m_recorder.start(spec);
}
void CaptureState::stop() { m_recorder.stop(); }
StreamState::StreamState(App *app) : QObject(app), m_app(app) {}
StreamState::~StreamState()
{
    for (auto *job : std::as_const(m_jobs)) {
        disconnect(job, nullptr, this, nullptr);
        delete job;
    }
    m_jobs.clear();
}
void StreamState::add(const QString &name, const QString &protocol, const QString &url, int kbps)
{
    m_app->destinations()->append({{"uid", QUuid::createUuid().toString(QUuid::Id128)},
                                   {"name", name},
                                   {"protocol", protocol},
                                   {"url", url},
                                   {"format", protocol == u"icecast" ? "MP3" : "AAC"},
                                   {"kbps", kbps},
                                   {"status", "idle"},
                                   {"bitrate", 0.0},
                                   {"history", QVariantList()},
                                   {"error", QString()}});
}
void StreamState::remove(int row)
{
    const auto id = m_app->destinations()->value(row, "uid").toString();
    if (m_jobs.contains(id))
        return;
    m_app->destinations()->removeAt(row);
}
void StreamState::start(int row)
{
    auto *model = m_app->destinations();
    const auto item = model->get(row);
    const QString id = item.value("uid").toString();
    if (id.isEmpty() || m_jobs.contains(id))
        return;
    auto *p = m_app->project();
    const auto *probe = p->probe(p->currentInput());
    if (!probe) {
        m_app->toast(tr("Add a readable input first"));
        return;
    }
    auto chain = p->graphActive() ? ChainResult()
                                  : ChainCompiler::compile(p->steps()->rows(), m_app->catalog()->catalog(), {},
                                                           p->track().value("channels", 2).toInt());
    if (!chain.error.isEmpty()) {
        model->update(row, {{"status", "error"}, {"error", chain.error}});
        return;
    }
    StreamSpec spec{probe->path,
                    chain.filter,
                    item.value("protocol").toString(),
                    item.value("url").toString(),
                    p->track().value("audioIndex").toInt(),
                    item.value("kbps", 128).toInt()};
    // A graph streams the selected output's branch.
    if (p->graphActive())
        spec.graph = p->graphPlan(p->outputs()->value(std::max(0, p->currentOutput()), "uid").toString());
    auto *job = new StreamOut(spec, m_app->logStore(), this);
    m_jobs.insert(id, job);
    model->update(row, {{"status", "connecting"}, {"error", QString()}, {"history", QVariantList()}});
    connect(job, &StreamOut::reading, this, [this, id](double bitrate) {
        auto *model = m_app->destinations();
        const int row = model->find("uid", id);
        auto history = model->value(row, "history").toList();
        history << bitrate;
        while (history.size() > 120)
            history.removeFirst();
        model->update(row, {{"status", "live"}, {"bitrate", bitrate}, {"history", history}});
    });
    connect(job, &StreamOut::finished, this, [this, id, job](bool ok, bool stopped, const QString &error) {
        m_jobs.remove(id);
        m_app->destinations()->update(
            m_app->destinations()->find("uid", id),
            {{"status", ok || stopped ? "stopped" : "error"}, {"bitrate", 0.0}, {"error", error}});
        job->deleteLater();
    });
    job->start();
}
void StreamState::stop(int row)
{
    const auto id = m_app->destinations()->value(row, "uid").toString();
    if (auto *job = m_jobs.value(id))
        job->stop();
}
void StreamState::stopAll()
{
    for (auto *job : std::as_const(m_jobs))
        job->stop();
}
QString ToolSettings::ffmpeg() const { return QSettings().value("tools/ffmpeg").toString(); }
QString ToolSettings::ffprobe() const { return QSettings().value("tools/ffprobe").toString(); }
QString ToolSettings::ffplay() const { return QSettings().value("tools/ffplay").toString(); }
bool ToolSettings::save(const QString &a, const QString &b, const QString &c)
{
    for (const auto &path : {a, b, c})
        if (!path.isEmpty() && QStandardPaths::findExecutable(path).isEmpty()) {
            m_error = tr("Executable not found: %1").arg(path);
            emit changed();
            return false;
        }
    QSettings settings;
    settings.setValue("tools/ffmpeg", a.isEmpty() ? a : QStandardPaths::findExecutable(a));
    settings.setValue("tools/ffprobe", b.isEmpty() ? b : QStandardPaths::findExecutable(b));
    settings.setValue("tools/ffplay", c.isEmpty() ? c : QStandardPaths::findExecutable(c));
    settings.sync();
    m_error = settings.status() == QSettings::NoError ? QString() : tr("Could not save tool settings");
    emit changed();
    return m_error.isEmpty();
}
