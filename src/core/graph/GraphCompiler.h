#pragma once
#include <caps/BuildInfo.h>
#include <catalog/Catalog.h>

namespace ffgui
{
// A compiled graph: FFmpeg input arguments, a labelled-pad -filter_complex and
// the pad that feeds each output.
struct GraphPlan {
    QStringList inputArgs;
    QString filter, error;
    QMap<QString, QString> outputs; // output uid → pad label
    double duration = 0;            // the longest connected output, seconds
    bool aligned = true;            // every output keeps the sources' timeline
    bool seekable = true;           // aligned, and every input plays in step with the output
    QVariantMap variant() const;
};

// Compiles the editable graph. Nodes are maps with `uid` and `kind`: sources
// carry path, audioIndex, channels, duration, probing and error; steps carry
// their row (type, params, bypass, range); outputs carry outputUid. Links
// connect `from`:`fromPort` to `to`:`toPort`; ports are zero-based.
class GraphCompiler
{
public:
    static constexpr int kMaxNodes = 256, kMaxLinks = 1024, kMaxSources = 16, kMaxPorts = 8,
                         kMaxChannels = 24;

    static bool operation(const QString &type);
    static int inputs(const QVariantMap &node);
    static int outputs(const QVariantMap &node);
    // Channels in a standard layout name ("5.1") or a decomposition ("FL+FR"); 0 if unknown.
    static int layoutChannels(const QString &layout);
    // Short labels for a node's ports when they differ ("main", "sidechain", "FL"); empty for one plain port.
    static QStringList portNames(const QVariantMap &node, bool output);
    // Ids, ports, one connection per input and cycles. Empty when the shape is valid.
    static QString validate(const QList<QVariantMap> &nodes, const QList<QVariantMap> &links);
    // `onlyOutput` sinks every other output (preview, streaming, Compare).
    static GraphPlan compile(const QList<QVariantMap> &nodes, const QList<QVariantMap> &links,
                             const Catalog &catalog, const BuildInfo &caps = {},
                             const QString &onlyOutput = {});
    // Whether the graph is one source → steps → outputs path; `order` receives the step uids.
    static bool linear(const QList<QVariantMap> &nodes, const QList<QVariantMap> &links,
                       QStringList *order = nullptr);
};
} // namespace ffgui
