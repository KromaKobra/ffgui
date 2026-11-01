#include "Project.h"
#include "CoverStore.h"
#include "CatalogFacade.h"
#include "Selection.h"
#include <project/SnapshotCommand.h>
#include <command/CommandBuilder.h>
#include <QJsonDocument>
#include <QSignalBlocker>
#include <QDir>

#include <log/LogStore.h>
#include <metadata/CoverArt.h>
#include <probe/Prober.h>
#include <util/Format.h>

#include <QDirIterator>
#include <QFileInfo>
#include <QUuid>
#include <cmath>

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
    : QObject(parent)
    , m_inputs(new InputsModel(this))
    , m_steps(new StepsModel(this))
    , m_graph(new GraphModel(this))
    , m_outputs(new OutputsModel(this))
{
    connect(&m_undo, &QUndoStack::cleanChanged, this, &Project::modifiedChanged);
    connect(m_inputs, &RowModel::revisionChanged, this, &Project::inputChanged);


    connect(this, &Project::inputChanged, this, &Project::refreshDerived);
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

void Project::addGenerator(const QString &, const QString &) { emit notice(tr("Generated inputs aren't available yet")); }

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

void Project::setCoverArt(const QUrl &) { emit notice(tr("Cover art isn't available yet")); }
void Project::useImpulseResponse(const QUrl &) { emit notice(tr("Impulse responses aren't available yet")); }

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
    r.markers = m_markers;
    for (const auto &row : m_inputs->rows()) {
        auto input = pick(row, { "uid", "name", "path", "kind" });
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
    ProjectEdit edit(this, tr("Add step"));
    auto ranged = row;
    if (m_pendingRangeEnd > m_pendingRangeStart) {
        ranged.insert(QStringLiteral("hasRange"), true); ranged.insert(QStringLiteral("rangeStart"), m_pendingRangeStart); ranged.insert(QStringLiteral("rangeEnd"), m_pendingRangeEnd);
        m_pendingRangeStart = m_pendingRangeEnd = 0;
    }
    m_steps->insertAt(index, ranged);
}
void Project::addStep(const QString &type, const QVariantMap &params)
{
    const auto row = makeStep(type, params);
    if (row.isEmpty()) { emit notice(tr("Unknown step: %1").arg(type)); return; }
    ProjectEdit edit(this, tr("Add step"));
    m_steps->append(row);
}
void Project::addRangedStep(const QString &type, const QVariantMap &params, double start, double end)
{
    ProjectEdit edit(this, tr("Add ranged step"));
    addStep(type, params);
    setRange(m_steps->count()-1,start,end);
}
void Project::removeStep(int index) { ProjectEdit edit(this, tr("Remove step")); m_steps->removeAt(index); }
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
    ProjectEdit edit(this, tr("Apply %1").arg(name));
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
    } else if (action == u"marker") m_markers << QVariantMap { { "start", start }, { "end", end }, { "title", tr("Marker") } };
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

void Project::setTag(const QString &, const QString &) { emit notice(tr("Editing tags isn't available yet")); }
void Project::removeTag(const QString &) { }

void Project::newProject()
{
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
    const auto &catalog = m_catalog ? m_catalog->catalog() : Catalog::builtIn();
    m_keptStart = 0; m_keptEnd = duration();
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
        if (s->timeline == u"trim" && !row.value(QStringLiteral("bypass")).toBool()) {
            const auto p = row.value(QStringLiteral("params")).toMap();
            if (!p.value(QStringLiteral("cut")).toBool()) { m_keptStart = p.value(QStringLiteral("start")).toDouble(); m_keptEnd = p.value(QStringLiteral("end")).toDouble(); }
        }
    }
    const auto trim = ChainCompiler::compile(m_steps->rows(), catalog, {}, track().value(QStringLiteral("channels"),2).toInt(), true);
    if (trim.hasTrim) { m_keptStart = trim.start; m_keptEnd = trim.end > trim.start ? std::min(duration(),trim.end) : duration(); }
    qint64 size = 0;
    for (int i = 0; i < m_outputs->count(); ++i) {
        const auto out = m_outputs->get(i);
        const auto *codec = catalog.codec(out.value(QStringLiteral("codec")).toString());
        const auto advice = CopyAdvisor::advise(track(), out, m_steps->rows(), catalog);
        const double kbps = codec ? SizeEstimator::kbps(*codec, out, track()) : 0;
        const qint64 bytes = SizeEstimator::bytes(kbps, ChainCompiler::duration(m_steps->rows(), catalog, duration()));
        size += bytes;
        const QString path = CommandBuilder::outputPath(input().value(QStringLiteral("path")).toString(), out, catalog);
        m_outputs->update(i, { { "fileName", QFileInfo(path).fileName() }, { "codecLabel", codec ? codec->label : QString() }, { "kbps", kbps }, { "sizeBytes", double(bytes) }, { "sizeText", format::bytes(bytes) }, { "reencodeReason", advice.reason }, { "lossyToLossy", advice.lossyToLossy }, { "problem", codec ? QString() : tr("Choose a codec") } });
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
    return m_catalog && m_catalog->savePreset(name, chain ? QStringLiteral("chain") : QStringLiteral("output"), m_outputs->get(m_currentOutput), m_steps->rows());
}
