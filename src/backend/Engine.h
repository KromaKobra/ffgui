#pragma once

#include <analysis/AnalysisPass.h>
#include <command/GraphProbe.h>
#include <command/CommandBuilder.h>
#include <render/RenderQueue.h>
#include <dsp/LoudnessMeter.h>
#include <playback/PlaybackEngine.h>

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <memory>

class App;
class MediaState;

// Everything that runs ffmpeg for the input on screen, behind App: decoding
// for the Media View, the analysis pass and selection stats, playback with
// live meters, ffplay, and the capability check. It follows the current input
// and keeps results per input so switching back is instant.
class Engine : public QObject
{
    Q_OBJECT

public:
    explicit Engine(App *app);
    ~Engine() override;

    void start();
    void openInFfplay();

private:
    void refreshCaps(bool useCache);
    void currentChanged();
    void startAnalysis();
    void applyAnalysis(const ffgui::AnalysisResult &result);
    void applyClipping();
    void measureRange(double start, double end);
    void updateBlockers();
    QVariantMap renderTrack() const;
    void updateChain();
    void startRender(const QString &scope);
    void syncQueue();
    void enqueue();
    void liveParam(int index, const QString &key, const QVariant &value);
    ffgui::BuildInfo m_build;
    ffgui::GraphProbe m_graphProbe;
    ffgui::RenderQueue m_renderQueue;
    QSet<QString> m_foreground;
    QTimer m_chainTimer;
    QString m_processedFilter;
    QString m_liveFilter;
    QPair<double,double> m_normalizeRange { -1, -1 };

    void playingChanged();
    void seeked(double position);
    void applySolo();
    void tick();
    void scheduleIdleMeters();
    void idleMeters();
    void idleScopes(MediaState *media);
    QPointer<ffgui::ProcessJob> m_scopeJob;
    QString m_scopeKey;
    qint64 m_scopeFrame = -1;

    App *m_app;
    ffgui::PlaybackEngine m_player;
    QString m_key;                      // input uid : audio index : path
    QString m_uid;
    QPointer<ffgui::AnalysisPass> m_pass;
    QPointer<ffgui::AnalysisPass> m_rangePass;
    QHash<QString, ffgui::AnalysisResult> m_results;
    std::unique_ptr<ffgui::LoudnessMeter> m_loudness;
    ffgui::Correlation m_correlation;
    std::vector<float> m_tap;
    QTimer m_tick;
    QTimer m_idle;
};
