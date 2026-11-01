#pragma once
#include <QVariantMap>
#include <tools/ProcessJob.h>
#include <vector>
namespace ffgui
{
struct CompareSpec {
    QString a, b, filterA, filterB;
    int trackA = 0, trackB = 0;
    double offset = 0;
    QVariantMap graphB; // a GraphPlan for one output, compared as B instead of b/filterB
};
class Comparer : public QObject
{
    Q_OBJECT
  public:
    Comparer(LogStore *log, QObject *parent = nullptr);
    ~Comparer() override;
    void start(const CompareSpec &spec);
    void cancel();
    static double findOffset(const std::vector<float> &a, const std::vector<float> &b, int rate,
                             double *confidence = nullptr);
    static QVariantMap parseScores(const QString &text);
  signals:
    void finished(const QVariantMap &result);

  private:
    void decode();
    void hash(int side);
    void read();
    LogStore *m_log;
    ProcessJob *m_job = nullptr;
    CompareSpec m_spec;
    QVariantMap m_result;
    std::vector<float> m_a, m_b;
    QByteArray m_pending;
    QString m_hashA;
};
} // namespace ffgui
