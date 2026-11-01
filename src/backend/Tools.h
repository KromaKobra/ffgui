#pragma once
#include "Models.h"
#include "RowModel.h"
#include <QMutex>
#include <QQmlEngine>
#include <QSettings>
#include <capture/Capture.h>
#include <compare/Comparer.h>
class App;

class ConsoleSession : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by App")
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
  public:
    ConsoleSession(ffgui::LogStore *log, QObject *parent = nullptr);
    ~ConsoleSession() override;
    bool active() const { return m_job != nullptr; }
    QString error() const { return m_error; }
    Q_INVOKABLE void run(const QString &text);
    Q_INVOKABLE void stop();
  signals:
    void changed();
    void finished(bool ok);

  private:
    ffgui::LogStore *m_log;
    ffgui::ProcessJob *m_job = nullptr;
    QString m_error;
    QByteArray m_stdout;
    void read();
};

class TranscriptDocument : public RowModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by App")
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
  public:
    explicit TranscriptDocument(App *app);
    ~TranscriptDocument() override;
    Q_PROPERTY(bool active READ active NOTIFY errorChanged)
    bool active() const { return m_job != nullptr; }
    Q_INVOKABLE void transcribe(const QString &model, const QString &language = QStringLiteral("auto"));
    Q_INVOKABLE void cancel();
    QString error() const { return m_error; }
    Q_INVOKABLE bool importFile(const QUrl &file);
    Q_INVOKABLE bool exportFile(const QUrl &file, const QString &format) const;
    Q_INVOKABLE bool setSegment(int row, double start, double end, const QString &text);
    Q_INVOKABLE void addSegment(double start, double end, const QString &text);
    Q_INVOKABLE void removeSegment(int row) { removeAt(row); }
    void showInput(const QString &uid);
  signals:
    void errorChanged();

  private:
    App *m_app;
    ffgui::ProcessJob *m_job = nullptr;
    QString m_error, m_uid;
    QHash<QString, QList<QVariantMap>> m_documents;
};

class ScopeState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by App")
    Q_PROPERTY(bool ready READ ready NOTIFY updated)
    Q_PROPERTY(QVariantMap values READ values NOTIFY updated)
  public:
    explicit ScopeState(QObject *parent = nullptr) : QObject(parent) {}
    QVariantMap values() const;
    bool ready() const { return !values().isEmpty(); }
    void consume(const float *samples, int frames, int channels, int rate);
    void publish();
    void clear();
  signals:
    void updated();

  private:
    mutable QMutex m_mutex;
    QVariantMap m_values;
    std::vector<float> m_samples;
    int m_channels = 0, m_rate = 48000;
};

class CompareState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by App")
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QVariantMap result READ result NOTIFY changed)
    Q_PROPERTY(double offset READ offset WRITE setOffset NOTIFY changed)
  public:
    explicit CompareState(App *app);
    bool active() const { return m_active; }
    QString error() const { return m_error; }
    QVariantMap result() const { return m_result; }
    double offset() const { return m_offset; }
    void setOffset(double offset);
    Q_INVOKABLE void run(int mode = 0, int a = -1, int b = -1);
    Q_INVOKABLE void findOffset();
    Q_INVOKABLE void cancel();
  signals:
    void changed();

  private:
    App *m_app;
    ffgui::Comparer m_comparer;
    bool m_active = false;
    QString m_error;
    QVariantMap m_result;
    double m_offset = 0;
    int m_mode = 0, m_a = -1, m_b = -1;
};

class CaptureState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by App")
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(double elapsed READ elapsed NOTIFY changed)
    Q_PROPERTY(double level READ level NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString output READ output WRITE setOutput NOTIFY changed)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY changed)
  public:
    explicit CaptureState(App *app);
    bool active() const { return m_active; }
    bool refreshing() const { return m_refreshing; }
    double elapsed() const { return m_elapsed; }
    double level() const { return m_level; }
    QString error() const { return m_error; }
    QString output() const { return m_output; }
    void setOutput(const QString &path);
    Q_INVOKABLE void refreshDevices();
    Q_INVOKABLE void start(const QVariantMap &options);
    Q_INVOKABLE void stop();
  signals:
    void changed();

  private:
    App *m_app;
    ffgui::DeviceList m_devices;
    ffgui::Recorder m_recorder;
    bool m_active = false, m_refreshing = false;
    double m_elapsed = 0, m_level = -100;
    QString m_error, m_output;
};

class StreamState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by App")
  public:
    explicit StreamState(App *app);
    ~StreamState() override;
    Q_INVOKABLE void add(const QString &name, const QString &protocol, const QString &url, int kbps = 128);
    Q_INVOKABLE void remove(int row);
    Q_INVOKABLE void start(int row);
    Q_INVOKABLE void stop(int row);
    Q_INVOKABLE void stopAll();

  private:
    App *m_app;
    QHash<QString, ffgui::StreamOut *> m_jobs;
};

class ToolSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by App")
    Q_PROPERTY(QString ffmpeg READ ffmpeg NOTIFY changed)
    Q_PROPERTY(QString ffprobe READ ffprobe NOTIFY changed)
    Q_PROPERTY(QString ffplay READ ffplay NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
  public:
    explicit ToolSettings(QObject *parent = nullptr) : QObject(parent) {}
    QString ffmpeg() const;
    QString ffprobe() const;
    QString ffplay() const;
    QString error() const { return m_error; }
    Q_INVOKABLE bool save(const QString &ffmpeg, const QString &ffprobe, const QString &ffplay);
  signals:
    void changed();

  private:
    QString m_error;
};
