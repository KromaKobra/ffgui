#pragma once
#include <QObject>
#include <QVariantMap>

namespace ffgui
{
class LogStore;
class ProcessJob;
struct NegotiatedGraph {
    QList<QVariantMap> nodes;
    QString error;
};
class GraphProbe : public QObject
{
    Q_OBJECT
public:
    explicit GraphProbe(LogStore *log = nullptr, QObject *parent = nullptr);
    ~GraphProbe() override;
    void run(const QString &input, int audioIndex, const QString &filter, const QVariantMap &output);
    void cancel();
    static NegotiatedGraph parse(QString text);
signals:
    void finished(const ffgui::NegotiatedGraph &graph);

private:
    LogStore *m_log;
    ProcessJob *m_job = nullptr;
};
} // namespace ffgui
