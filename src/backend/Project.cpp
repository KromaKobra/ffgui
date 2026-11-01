#include "Project.h"
#include "CatalogFacade.h"
#include "CoverStore.h"
#include "Selection.h"
#include <QDir>
#include <QJsonDocument>
#include <QSignalBlocker>
#include <command/CommandBuilder.h>
#include <graph/GraphCompiler.h>
#include <project/SnapshotCommand.h>

#include <log/LogStore.h>
#include <metadata/CoverArt.h>
#include <probe/Prober.h>
#include <util/Format.h>

#include <QBuffer>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFileInfo>
#include <QImageReader>
#include <QStandardPaths>
#include <QUuid>
#include <cmath>
#include <tools/FfTools.h>
#include <tools/ProcessJob.h>

using namespace ffgui;

namespace {

constexpr int kMaxProbes = 4;

// Extensions ffgui picks up from a folder.
const QStringList &audioExtensions()
{
    static const QStringList ext { "wav", "flac", "mp3", "m4a", "aac", "ogg", "oga", "opus", "mka", "aif", "aiff",
                                   "aifc", "wma", "ac3", "eac3", "dts", "caf", "w64", "ape", "wv", "mp2", "amr",
                                   "au", "snd", "tta", "mpc", "spx", "mkv", "mp4", "mov", "webm" };
    return ext;
}

QVariantMap trackMap(const AudioTrack &t)
{
    return { { "index", t.index }, { "audioIndex", t.audioIndex }, { "codec", t.codec }, { "codecLong", t.codecLong },
             { "profile", t.profile }, { "sampleFmt", t.sampleFmt }, { "rate", t.sampleRate }, { "channels", t.channels },
             { "layout", t.layout }, { "channelNames", t.channelNames }, { "depth", t.bitsPerSample },
             { "bitrate", double(t.bitRate) }, { "duration", t.duration }, { "frames", double(t.frames) },
             { "lang", t.language }, { "title", t.title }, { "isDefault", t.isDefault },
             { "disposition", t.disposition }, { "sideData", t.sideData }, { "tags", t.tags } };
}

QVariantList chapterList(const QList<Chapter> &chapters)
{
    QVariantList list;
    for (const Chapter &c : chapters)
        list << QVariantMap { { "start", c.start }, { "end", c.end }, { "title", c.title } };
    return list;
}

} // namespace

// Nested operations (preset application, adding a folder through addFiles)
// form one undo command. Background probe results never enter this stack.
class ProjectEdit
{
public:
    ProjectEdit(Project *p, QString label, QString key = {})
        : p(p), label(std::move(label)), key(std::move(key)), active(!p->m_editing)
    {
        if (active) { before = p->snapshot(); p->m_editing = true; }
    }
    ~ProjectEdit()
    {
        if (!active) return;
        p->m_editing = false;
        p->syncGraphState();
        const auto after = p->snapshot();
        if (before == after) return;
        p->refreshDerived();
        p->m_undo.push(new SnapshotCommand(label, key, before, after, [project = p](const QVariantMap &s) { project->restoreSnapshot(s); }));
    }
private:
    Project *p;
    QString label, key;
    QVariantMap before;
    bool active;
};

Project::Project(QObject *parent)
    : QObject(parent), m_inputs(new InputsModel(this)), m_steps(new StepsModel(this)),
      m_graph(new GraphModel(this)), m_graphNodes(new GraphModel(this)),
      m_graphLinks(new RowModel({"uid", "from", "to", "fromPort", "toPort", "label"}, this)),
      m_outputs(new OutputsModel(this))
{
    connect(&m_undo, &QUndoStack::cleanChanged, this, &Project::modifiedChanged);
    connect(m_inputs, &RowModel::revisionChanged, this, &Project::inputChanged);


    connect(this, &Project::inputChanged, this, &Project::refreshDerived);
    connect(this, &Project::inputChanged, this, &Project::tagsChanged);
}

Project::~Project()
{
    disconnect(&m_undo, nullptr, this, nullptr);
    // Probers and cover jobs are QObject children; cancel their callbacks
    // before the recipe/probe caches are destroyed.
    for (QObject *child : children()) disconnect(child, nullptr, this, nullptr);
}

void Project::setServices(LogStore *log, std::shared_ptr<CoverStore> covers)
{
    m_log = log;
    m_covers = std::move(covers);
}

void Project::setName(const QString &name)
{
    ProjectEdit edit(this, tr("Rename project"));
    if (m_name == name)
        return;
    m_name = name;
    emit nameChanged();
}

void Project::setCurrentInput(int index)
{
    index = m_inputs->count() ? std::clamp(index, -1, m_inputs->count() - 1) : -1;
    if (m_currentInput == index)
        return;
    m_currentInput = index;
    m_currentTrack = -1;
    emit currentInputChanged();
    emit currentTrackChanged();
    emit inputChanged();
}

void Project::setCurrentOutput(int index)
{
    index = m_outputs->count() ? std::clamp(index, -1, m_outputs->count() - 1) : -1;
    if (m_currentOutput == index)
        return;
    m_currentOutput = index;
    emit currentOutputChanged();
}

void Project::setCurrentTrack(int index)
{
    const int count = int(input().value(QStringLiteral("tracks")).toList().size());
    index = index >= 0 && index < count ? index : -1;
    if (m_currentTrack == index)
        return;
    m_currentTrack = index;
    emit currentTrackChanged();
    emit inputChanged();
}

QVariantMap Project::input() const
{
    return m_inputs->get(m_currentInput);
}

int Project::shownTrack() const
{
    const ProbeResult *p = probe(m_currentInput);
    if (!p || p->tracks.isEmpty())
        return -1;
    return m_currentTrack >= 0 && m_currentTrack < p->tracks.size() ? m_currentTrack : p->defaultTrack();
}

QVariantMap Project::track() const
{
    const QVariantList tracks = input().value(QStringLiteral("tracks")).toList();
    if (m_currentTrack >= 0 && m_currentTrack < tracks.size())
        return tracks.at(m_currentTrack).toMap();
    for (const QVariant &t : tracks)
        if (t.toMap().value(QStringLiteral("isDefault")).toBool())
            return t.toMap();
    return tracks.isEmpty() ? QVariantMap() : tracks.first().toMap();
}

double Project::duration() const
{
    // A graph's longest output; refreshDerived() keeps the plan current.
    if (graphActive() && m_graphPlan.value(QStringLiteral("error")).toString().isEmpty() &&
        m_graphPlan.value(QStringLiteral("duration")).toDouble() > 0)
        return m_graphPlan.value(QStringLiteral("duration")).toDouble();
    return input().value(QStringLiteral("duration")).toDouble();
}

int Project::activeSteps() const
{
    int n = 0;
    for (const QVariantMap &s : m_steps->rows())
        n += s.value(QStringLiteral("bypass")).toBool() ? 0 : 1;
    return n;
}

const ProbeResult *Project::probe(int index) const
{
    const auto it = m_info.constFind(uid(index));
    return it == m_info.constEnd() ? nullptr : it->probe.get();
}

// ----------------------------------------------------------------------------
// Inputs

QString Project::addRow(const QVariantMap &row)
{
    const QString uid = QUuid::createUuid().toString(QUuid::Id128).left(12);
    QVariantMap r = row;
    r.insert(QStringLiteral("uid"), uid);
    r.insert(QStringLiteral("probing"), true);
    r.insert(QStringLiteral("health"), QStringLiteral("unknown"));
    r.insert(QStringLiteral("healthText"), tr("Reading…"));
    r.insert(QStringLiteral("tracks"), QVariantList());
    r.insert(QStringLiteral("chapters"), QVariantList());
    r.insert(QStringLiteral("tags"), QVariantMap());
    r.insert(QStringLiteral("cover"), QString());
    r.insert(QStringLiteral("count"), r.value(QStringLiteral("count"), 0));
    r.insert(QStringLiteral("duration"), 0.0);
    r.insert(QStringLiteral("size"), 0.0);
    r.insert(QStringLiteral("bitrate"), 0.0);
    m_info.insert(uid, InputInfo());
    m_info[uid].epoch = ++m_probeEpoch;
    m_inputs->append(r);
    if (m_currentInput < 0)
        setCurrentInput(m_inputs->count() - 1);
    return uid;
}

void Project::updateRow(const QString &uid, const QVariantMap &changes)
{
    const int row = indexOf(uid);
    if (row >= 0)
        m_inputs->update(row, changes);
}

void Project::addFiles(const QList<QUrl> &files)
{
    ProjectEdit edit(this, tr("Add inputs"));
    int added = 0;
    QString firstUid;
    for (const QUrl &url : files) {
        const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
        if (!url.isLocalFile() && !path.startsWith(u'/')) {
            addUrl(url.toString());
            continue;
        }
        const QFileInfo info(path);
        if (info.isDir()) {
            addFolder(QUrl::fromLocalFile(path));
            continue;
        }
        if (!info.exists()) {
            emit notice(tr("%1 doesn't exist").arg(info.fileName()));
            continue;
        }
        const int existing = m_inputs->find(QStringLiteral("path"), info.absoluteFilePath());
        if (existing >= 0) {
            setCurrentInput(existing);
            continue;
        }
        const QString uid = addRow({ { "name", info.fileName() }, { "path", info.absoluteFilePath() }, { "kind", "file" },
                                     { "container", info.suffix().toUpper() } });
        if (firstUid.isEmpty())
            firstUid = uid;
        m_info[uid].pending = 1;
        m_probeQueue << ProbeTask { uid, info.absoluteFilePath(), true };
        ++added;
    }
    probeNext();
    if (!firstUid.isEmpty())
        setCurrentInput(indexOf(firstUid));
}

void Project::addFolder(const QUrl &folder)
{
    ProjectEdit edit(this, tr("Add folder"));
    const QString dir = folder.isLocalFile() ? folder.toLocalFile() : folder.toString();
    QStringList files;
    QDirIterator it(dir, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString f = it.next();
        if (audioExtensions().contains(QFileInfo(f).suffix().toLower()))
            files << f;
    }
    files.sort(Qt::CaseInsensitive);
    if (files.isEmpty()) {
        emit notice(tr("No audio files in %1").arg(QDir(dir).dirName()));
        return;
    }
    const QString uid = addRow({ { "name", QDir(dir).dirName() + u'/' }, { "path", files.first() }, { "kind", "folder" },
                                 { "count", int(files.size()) }, { "container", QFileInfo(files.first()).suffix().toUpper() } });
    InputInfo &info = m_info[uid];
    info.files = files;
    info.pending = int(files.size());
    for (int i = 0; i < files.size(); ++i)
        m_probeQueue << ProbeTask { uid, files[i], i == 0 };
    probeNext();
    setCurrentInput(indexOf(uid));
}

void Project::addUrl(const QString &url)
{
    ProjectEdit edit(this, tr("Add URL"));
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty())
        return;
    const int existing = m_inputs->find(QStringLiteral("path"), trimmed);
    if (existing >= 0) {
        setCurrentInput(existing);
        return;
    }
    const QString uid = addRow({ { "name", trimmed }, { "path", trimmed }, { "kind", "url" }, { "container", "URL" } });
    m_info[uid].pending = 1;
    m_probeQueue << ProbeTask { uid, trimmed, true };
    probeNext();
    setCurrentInput(indexOf(uid));
}

void Project::addGenerator(const QString &source, const QString &name)
{
    if (m_generating || source.trimmed().isEmpty())
        return;
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/generated");
    if (!QDir().mkpath(dir)) {
        emit notice(tr("Couldn't create the generated audio folder"));
        return;
    }
    const QString path = dir + u'/' + QUuid::createUuid().toString(QUuid::Id128) + QStringLiteral(".wav");
    auto *job = new ProcessJob(FfTools::paths().ffmpeg,
                               {"-hide_banner", "-nostdin", "-loglevel", "level+error", "-n", "-f", "lavfi",
                                "-i", source, "-t", "600", "-c:a", "pcm_f32le", path},
                               this);
    m_generator = job;
    m_generating = true;
    emit generationChanged();
    job->setLog(m_log, QStringLiteral("generate"), QStringLiteral("error"));
    job->setBackground(true);
    connect(job, &ProcessJob::finished, this, [this, job, path, name, source](bool ok) {
        m_generator = nullptr;
        m_generating = false;
        emit generationChanged();
        if (ok) {
            ProjectEdit edit(this, tr("Generate input"));
            addFiles({QUrl::fromLocalFile(path)});
            const int row = m_inputs->find(QStringLiteral("path"), path);
            m_inputs->update(row, {{"kind", "generator"}, {"name", name}, {"lavfi", source}});
        } else
            emit notice(job->problems(QStringLiteral("error")).join(u'\n'));
        job->deleteLater();
    });
    job->start();
}

void Project::probeNext()
{
    while (m_probesRunning < kMaxProbes && !m_probeQueue.isEmpty()) {
        const ProbeTask task = m_probeQueue.takeFirst();
        if (!m_info.contains(task.uid))
            continue;           // removed meanwhile
        startProbe(task.uid, task.path, task.primary);
    }
}

void Project::startProbe(const QString &uid, const QString &path, bool primary)
{
    ++m_probesRunning;
    const int epoch = m_info.value(uid).epoch;
    auto *prober = new Prober(path, m_log, this);
    connect(prober, &Prober::finished, this,
            [this, prober, uid, path, primary, epoch](bool ok, const ProbeResult &result, const QString &error, const QStringList &warnings) {
                --m_probesRunning;
                prober->deleteLater();
                if (m_info.contains(uid) && m_info.value(uid).epoch == epoch)
                    probeFinished(uid, path, primary, ok, result, error, warnings);
                probeNext();
            });
    prober->start();
}

void Project::probeFinished(const QString &uid, const QString &path, bool primary, bool ok, const ProbeResult &result,
                            const QString &error, const QStringList &warnings)
{
    auto it = m_info.find(uid);
    if (it == m_info.end())
        return;
    InputInfo &info = *it;
    if (indexOf(uid) >= 0 && info.files.isEmpty() && m_inputs->value(indexOf(uid), QStringLiteral("path")).toString() != path) return;
    --info.pending;
    const bool folder = !info.files.isEmpty();

    if (ok) {
        info.duration += result.duration;
        info.size += result.size;
        if (primary || !info.probe) {
            info.probe = std::make_shared<ProbeResult>(result);
            QVariantList tracks;
            for (const AudioTrack &t : result.tracks)
                tracks << trackMap(t);
            QVariantMap changes { { "formatName", result.formatName }, { "formatLong", result.formatLong },
                                  { "container", result.containerLabel() }, { "bitrate", double(result.bitRate) },
                                  { "tracks", tracks }, { "chapters", chapterList(result.chapters) },
                                  { "tags", result.tags } };
            if (folder)
                changes.insert(QStringLiteral("path"), path);
            updateRow(uid, changes);
            if (result.coverStream >= 0)
                fetchCover(uid, result);
        }
        for (const QString &w : warnings)
            if (info.warnings.size() < 20)
                info.warnings << (folder ? QFileInfo(path).fileName() + QStringLiteral(": ") + w : w);
    } else {
        ++info.failed;
        if (info.firstError.isEmpty())
            info.firstError = folder ? QFileInfo(path).fileName() + QStringLiteral(" (") + error + u')' : error;
    }

    if (info.pending > 0 && folder) {
        updateRow(uid, { { "duration", info.duration }, { "size", double(info.size) },
                         { "healthText", tr("Reading %1 of %2…").arg(info.files.size() - info.pending).arg(info.files.size()) } });
        return;
    }
    if (info.pending > 0)
        return;

    QString health, text;
    if (!info.probe) {
        health = QStringLiteral("error");
        text = folder ? tr("None of the %1 files could be read").arg(info.files.size()) : info.firstError;
    } else if (info.failed > 0) {
        health = QStringLiteral("error");
        text = tr("%1 of %2 files fail to read: %3").arg(info.failed).arg(info.files.size()).arg(info.firstError);
    } else if (!info.warnings.isEmpty()) {
        health = QStringLiteral("warn");
        text = info.warnings.size() == 1 ? info.warnings.first()
                                          : tr("%1 warnings: %2").arg(info.warnings.size()).arg(info.warnings.mid(0, 2).join(QStringLiteral("; ")));
    } else {
        health = QStringLiteral("ok");
        text = tr("Reads cleanly; checking the full decode…");
    }
    updateRow(uid, { { "probing", false }, { "duration", info.duration }, { "size", double(info.size) },
                     { "health", health }, { "healthText", text } });
    if (!info.probe && m_log)
        m_log->append({}, QStringLiteral("error"), QStringLiteral("%1: %2").arg(path, text), QStringLiteral("probe"));
    emit inputProbed(uid, info.probe != nullptr);
}

void Project::fetchCover(const QString &uid, const ProbeResult &result)
{
    if (!m_covers)
        return;
    auto *job = new CoverArt(result.path, result.coverStream, m_log, this);
    connect(job, &CoverArt::finished, this, [this, job, uid](bool ok, const QImage &image) {
        job->deleteLater();
        if (!ok || !m_info.contains(uid))
            return;
        m_covers->set(uid, image);
        updateRow(uid, { { "cover", QStringLiteral("image://cover/%1?v=%2").arg(uid).arg(++m_coverVersion) } });
    });
    job->start();
}

void Project::setHealth(const QString &uid, const QString &health, const QString &text)
{
    updateRow(uid, { { "health", health }, { "healthText", text } });
}

void Project::removeInput(int index)
{
    ProjectEdit edit(this, tr("Remove input"));
    const QString id = uid(index);
    if (id.isEmpty())
        return;
    // Keep probe data for undo; source rows determine which inputs are active.
    if (m_covers)
        m_covers->remove(id);
    const bool wasCurrent = index == m_currentInput;
    m_inputs->removeAt(index);
    if (m_currentInput > index) {
        --m_currentInput;
        emit currentInputChanged();
    } else if (wasCurrent) {
        m_currentInput = -1;
        setCurrentInput(std::min(index, m_inputs->count() - 1));
        if (m_currentInput < 0) {
            emit currentInputChanged();
            emit inputChanged();
        }
    }
}

void Project::reprobe(int index)
{
    const QString id = uid(index);
    auto it = m_info.find(id);
    if (it == m_info.end() || it->pending > 0)
        return;
    const QStringList files = it->files;
    const QString path = m_inputs->value(index, QStringLiteral("path")).toString();
    *it = InputInfo();
    it->epoch = ++m_probeEpoch;
    it->files = files;
    updateRow(id, { { "probing", true }, { "health", "unknown" }, { "healthText", tr("Reading…") } });
    if (files.isEmpty()) {
        it->pending = 1;
        m_probeQueue << ProbeTask { id, path, true };
    } else {
        it->pending = int(files.size());
        for (int i = 0; i < files.size(); ++i)
            m_probeQueue << ProbeTask { id, files[i], i == 0 };
    }
    probeNext();
}

void Project::replaceInput(int index, const QUrl &file)
{
    ProjectEdit edit(this, tr("Replace input"));
    if (index < 0 || index >= m_inputs->count() || !file.isLocalFile())
        return;
    const QFileInfo info(file.toLocalFile());
    const QString id = uid(index);
    m_info[id] = InputInfo();
    m_info[id].epoch = ++m_probeEpoch;
    m_info[id].pending = 1;
    m_inputs->update(index, { { "name", info.fileName() }, { "path", info.absoluteFilePath() }, { "kind", "file" },
                              { "count", 0 }, { "probing", true }, { "health", "unknown" }, { "healthText", tr("Reading…") },
                              { "cover", QString() } });
    m_probeQueue << ProbeTask { id, info.absoluteFilePath(), true };
    probeNext();
}

void Project::setCoverArt(const QUrl &url)
{
    if (!hasInput() || !url.isLocalFile())
        return;
    QImageReader reader(url.toLocalFile());
    if (reader.size().width() > 16384 || reader.size().height() > 16384) {
        emit notice(tr("Choose cover art smaller than 16384 pixels"));
        return;
    }
    QImage image = reader.read();
    if (image.isNull()) {
        emit notice(tr("Couldn't read the cover image"));
        return;
    }
    if (image.width() > 2048 || image.height() > 2048)
        image = image.scaled(2048, 2048, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    ProjectEdit edit(this, tr("Replace cover art"));
    auto data = m_edits.value(uid(m_currentInput)).toMap();
    data.insert(QStringLiteral("coverAction"), QStringLiteral("replace"));
    data.insert(QStringLiteral("coverData"), QString::fromLatin1(png.toBase64()));
    m_edits.insert(uid(m_currentInput), data);
    emit tagsChanged();
}
void Project::useImpulseResponse(const QUrl &file)
{
    if (!file.isLocalFile() || !hasInput())
        return;
    ProjectEdit edit(this, tr("Add impulse response"));
    const int originalInput = m_currentInput;
    promoteGraph();
    addFiles({file});
    const auto source = addGraphSource(m_inputs->find(QStringLiteral("path"), file.toLocalFile()));
    setCurrentInput(originalInput);
    const auto convolution = addGraphOperation(QStringLiteral("convolve"));
    if (!source.isEmpty() && !convolution.isEmpty())
        connectGraph(source, 0, convolution, 1);
}

// ----------------------------------------------------------------------------
// Chain, outputs and files.

void Project::setCatalog(CatalogFacade *catalog)
{
    m_catalog = catalog;
    connect(catalog, &CatalogFacade::changed, this, &Project::refreshDerived);
    refreshDerived();
}

namespace {
QString newId() { return QUuid::createUuid().toString(QUuid::Id128).left(12); }
QVariantMap pick(const QVariantMap &row, const QStringList &keys)
{
    QVariantMap result;
    for (const auto &key : keys) if (row.value(key).isValid()) result.insert(key, row.value(key));
    return result;
}
void reconcile(RowModel *model, const QList<QVariantMap> &rows)
{
    for (int i = 0; i < rows.size(); ++i) {
        const int existing = model->find(QStringLiteral("uid"), rows[i].value(QStringLiteral("uid")));
        if (existing < 0) model->insertAt(i, rows[i]);
        else {
            if (existing != i) model->move(existing, i);
            model->update(i, rows[i]);
        }
    }
    while (model->count() > rows.size()) model->removeAt(model->count()-1);
}
}

Recipe Project::recipe() const
{
    Recipe r;
    r.name = m_name;
    r.metadata = m_tags;
    r.edits = m_edits;
    r.graph = m_graphState;
    r.markers = m_markers;
    for (const auto &row : m_inputs->rows()) {
        auto input = pick(row, {"uid", "name", "path", "kind", "lavfi"});
        input.insert(QStringLiteral("files"), inputFiles(m_inputs->find(QStringLiteral("uid"), row.value(QStringLiteral("uid")))));
        r.inputs << input;
    }
    for (const auto &row : m_steps->rows()) r.nodes << pick(row, { "uid", "type", "params", "bypass", "hasRange", "rangeStart", "rangeEnd" });
    for (int i = 1; i < r.nodes.size(); ++i)
        r.links << QVariantMap { { "from", r.nodes[i-1].value(QStringLiteral("uid")) }, { "to", r.nodes[i].value(QStringLiteral("uid")) } };
    for (const auto &row : m_outputs->rows()) r.outputs << pick(row, { "uid", "pattern", "folder", "preset", "container", "codec", "quality", "rate", "depth", "channels", "metadata", "encoderOptions", "muxerOptions" });
    return r;
}

QStringList Project::inputFiles(int index) const { return m_info.value(uid(index)).files; }

QVariantMap Project::snapshot() const
{
    return RecipeJson::encode(recipe()).toVariantMap();
}

void Project::restoreSnapshot(const QVariantMap &snapshot)
{
    Recipe r;
    QString error;
    if (!RecipeJson::decode(QJsonObject::fromVariantMap(snapshot), &r, &error)) { emit notice(error); return; }
    applyRecipe(r);
}

void Project::applyRecipe(const Recipe &r)
{
    const QString current = uid(m_currentInput);
    const QString output = m_outputs->value(m_currentOutput, QStringLiteral("uid")).toString();
    QList<QVariantMap> inputs;
    for (const auto &row : r.inputs) {
        const QString id = row.value(QStringLiteral("uid")).toString();
        QVariantMap full = m_inputs->get(indexOf(id));
        if (full.isEmpty()) full = { { "tracks", QVariantList() }, { "chapters", QVariantList() }, { "tags", QVariantMap() }, { "cover", QString() }, { "probing", true }, { "duration", 0.0 }, { "health", "unknown" }, { "healthText", tr("Reading…") }, { "container", QFileInfo(row.value(QStringLiteral("path")).toString()).suffix().toUpper() } };
        for (auto it = row.begin(); it != row.end(); ++it) full.insert(it.key(), it.value());
        inputs << full;
        auto &info = m_info[id];
        if (info.probe && info.probe->path != row.value(QStringLiteral("path")).toString()) info = InputInfo();
        if (!info.epoch) info.epoch = ++m_probeEpoch;
        info.files = row.value(QStringLiteral("files")).toStringList();
    }
    reconcile(m_inputs, inputs);
    reconcile(m_steps, r.nodes);
    reconcile(m_outputs, r.outputs);
    m_name = r.name;
    m_tags = r.metadata;
    m_edits = r.edits;
    m_graphState = r.graph;
    m_markers = r.markers;
    m_currentInput = std::clamp(indexOf(current), 0, std::max(0, m_inputs->count()-1));
    if (!m_inputs->count()) m_currentInput = -1;
    m_currentOutput = m_outputs->find(QStringLiteral("uid"), output);
    if (m_currentOutput < 0 && m_outputs->count()) m_currentOutput = 0;
    for (int i = 0; i < m_inputs->count(); ++i) {
        auto &info = m_info[uid(i)];
        if (info.probe) {
            const auto &p = *info.probe;
            QVariantList tracks;
            for (const auto &track : p.tracks) tracks << trackMap(track);
            QVariantMap restored { { "tracks", tracks }, { "chapters", chapterList(p.chapters) },
                { "duration", info.duration }, { "size", double(info.size) }, { "probing", info.pending > 0 },
                { "formatName", p.formatName }, { "formatLong", p.formatLong }, { "container", p.containerLabel() },
                { "bitrate", double(p.bitRate) }, { "tags", p.tags } };
            if (m_inputs->value(i, QStringLiteral("health")).toString() == u"unknown" && !info.pending) {
                restored.insert(QStringLiteral("health"), info.failed ? QStringLiteral("error") : info.warnings.isEmpty() ? QStringLiteral("ok") : QStringLiteral("warn"));
                restored.insert(QStringLiteral("healthText"), info.failed ? info.firstError : info.warnings.isEmpty() ? tr("Reads cleanly; checking the full decode…") : info.warnings.join(QStringLiteral("; ")));
            }
            m_inputs->update(i, restored);
            if (p.coverStream >= 0 && m_inputs->value(i, QStringLiteral("cover")).toString().isEmpty()) fetchCover(uid(i), p);
        } else if (info.pending == 0) {
            m_inputs->update(i, { { "probing", true }, { "tracks", QVariantList() }, { "chapters", QVariantList() }, { "duration", 0.0 } });
            const QStringList paths = info.files.isEmpty() ? QStringList { m_inputs->value(i, QStringLiteral("path")).toString() } : info.files;
            info.pending = int(paths.size());
            for (int f = 0; f < paths.size(); ++f) m_probeQueue << ProbeTask { uid(i), paths[f], f == 0 };
        }
    }
    emit nameChanged(); emit tagsChanged();
    emit currentInputChanged(); emit currentOutputChanged(); emit inputChanged();
    refreshDerived();
    probeNext();
}

QVariantMap Project::makeStep(const QString &type, const QVariantMap &overrides) const
{
    const auto &catalog = m_catalog ? m_catalog->catalog() : Catalog::builtIn();
    const auto *spec = catalog.step(type);
    if (!spec) return {};
    QVariantMap params = spec->defaults();
    if (type == u"trim") params.insert(QStringLiteral("end"), duration());
    for (auto it = overrides.begin(); it != overrides.end(); ++it) params.insert(it.key(), it.value());
    return { { "uid", newId() }, { "type", type }, { "params", params }, { "bypass", false }, { "hasRange", false }, { "rangeStart", 0.0 }, { "rangeEnd", 0.0 } };
}

void Project::insertStep(int index, const QString &type)
{
    const auto row = makeStep(type);
    if (row.isEmpty()) { emit notice(tr("Unknown step: %1").arg(type)); return; }
    const bool operation = ffgui::GraphCompiler::operation(type);
    if ((operation || graphActive()) && graphFull()) return;
    ProjectEdit edit(this, tr("Add step"));
    // Graph operations connect several signals, so they promote the graph and
    // never take a time range.
    if (operation)
        promoteGraph();
    auto ranged = row;
    if (!operation && m_pendingRangeEnd > m_pendingRangeStart) {
        ranged.insert(QStringLiteral("hasRange"), true); ranged.insert(QStringLiteral("rangeStart"), m_pendingRangeStart); ranged.insert(QStringLiteral("rangeEnd"), m_pendingRangeEnd);
        m_pendingRangeStart = m_pendingRangeEnd = 0;
    }
    m_steps->insertAt(index, ranged);
}
void Project::addStep(const QString &type, const QVariantMap &params)
{
    const auto row = makeStep(type, params);
    if (row.isEmpty()) { emit notice(tr("Unknown step: %1").arg(type)); return; }
    const bool operation = ffgui::GraphCompiler::operation(type);
    if ((operation || graphActive()) && graphFull()) return;
    ProjectEdit edit(this, tr("Add step"));
    if (operation)
        promoteGraph();
    m_steps->append(row);
}
void Project::addRangedStep(const QString &type, const QVariantMap &params, double start, double end)
{
    ProjectEdit edit(this, tr("Add ranged step"));
    addStep(type, params);
    setRange(m_steps->count()-1,start,end);
}
void Project::removeStep(int index)
{
    // In graph mode the node's neighbours are reconnected.
    if (graphActive()) { removeGraphNode(m_steps->value(index, QStringLiteral("uid")).toString()); return; }
    ProjectEdit edit(this, tr("Remove step"));
    m_steps->removeAt(index);
}
void Project::moveStep(int from, int to) { ProjectEdit edit(this, tr("Move step")); m_steps->move(from, to); }
void Project::editStep(int index, const QVariantMap &changes, const QString &label, const QString &key)
{
    ProjectEdit edit(this, label, key);
    m_steps->update(index, changes);
}
void Project::setBypass(int index, bool bypass) { editStep(index, { { "bypass", bypass } }, tr("Bypass step")); }
void Project::setParam(int index, const QString &key, const QVariant &value)
{
    auto params = m_steps->value(index, QStringLiteral("params")).toMap();
    params.insert(key, value);
    if (key == u"volume") params.remove(QStringLiteral("mute"));
    editStep(index, { { "params", params } }, tr("Change %1").arg(key), m_steps->value(index, QStringLiteral("uid")).toString() + u':' + key);
    emit paramEdited(index, key, value);
}
void Project::setRange(int index, double start, double end)
{
    if (!std::isfinite(start) || !std::isfinite(end)) return;
    if (end < start) std::swap(start, end);
    start = std::max(0.0, start); end = std::max(start, end);
    if (duration() > 0) { start = std::min(start, duration()); end = std::min(end, duration()); }
    editStep(index, { { "hasRange", true }, { "rangeStart", start }, { "rangeEnd", end } }, tr("Change step range"), m_steps->value(index, QStringLiteral("uid")).toString() + QStringLiteral(":range"));
}
void Project::clearRange(int index) { editStep(index, { { "hasRange", false } }, tr("Clear step range")); }
void Project::applyChainPreset(const QString &name)
{
    const auto &catalog = m_catalog ? m_catalog->catalog() : Catalog::builtIn();
    const auto *preset = catalog.preset(name);
    if (!preset || preset->kind != u"chain") return;
    if (graphActive() && !canUseRow()) { emit notice(tr("Chain presets hold a single path; save the project to keep graph connections")); return; }
    ProjectEdit edit(this, tr("Apply %1").arg(name));
    useRow();
    m_steps->clear();
    for (const auto &step : preset->steps) {
        addStep(step.type, step.params);
        m_steps->update(m_steps->count()-1, step.settings);
    }
}
void Project::applySelectionAction(const QString &action, double start, double end)
{
    if (end <= start || start < 0) return;
    ProjectEdit edit(this, tr("Selection: %1").arg(action));
    if (action == u"trim" || action == u"cut") addStep(QStringLiteral("trim"), { { "start", start }, { "end", end }, { "cut", action == u"cut" } });
    else if (action == u"fadeIn" || action == u"fadeOut") addStep(QStringLiteral("fade"), { { "t", action == u"fadeIn" ? "in" : "out" }, { "st", start }, { "d", end-start } });
    else if (action == u"silence") {
        addStep(QStringLiteral("gain"), { { "volume", -120.0 }, { "mute", true } });
        setRange(m_steps->count()-1, start, end);
    } else if (action == u"marker")
        m_markers << QVariantMap{
            {"start", start}, {"end", end}, {"title", tr("Marker")}, {"input", uid(m_currentInput)}};
    else {
        if (action == u"step") { m_pendingRangeStart = start; m_pendingRangeEnd = end; }
        emit selectionActionRequested(action, start, end);
    }
}

void Project::addOutput(const QString &preset)
{
    ProjectEdit edit(this, tr("Add output"));
    m_outputs->append({ { "uid", newId() }, { "pattern", "{name}-processed" + (m_outputs->count() ? QStringLiteral("-%1").arg(m_outputs->count()+1) : QString()) }, { "folder", QString() }, { "preset", QString() }, { "metadata", "copy" }, { "encoderOptions", QVariantMap() }, { "muxerOptions", QVariantMap() } });
    setCurrentOutput(m_outputs->count()-1);
    QString name = preset.isEmpty() ? QStringLiteral("Archive FLAC") : preset;
    if (preset.isEmpty() && m_catalog && !m_catalog->preset(name).value(QStringLiteral("available")).toBool()) {
        for (const auto &p : m_catalog->presets()->rows())
            if (p.value(QStringLiteral("available")).toBool()) { name = p.value(QStringLiteral("name")).toString(); break; }
    }
    applyPreset(m_currentOutput, name);
}
void Project::removeOutput(int index)
{
    ProjectEdit edit(this, tr("Remove output"));
    m_outputs->removeAt(index);
    setCurrentOutput(std::min(m_currentOutput, m_outputs->count()-1));
}
void Project::setOutput(int index, const QVariantMap &changes)
{
    ProjectEdit edit(this, tr("Change output"), m_outputs->value(index, QStringLiteral("uid")).toString() + (changes.size() == 1 ? u':' + changes.firstKey() : QString()));
    auto adjusted = changes;
    if (adjusted.contains(QStringLiteral("folder"))) {
        const QUrl url(adjusted.value(QStringLiteral("folder")).toString());
        if (url.isLocalFile()) adjusted.insert(QStringLiteral("folder"), url.toLocalFile());
    }
    const auto &catalog = m_catalog ? m_catalog->catalog() : Catalog::builtIn();
    if (changes.contains(QStringLiteral("container"))) {
        const auto *c = catalog.container(changes.value(QStringLiteral("container")).toString());
        const QString codec = changes.value(QStringLiteral("codec"), m_outputs->value(index, QStringLiteral("codec"))).toString();
        if (c && !c->codecs.contains(codec) && !c->codecs.isEmpty()) adjusted.insert(QStringLiteral("codec"), c->codecs.first());
    }
    if (changes.value(QStringLiteral("depth")).toInt() > 0) {
        const QString current = m_outputs->value(index, QStringLiteral("codec")).toString();
        if (current.startsWith(u"pcm_")) {
            const int depth = changes.value(QStringLiteral("depth")).toInt();
            const QString target = (depth == 32 ? QStringLiteral("pcm_f32") : QStringLiteral("pcm_s%1").arg(depth)) + (current.endsWith(u"be") ? QStringLiteral("be") : QStringLiteral("le"));
            if (catalog.codec(target)) adjusted.insert(QStringLiteral("codec"), target);
        }
    }
    if (adjusted.contains(QStringLiteral("codec"))) {
        if (const auto *c = catalog.codec(adjusted.value(QStringLiteral("codec")).toString())) {
            adjusted.insert(QStringLiteral("quality"), c->quality.defaultValue);
            adjusted.insert(QStringLiteral("depth"), c->depth);
        }
        adjusted.insert(QStringLiteral("encoderOptions"), QVariantMap());
    }
    adjusted.insert(QStringLiteral("preset"), QString());
    m_outputs->update(index, adjusted);
}
void Project::applyPreset(int index, const QString &name)
{
    const auto &catalog = m_catalog ? m_catalog->catalog() : Catalog::builtIn();
    const auto *p = catalog.preset(name);
    if (!p || p->kind != u"output") return;
    ProjectEdit edit(this, tr("Apply output preset"));
    m_outputs->update(index, { { "preset", name }, { "container", p->container }, { "codec", p->codec }, { "quality", p->quality }, { "rate", p->rate }, { "depth", p->depth }, { "channels", p->channels }, { "encoderOptions", QVariantMap() }, { "muxerOptions", QVariantMap() } });
    m_outputs->update(index, p->settings);
}

QVariantMap Project::tags() const
{
    const auto data = m_edits.value(uid(m_currentInput)).toMap();
    return data.contains(QStringLiteral("tags")) ? data.value(QStringLiteral("tags")).toMap()
           : !m_tags.isEmpty()                   ? m_tags
                                                 : input().value(QStringLiteral("tags")).toMap();
}
void Project::setTag(const QString &key, const QString &value)
{
    if (key == u"__cover") {
        removeCover();
        return;
    }
    if (!hasInput() || key.trimmed().isEmpty())
        return;
    ProjectEdit edit(this, tr("Edit tag"), uid(m_currentInput) + QStringLiteral(":tag:") + key);
    auto all = tags();
    all.insert(key, value);
    auto data = m_edits.value(uid(m_currentInput)).toMap();
    data.insert(QStringLiteral("tags"), all);
    m_edits.insert(uid(m_currentInput), data);
    emit tagsChanged();
}
void Project::removeTag(const QString &key)
{
    if (!hasInput())
        return;
    ProjectEdit edit(this, tr("Remove tag"));
    auto all = tags();
    all.remove(key);
    auto data = m_edits.value(uid(m_currentInput)).toMap();
    data.insert(QStringLiteral("tags"), all);
    m_edits.insert(uid(m_currentInput), data);
    emit tagsChanged();
}
QVariantList Project::chapters() const
{
    const auto data = m_edits.value(uid(m_currentInput)).toMap();
    QVariantList result = data.contains(QStringLiteral("chapters"))
                              ? data.value(QStringLiteral("chapters")).toList()
                              : input().value(QStringLiteral("chapters")).toList();
    for (const auto &marker : m_markers)
        if (marker.value(QStringLiteral("input"), uid(m_currentInput)).toString() == uid(m_currentInput))
            result << marker;
    std::stable_sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        return a.toMap().value(QStringLiteral("start")).toDouble() <
               b.toMap().value(QStringLiteral("start")).toDouble();
    });
    return result;
}
void Project::storeChapters(const QVariantList &chapters)
{
    auto data = m_edits.value(uid(m_currentInput)).toMap();
    data.insert(QStringLiteral("chapters"), chapters);
    m_edits.insert(uid(m_currentInput), data);
    m_markers.removeIf([this](const auto &m) {
        return m.value(QStringLiteral("input"), uid(m_currentInput)).toString() == uid(m_currentInput);
    });
    emit tagsChanged();
}
void Project::setChapter(int index, double start, double end, const QString &title)
{
    auto all = chapters();
    if (index < 0 || index >= all.size() || !std::isfinite(start) || !std::isfinite(end) || start < 0 ||
        end <= start)
        return;
    ProjectEdit edit(this, tr("Edit chapter"));
    all[index] = QVariantMap{{"start", start}, {"end", end}, {"title", title}};
    storeChapters(all);
}
void Project::removeChapter(int index)
{
    auto all = chapters();
    if (index < 0 || index >= all.size())
        return;
    ProjectEdit edit(this, tr("Remove chapter"));
    all.removeAt(index);
    storeChapters(all);
}
void Project::addChapter(double start, double end, const QString &title)
{
    if (!hasInput() || !std::isfinite(start) || !std::isfinite(end) || start < 0 || end <= start)
        return;
    ProjectEdit edit(this, tr("Add chapter"));
    auto all = chapters();
    all << QVariantMap{{"start", start}, {"end", end}, {"title", title}};
    storeChapters(all);
}
QVariantList Project::editableTracks() const
{
    QVariantList tracks = input().value(QStringLiteral("tracks")).toList();
    const auto edits = m_edits.value(uid(m_currentInput)).toMap().value(QStringLiteral("tracks")).toMap();
    for (auto &value : tracks) {
        auto row = value.toMap();
        const auto changes =
            edits.value(QString::number(row.value(QStringLiteral("audioIndex")).toInt())).toMap();
        for (auto it = changes.begin(); it != changes.end(); ++it)
            row.insert(it.key(), it.value());
        value = row;
    }
    return tracks;
}
void Project::setTrackMetadata(int audioIndex, const QVariantMap &changes)
{
    if (!hasInput() || audioIndex < 0 ||
        audioIndex >= input().value(QStringLiteral("tracks")).toList().size())
        return;
    ProjectEdit edit(this, tr("Edit track metadata"));
    auto data = m_edits.value(uid(m_currentInput)).toMap();
    auto tracks = data.value(QStringLiteral("tracks")).toMap();
    auto track = tracks.value(QString::number(audioIndex)).toMap();
    for (const auto &key : {QStringLiteral("title"), QStringLiteral("lang"), QStringLiteral("isDefault")})
        if (changes.contains(key))
            track.insert(key, changes.value(key));
    tracks.insert(QString::number(audioIndex), track);
    data.insert(QStringLiteral("tracks"), tracks);
    m_edits.insert(uid(m_currentInput), data);
    emit tagsChanged();
}
QString Project::cover() const
{
    const auto data = m_edits.value(uid(m_currentInput)).toMap();
    const QString action = data.value(QStringLiteral("coverAction")).toString();
    return action == u"remove" ? QString()
           : action == u"replace"
               ? QStringLiteral("data:image/png;base64,") + data.value(QStringLiteral("coverData")).toString()
               : input().value(QStringLiteral("cover")).toString();
}
void Project::removeCover()
{
    if (!hasInput())
        return;
    ProjectEdit edit(this, tr("Remove cover art"));
    auto data = m_edits.value(uid(m_currentInput)).toMap();
    data.insert(QStringLiteral("coverAction"), QStringLiteral("remove"));
    data.remove(QStringLiteral("coverData"));
    m_edits.insert(uid(m_currentInput), data);
    emit tagsChanged();
}
void Project::extractCover(const QUrl &file)
{
    if (!file.isLocalFile())
        return;
    const auto data = m_edits.value(uid(m_currentInput)).toMap();
    if (data.value(QStringLiteral("coverAction")).toString() == u"replace") {
        const auto image = QImage::fromData(
            QByteArray::fromBase64(data.value(QStringLiteral("coverData")).toString().toLatin1()));
        emit notice(image.save(file.toLocalFile()) ? tr("Cover art exported")
                                                   : tr("Couldn't write cover art"));
        return;
    }
    const auto *p = probe(m_currentInput);
    if (!p || p->coverStream < 0)
        return;
    auto *job = new CoverArt(p->path, p->coverStream, m_log, this);
    connect(job, &CoverArt::finished, this, [this, job, file](bool ok, const QImage &image) {
        emit notice(ok && image.save(file.toLocalFile()) ? tr("Cover art exported")
                                                         : tr("Couldn't write cover art"));
        job->deleteLater();
    });
    job->start();
}
QVariantMap Project::outputMetadata(int inputIndex) const
{
    if (inputIndex < 0 || inputIndex == m_currentInput) {
        auto data = m_edits.value(uid(m_currentInput)).toMap();
        data.insert(QStringLiteral("tags"), tags());
        data.insert(QStringLiteral("chapters"), chapters());
        return data;
    }
    const QString id = uid(inputIndex);
    auto data = m_edits.value(id).toMap();
    const auto original = m_inputs->get(inputIndex);
    if (!data.contains(QStringLiteral("tags")))
        data.insert(QStringLiteral("tags"),
                    m_tags.isEmpty() ? original.value(QStringLiteral("tags")).toMap() : m_tags);
    auto chapters = data.contains(QStringLiteral("chapters"))
                        ? data.value(QStringLiteral("chapters")).toList()
                        : original.value(QStringLiteral("chapters")).toList();
    for (const auto &marker : m_markers)
        if (marker.value(QStringLiteral("input")).toString() == id)
            chapters << marker;
    data.insert(QStringLiteral("chapters"), chapters);
    return data;
}

void Project::newProject()
{
    if (m_generator) {
        disconnect(m_generator, nullptr, this, nullptr);
        delete m_generator;
        m_generator = nullptr;
        m_generating = false;
        emit generationChanged();
    }
    m_probeQueue.clear();
    m_info.clear();
    m_currentTrack = -1;
    m_pendingRangeStart = m_pendingRangeEnd = 0;
    if (m_selection) m_selection->clear();
    applyRecipe(Recipe());
    m_fileUrl = QUrl();
    m_undo.clear();
    m_undo.setClean();
    emit fileUrlChanged();
}
bool Project::open(const QUrl &file)
{
    Recipe recipe;
    QString error;
    if (!file.isLocalFile() || !RecipeJson::load(file.toLocalFile(), &recipe, &error)) { emit notice(tr("Couldn't open project: %1").arg(error)); return false; }
    if (m_generator) {
        disconnect(m_generator, nullptr, this, nullptr);
        delete m_generator;
        m_generator = nullptr;
        m_generating = false;
        emit generationChanged();
    }
    m_probeQueue.clear();
    m_info.clear();
    m_currentTrack = -1;
    if (m_selection) m_selection->clear();
    applyRecipe(recipe);
    m_fileUrl = file;
    m_undo.clear();
    m_undo.setClean();
    emit fileUrlChanged();
    return true;
}
bool Project::save(const QUrl &file)
{
    const QUrl destination = file.isEmpty() ? m_fileUrl : file;
    QString error;
    if (!destination.isLocalFile() || !RecipeJson::save(destination.toLocalFile(), recipe(), &error)) { emit notice(tr("Couldn't save project: %1").arg(error)); return false; }
    m_fileUrl = destination;
    m_undo.setClean();
    emit fileUrlChanged();
    return true;
}

void Project::refreshDerived()
{
    if (m_derived) return;
    m_derived = true;
    syncGraphState();
    const auto &catalog = m_catalog ? m_catalog->catalog() : Catalog::builtIn();
    for (int i = 0; i < m_steps->count(); ++i) {
        const auto row = m_steps->get(i);
        const auto *s = catalog.step(row.value(QStringLiteral("type")).toString());
        if (!s) {
            m_steps->update(i, { { "name", row.value(QStringLiteral("type")).toString() }, { "glyph", QStringLiteral("?") }, { "category", tr("Unknown") }, { "filter", QString() }, { "timeline", QString() }, { "rangeSupported", false }, { "available", false }, { "filterText", QString() }, { "warning", tr("Unknown step type") }, { "formatIn", QString() }, { "formatOut", QString() }, { "formatChanged", false } });
            continue;
        }
        bool range = !s->filters.isEmpty();
        for (const auto &f : s->filters) range &= m_catalog && m_catalog->filterTimeline(f);
        QString error;
        const QString filter = StepBuilders::build(*s, row, track().value(QStringLiteral("channels"), 2).toInt(), &error);
        const QString problem = m_catalog ? m_catalog->stepProblem(*s) : QString();
        m_steps->update(i, { { "name", s->name }, { "glyph", s->glyph }, { "category", s->category }, { "filter", s->filters.join(QStringLiteral(" + ")) }, { "timeline", s->timeline }, { "rangeSupported", range }, { "available", problem.isEmpty() }, { "filterText", filter }, { "warning", !problem.isEmpty() ? problem : error }, { "formatIn", row.value(QStringLiteral("formatIn"), QString()).toString() }, { "formatOut", row.value(QStringLiteral("formatOut"), QString()).toString() }, { "formatChanged", row.value(QStringLiteral("formatChanged"), false).toBool() } });
    }
    // Output names label the graph's output nodes; the graph's plan gives the
    // timeline. A graph keeps its whole timeline; a row keeps what its Trim
    // steps keep.
    const bool graph = graphActive();
    const auto sourceTrack = renderTrack();
    const QString source = m_inputs->value(renderInput(), QStringLiteral("path")).toString();
    for (int i = 0; i < m_outputs->count(); ++i) {
        const auto out = m_outputs->get(i);
        const auto *codec = catalog.codec(out.value(QStringLiteral("codec")).toString());
        auto advice = CopyAdvisor::advise(sourceTrack, out, m_steps->rows(), catalog);
        if (graph) { advice.copy = false; advice.reason = tr("Graph processing requires encoding"); }
        m_outputs->update(i, { { "fileName", QFileInfo(CommandBuilder::outputPath(source, out, catalog)).fileName() }, { "codecLabel", codec ? codec->label : QString() }, { "reencodeReason", advice.reason }, { "lossyToLossy", advice.lossyToLossy }, { "problem", codec ? QString() : tr("Choose a codec") } });
    }
    refreshGraph();
    m_keptStart = 0; m_keptEnd = duration();
    const auto trim = graph ? ChainResult() : ChainCompiler::compile(m_steps->rows(), catalog, {}, track().value(QStringLiteral("channels"),2).toInt(), true);
    if (trim.hasTrim) { m_keptStart = trim.start; m_keptEnd = trim.end > trim.start ? std::min(duration(),trim.end) : duration(); }
    const double length = graph ? duration() : ChainCompiler::duration(m_steps->rows(), catalog, duration());
    qint64 size = 0;
    for (int i = 0; i < m_outputs->count(); ++i) {
        const auto *codec = catalog.codec(m_outputs->value(i, QStringLiteral("codec")).toString());
        const double kbps = codec ? SizeEstimator::kbps(*codec, m_outputs->get(i), sourceTrack) : 0;
        const qint64 bytes = SizeEstimator::bytes(kbps, length);
        size += bytes;
        m_outputs->update(i, { { "kbps", kbps }, { "sizeBytes", double(bytes) }, { "sizeText", format::bytes(bytes) } });
    }
    m_totalSizeText = size > 0 ? format::bytes(size) : QString();
    m_derived = false;
    emit chainChanged(); emit outputsChanged();
}

void Project::setNegotiated(const QList<QVariantMap> &nodes, const QVariantMap &converter)
{
    m_converter = converter;
    m_graph->setRows(nodes);
    emit negotiatedChanged();
    // Negotiation is derived data, not an edit and must not retrigger a probe.
}

bool Project::savePreset(const QString &name, bool chain)
{
    if (chain && graphActive() && !canUseRow()) {
        emit notice(tr("Save the project to preserve graph connections"));
        return false;
    }
    return m_catalog && m_catalog->savePreset(name, chain ? QStringLiteral("chain") : QStringLiteral("output"), m_outputs->get(m_currentOutput), m_steps->rows());
}

// ----------------------------------------------------------------------------
// Graph mode. The editable topology (graphNodes, graphLinks) is separate from
// the negotiated FFmpeg graph in `graph`. A linear project derives it from its
// rows; the first wiring edit promotes it into m_graphState, the recipe's
// `graph` field: node descriptors that name a step, input or output by uid,
// port links and positions. Step parameters stay in the step rows.

namespace {
QString endpointOf(const QVariantMap &link, bool target)
{
    return target ? link.value(QStringLiteral("to")).toString() + u':' + QString::number(link.value(QStringLiteral("toPort")).toInt())
                  : link.value(QStringLiteral("from")).toString() + u':' + QString::number(link.value(QStringLiteral("fromPort")).toInt());
}
QVariantMap graphLink(const QString &uid, const QString &from, int fromPort, const QString &to, int toPort)
{
    return { { "uid", uid }, { "from", from }, { "to", to }, { "fromPort", fromPort }, { "toPort", toPort } };
}
}

QList<QVariantMap> Project::graphDescriptors() const
{
    QList<QVariantMap> nodes;
    if (graphActive()) {
        for (const auto &v : m_graphState.value(QStringLiteral("nodes")).toList())
            nodes << v.toMap();
        return nodes;
    }
    if (hasInput())
        nodes << QVariantMap { { "uid", "in_" + uid(m_currentInput) }, { "kind", "source" }, { "inputUid", uid(m_currentInput) },
                               { "audioIndex", track().value(QStringLiteral("audioIndex"), 0) } };
    for (const auto &step : m_steps->rows())
        nodes << QVariantMap { { "uid", step.value(QStringLiteral("uid")) }, { "kind", "step" } };
    for (const auto &out : m_outputs->rows())
        nodes << QVariantMap { { "uid", "out_" + out.value(QStringLiteral("uid")).toString() }, { "kind", "output" }, { "outputUid", out.value(QStringLiteral("uid")) } };
    return nodes;
}

QList<QVariantMap> Project::graphConnections() const
{
    QList<QVariantMap> links;
    if (graphActive()) {
        for (const auto &v : m_graphState.value(QStringLiteral("links")).toList())
            links << v.toMap();
        return links;
    }
    QString previous = hasInput() ? "in_" + uid(m_currentInput) : QString();
    auto add = [&](const QString &to) {
        if (!previous.isEmpty())
            links << graphLink("link_" + previous + u'_' + to, previous, 0, to, 0);
    };
    for (const auto &step : m_steps->rows()) {
        add(step.value(QStringLiteral("uid")).toString());
        previous = step.value(QStringLiteral("uid")).toString();
    }
    for (const auto &out : m_outputs->rows())
        add("out_" + out.value(QStringLiteral("uid")).toString());
    return links;
}

// Keeps the stored topology consistent with the rows: nodes whose step, input
// or output is gone leave with their links, as do links to ports a node no
// longer has; new steps join the selected output's branch and new outputs
// share the first output's signal. Positions of departed nodes are dropped.
void Project::syncGraphState()
{
    if (graphActive()) {
        QVariantList nodes, links;
        QHash<QString, QVariantMap> shape;
        for (const auto &v : m_graphState.value(QStringLiteral("nodes")).toList()) {
            auto n = v.toMap();
            const QString id = n.value(QStringLiteral("uid")).toString(), kind = n.value(QStringLiteral("kind")).toString();
            const int step = kind == u"step" ? m_steps->find(QStringLiteral("uid"), id) : -1;
            const bool exists = kind == u"step" ? step >= 0
                              : kind == u"output" ? m_outputs->find(QStringLiteral("uid"), n.value(QStringLiteral("outputUid"))) >= 0
                              : kind == u"source" && indexOf(n.value(QStringLiteral("inputUid")).toString()) >= 0;
            if (id.isEmpty() || !exists || shape.contains(id))
                continue;
            if (step >= 0) {
                n.insert(QStringLiteral("type"), m_steps->value(step, QStringLiteral("type")));
                n.insert(QStringLiteral("params"), m_steps->value(step, QStringLiteral("params")));
            }
            shape.insert(id, n);
            nodes << pick(n, { "uid", "kind", "inputUid", "audioIndex", "outputUid" });
        }
        QSet<QString> linkIds, targets;
        for (const auto &v : m_graphState.value(QStringLiteral("links")).toList()) {
            const auto l = v.toMap();
            const QString id = l.value(QStringLiteral("uid")).toString(), target = endpointOf(l, true);
            const auto from = shape.value(l.value(QStringLiteral("from")).toString()), to = shape.value(l.value(QStringLiteral("to")).toString());
            const int fromPort = l.value(QStringLiteral("fromPort")).toInt(), toPort = l.value(QStringLiteral("toPort")).toInt();
            if (id.isEmpty() || from.isEmpty() || to.isEmpty() || linkIds.contains(id) || targets.contains(target) ||
                fromPort < 0 || fromPort >= ffgui::GraphCompiler::outputs(from) || toPort < 0 || toPort >= ffgui::GraphCompiler::inputs(to))
                continue;
            linkIds.insert(id);
            targets.insert(target);
            links << l;
        }
        auto outputNode = [&](const QVariant &outputUid) {
            for (const auto &v : nodes)
                if (v.toMap().value(QStringLiteral("kind")).toString() == u"output" && v.toMap().value(QStringLiteral("outputUid")) == outputUid)
                    return v.toMap().value(QStringLiteral("uid")).toString();
            return QString();
        };
        for (const auto &step : m_steps->rows()) {
            const QString id = step.value(QStringLiteral("uid")).toString();
            if (shape.contains(id))
                continue;
            nodes << QVariantMap { { "uid", id }, { "kind", "step" } };
            shape.insert(id, step);
            // A new step goes in front of the selected output; extra inputs of
            // an operation stay open until they are wired.
            const QString sink = outputNode(m_outputs->value(std::max(0, m_currentOutput), QStringLiteral("uid")));
            if (sink.isEmpty())
                continue;
            for (auto &v : links) {
                auto l = v.toMap();
                if (l.value(QStringLiteral("to")).toString() != sink)
                    continue;
                l.insert(QStringLiteral("to"), id);
                l.insert(QStringLiteral("toPort"), 0);
                v = l;
                break;
            }
            links << graphLink(newId(), id, 0, sink, 0);
        }
        for (const auto &out : m_outputs->rows()) {
            if (!outputNode(out.value(QStringLiteral("uid"))).isEmpty())
                continue;
            const QString id = "out_" + out.value(QStringLiteral("uid")).toString();
            QVariantMap feed;
            for (const auto &v : links)
                if (shape.value(v.toMap().value(QStringLiteral("to")).toString()).value(QStringLiteral("kind")).toString() == u"output") {
                    feed = v.toMap();
                    break;
                }
            nodes << QVariantMap { { "uid", id }, { "kind", "output" }, { "outputUid", out.value(QStringLiteral("uid")) } };
            shape.insert(id, nodes.last().toMap());
            if (!feed.isEmpty())
                links << graphLink(newId(), feed.value(QStringLiteral("from")).toString(), feed.value(QStringLiteral("fromPort")).toInt(), id, 0);
        }
        m_graphState.insert(QStringLiteral("nodes"), nodes);
        m_graphState.insert(QStringLiteral("links"), links);
    }
    if (!m_graphState.contains(QStringLiteral("positions")))
        return;
    QSet<QString> ids;
    for (const auto &n : graphDescriptors())
        ids.insert(n.value(QStringLiteral("uid")).toString());
    auto positions = m_graphState.value(QStringLiteral("positions")).toMap();
    for (auto it = positions.begin(); it != positions.end();)
        it = ids.contains(it.key()) ? std::next(it) : positions.erase(it);
    if (positions.isEmpty())
        m_graphState.remove(QStringLiteral("positions"));
    else
        m_graphState.insert(QStringLiteral("positions"), positions);
}

// Where nodes without a stored position go: columns by their longest path from
// a source, with each source just before its first consumer; a node keeps the
// row of whatever feeds its first input, so chains stay straight.
QHash<QString, QPoint> Project::graphLayout(const QList<QVariantMap> &nodes, const QList<QVariantMap> &links) const
{
    QHash<QString, int> depth, order;
    QHash<QString, QString> feeder;
    QHash<QString, QStringList> consumers;
    for (const auto &l : links) {
        const QString from = l.value(QStringLiteral("from")).toString(), to = l.value(QStringLiteral("to")).toString();
        consumers[from] << to;
        if (l.value(QStringLiteral("toPort")).toInt() == 0)
            feeder.insert(to, from);
    }
    for (int pass = 0; pass < nodes.size(); ++pass) {
        bool changed = false;
        for (const auto &l : links) {
            const int d = depth.value(l.value(QStringLiteral("from")).toString()) + 1;
            if (d > depth.value(l.value(QStringLiteral("to")).toString())) {
                depth.insert(l.value(QStringLiteral("to")).toString(), d);
                changed = true;
            }
        }
        if (!changed) break;
    }
    QStringList ids;
    for (const auto &n : nodes) {
        const QString id = n.value(QStringLiteral("uid")).toString();
        order.insert(id, int(ids.size()));
        ids << id;
        if (n.value(QStringLiteral("kind")).toString() == u"source" && consumers.contains(id)) {
            int first = INT_MAX;
            for (const auto &c : consumers.value(id))
                first = std::min(first, depth.value(c));
            depth.insert(id, std::max(0, first - 1));
        }
    }
    std::stable_sort(ids.begin(), ids.end(), [&](const QString &a, const QString &b) { return depth.value(a) < depth.value(b); });
    QHash<QString, QPoint> cells;
    QSet<QPoint> taken;
    for (const auto &id : ids) {
        const int col = depth.value(id);
        int row = cells.contains(feeder.value(id)) ? cells.value(feeder.value(id)).y() : 0;
        if (taken.contains(QPoint(col, row)))
            for (row = 0; taken.contains(QPoint(col, row)); ++row) {}
        taken.insert(QPoint(col, row));
        cells.insert(id, QPoint(col, row));
    }
    return cells;
}

// Descriptors with what the editor shows and the compiler needs: step rows,
// source paths and formats, output names, ports and positions.
QList<QVariantMap> Project::graphDefinition() const
{
    auto nodes = graphDescriptors();
    const auto cells = graphLayout(nodes, graphConnections());
    const auto positions = m_graphState.value(QStringLiteral("positions")).toMap();
    for (auto &node : nodes) {
        const QString id = node.value(QStringLiteral("uid")).toString(), kind = node.value(QStringLiteral("kind")).toString();
        if (kind == u"step") {
            const int row = m_steps->find(QStringLiteral("uid"), id);
            const auto step = m_steps->get(row);
            for (auto it = step.begin(); it != step.end(); ++it)
                node.insert(it.key(), it.value());
            node.insert(QStringLiteral("stepIndex"), row);
            node.insert(QStringLiteral("label"), step.value(QStringLiteral("name")));
            node.insert(QStringLiteral("detail"), step.value(QStringLiteral("filter")));
        } else if (kind == u"source") {
            const int row = indexOf(node.value(QStringLiteral("inputUid")).toString());
            const auto input = m_inputs->get(row);
            const QString name = input.value(QStringLiteral("name"), tr("Missing source")).toString();
            const int audio = node.value(QStringLiteral("audioIndex")).toInt();
            node.insert(QStringLiteral("label"), name);
            node.insert(QStringLiteral("stepIndex"), -1);
            node.insert(QStringLiteral("path"), QString());
            node.insert(QStringLiteral("probing"), input.value(QStringLiteral("probing"), false));
            node.insert(QStringLiteral("detail"), tr("Audio %1").arg(audio + 1));
            const auto *info = probe(row);
            if (row < 0)
                node.insert(QStringLiteral("error"), tr("A graph source's input was removed"));
            else if (graphActive() && !inputFiles(row).isEmpty())
                node.insert(QStringLiteral("error"), tr("%1 is a folder; graph sources are single files").arg(name));
            else if (!info && !input.value(QStringLiteral("probing")).toBool())
                node.insert(QStringLiteral("error"), tr("%1 can't be read").arg(name));
            else if (info) {
                const auto track = std::find_if(info->tracks.begin(), info->tracks.end(), [audio](const AudioTrack &t) { return t.audioIndex == audio; });
                if (track == info->tracks.end())
                    node.insert(QStringLiteral("error"), tr("%1 has no audio track %2").arg(name).arg(audio + 1));
                else {
                    node.insert(QStringLiteral("path"), info->path);
                    node.insert(QStringLiteral("channels"), track->channels);
                    node.insert(QStringLiteral("rate"), track->sampleRate);
                    node.insert(QStringLiteral("duration"), info->duration);
                    const QString format = tr("%1 kHz · %2 ch").arg(track->sampleRate / 1000.0, 0, 'g', 4).arg(track->channels);
                    node.insert(QStringLiteral("detail"), info->tracks.size() > 1 ? tr("Audio %1 · %2").arg(audio + 1).arg(format) : format);
                }
            }
        } else {
            const int row = m_outputs->find(QStringLiteral("uid"), node.value(QStringLiteral("outputUid")));
            const QString codec = m_outputs->value(row, QStringLiteral("codecLabel")).toString();
            node.insert(QStringLiteral("label"), m_outputs->value(row, QStringLiteral("fileName")));
            node.insert(QStringLiteral("detail"), codec.isEmpty() ? tr("Output") : codec);
            node.insert(QStringLiteral("stepIndex"), -1);
        }
        const auto position = positions.value(id).toMap();
        const QPoint cell = cells.value(id);
        node.insert(QStringLiteral("x"), position.value(QStringLiteral("x"), 16 + cell.x() * 180));
        node.insert(QStringLiteral("y"), position.value(QStringLiteral("y"), 12 + cell.y() * 84));
        node.insert(QStringLiteral("inputs"), ffgui::GraphCompiler::inputs(node));
        node.insert(QStringLiteral("outputs"), ffgui::GraphCompiler::outputs(node));
        node.insert(QStringLiteral("inputNames"), ffgui::GraphCompiler::portNames(node, false));
        node.insert(QStringLiteral("outputNames"), ffgui::GraphCompiler::portNames(node, true));
        if (!node.contains(QStringLiteral("bypass")))
            node.insert(QStringLiteral("bypass"), false);
    }
    return nodes;
}

void Project::promoteGraph()
{
    if (graphActive())
        return;
    QVariantList nodes, links;
    for (const auto &node : graphDescriptors())
        nodes << node;
    for (const auto &link : graphConnections())
        links << link;
    m_graphState.insert(QStringLiteral("active"), true);
    m_graphState.insert(QStringLiteral("nodes"), nodes);
    m_graphState.insert(QStringLiteral("links"), links);
}

QVariantMap Project::compileGraph(const QList<QVariantMap> &nodes, const QList<QVariantMap> &links, const QString &onlyOutput) const
{
    const auto &catalog = m_catalog ? m_catalog->catalog() : Catalog::builtIn();
    for (const auto &n : nodes) {
        if (n.value(QStringLiteral("kind")).toString() != u"step" || n.value(QStringLiteral("bypass")).toBool() || !m_catalog)
            continue;
        if (const auto *spec = catalog.step(n.value(QStringLiteral("type")).toString())) {
            const QString problem = m_catalog->stepProblem(*spec);
            if (!problem.isEmpty())
                return { { "error", problem } };
        }
    }
    return ffgui::GraphCompiler::compile(nodes, links, catalog, {}, onlyOutput).variant();
}

void Project::refreshGraph()
{
    const auto nodes = graphDefinition();
    auto links = graphConnections();
    QHash<QString, QString> labels;
    for (const auto &n : nodes)
        labels.insert(n.value(QStringLiteral("uid")).toString(), n.value(QStringLiteral("label")).toString());
    for (auto &l : links)
        l.insert(QStringLiteral("label"), QStringLiteral("%1:%2 → %3:%4")
                                              .arg(labels.value(l.value(QStringLiteral("from")).toString()))
                                              .arg(l.value(QStringLiteral("fromPort")).toInt() + 1)
                                              .arg(labels.value(l.value(QStringLiteral("to")).toString()))
                                              .arg(l.value(QStringLiteral("toPort")).toInt() + 1));
    // Reconcile by uid so node cards survive edits, drags and probes.
    reconcile(m_graphNodes, nodes);
    reconcile(m_graphLinks, links);
    m_canUseRow = !graphActive() || ffgui::GraphCompiler::linear(nodes, links);
    m_graphPlan = graphActive() ? compileGraph(nodes, links, {}) : QVariantMap();
    m_graphError = m_graphPlan.value(QStringLiteral("error")).toString();
    emit graphChanged();
}

QVariantMap Project::graphPlan(const QString &onlyOutput) const
{
    if (!graphActive())
        return {};
    if (onlyOutput.isEmpty())
        return m_graphPlan;
    return compileGraph(graphDefinition(), graphConnections(), onlyOutput);
}

bool Project::graphFull()
{
    if (m_graphNodes->count() < ffgui::GraphCompiler::kMaxNodes)
        return false;
    emit notice(tr("A graph holds at most %1 nodes").arg(ffgui::GraphCompiler::kMaxNodes));
    return true;
}

int Project::renderInput() const
{
    if (!graphActive())
        return m_currentInput;
    int first = -1;
    for (const auto &n : graphDescriptors()) {
        if (n.value(QStringLiteral("kind")).toString() != u"source")
            continue;
        const int input = indexOf(n.value(QStringLiteral("inputUid")).toString());
        if (input == m_currentInput)
            return input;
        if (first < 0)
            first = input;
    }
    return first >= 0 ? first : m_currentInput;
}

QVariantMap Project::renderTrack() const
{
    if (!graphActive())
        return track();
    const int input = renderInput();
    for (const auto &n : graphDescriptors())
        if (n.value(QStringLiteral("kind")).toString() == u"source" && indexOf(n.value(QStringLiteral("inputUid")).toString()) == input)
            for (const auto &t : m_inputs->value(input, QStringLiteral("tracks")).toList())
                if (t.toMap().value(QStringLiteral("audioIndex")) == n.value(QStringLiteral("audioIndex")))
                    return t.toMap();
    return input == m_currentInput ? track() : QVariantMap();
}

QString Project::addGraphSource(int inputIndex, int audioIndex)
{
    if (inputIndex < 0 || inputIndex >= m_inputs->count() || graphFull())
        return {};
    if (!inputFiles(inputIndex).isEmpty()) {
        emit notice(tr("%1 is a folder; graph sources are single files").arg(m_inputs->value(inputIndex, QStringLiteral("name")).toString()));
        return {};
    }
    const auto *p = probe(inputIndex);
    if (audioIndex < 0)
        audioIndex = p && !p->tracks.isEmpty() ? p->tracks[p->defaultTrack()].audioIndex : 0;
    if (p && std::none_of(p->tracks.begin(), p->tracks.end(), [audioIndex](const AudioTrack &t) { return t.audioIndex == audioIndex; })) {
        emit notice(tr("%1 has no audio track %2").arg(m_inputs->value(inputIndex, QStringLiteral("name")).toString()).arg(audioIndex + 1));
        return {};
    }
    ProjectEdit edit(this, tr("Add graph source"));
    promoteGraph();
    const QString id = "src_" + newId();
    auto nodes = m_graphState.value(QStringLiteral("nodes")).toList();
    nodes << QVariantMap { { "uid", id }, { "kind", "source" }, { "inputUid", uid(inputIndex) }, { "audioIndex", audioIndex } };
    m_graphState.insert(QStringLiteral("nodes"), nodes);
    return id;
}

QString Project::addGraphOperation(const QString &type)
{
    if (!ffgui::GraphCompiler::operation(type) || graphFull())
        return {};
    ProjectEdit edit(this, tr("Add graph operation"));
    promoteGraph();
    addStep(type);
    // Callers wire the node next, possibly within the same edit.
    syncGraphState();
    return m_steps->value(m_steps->count() - 1, QStringLiteral("uid")).toString();
}

bool Project::connectGraph(const QString &from, int fromPort, const QString &to, int toPort)
{
    syncGraphState();
    const auto nodes = graphDefinition();
    auto links = graphConnections();
    // A new connection replaces whatever fed that input port.
    links.removeIf([&](const QVariantMap &l) { return l.value(QStringLiteral("to")).toString() == to && l.value(QStringLiteral("toPort")).toInt() == toPort; });
    links << graphLink(newId(), from, fromPort, to, toPort);
    const QString error = from == to ? tr("A node can't feed itself") : ffgui::GraphCompiler::validate(nodes, links);
    if (!error.isEmpty()) {
        emit notice(error);
        return false;
    }
    ProjectEdit edit(this, tr("Connect graph ports"));
    promoteGraph();
    QVariantList saved;
    for (const auto &l : links)
        saved << l;
    m_graphState.insert(QStringLiteral("links"), saved);
    return true;
}

void Project::disconnectGraph(const QString &linkUid)
{
    const auto existing = graphConnections();
    if (std::none_of(existing.begin(), existing.end(), [&](const QVariantMap &l) { return l.value(QStringLiteral("uid")) == linkUid; }))
        return;
    ProjectEdit edit(this, tr("Disconnect graph ports"));
    promoteGraph();
    auto links = m_graphState.value(QStringLiteral("links")).toList();
    links.removeIf([&](const QVariant &v) { return v.toMap().value(QStringLiteral("uid")).toString() == linkUid; });
    m_graphState.insert(QStringLiteral("links"), links);
}

void Project::moveGraphNode(const QString &id, double x, double y)
{
    const auto nodes = graphDescriptors();
    if (!std::isfinite(x) || !std::isfinite(y) || std::none_of(nodes.begin(), nodes.end(), [&](const QVariantMap &n) { return n.value(QStringLiteral("uid")) == id; }))
        return;
    ProjectEdit edit(this, tr("Move graph node"), "position:" + id);
    auto positions = m_graphState.value(QStringLiteral("positions")).toMap();
    positions.insert(id, QVariantMap { { "x", std::clamp(x, 0., 10000.) }, { "y", std::clamp(y, 0., 10000.) } });
    m_graphState.insert(QStringLiteral("positions"), positions);
}

// Removing a node reconnects whatever fed its first input to everything it fed,
// so deleting an effect or a split keeps the signal flowing.
void Project::removeGraphNode(const QString &id)
{
    syncGraphState();
    QVariantMap row;
    for (const auto &n : graphDefinition())
        if (n.value(QStringLiteral("uid")) == id)
            row = n;
    if (row.isEmpty())
        return;
    ProjectEdit edit(this, tr("Remove graph node"));
    promoteGraph();
    const QString kind = row.value(QStringLiteral("kind")).toString();
    if (kind == u"output") {
        removeOutput(m_outputs->find(QStringLiteral("uid"), row.value(QStringLiteral("outputUid"))));
        return;
    }
    auto nodes = m_graphState.value(QStringLiteral("nodes")).toList(), links = m_graphState.value(QStringLiteral("links")).toList();
    QVariantMap feed;
    for (const auto &v : links)
        if (v.toMap().value(QStringLiteral("to")).toString() == id && v.toMap().value(QStringLiteral("toPort")).toInt() == 0)
            feed = v.toMap();
    if (!feed.isEmpty())
        for (auto &v : links) {
            auto l = v.toMap();
            if (l.value(QStringLiteral("from")).toString() != id)
                continue;
            l.insert(QStringLiteral("from"), feed.value(QStringLiteral("from")));
            l.insert(QStringLiteral("fromPort"), feed.value(QStringLiteral("fromPort")));
            v = l;
        }
    nodes.removeIf([&](const QVariant &v) { return v.toMap().value(QStringLiteral("uid")).toString() == id; });
    links.removeIf([&](const QVariant &v) {
        const auto l = v.toMap();
        return l.value(QStringLiteral("from")).toString() == id || l.value(QStringLiteral("to")).toString() == id;
    });
    m_graphState.insert(QStringLiteral("nodes"), nodes);
    m_graphState.insert(QStringLiteral("links"), links);
    if (kind == u"step")
        m_steps->removeAt(m_steps->find(QStringLiteral("uid"), id));
}

QString Project::routeTrack(int inputIndex, int audioIndex, double x, double y, const QString &target)
{
    ProjectEdit edit(this, tr("Route track"));
    const QString source = addGraphSource(inputIndex, audioIndex);
    if (source.isEmpty())
        return {};
    moveGraphNode(source, x, y);
    QVariantMap node;
    for (const auto &n : graphDefinition())
        if (n.value(QStringLiteral("uid")) == target)
            node = n;
    if (node.isEmpty())
        return source;
    QSet<int> connected;
    for (const auto &l : graphConnections())
        if (l.value(QStringLiteral("to")) == target)
            connected.insert(l.value(QStringLiteral("toPort")).toInt());
    const int inputs = node.value(QStringLiteral("inputs")).toInt();
    int port = 0;
    while (port < inputs && connected.contains(port))
        ++port;
    if (port < inputs)
        connectGraph(source, 0, target, port);
    else if (node.value(QStringLiteral("kind")).toString() == u"output")
        connectGraph(source, 0, target, 0);
    return source;
}

bool Project::useRow()
{
    if (!graphActive())
        return true;
    QStringList order;
    const auto nodes = graphDefinition();
    if (!ffgui::GraphCompiler::linear(nodes, graphConnections(), &order))
        return false;
    ProjectEdit edit(this, tr("Use chain row"));
    for (const auto &n : nodes) {
        if (n.value(QStringLiteral("kind")).toString() != u"source")
            continue;
        setCurrentInput(indexOf(n.value(QStringLiteral("inputUid")).toString()));
        const auto tracks = input().value(QStringLiteral("tracks")).toList();
        for (int t = 0; t < tracks.size(); ++t)
            if (tracks[t].toMap().value(QStringLiteral("audioIndex")) == n.value(QStringLiteral("audioIndex")))
                setCurrentTrack(t);
    }
    for (int i = 0; i < order.size(); ++i)
        m_steps->move(m_steps->find(QStringLiteral("uid"), order[i]), i);
    m_graphState.remove(QStringLiteral("active"));
    m_graphState.remove(QStringLiteral("nodes"));
    m_graphState.remove(QStringLiteral("links"));
    return true;
}
