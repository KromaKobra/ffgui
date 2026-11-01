#pragma once
#include <catalog/Catalog.h>
#include <caps/BuildInfo.h>

namespace ffgui
{
struct ChainResult {
    QString filter, error;
    double start = 0, end = 0;
    bool hasTrim = false, alignment = true;
};

class StepBuilders
{
public:
    static QString build(const StepType &type, const QVariantMap &node, int channels, QString *error = nullptr);
};
class ChainCompiler
{
public:
    static ChainResult compile(const QList<QVariantMap> &nodes, const Catalog &catalog, const BuildInfo &build,
                               int channels, bool preview = false, bool copy = false);
    static double duration(const QList<QVariantMap> &nodes, const Catalog &catalog, double sourceDuration);
};

struct CopyAdvice {
    bool copy = false, lossyToLossy = false;
    QString reason, trimLabel;
};
class CopyAdvisor
{
public:
    static CopyAdvice advise(const QVariantMap &track, const QVariantMap &output, const QList<QVariantMap> &nodes,
                             const Catalog &catalog);
};
class SizeEstimator
{
public:
    static double kbps(const CodecSpec &codec, const QVariantMap &output, const QVariantMap &track);
    static qint64 bytes(double kbps, double duration);
};

struct BuiltCommand {
    QStringList args, groups, paths;
    QString error;
    double duration = 0;
};
class CommandBuilder
{
public:
    static BuiltCommand build(const QString &input, int audioIndex, const QVariantMap &track, double duration,
                              const QList<QVariantMap> &nodes, const QList<QVariantMap> &outputs,
                              const Catalog &catalog, const BuildInfo &capabilities, double selectionStart = -1,
                              double selectionEnd = -1);
    static QString outputPath(const QString &input, const QVariantMap &output, const Catalog &catalog);
};
} // namespace ffgui
