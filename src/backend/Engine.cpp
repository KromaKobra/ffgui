#include "Engine.h"
#include "App.h"
#include "FormatApi.h"
#include <probe/Prober.h>
#include <tools/ShellQuote.h>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QFileInfo>

#include <caps/CapabilityProbe.h>
#include <log/LogStore.h>
#include <probe/ProbeResult.h>

#include <cmath>

using namespace ffgui;

namespace {

QVariant finiteOrNull(double v)
{
    return std::isfinite(v) ? QVariant(v) : QVariant();
}

QVariantMap channelStatsMap(const analysis::ChannelStats &c)
{
    return { { "peak", finiteOrNull(c.peakDb) }, { "rms", finiteOrNull(c.rmsDb) }, { "dc", finiteOrNull(c.dcOffset) },
             { "crest", finiteOrNull(c.crest > 0 ? 20 * std::log10(c.crest) : NAN) }, { "bitDepth", c.bitDepth },
             { "noiseFloor", finiteOrNull(c.noiseFloorDb) } };
}

} // namespace

Engine::Engine(App *app)
    : QObject(app)
    , m_app(app)
    , m_graphProbe(app->logStore())
    , m_renderQueue(app->logStore())
    , m_player(app->logStore())
{
    Project *project = app->project();
    m_chainTimer.setSingleShot(true);
    m_chainTimer.setInterval(300);
    connect(&m_chainTimer, &QTimer::timeout, this, &Engine::updateChain);
    for (auto signal : { &Project::chainChanged, &Project::outputsChanged, &Project::currentOutputChanged })
        connect(project, signal, this, [this] { m_graphProbe.cancel(); updateBlockers(); m_chainTimer.start(); });
    connect(project, &Project::paramEdited, this, &Engine::liveParam);
    connect(project, &Project::selectionActionRequested, this, [this](const QString &action, double start, double end) {
        if (action != u"normalize") return;
        const auto stats = m_app->analysis()->rangeStats();
        const double lufs = stats.value(QStringLiteral("lufs"), NAN).toDouble();
        if (!std::isfinite(lufs) || stats.value(QStringLiteral("start")).toDouble() != start || stats.value(QStringLiteral("end")).toDouble() != end) {
            m_normalizeRange = { start, end };
            measureRange(start, end);
            return;
        }
        m_app->project()->addRangedStep(QStringLiteral("gain"), { { "volume", -16-lufs } }, start, end);
    });
    connect(app->render(), &Render::startRequested, this, &Engine::startRender);
    connect(app->render(), &Render::enqueueRequested, this, &Engine::enqueue);
    connect(app->render(), &Render::stopRequested, &m_renderQueue, &RenderQueue::stopAll);
    connect(app->queue(), &QueueModel::actionRequested, &m_renderQueue, &RenderQueue::action);
    connect(&m_renderQueue, &RenderQueue::changed, this, &Engine::syncQueue);
    connect(&m_renderQueue, &RenderQueue::completed, this, [this](const RenderTask &task) {
        if (task.status == u"done") m_app->toast(tr("Rendered %1").arg(QFileInfo(task.output).fileName()), QStringLiteral("success"), tr("Open"), QStringLiteral("open:") + task.output);
        else if (task.status == u"failed") m_app->toast(task.error, QStringLiteral("error"));
        m_foreground.remove(task.uid);
    });
    connect(app->media()->processed(), &MediaState::decoded, this, [this](const QString &, bool ok) {
        if (!ok) return;
        const auto data = m_app->media()->processed()->data();
        if (!data || !data->finished()) return;
        double peak = 0;
        for (int c = 0; c < m_app->media()->processed()->channels(); ++c) {
            const auto p = data->peakRange(c, 0, m_app->project()->duration());
            peak = std::max(peak, double(std::max(std::abs(p.min), std::abs(p.max))));
        }
        auto *steps = m_app->project()->steps();
        int last = -1;
        for (int i = 0; i < steps->count(); ++i) if (!steps->value(i, QStringLiteral("bypass")).toBool()) last = i;
        if (last >= 0 && peak > 1) steps->update(last, { { "warning", tr("clips +%1 dB").arg(20*std::log10(peak), 0, 'f', 1) } });
    });
    connect(&m_graphProbe, &GraphProbe::finished, this, [this](const NegotiatedGraph &graph) {
        if (!graph.error.isEmpty()) return;
        QList<QVariantMap> rows;
        QVariantMap converter;
        FormatApi format;
        for (auto node : graph.nodes) {
            const auto from = node.value(QStringLiteral("from")).toMap(), to = node.value(QStringLiteral("to")).toMap();
            node.insert(QStringLiteral("detail"), format.signal(from) + QStringLiteral(" → ") + format.signal(to));
            node.insert(QStringLiteral("x"), double(rows.size())*180); node.insert(QStringLiteral("y"), 50);
            node.insert(QStringLiteral("bypass"), false);
            rows << node;
            if (node.value(QStringLiteral("kind")).toString() == u"auto") converter = { { "filter", node.value(QStringLiteral("label")) }, { "from", from }, { "to", to }, { "reason", tr("FFmpeg inserts this conversion") } };
            for (int i = 0; i < m_app->project()->steps()->count(); ++i) {
                const QString uid = m_app->project()->steps()->value(i, QStringLiteral("uid")).toString();
                if (node.value(QStringLiteral("uid")).toString().endsWith(u'_' + uid))
                    m_app->project()->steps()->update(i, { { "formatIn", format.signal(from) }, { "formatOut", format.signal(to) }, { "formatChanged", from != to } });
            }
        }
        m_app->project()->setNegotiated(rows, converter);
    });
    connect(project, &Project::currentInputChanged, this, &Engine::currentChanged);
    connect(project, &Project::currentTrackChanged, this, &Engine::currentChanged);
    connect(project, &Project::inputChanged, this, &Engine::currentChanged);
    connect(project, &Project::inputProbed, this, &Engine::currentChanged);
    connect(project->inputs(), &RowModel::countChanged, this, &Engine::updateBlockers);
    connect(project->outputs(), &RowModel::countChanged, this, &Engine::updateBlockers);
    connect(project, &Project::inputChanged, this, &Engine::updateBlockers);

    connect(app->media(), &MediaState::decoded, this, [this](const QString &key, bool) {
        if (key == m_key)
            applyClipping();
    });
    connect(app->media(), &MediaState::updated, this, &Engine::scheduleIdleMeters);
    connect(app->media()->processed(), &MediaState::updated, this, &Engine::scheduleIdleMeters);

    Analysis *analysis = app->analysis();
    connect(analysis, &Analysis::restartRequested, this, [this] {
        m_results.remove(m_key);
        startAnalysis();
    });
    connect(analysis, &Analysis::rangeRequested, this, &Engine::measureRange);
    connect(analysis, &Analysis::seriesChanged, this, &Engine::scheduleIdleMeters);
    connect(analysis, &Analysis::resultsChanged, this, &Engine::scheduleIdleMeters);
    connect(app->caps(), &Capabilities::refreshRequested, this, [this] { refreshCaps(false); });

    Transport *transport = app->transport();
    connect(transport, &Transport::playingChanged, this, &Engine::playingChanged);
    connect(transport, &Transport::seeked, this, &Engine::seeked);
    connect(transport, &Transport::positionChanged, this, &Engine::scheduleIdleMeters);
    connect(transport, &Transport::speedChanged, this, [this] { m_player.setSpeed(m_app->transport()->speed()); });
    connect(transport, &Transport::volumeChanged, this, [this] { m_player.setVolume(m_app->transport()->volume()); });
    connect(transport, &Transport::soloMaskChanged, this, &Engine::applySolo);
    connect(transport, &Transport::abChanged, this, [this] { m_player.setProcessed(m_app->transport()->ab() == u"processed"); scheduleIdleMeters(); });
    m_player.setVolume(transport->volume());
    connect(&m_player, &PlaybackEngine::playingChanged, this, [this](bool playing) {
        if (!playing)
            m_app->transport()->setPlaying(false);
    });
    connect(&m_player, &PlaybackEngine::ended, this, [this] {
        Transport *t = m_app->transport();
        Selection *sel = m_app->selection();
        if (t->loop()) {
            t->seek(sel->isRange() ? sel->start() : 0);
            t->play();
            return;
        }
        t->setPosition(t->duration());
    });
    connect(&m_player, &PlaybackEngine::error, this, [this](const QString &message) {
        m_app->transport()->setPlaying(false);
        m_app->toast(message, QStringLiteral("error"));
    });

    m_tick.setInterval(33);
    connect(&m_tick, &QTimer::timeout, this, &Engine::tick);
    m_idle.setSingleShot(true);
    m_idle.setInterval(40);
    connect(&m_idle, &QTimer::timeout, this, &Engine::idleMeters);
}

Engine::~Engine()
{
    if (m_scopeJob) {
        disconnect(m_scopeJob, nullptr, this, nullptr);
        delete m_scopeJob;
    }
    m_player.stop();
    m_graphProbe.cancel();
    disconnect(&m_renderQueue, nullptr, this, nullptr);
    m_renderQueue.stopAll();
}

void Engine::start()
{
    updateBlockers();
    refreshCaps(true);
}

// ----------------------------------------------------------------------------
// Capabilities

void Engine::refreshCaps(bool useCache)
{
    Capabilities::Snapshot detecting;
    detecting.state = QStringLiteral("detecting");
    m_app->caps()->apply(detecting);
    auto *probe = new CapabilityProbe(m_app->logStore(), this);
    connect(probe, &CapabilityProbe::finished, this,
            [this, probe](bool ok, const ToolPaths &paths, const BuildInfo &info, const QString &) {
                probe->deleteLater();
                Capabilities::Snapshot s;
                s.state = ok ? QStringLiteral("ready") : QStringLiteral("missing");
                s.version = info.version;
                s.ffmpegPath = paths.ffmpeg;
                s.ffprobePath = paths.ffprobe;
                s.ffplayPath = paths.ffplay;
                s.configuration = info.configuration;
                s.filters = info.filters.keys();
                s.encoders = info.audioEncoders.keys();
                s.muxers = info.muxers.keys();
                for (const LibraryInfo &lib : info.libraries)
                    s.libraries << QVariantMap { { "name", lib.name }, { "found", lib.found }, { "unlocks", lib.unlocks },
                                                 { "kind", QStringLiteral("library") } };
                m_app->caps()->apply(s);
                m_build = info;
                m_app->catalog()->setCapabilities(info);
                updateBlockers();
                m_chainTimer.start();
            });
    probe->run(useCache);
}

// ----------------------------------------------------------------------------
// The input on screen

void Engine::currentChanged()
{
    Project *project = m_app->project();
    const int index = project->currentInput();
    const ProbeResult *probe = project->probe(index);
    const int trackIndex = project->shownTrack();
    const QString uid = project->uid(index);

    if (!probe || trackIndex < 0) {
        if (m_key.isEmpty() && uid == m_uid)
            return;
        m_key.clear();
        m_uid = uid;
        m_app->transport()->setPlaying(false);
        m_player.stop();
        m_player.setSource({});
        if (m_pass)
            m_pass->cancel();
        m_app->analysis()->reset();
        m_app->media()->clear();
        m_app->media()->processed()->clear();
        m_app->project()->setNegotiated({}, {});
        m_processedFilter.clear();
        m_app->transport()->setAbAvailability(false, tr("Add an input to compare"));
        m_app->meters()->resetAll();
        m_app->scopes()->clear();
        m_scopeKey.clear();
        m_scopeFrame = -1;
        if (m_scopeJob) {
            disconnect(m_scopeJob, nullptr, this, nullptr);
            delete m_scopeJob;
            m_scopeJob = nullptr;
        }
        // A row that is still being read keeps its length for the ruler.
        m_app->transport()->setDuration(project->duration());
        m_app->transport()->setMarkers({});
        return;
    }

    const AudioTrack &track = probe->tracks[trackIndex];
    const QString key = uid + u':' + QString::number(track.audioIndex) + u':' + probe->path;
    if (key == m_key)
        return;
    m_key = key;
    m_uid = uid;
    m_normalizeRange = { -1, -1 };
    const bool live = probe->duration <= 0;

    Transport *transport = m_app->transport();
    transport->setPlaying(false);
    m_player.stop();
    transport->setDuration(probe->duration);
    transport->seek(0);
    QList<double> markers;
    for (const Chapter &c : probe->chapters)
        markers << c.start;
    transport->setMarkers(markers);

    const QStringList meterNames = track.channels == 1 ? QStringList { QStringLiteral("M") }
                                 : track.channels == 2 ? QStringList { QStringLiteral("L"), QStringLiteral("R") }
                                                       : track.channelNames;
    m_app->meters()->setChannelNames(meterNames);
    m_app->meters()->resetAll();
    m_player.setSource({ probe->path, track.audioIndex, track.channels, track.sampleRate, track.channelNames, probe->duration });
    applySolo();
    m_processedFilter.clear();
    m_chainTimer.start();

    if (m_pass)
        m_pass->cancel();
    if (m_rangePass)
        m_rangePass->cancel();
    m_app->analysis()->reset();

    if (live) {
        m_app->media()->clear(true);
        m_app->analysis()->setState(QStringLiteral("idle"), project->input().value(QStringLiteral("name")).toString());
        return;
    }
    m_app->media()->show(key, { probe->path, track.audioIndex, track.channels, track.sampleRate, probe->duration });
    if (m_results.contains(key))
        applyAnalysis(m_results.value(key));
    else
        startAnalysis();
    scheduleIdleMeters();
}

void Engine::startAnalysis()
{
    Project *project = m_app->project();
    const ProbeResult *probe = project->probe(project->currentInput());
    const int trackIndex = project->shownTrack();
    if (m_key.isEmpty() || !probe || trackIndex < 0 || probe->duration <= 0)
        return;
    const AudioTrack &track = probe->tracks[trackIndex];
    if (m_pass)
        m_pass->cancel();

    Analysis *analysis = m_app->analysis();
    const QString name = project->input().value(QStringLiteral("name")).toString();
    analysis->setState(QStringLiteral("running"), name);
    analysis->setProgress(0, tr("levels, loudness, silence"));

    AnalysisPass::Options options;
    options.input = probe->path;
    options.audioIndex = track.audioIndex;
    options.channels = track.channels;
    options.sampleRate = track.sampleRate;
    options.duration = probe->duration;
    auto *pass = new AnalysisPass(options, m_app->logStore(), this);
    m_pass = pass;
    const QString key = m_key;
    connect(pass, &AnalysisPass::progress, this, [this, pass](double f) {
        if (pass == m_pass)
            m_app->analysis()->setProgress(f, tr("levels, loudness, silence"));
    });
    connect(pass, &AnalysisPass::finished, this, [this, pass, key](const AnalysisResult &r) {
        pass->deleteLater();
        if (r.error == u"cancelled")
            return;
        if (r.ok)
            m_results.insert(key, r);
        if (pass == m_pass && key == m_key)
            applyAnalysis(r);
    });
    pass->start();
}

void Engine::applyAnalysis(const AnalysisResult &r)
{
    Analysis *a = m_app->analysis();
    Project *project = m_app->project();
    const QString name = project->input().value(QStringLiteral("name")).toString();
    if (!r.ok) {
        a->setState(QStringLiteral("failed"), name, r.error);
        project->setHealth(m_uid, QStringLiteral("error"), tr("Decoding fails: %1").arg(r.error));
        return;
    }

    QVariantList dc, perChannel;
    for (const analysis::ChannelStats &c : r.levels.channels) {
        dc << (std::isfinite(c.dcOffset) ? c.dcOffset : 0.0);
        perChannel << channelStatsMap(c);
    }
    const analysis::ChannelStats &o = r.levels.overall;
    a->setPart(QStringLiteral("levels"),
               QVariantMap { { "peak", finiteOrNull(o.peakDb) }, { "rms", finiteOrNull(o.rmsDb) },
                             { "crest", finiteOrNull(o.peakDb - o.rmsDb) }, { "dc", dc }, { "channels", perChannel },
                             { "bitDepth", o.bitDepth }, { "noiseFloor", finiteOrNull(o.noiseFloorDb) } });
    a->setPart(QStringLiteral("loudness"),
               QVariantMap { { "integrated", finiteOrNull(r.loudness.integrated) }, { "lra", finiteOrNull(r.loudness.lra) },
                             { "truePeak", finiteOrNull(r.loudness.truePeak) }, { "threshold", finiteOrNull(r.loudness.threshold) },
                             { "lraLow", finiteOrNull(r.loudness.lraLow) }, { "lraHigh", finiteOrNull(r.loudness.lraHigh) },
                             { "maxShortTerm", finiteOrNull(r.maxShortTerm) }, { "maxMomentary", finiteOrNull(r.maxMomentary) } });
    QVariantList silence;
    for (const analysis::Interval &s : r.silence)
        silence << QVariantMap { { "start", s.start }, { "end", s.end } };
    a->setPart(QStringLiteral("silence"), silence);
    if (!r.phase.isEmpty())
        a->setPart(QStringLiteral("phase"), QVariantMap { { "mean", finiteOrNull(r.phaseMean) } });
    a->setPart(QStringLiteral("spectrum"), QVariantMap { { "centroid", finiteOrNull(r.centroid) }, { "rolloff", finiteOrNull(r.rolloff) },
                                                         { "flatness", finiteOrNull(r.flatness) } });
    a->setPart(QStringLiteral("decode"), QVariantMap { { "errors", r.decodeErrors }, { "warnings", r.decodeWarnings },
                                                       { "messages", r.decodeMessages } });

    auto series = [&](const QVector<float> &values) {
        auto s = std::make_shared<Analysis::Series>();
        s->times = r.times.mid(0, values.size());
        s->values = values;
        return std::shared_ptr<const Analysis::Series>(s);
    };
    a->setSeries(QStringLiteral("momentary"), series(r.momentary));
    a->setSeries(QStringLiteral("shortTerm"), series(r.shortTerm));
    if (!r.phase.isEmpty())
        a->setSeries(QStringLiteral("phase"), series(r.phase));
    a->setProgress(1, {});
    a->setState(QStringLiteral("done"), name);

    // The decode check sets the input's health; a probe warning stays.
    const QString kind = project->input().value(QStringLiteral("kind")).toString();
    if (r.decodeErrors > 0)
        project->setHealth(m_uid, QStringLiteral("error"),
                           (r.decodeErrors == 1 ? tr("1 decode error: ") : tr("%1 decode errors: ").arg(r.decodeErrors)) + r.decodeMessages.value(0));
    else if (r.decodeWarnings > 0)
        project->setHealth(m_uid, QStringLiteral("warn"),
                           (r.decodeWarnings == 1 ? tr("1 warning: ") : tr("%1 warnings: ").arg(r.decodeWarnings)) + r.decodeMessages.mid(0, 2).join(QStringLiteral("; ")));
    else if (project->input().value(QStringLiteral("health")).toString() == u"ok" && kind != u"folder")
        project->setHealth(m_uid, QStringLiteral("ok"), tr("Decodes cleanly"));
    applyClipping();
}

void Engine::applyClipping()
{
    const std::shared_ptr<MediaData> data = m_app->media()->data();
    if (!data || !data->finished())
        return;
    QVariantList times;
    for (double t : data->clipTimes())
        times << t;
    m_app->analysis()->setPart(QStringLiteral("clipping"), QVariantMap { { "samples", double(data->clipSamples()) }, { "times", times } });
}

void Engine::measureRange(double start, double end)
{
    Project *project = m_app->project();
    const ProbeResult *probe = project->probe(project->currentInput());
    const int trackIndex = project->shownTrack();
    if (m_rangePass)
        m_rangePass->cancel();
    if (m_key.isEmpty() || !probe || trackIndex < 0 || end - start < 0.01) {
        m_app->analysis()->setRangeStats({});
        return;
    }
    const AudioTrack &track = probe->tracks[trackIndex];
    AnalysisPass::Options options;
    options.input = probe->path;
    options.audioIndex = track.audioIndex;
    options.channels = track.channels;
    options.sampleRate = track.sampleRate;
    options.duration = probe->duration;
    options.start = start;
    options.length = end - start;
    auto *pass = new AnalysisPass(options, m_app->logStore(), this);
    m_rangePass = pass;
    connect(pass, &AnalysisPass::finished, this, [this, pass, start, end](const AnalysisResult &r) {
        pass->deleteLater();
        if (pass != m_rangePass || !r.ok)
            return;
        QVariantList dc;
        for (const analysis::ChannelStats &c : r.levels.channels)
            dc << (std::isfinite(c.dcOffset) ? c.dcOffset : 0.0);
        QVariantMap stats { { "start", start }, { "end", end }, { "peak", finiteOrNull(r.levels.overall.peakDb) },
                            { "rms", finiteOrNull(r.levels.overall.rmsDb) }, { "dc", dc } };
        // Integrated loudness needs at least one 400 ms block.
        if (end - start >= 0.4 && r.loudness.integrated > -70)
            stats.insert(QStringLiteral("lufs"), r.loudness.integrated);
        if (std::isfinite(r.phaseMean))
            stats.insert(QStringLiteral("correlation"), r.phaseMean);
        m_app->analysis()->setRangeStats(stats);
        if (m_normalizeRange == QPair<double,double>(start,end)) {
            m_normalizeRange = { -1, -1 };
            const double lufs = stats.value(QStringLiteral("lufs"), NAN).toDouble();
            if (std::isfinite(lufs)) {
                m_app->project()->addRangedStep(QStringLiteral("gain"), { { "volume", -16-lufs } }, start, end);
            } else m_app->toast(tr("This range is too short or quiet to normalize"));
        }
    });
    pass->start();
}

QVariantMap Engine::renderTrack() const
{
    auto *p = m_app->project();
    const int input = p->renderInput();
    auto track = p->renderTrack();
    const auto *probe = p->probe(input);
    track.insert(QStringLiteral("coverStream"), probe ? probe->coverStream : -1);
    track.insert(QStringLiteral("metadataEdits"), p->outputMetadata(input));
    if (p->graphActive())
        track.insert(QStringLiteral("graphPlan"), p->graphPlan());
    return track;
}

void Engine::updateBlockers()
{
    auto *project = m_app->project();
    // A graph names its own sources; renderInput() supplies the output's name and tags.
    const int input = project->renderInput();
    QString blocker;
    if (!project->hasInput()) blocker = tr("Add an input first");
    else if (project->inputs()->value(input, QStringLiteral("probing")).toBool()) blocker = tr("Reading the input…");
    else if (!project->probe(input) || project->renderTrack().isEmpty()) blocker = tr("Choose a readable audio track");
    else if (!project->outputs()->count()) blocker = tr("Choose a format first");
    else if (m_app->caps()->state() != u"ready") blocker = tr("Waiting for FFmpeg");
    if (blocker.isEmpty()) {
        auto command = CommandBuilder::build(
            project->probe(input)->path,
            project->renderTrack().value(QStringLiteral("audioIndex")).toInt(), renderTrack(), project->duration(),
            project->steps()->rows(), project->outputs()->rows(), m_app->catalog()->catalog(), m_build);
        blocker = command.error;
        if (blocker.isEmpty() && !command.prepare(&blocker))
            m_app->logLine(QStringLiteral("error"), blocker);
        if (blocker.isEmpty()) m_app->command()->set(command.args, command.groups);
        else m_app->command()->clear(blocker);
    } else m_app->command()->clear(blocker);
    m_app->render()->setBlocker(blocker);
}

void Engine::updateChain()
{
    auto *p = m_app->project();
    const auto *probe = p->probe(p->currentInput());
    if (!probe || p->shownTrack() < 0) return;
    const auto &track = probe->tracks[p->shownTrack()];
    for (const auto &node : p->steps()->rows()) {
        const auto spec = m_app->catalog()->step(node.value(QStringLiteral("type")).toString());
        m_app->catalog()->filterOptions(spec.value(QStringLiteral("primaryFilter")).toString());
    }
    auto *t = m_app->transport();
    QList<double> markers;
    for (const auto &c : p->chapters())
        markers << c.toMap().value(QStringLiteral("start")).toDouble();
    t->setMarkers(markers);
    double target = NAN;
    for (const auto &s : p->steps()->rows())
        if (s.value(QStringLiteral("type")).toString() == u"loudnorm" && !s.value(QStringLiteral("bypass")).toBool()) target = s.value(QStringLiteral("params")).toMap().value(QStringLiteral("I"), -16).toDouble();
    m_app->meters()->setTarget(target);
    if (p->graphActive()) {
        updateGraph();
        return;
    }
    t->setDuration(probe->duration);
    const auto chain = ChainCompiler::compile(p->steps()->rows(), m_app->catalog()->catalog(), m_build, track.channels, true);
    t->setAbAvailability(chain.error.isEmpty() && !chain.filter.isEmpty() && chain.alignment,
        !chain.error.isEmpty() ? chain.error : !chain.alignment ? tr("Speed changes prevent aligned A/B") : tr("Add a step to compare"));
    if (!chain.error.isEmpty()) {
        m_player.setProcessedFilter({});
        m_app->media()->processed()->clear();
        m_processedFilter.clear();
    }
    if (chain.error.isEmpty() && chain.filter != m_processedFilter) {
        m_processedFilter = chain.filter;
        m_player.setProcessedFilter(chain.filter, chain.filter == m_liveFilter);
        m_liveFilter.clear();
        if (chain.filter.isEmpty()) m_app->media()->processed()->clear();
        else if (probe->duration > 0) {
            const QString hash = QString::fromLatin1(QCryptographicHash::hash(chain.filter.toUtf8(), QCryptographicHash::Sha256).toHex());
            m_app->media()->processed()->show(m_key + u':' + hash, { probe->path, track.audioIndex, track.channels, track.sampleRate, probe->duration, chain.filter });
        }
    }
    if (!p->outputs()->count()) { p->setNegotiated({}, {}); return; }
    const auto advice = CopyAdvisor::advise(p->track(), p->outputs()->get(p->currentOutput()), p->steps()->rows(), m_app->catalog()->catalog());
    if (advice.copy) {
        p->setNegotiated({ { { "uid", QStringLiteral("copy") }, { "kind", QStringLiteral("copy") }, { "label", tr("Stream copy") }, { "detail", tr("Audio packets pass through unchanged") }, { "x", 0.0 }, { "y", 50.0 }, { "inputs", 1 }, { "outputs", 1 }, { "bypass", false } } }, {});
        return;
    }
    const auto full = ChainCompiler::compile(p->steps()->rows(), m_app->catalog()->catalog(), m_build, track.channels);
    if (full.error.isEmpty()) m_graphProbe.run(probe->path, track.audioIndex, full.filter, p->outputs()->get(p->currentOutput()));
}

// Graph mode: processed playback and the processed view follow the selected
// output's branch, which sets the timeline; the original side stays the
// selected input, so A/B needs that input to feed an aligned graph.
void Engine::updateGraph()
{
    auto *p = m_app->project();
    auto *t = m_app->transport();
    const auto *probe = p->probe(p->currentInput());
    const auto &track = probe->tracks[p->shownTrack()];
    const QString out = p->outputs()->value(std::max(0, p->currentOutput()), QStringLiteral("uid")).toString();
    const auto plan = p->graphPlan(out);
    const QString error = plan.value(QStringLiteral("error")).toString();
    const QString pad = plan.value(QStringLiteral("outputs")).toMap().value(out).toString();
    const double duration = plan.value(QStringLiteral("duration")).toDouble();
    const bool feeds = p->renderInput() == p->currentInput();
    t->setDuration(error.isEmpty() && duration > 0 ? duration : probe->duration);
    t->setAbAvailability(error.isEmpty() && feeds && plan.value(QStringLiteral("aligned")).toBool(),
                         !error.isEmpty() ? error
                         : !feeds         ? tr("The selected input doesn't feed this graph")
                                          : tr("This graph changes the timeline"));
    if (!error.isEmpty() || pad.isEmpty()) {
        m_player.setProcessedFilter({});
        m_app->media()->processed()->clear();
        m_processedFilter.clear();
        return;
    }
    const QString signature = QString::fromLatin1(
        QCryptographicHash::hash(QJsonDocument::fromVariant(plan).toJson(), QCryptographicHash::Sha256).toHex());
    if (signature != m_processedFilter) {
        m_processedFilter = signature;
        PcmReader::Source source{probe->path, track.audioIndex, track.channels, track.sampleRate,
                                 duration > 0 ? duration : probe->duration};
        source.graphInputs = plan.value(QStringLiteral("inputs")).toStringList();
        source.graphFilter = plan.value(QStringLiteral("filter")).toString();
        source.graphOutput = pad;
        source.graphSeekable = plan.value(QStringLiteral("seekable")).toBool();
        m_player.setProcessedGraph(source);
        m_app->media()->processed()->show(m_key + u':' + signature, source);
    }
    if (p->outputs()->count())
        m_graphProbe.runGraph(plan.value(QStringLiteral("inputs")).toStringList(),
                              plan.value(QStringLiteral("filter")).toString(), pad,
                              p->outputs()->get(std::max(0, p->currentOutput())));
    scheduleIdleMeters();
}

void Engine::liveParam(int index, const QString &key, const QVariant &v)
{
    if (m_app->project()->graphActive())
        return;
    const auto row = m_app->project()->steps()->get(index);
    const auto spec = m_app->catalog()->step(row.value(QStringLiteral("type")).toString());
    if (!spec.value(QStringLiteral("builder")).toString().isEmpty()) return;
    const QString filter = spec.value(QStringLiteral("primaryFilter")).toString();
    const auto all = m_app->catalog()->filterOptions(filter);
    bool runtime = false;
    for (const auto &o : all) if (o.toMap().value(QStringLiteral("key")) == key) runtime = o.toMap().value(QStringLiteral("runtime")).toBool();
    if (!runtime) return;
    QString value = v.metaType().id() == QMetaType::Bool ? (v.toBool() ? QStringLiteral("1") : QStringLiteral("0")) : v.toString();
    if (spec.value(QStringLiteral("template")).toString().contains(u'{' + key + QStringLiteral("}dB"))) value += QStringLiteral("dB");
    if (m_player.sendLiveCommand(filter + u'@' + row.value(QStringLiteral("uid")).toString(), key, value))
        m_liveFilter = ChainCompiler::compile(m_app->project()->steps()->rows(), m_app->catalog()->catalog(), m_build, m_player.source().channels, true).filter;
}

void Engine::startRender(const QString &scope)
{
    auto *p = m_app->project();
    const auto *probe = p->probe(p->renderInput());
    if (!probe) return;
    const bool selection = scope == u"selection";
    if (selection && !m_app->selection()->isRange()) { m_app->toast(tr("Select a range first")); return; }
    RenderTask task;
    task.input = probe->path;
    task.command = CommandBuilder::build(probe->path, p->renderTrack().value(QStringLiteral("audioIndex")).toInt(), renderTrack(), p->duration(), p->steps()->rows(), p->outputs()->rows(), m_app->catalog()->catalog(), m_build,
        selection ? m_app->selection()->start() : -1, selection ? m_app->selection()->end() : -1);
    if (!task.command.error.isEmpty()) { m_app->toast(task.command.error, QStringLiteral("error")); return; }
    task.output = task.command.paths.value(0);
    m_foreground.insert(task.uid);
    m_renderQueue.add({ task });
}

void Engine::syncQueue()
{
    QList<QVariantMap> rows;
    double progress = 0, speed = 0, eta = 0;
    int count = 0;
    QString label;
    for (const auto &task : m_renderQueue.tasks()) {
        rows << QVariantMap { { "uid", task.uid }, { "file", QFileInfo(task.input).fileName() }, { "output", QFileInfo(task.output).fileName() }, { "status", task.status == u"failed" ? QStringLiteral("error") : task.status }, { "progress", task.progress }, { "speed", task.speed > 0 ? QStringLiteral("%1×").arg(task.speed, 0, 'f', 1) : QString() }, { "eta", task.eta }, { "error", task.error }, { "command", task.command.groups.join(u' ') } };
        if (m_foreground.contains(task.uid)) {
            ++count; progress += task.progress; speed = task.speed; eta += task.eta;
            label = QFileInfo(task.output).fileName();
        }
    }
    // Keep existing rows/delegates while progress changes.
    auto *model = m_app->queue();
    for (int i = 0; i < rows.size(); ++i) {
        const int existing = model->find(QStringLiteral("uid"), rows[i].value(QStringLiteral("uid")));
        if (existing < 0) model->insertAt(i, rows[i]);
        else { if (existing != i) model->move(existing, i); model->update(i, rows[i]); }
    }
    while (model->count() > rows.size()) model->removeAt(model->count()-1);
    model->setPaused(m_renderQueue.paused());
    m_app->render()->setActive(count > 0, count, label);
    if (count > 0) m_app->render()->setProgress(progress/count, speed, eta);
}

void Engine::enqueue()
{
    auto *p = m_app->project();
    if (p->graphActive()) {
        startRender(QStringLiteral("all"));
        return;
    }
    const auto nodes = p->steps()->rows(), outputs = p->outputs()->rows();
    const auto catalog = m_app->catalog()->catalog();
    QHash<QString, QVariantMap> editsByPath;
    for (int i = 0; i < p->inputs()->count(); ++i)
        editsByPath.insert(p->inputs()->value(i, QStringLiteral("path")).toString(), p->outputMetadata(i));
    // Probe each batch member, rather than applying the first file's format
    // and duration to a heterogeneous folder. Serial probing bounds memory.
    auto files = std::make_shared<QStringList>();
    for (int i = 0; i < p->inputs()->count(); ++i) {
        const auto folder = p->inputFiles(i);
        if (folder.isEmpty()) *files << p->inputs()->value(i, QStringLiteral("path")).toString();
        else *files << folder;
    }
    auto next = std::make_shared<std::function<void()>>();
    const std::weak_ptr<std::function<void()>> weakNext = next;
    *next = [this, files, nodes, outputs, catalog, editsByPath, weakNext] {
        if (files->isEmpty())
            return;
        const QString path = files->takeFirst();
        auto *probe = new Prober(path, m_app->logStore(), this);
        auto keepNext = weakNext.lock();
        connect(
            probe, &Prober::finished, this,
            [this, probe, path, nodes, outputs, catalog, editsByPath,
             keepNext](bool ok, const ProbeResult &result, const QString &error, const QStringList &) {
                probe->deleteLater();
                if (ok && !result.tracks.isEmpty()) {
                    const auto &track = result.tracks[result.defaultTrack()];
                    QVariantMap format{{"codec", track.codec},           {"rate", track.sampleRate},
                                       {"depth", track.bitsPerSample},   {"channels", track.channels},
                                       {"audioIndex", track.audioIndex}, {"coverStream", result.coverStream}};
                    auto edits = editsByPath.value(path);
                    if (edits.isEmpty()) {
                        QVariantList chapters;
                        for (const auto &c : result.chapters)
                            chapters << QVariantMap{{"start", c.start}, {"end", c.end}, {"title", c.title}};
                        edits = {{"tags", result.tags}, {"chapters", chapters}};
                    }
                    format.insert(QStringLiteral("metadataEdits"), edits);
                    m_renderQueue.add(BatchExpander::expand({path}, format, result.duration, nodes, outputs,
                                                            catalog, m_build));
                } else
                    m_app->toast(tr("Skipping %1: %2").arg(path, error), QStringLiteral("error"));
                if (keepNext)
                    (*keepNext)();
            });
        probe->start();
    };
    (*next)();
    m_app->requestPanel(QStringLiteral("queue"));
}

// ----------------------------------------------------------------------------
// Playback and meters

void Engine::playingChanged()
{
    Transport *t = m_app->transport();
    if (t->playing() && !m_player.playing()) {
        if (t->position() < m_app->project()->keptStart() || t->position() >= m_app->project()->keptEnd()) t->seek(m_app->project()->keptStart());
        if (!m_player.play(t->position())) {
            t->setPlaying(false);
            return;
        }
        m_loudness = std::make_unique<LoudnessMeter>(m_player.outputFormat().sampleRate(), m_player.source().channelNames);
        m_correlation.take();
        m_app->meters()->resetAll();
        m_tick.start();
    } else if (!t->playing() && m_player.playing()) {
        m_player.stop();
    }
    if (!t->playing()) {
        m_tick.stop();
        scheduleIdleMeters();
    }
}

void Engine::seeked(double position)
{
    if (m_player.playing())
        m_player.play(position);
}

void Engine::applySolo()
{
    const int mask = m_app->transport()->soloMask();
    const int channels = m_player.source().channels;
    std::vector<float> gains(size_t(std::max(channels, 0)), 1.0f);
    if (mask != 0)
        for (int c = 0; c < channels; ++c)
            gains[size_t(c)] = (mask & (1 << c)) ? 1.0f : 0.0f;
    m_player.setChannelGains(gains);
}

void Engine::tick()
{
    Transport *t = m_app->transport();
    if (!m_player.playing())
        return;
    double pos = m_player.position();
    Selection *sel = m_app->selection();
    if (pos >= m_app->project()->keptEnd() && m_app->project()->keptEnd() > 0) {
        if (t->loop()) t->seek(m_app->project()->keptStart());
        else { t->setPosition(m_app->project()->keptEnd()); t->pause(); }
        return;
    }
    if (t->loop() && sel->isRange() && pos >= sel->end()) {
        t->seek(sel->start());
        return;
    }
    t->setPosition(std::min(pos, t->duration()));

    // Meter the frames the device took since the last tick.
    const int channels = m_player.tapChannels();
    if (channels <= 0)
        return;
    m_tap.resize(size_t(8192) * channels);
    std::vector<double> peak(size_t(channels), 0), sum(size_t(channels), 0);
    qint64 total = 0, got;
    while ((got = m_player.takeTap(m_tap.data(), 8192)) > 0) {
        for (qint64 f = 0; f < got; ++f)
            for (int c = 0; c < channels; ++c) {
                const double v = m_tap[size_t(f * channels + c)];
                peak[size_t(c)] = std::max(peak[size_t(c)], std::abs(v));
                sum[size_t(c)] += v * v;
            }
        m_app->scopes()->consume(m_tap.data(), int(got), channels, m_player.outputFormat().sampleRate());
        if (m_loudness)
            m_loudness->process(m_tap.data(), got);
        if (channels >= 2)
            m_correlation.process(m_tap.data(), got, channels);
        total += got;
    }
    if (total == 0)
        return;

    m_app->scopes()->publish();
    Meters::Reading r;
    const QStringList names = m_app->meters()->channelNames();
    for (int c = 0; c < int(names.size()) && c < channels; ++c) {
        r.peak << (peak[size_t(c)] > 0 ? 20 * std::log10(peak[size_t(c)]) : -INFINITY);
        const double rms = std::sqrt(sum[size_t(c)] / double(total));
        r.rms << (rms > 0 ? 20 * std::log10(rms) : -INFINITY);
        r.clipped << (peak[size_t(c)] >= 0.999);
    }
    if (m_loudness) {
        r.momentary = m_loudness->momentary();
        r.shortTerm = m_loudness->shortTerm();
        r.truePeak = m_loudness->truePeak();
        const QVariant analysed = m_app->analysis()->results().value(QStringLiteral("loudness")).toMap().value(QStringLiteral("integrated"));
        r.integrated = analysed.isValid() ? analysed.toDouble() : m_loudness->integrated();
    }
    r.correlation = channels >= 2 ? m_correlation.take(m_app->meters()->correlation()) : 1.0;
    r.live = true;
    m_app->meters()->apply(r);
}

void Engine::scheduleIdleMeters()
{
    if (!m_player.playing())
        m_idle.start();
}

void Engine::idleMeters()
{
    if (m_player.playing())
        return;
    // Stopped: the values at the playhead.
    MediaState *media = m_app->transport()->ab() == u"processed" && m_app->media()->processed()->ready() ? m_app->media()->processed() : m_app->media();
    Analysis *analysis = m_app->analysis();
    idleScopes(media);
    const double t = m_app->transport()->position();
    Meters::Reading r;
    const QStringList names = m_app->meters()->channelNames();
    for (int c = 0; c < names.size(); ++c) {
        r.peak << media->levelAt(c, t, 0.05);
        r.rms << media->rmsAt(c, t, 0.05);
        r.clipped << false;
    }
    r.momentary = analysis->seriesAt(QStringLiteral("momentary"), t);
    r.shortTerm = analysis->seriesAt(QStringLiteral("shortTerm"), t);
    const QVariantMap loudness = analysis->results().value(QStringLiteral("loudness")).toMap();
    r.integrated = loudness.value(QStringLiteral("integrated"), -INFINITY).toDouble();
    r.truePeak = loudness.value(QStringLiteral("truePeak"), -INFINITY).toDouble();
    const double phase = analysis->seriesAt(QStringLiteral("phase"), t);
    r.correlation = std::isnan(phase) ? (names.size() == 1 ? 1.0 : 0.0) : phase;
    r.live = false;
    m_app->meters()->apply(r);
}

// Scopes keep their own short PCM window: borrowing the Media View's single
// zoom window would make scrolling and stopped scope refreshes cancel each other.
void Engine::idleScopes(MediaState *media)
{
    if (!media->data() || !media->data()->finished() || media->data()->frames() <= 0)
        return;
    const auto source = media->source();
    const qint64 frame = std::clamp(qint64(m_app->transport()->position() * source.sampleRate), qint64(0),
                                    std::max(qint64(0), media->data()->frames() - 4096));
    if (m_scopeKey == media->key() && m_scopeFrame == frame)
        return;
    m_scopeKey = media->key();
    m_scopeFrame = frame;
    if (m_scopeJob) {
        disconnect(m_scopeJob, nullptr, this, nullptr);
        delete m_scopeJob;
    }
    auto *job = new ProcessJob(
        FfTools::paths().ffmpeg,
        PcmReader::decodeArgs(source, double(frame) / source.sampleRate, 4096.0 / source.sampleRate), this);
    m_scopeJob = job;
    job->setBackground(true);
    job->setLog(m_app->logStore(), QStringLiteral("scope samples"), QStringLiteral("error"));
    job->setKeepStderr(false);
    auto bytes = std::make_shared<QByteArray>();
    connect(job, &ProcessJob::stdoutReady, this, [job, bytes, source] {
        const auto part = job->readStdout();
        if (bytes->size() < 4096 * source.channels * 4)
            bytes->append(part.left(4096 * source.channels * 4 - bytes->size()));
    });
    connect(job, &ProcessJob::finished, this, [this, job, bytes, source](bool ok) {
        const auto part = job->readStdout();
        if (bytes->size() < 4096 * source.channels * 4)
            bytes->append(part.left(4096 * source.channels * 4 - bytes->size()));
        m_scopeJob = nullptr;
        job->deleteLater();
        if (!ok || m_player.playing())
            return;
        m_app->scopes()->clear();
        m_app->scopes()->consume(reinterpret_cast<const float *>(bytes->constData()),
                                 bytes->size() / (4 * source.channels), source.channels, source.sampleRate);
        m_app->scopes()->publish();
    });
    job->start();
}

void Engine::openInFfplay()
{
    Project *project = m_app->project();
    const ProbeResult *probe = project->probe(project->currentInput());
    if (!probe) {
        m_app->toast(tr("Add an input first"));
        return;
    }
    QString error;
    if (!launchFfplay(probe->path, m_app->transport()->position(),
                      project->input().value(QStringLiteral("name")).toString() + QStringLiteral(" — ffplay"),
                      m_app->logStore(), &error))
        m_app->toast(error, QStringLiteral("error"));
}
