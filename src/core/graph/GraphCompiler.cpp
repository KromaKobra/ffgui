#include "GraphCompiler.h"
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <command/CommandBuilder.h>

namespace ffgui
{
namespace
{
QString endpoint(const QString &id, int port) { return id + u':' + QString::number(port); }
QString number(double n) { return QString::number(n, 'g', 12); }

// Standard layouts from `ffmpeg -layouts` (FFmpeg 9.0), with their channels.
const QHash<QString, QString> &standardLayouts()
{
    static const QHash<QString, QString> layouts{
        {"mono", "FC"},
        {"stereo", "FL+FR"},
        {"2.1", "FL+FR+LFE"},
        {"3.0", "FL+FR+FC"},
        {"3.0(back)", "FL+FR+BC"},
        {"4.0", "FL+FR+FC+BC"},
        {"quad", "FL+FR+BL+BR"},
        {"quad(side)", "FL+FR+SL+SR"},
        {"3.1", "FL+FR+FC+LFE"},
        {"5.0", "FL+FR+FC+BL+BR"},
        {"5.0(side)", "FL+FR+FC+SL+SR"},
        {"4.1", "FL+FR+FC+LFE+BC"},
        {"5.1", "FL+FR+FC+LFE+BL+BR"},
        {"5.1(side)", "FL+FR+FC+LFE+SL+SR"},
        {"6.0", "FL+FR+FC+BC+SL+SR"},
        {"6.0(front)", "FL+FR+FLC+FRC+SL+SR"},
        {"3.1.2", "FL+FR+FC+LFE+TFL+TFR"},
        {"hexagonal", "FL+FR+FC+BL+BR+BC"},
        {"6.1", "FL+FR+FC+LFE+BC+SL+SR"},
        {"6.1(back)", "FL+FR+FC+LFE+BL+BR+BC"},
        {"6.1(front)", "FL+FR+LFE+FLC+FRC+SL+SR"},
        {"7.0", "FL+FR+FC+BL+BR+SL+SR"},
        {"7.0(front)", "FL+FR+FC+FLC+FRC+SL+SR"},
        {"7.1", "FL+FR+FC+LFE+BL+BR+SL+SR"},
        {"7.1(wide)", "FL+FR+FC+LFE+BL+BR+FLC+FRC"},
        {"7.1(wide-side)", "FL+FR+FC+LFE+FLC+FRC+SL+SR"},
        {"5.1.2", "FL+FR+FC+LFE+SL+SR+TFL+TFR"},
        {"5.1.2(back)", "FL+FR+FC+LFE+BL+BR+TFL+TFR"},
        {"octagonal", "FL+FR+FC+BL+BR+BC+SL+SR"},
        {"cube", "FL+FR+BL+BR+TFL+TFR+TBL+TBR"},
        {"5.1.4", "FL+FR+FC+LFE+SL+SR+TFL+TFR+TBL+TBR"},
        {"7.1.2", "FL+FR+FC+LFE+BL+BR+SL+SR+TFL+TFR"},
        {"7.1.4", "FL+FR+FC+LFE+BL+BR+SL+SR+TFL+TFR+TBL+TBR"},
        {"7.2.3", "FL+FR+FC+LFE+BL+BR+SL+SR+TFL+TFR+TBC+LFE2"},
        {"9.1.4", "FL+FR+FC+LFE+BL+BR+FLC+FRC+SL+SR+TFL+TFR+TBL+TBR"},
        {"9.1.6", "FL+FR+FC+LFE+BL+BR+FLC+FRC+SL+SR+TFL+TFR+TBL+TBR+TSL+TSR"},
        {"hexadecagonal", "FL+FR+FC+BL+BR+BC+SL+SR+TFL+TFC+TFR+TBL+TBC+TBR+WL+WR"},
        {"binaural", "BIL+BIR"},
        {"downmix", "DL+DR"},
        {"22.2", "FL+FR+FC+LFE+BL+BR+FLC+FRC+BC+SL+SR+TC+TFL+TFC+TFR+TBL+TBC+TBR+LFE2+TSL+TSR+BFC+BFL+BFR"}};
    return layouts;
}

// A layout's channel names, or none when it is not a layout.
QStringList layoutNames(const QString &layout)
{
    if (const auto it = standardLayouts().constFind(layout); it != standardLayouts().constEnd())
        return it->split(u'+');
    static const QRegularExpression decomposition("^[A-Z][A-Z0-9]*(\\+[A-Z][A-Z0-9]*)*$");
    return decomposition.match(layout).hasMatch() ? layout.split(u'+') : QStringList();
}

// Crossover frequencies, or an error unless they are positive and ascending.
QList<double> frequencies(const QVariantMap &params, QString *error)
{
    QList<double> out;
    for (const auto &part : params.value("split", "500").toString().split(u' ', Qt::SkipEmptyParts)) {
        bool ok = false;
        const double f = part.toDouble(&ok);
        if (!ok || !std::isfinite(f) || f <= 0) {
            *error = QStringLiteral("Crossover frequencies must be positive numbers");
            return {};
        }
        if (!out.isEmpty() && f <= out.last()) {
            *error = QStringLiteral("Crossover frequencies must increase");
            return {};
        }
        out << f;
    }
    if (out.isEmpty() || out.size() >= GraphCompiler::kMaxPorts)
        *error = QStringLiteral("A crossover takes 1 to %1 frequencies").arg(GraphCompiler::kMaxPorts - 1);
    return out;
}

struct Topology {
    QMap<QString, QVariantMap> byId;
    QMap<QString, QString> incoming; // input endpoint → output endpoint
    QMap<QString, QList<QVariantMap>> outgoing;
    QStringList order;
    QString error;
};

Topology analyse(const QList<QVariantMap> &nodes, const QList<QVariantMap> &links)
{
    Topology t;
    if (nodes.size() > GraphCompiler::kMaxNodes || links.size() > GraphCompiler::kMaxLinks) {
        t.error = QStringLiteral("Graphs hold at most %1 nodes and %2 connections")
                      .arg(GraphCompiler::kMaxNodes)
                      .arg(GraphCompiler::kMaxLinks);
        return t;
    }
    static const QRegularExpression id("^[A-Za-z0-9_]+$");
    QMap<QString, int> indegree;
    for (const auto &node : nodes) {
        const auto uid = node.value("uid").toString();
        if (!id.match(uid).hasMatch() || t.byId.contains(uid)) {
            t.error = QStringLiteral("Invalid or duplicate graph node id");
            return t;
        }
        t.byId[uid] = node;
        indegree[uid] = 0;
    }
    for (const auto &link : links) {
        const auto from = link.value("from").toString(), to = link.value("to").toString();
        const int fp = link.value("fromPort").toInt(), tp = link.value("toPort").toInt();
        if (!t.byId.contains(from) || !t.byId.contains(to)) {
            t.error = QStringLiteral("Connection refers to a missing node");
            return t;
        }
        if (fp < 0 || fp >= GraphCompiler::outputs(t.byId[from]) || tp < 0 ||
            tp >= GraphCompiler::inputs(t.byId[to])) {
            t.error = QStringLiteral("Connection refers to a missing port");
            return t;
        }
        const auto target = endpoint(to, tp);
        if (t.incoming.contains(target)) {
            t.error = QStringLiteral("An input port accepts one connection");
            return t;
        }
        t.incoming[target] = endpoint(from, fp);
        t.outgoing[from] << link;
        ++indegree[to];
    }
    QStringList ready;
    for (const auto &node : nodes)
        if (indegree[node.value("uid").toString()] == 0)
            ready << node.value("uid").toString();
    while (!ready.isEmpty()) {
        const auto uid = ready.takeFirst();
        t.order << uid;
        for (const auto &link : t.outgoing[uid])
            if (--indegree[link.value("to").toString()] == 0)
                ready << link.value("to").toString();
    }
    if (t.order.size() != nodes.size())
        t.error = QStringLiteral("Connections must not form a cycle");
    return t;
}

// The filter text of a multi-port operation between its input and output pads.
struct Operation {
    QString text, error;
    double duration = 0;
    int channels = 2;
    bool aligned = true;
};

Operation compileOperation(const QString &id, const QVariantMap &node, const QStringList &ins, const QStringList &outs,
                    const QList<double> &durations, const QList<int> &channels, const StepType *spec,
                    const BuildInfo &caps)
{
    Operation op;
    const auto type = node.value("type").toString();
    const auto params = node.value("params").toMap();
    op.duration = durations.value(0);
    op.channels = channels.value(0, 2);
    QString filter;
    if (type == u"amix") {
        const auto mode = params.value("duration", "longest").toString();
        if (!QStringList{"longest", "shortest", "first"}.contains(mode)) {
            op.error = QStringLiteral("Mix duration is longest, shortest or first");
            return op;
        }
        filter = QStringLiteral("amix=inputs=%1:duration=%2:normalize=%3")
                     .arg(ins.size())
                     .arg(mode)
                     .arg(params.value("normalize", true).toBool() ? 1 : 0);
        if (mode == u"longest")
            op.duration = *std::max_element(durations.begin(), durations.end());
        else if (mode == u"shortest")
            op.duration = *std::min_element(durations.begin(), durations.end());
    } else if (type == u"duck")
        filter = "sidechaincompress=threshold=" + number(params.value("threshold", .125).toDouble()) +
                 ":ratio=" + number(params.value("ratio", 4).toDouble());
    else if (type == u"crossfade") {
        const double fade = params.value("d", .5).toDouble();
        if (!std::isfinite(fade) || fade <= 0) {
            op.error = QStringLiteral("Crossfade duration must be positive");
            return op;
        }
        if (durations[0] > 0 && durations[1] > 0 && fade > std::min(durations[0], durations[1])) {
            op.error = QStringLiteral("Crossfade exceeds the shorter source");
            return op;
        }
        const bool overlap = params.value("overlap", true).toBool();
        filter = "acrossfade=d=" + number(fade) + ":overlap=" + (overlap ? u'1' : u'0');
        op.duration = durations[0] + durations[1] - (overlap ? fade : 0);
        op.aligned = false;
    } else if (type == u"merge") {
        // amerge stops with its shortest input.
        filter = "amerge=inputs=" + QString::number(ins.size());
        op.channels = 0;
        for (const int c : channels)
            op.channels += c;
        op.duration = *std::min_element(durations.begin(), durations.end());
    } else if (type == u"split")
        filter = "asplit=" + QString::number(outs.size());
    else if (type == u"channelsplit") {
        const auto layout = params.value("channel_layout", "stereo").toString();
        const int count = GraphCompiler::layoutChannels(layout);
        if (count < 1 || count > GraphCompiler::kMaxChannels) {
            op.error = QStringLiteral("Choose a standard channel layout");
            return op;
        }
        filter = "channelsplit=channel_layout=" + layout;
        op.channels = 1;
    } else if (type == u"crossover") {
        const auto split = frequencies(params, &op.error);
        if (!op.error.isEmpty())
            return op;
        QStringList text;
        for (const double f : split)
            text << number(f);
        filter = "acrossover=split='" + text.join(u' ') + u'\'';
    } else
        filter = "afir=dry=1:wet=" + number(params.value("wet", 1).toDouble());

    // Other options pass through, except those that change the node's ports
    // or timeline, which the graph derives itself.
    static const QHash<QString, QStringList> written{
        {"amix", {"inputs", "duration", "normalize"}}, {"duck", {"threshold", "ratio"}},
        {"crossfade", {"d", "overlap"}},              {"merge", {"inputs"}},
        {"split", {"outputs"}},                       {"channelsplit", {"channel_layout"}},
        {"crossover", {"split"}},                     {"convolve", {"wet"}}};
    static const QRegularExpression name("^[A-Za-z_][A-Za-z0-9_]*$");
    for (auto it = params.begin(); it != params.end(); ++it) {
        if (written.value(type).contains(it.key()))
            continue;
        if (spec && spec->fixed.contains(it.key())) {
            op.error = QStringLiteral("%1 is set by the graph's connections").arg(it.key());
            return op;
        }
        if (!name.match(it.key()).hasMatch()) {
            op.error = QStringLiteral("Invalid filter option name");
            return op;
        }
        filter += u':' + it.key() + u'=' + StepBuilders::optionValue(it.value());
    }
    const auto filterName = filter.section(u'=', 0, 0);
    if (!caps.version.isEmpty() && !caps.filters.contains(filterName)) {
        op.error = "This FFmpeg build lacks " + filterName;
        return op;
    }
    if (type == u"duck") {
        // sidechaincompress stops as soon as either queue reports EOF, and
        // which one does first varies between runs. Pad both sides and cut
        // the result to the main input's exact length.
        const auto p = "g_" + id + "_";
        op.text = QStringLiteral("[%1]asplit=2[%3main][%3length];[%2]apad[%3side];[%3main]apad[%3padded];"
                                 "[%3padded][%3side]%4[%3ducked];[%3length][%3ducked]"
                                 "amix=inputs=2:duration=first:weights='0 1':normalize=0")
                      .arg(ins[0], ins[1], p, filter);
    } else {
        for (const auto &pad : ins)
            op.text += '[' + pad + ']';
        op.text += filter;
    }
    return op;
}
} // namespace

QVariantMap GraphPlan::variant() const
{
    QVariantMap pads;
    for (auto it = outputs.begin(); it != outputs.end(); ++it)
        pads[it.key()] = it.value();
    return {{"inputs", inputArgs}, {"filter", filter},   {"outputs", pads},          {"duration", duration},
            {"error", error},      {"aligned", aligned}, {"seekable", seekable}};
}

bool GraphCompiler::operation(const QString &type)
{
    static const QStringList types{"amix",         "duck",      "crossfade", "merge",
                                   "split",        "channelsplit", "crossover", "convolve"};
    return types.contains(type);
}

int GraphCompiler::inputs(const QVariantMap &node)
{
    const auto kind = node.value("kind").toString(), type = node.value("type").toString();
    if (kind == u"source")
        return 0;
    if (type == u"amix" || type == u"merge")
        return std::clamp(node.value("params").toMap().value("inputs", 2).toInt(), 2, kMaxPorts);
    return type == u"duck" || type == u"crossfade" || type == u"convolve" ? 2 : 1;
}

int GraphCompiler::outputs(const QVariantMap &node)
{
    const auto kind = node.value("kind").toString(), type = node.value("type").toString();
    if (kind == u"output")
        return 0;
    const auto p = node.value("params").toMap();
    if (type == u"split")
        return std::clamp(p.value("outputs", 2).toInt(), 2, kMaxPorts);
    if (type == u"channelsplit")
        return std::clamp(layoutChannels(p.value("channel_layout", "stereo").toString()), 1, kMaxChannels);
    if (type == u"crossover")
        return std::clamp(int(p.value("split", "500").toString().split(u' ', Qt::SkipEmptyParts).size()) + 1,
                          2, kMaxPorts);
    return 1;
}

int GraphCompiler::layoutChannels(const QString &layout) { return int(layoutNames(layout).size()); }

QStringList GraphCompiler::portNames(const QVariantMap &node, bool output)
{
    const auto type = node.value("type").toString();
    const auto params = node.value("params").toMap();
    const int count = output ? outputs(node) : inputs(node);
    QStringList names;
    if (!output && type == u"duck")
        names = {"main", "sidechain"};
    else if (!output && type == u"crossfade")
        names = {"first", "second"};
    else if (!output && type == u"convolve")
        names = {"signal", "response"};
    else if (output && type == u"channelsplit")
        names = layoutNames(params.value("channel_layout", "stereo").toString());
    else if (output && type == u"crossover") {
        QString error;
        const auto split = frequencies(params, &error);
        for (int i = 0; error.isEmpty() && i <= split.size(); ++i)
            names << (i == 0             ? "< " + number(split[0])
                      : i == split.size() ? "> " + number(split[i - 1])
                                          : number(split[i - 1]) + u'–' + number(split[i]));
    }
    if (count < 2 && names.isEmpty())
        return {};
    while (names.size() < count)
        names << QString::number(names.size() + 1);
    return names.mid(0, count);
}

QString GraphCompiler::validate(const QList<QVariantMap> &nodes, const QList<QVariantMap> &links)
{
    return analyse(nodes, links).error;
}

GraphPlan GraphCompiler::compile(const QList<QVariantMap> &nodes, const QList<QVariantMap> &links,
                                 const Catalog &catalog, const BuildInfo &caps, const QString &onlyOutput)
{
    GraphPlan plan;
    auto fail = [&](const QString &error) {
        plan = {};
        plan.error = error;
        return plan;
    };
    const auto topology = analyse(nodes, links);
    if (!topology.error.isEmpty())
        return fail(topology.error);
    QMap<QString, QString> pads;     // input endpoint → the pad that feeds it
    QMap<QString, double> durations; // output endpoint → seconds
    QMap<QString, int> channels;     // output endpoint → channel count
    QStringList filters;
    int sources = 0, fans = 0;
    for (const auto &id : topology.order) {
        const auto &node = topology.byId[id];
        const auto kind = node.value("kind").toString(), type = node.value("type").toString();
        const auto label = node.value("label", id).toString();
        QStringList ins, outs;
        QList<double> inDurations;
        QList<int> inChannels;
        for (int p = 0; p < inputs(node); ++p) {
            const auto key = endpoint(id, p);
            if (!topology.incoming.contains(key))
                return fail(QStringLiteral("Connect input %1 of %2").arg(p + 1).arg(label));
            ins << pads.value(key);
            inDurations << durations.value(topology.incoming[key]);
            inChannels << channels.value(topology.incoming[key], 2);
        }
        for (int p = 0; p < outputs(node); ++p)
            outs << QStringLiteral("g_%1_%2").arg(id).arg(p);
        double duration = inDurations.value(0);
        int ch = inChannels.value(0, 2);
        QString text;
        if (kind == u"source") {
            if (sources >= kMaxSources)
                return fail(QStringLiteral("Graphs take at most %1 source tracks").arg(kMaxSources));
            if (!node.value("error").toString().isEmpty())
                return fail(node.value("error").toString());
            if (node.value("path").toString().isEmpty() || node.value("probing").toBool())
                return fail(QStringLiteral("Waiting for %1").arg(label));
            plan.inputArgs << "-i" << node.value("path").toString();
            text = QStringLiteral("[%1:a:%2]anull").arg(sources++).arg(node.value("audioIndex").toInt());
            duration = node.value("duration").toDouble();
            ch = node.value("channels", 2).toInt();
        } else if (kind == u"output") {
            const auto output = node.value("outputUid").toString();
            if (output.isEmpty())
                return fail(QStringLiteral("Choose an output for the graph"));
            if (plan.outputs.contains(output))
                return fail(QStringLiteral("An output takes one graph connection"));
            if (!onlyOutput.isEmpty() && output != onlyOutput)
                text = '[' + ins[0] + "]anullsink";
            else {
                const auto pad = "g_out_" + id;
                text = '[' + ins[0] + "]anull[" + pad + ']';
                plan.outputs[output] = pad;
                plan.duration = std::max(plan.duration, duration);
            }
        } else if (node.value("bypass").toBool()) {
            // A bypassed node passes its first input to every output.
            for (int p = 1; p < ins.size(); ++p)
                filters << '[' + ins[p] + "]anullsink";
            text = '[' + ins[0] + ']' + (outs.size() > 1 ? "asplit=" + QString::number(outs.size()) : "anull");
        } else if (operation(type)) {
            const auto op =
                compileOperation(id, node, ins, outs, inDurations, inChannels, catalog.step(type), caps);
            if (!op.error.isEmpty())
                return fail(op.error);
            text = op.text;
            duration = op.duration;
            ch = op.channels;
            plan.aligned &= op.aligned;
            // A convolution reads its impulse response from the start.
            plan.seekable &= op.aligned && type != u"convolve";
        } else {
            const auto compiled = ChainCompiler::compile({node}, catalog, caps, ch);
            if (!compiled.error.isEmpty())
                return fail(compiled.error);
            text = '[' + ins[0] + ']' + (compiled.filter.isEmpty() ? QStringLiteral("anull") : compiled.filter);
            duration = ChainCompiler::duration({node}, catalog, duration);
            const auto *spec = catalog.step(type);
            if (spec && spec->builder == u"mono")
                ch = 1;
            plan.aligned &= compiled.alignment && !compiled.hasTrim;
            plan.seekable &= compiled.alignment && !compiled.hasTrim;
        }
        if (kind == u"output") {
            filters << text;
            continue;
        }
        for (const auto &pad : outs)
            text += '[' + pad + ']';
        filters << text;
        // Unused outputs end in a sink; a port that feeds several inputs is split.
        for (int p = 0; p < outs.size(); ++p) {
            const auto key = endpoint(id, p);
            durations[key] = duration;
            channels[key] = ch;
            QList<QVariantMap> consumers;
            for (const auto &link : topology.outgoing[id])
                if (link.value("fromPort").toInt() == p)
                    consumers << link;
            if (consumers.isEmpty()) {
                filters << '[' + outs[p] + "]anullsink";
                continue;
            }
            if (consumers.size() == 1) {
                const auto &link = consumers.first();
                pads[endpoint(link.value("to").toString(), link.value("toPort").toInt())] = outs[p];
                continue;
            }
            QString fan = '[' + outs[p] + "]asplit=" + QString::number(consumers.size());
            for (const auto &link : consumers) {
                const auto pad = "g_fan_" + QString::number(fans++);
                fan += '[' + pad + ']';
                pads[endpoint(link.value("to").toString(), link.value("toPort").toInt())] = pad;
            }
            filters << fan;
        }
    }
    if (plan.outputs.isEmpty())
        return fail(QStringLiteral("Connect at least one output"));
    plan.filter = filters.join(u';');
    return plan;
}

bool GraphCompiler::linear(const QList<QVariantMap> &nodes, const QList<QVariantMap> &links,
                           QStringList *order)
{
    QMap<QString, QVariantMap> byId;
    QString source;
    int sources = 0, steps = 0;
    for (const auto &n : nodes) {
        const auto uid = n.value("uid").toString(), kind = n.value("kind").toString();
        byId[uid] = n;
        if (kind == u"source") {
            source = uid;
            ++sources;
        } else if (kind != u"output") {
            if (operation(n.value("type").toString()))
                return false;
            ++steps;
        }
    }
    if (sources != 1)
        return false;
    QSet<QString> targets;
    for (const auto &l : links) {
        const auto from = l.value("from").toString(), to = l.value("to").toString();
        if (!byId.contains(from) || !byId.contains(to) || l.value("fromPort").toInt() != 0 ||
            l.value("toPort").toInt() != 0 || targets.contains(to) ||
            byId[from].value("kind").toString() == u"output" || byId[to].value("kind").toString() == u"source")
            return false;
        targets.insert(to);
    }
    // Follow the single path of steps from the source.
    QString current = source, tail = source;
    QSet<QString> visited;
    QStringList path;
    while (!current.isEmpty()) {
        if (visited.contains(current))
            return false;
        visited.insert(current);
        QString next;
        int edges = 0;
        for (const auto &link : links)
            if (link.value("from").toString() == current &&
                byId[link.value("to").toString()].value("kind").toString() != u"output") {
                next = link.value("to").toString();
                ++edges;
            }
        if (edges > 1)
            return false;
        if (!next.isEmpty()) {
            path << next;
            tail = next;
        }
        current = next;
    }
    if (path.size() != steps)
        return false;
    // Every output hangs off the end of that path.
    for (const auto &n : nodes) {
        if (n.value("kind").toString() != u"output")
            continue;
        int count = 0;
        for (const auto &l : links)
            if (l.value("to") == n.value("uid")) {
                if (l.value("from").toString() != tail)
                    return false;
                ++count;
            }
        if (count != 1)
            return false;
    }
    if (order)
        *order = path;
    return true;
}
} // namespace ffgui
