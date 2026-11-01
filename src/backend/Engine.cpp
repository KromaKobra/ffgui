#include "Engine.h"
#include "App.h"

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
    , m_player(app->logStore())
{
    Project *project = app->project();
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
    m_player.stop();
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
        m_app->meters()->resetAll();
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
    });
    pass->start();
}

void Engine::updateBlockers()
{
    Project *project = m_app->project();
    QString blocker;
    if (!project->hasInput())
        blocker = tr("Add an input first");
    else if (project->input().value(QStringLiteral("probing")).toBool())
        blocker = tr("Reading the input…");
    else if (project->outputs()->count() == 0)
        blocker = tr("Choose a format first");
    m_app->render()->setBlocker(blocker);
    if (!project->hasInput())
        m_app->command()->clear(tr("Add an input to build a command"));
    else if (project->outputs()->count() == 0)
        m_app->command()->clear(tr("Choose a format to build a command"));
}

// ----------------------------------------------------------------------------
// Playback and meters

void Engine::playingChanged()
{
    Transport *t = m_app->transport();
    if (t->playing() && !m_player.playing()) {
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
        if (m_loudness)
            m_loudness->process(m_tap.data(), got);
        if (channels >= 2)
            m_correlation.process(m_tap.data(), got, channels);
        total += got;
    }
    if (total == 0)
        return;

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
    MediaState *media = m_app->media();
    Analysis *analysis = m_app->analysis();
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
