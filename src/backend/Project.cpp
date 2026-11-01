#include "Project.h"
#include "CoverStore.h"

#include <log/LogStore.h>
#include <metadata/CoverArt.h>
#include <probe/Prober.h>
#include <util/Format.h>

#include <QDirIterator>
#include <QFileInfo>
#include <QUuid>

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

Project::Project(QObject *parent)
    : QObject(parent)
    , m_inputs(new InputsModel(this))
    , m_steps(new StepsModel(this))
    , m_graph(new GraphModel(this))
    , m_outputs(new OutputsModel(this))
{
    connect(&m_undo, &QUndoStack::cleanChanged, this, &Project::modifiedChanged);
    connect(m_inputs, &RowModel::revisionChanged, this, &Project::inputChanged);
    connect(m_steps, &RowModel::revisionChanged, this, &Project::chainChanged);
    connect(m_outputs, &RowModel::revisionChanged, this, &Project::outputsChanged);
    connect(this, &Project::inputChanged, this, [this] {
        // No Trim step yet: the whole input is kept.
        if (m_keptStart != 0 || m_keptEnd != duration()) {
            m_keptStart = 0;
            m_keptEnd = duration();
            emit chainChanged();
        }
    });
}

void Project::setServices(LogStore *log, std::shared_ptr<CoverStore> covers)
{
    m_log = log;
    m_covers = std::move(covers);
}

void Project::setName(const QString &name)
{
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
    auto *prober = new Prober(path, m_log, this);
    connect(prober, &Prober::finished, this,
            [this, prober, uid, path, primary](bool ok, const ProbeResult &result, const QString &error, const QStringList &warnings) {
                --m_probesRunning;
                prober->deleteLater();
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
    const QString id = uid(index);
    if (id.isEmpty())
        return;
    m_info.remove(id);
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
    if (index < 0 || index >= m_inputs->count() || !file.isLocalFile())
        return;
    const QFileInfo info(file.toLocalFile());
    const QString id = uid(index);
    m_info[id] = InputInfo();
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
// Chain, outputs and files: Phase 2.

void Project::insertStep(int, const QString &) { emit notice(tr("Editing the chain isn't available yet")); }
void Project::addStep(const QString &, const QVariantMap &) { emit notice(tr("Editing the chain isn't available yet")); }
void Project::removeStep(int) { }
void Project::moveStep(int, int) { }
void Project::setBypass(int, bool) { }
void Project::setParam(int, const QString &, const QVariant &) { }
void Project::setRange(int, double, double) { }
void Project::clearRange(int) { }
void Project::applySelectionAction(const QString &, double, double) { emit notice(tr("Editing the chain isn't available yet")); }
void Project::applyChainPreset(const QString &) { emit notice(tr("Chain presets aren't available yet")); }

void Project::addOutput(const QString &) { emit notice(tr("Adding outputs isn't available yet")); }
void Project::removeOutput(int) { }
void Project::setOutput(int, const QVariantMap &) { }
void Project::applyPreset(int, const QString &) { }

void Project::setTag(const QString &, const QString &) { emit notice(tr("Editing tags isn't available yet")); }
void Project::removeTag(const QString &) { }

void Project::newProject() { }
bool Project::open(const QUrl &) { emit notice(tr("Opening projects isn't available yet")); return false; }
bool Project::save(const QUrl &) { emit notice(tr("Saving projects isn't available yet")); return false; }
