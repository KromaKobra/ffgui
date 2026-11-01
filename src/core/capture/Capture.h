#pragma once
#include <QTimer>
#include <QVariantMap>
#include <tools/ProcessJob.h>
namespace ffgui
{
struct CaptureSpec {
    QString backend = QStringLiteral("pulse"), device = QStringLiteral("default"), output,
            codec = QStringLiteral("pcm_s24le");
    int rate = 48000, channels = 2, depth = 24;
    double seconds = 0, splitSeconds = 0;
    qint64 maxBytes = 0;
    bool monitor = true;
};
class DeviceList : public QObject
{
    Q_OBJECT
  public:
    DeviceList(LogStore *log, QObject *parent = nullptr);
    ~DeviceList() override;
    void refresh();
    static QList<QVariantMap> parse(const QString &text, const QString &backend);
  signals:
    void finished(const QList<QVariantMap> &devices, const QString &error);

  private:
    LogStore *m_log;
    ProcessJob *m_job = nullptr;
    QList<QVariantMap> m_rows;
    void next(int backend);
};
class Recorder : public QObject
{
    Q_OBJECT
  public:
    Recorder(LogStore *log, QObject *parent = nullptr);
    ~Recorder() override;
    void start(const CaptureSpec &spec);
    void stop();
    bool active() const { return m_job != nullptr; }
    static QStringList args(const CaptureSpec &spec);
  signals:
    void reading(double elapsed, double levelDb);
    void finished(bool ok, const QStringList &files, const QString &error);

  private:
    void read();
    LogStore *m_log;
    ProcessJob *m_job = nullptr;
    CaptureSpec m_spec;
    QByteArray m_pending;
    qint64 m_frames = 0;
    QTimer m_limit;
    QStringList m_before;
};
struct StreamSpec {
    QString input, filter, protocol, url;
    int audioIndex = 0, bitrate = 128;
    QVariantMap graph;
};
class StreamOut : public QObject
{
    Q_OBJECT
  public:
    StreamOut(const StreamSpec &spec, LogStore *log, QObject *parent = nullptr);
    ~StreamOut() override;
    void start();
    void stop();
    static QStringList args(const StreamSpec &spec, QString *error = nullptr);
  signals:
    void reading(double bitrate);
    void finished(bool ok, bool stopped, const QString &error);

  private:
    StreamSpec m_spec;
    LogStore *m_log;
    ProcessJob *m_job = nullptr;
    qint64 m_bytes = 0;
    double m_time = 0;
    QString m_segmentPattern;
};
} // namespace ffgui
