#include "GraphProbe.h"
#include <tools/FfTools.h>
#include <tools/ProcessJob.h>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace ffgui
{
GraphProbe::GraphProbe(LogStore *log, QObject *parent) : QObject(parent), m_log(log) {}
GraphProbe::~GraphProbe() { cancel(); }

void GraphProbe::cancel()
{
    if (!m_job)
        return;
    disconnect(m_job, nullptr, this, nullptr);
    m_job->kill();
    m_job->deleteLater();
    m_job = nullptr;
}
void GraphProbe::run(const QString &input, int audioIndex, const QString &filter, const QVariantMap &output)
{
    cancel();
    QStringList args{
        "-hide_banner", "-v", "level+verbose", "-i", input, "-map", QStringLiteral("0:a:%1").arg(audioIndex)};
    if (!filter.isEmpty())
        args << "-af" << filter;
    if (!output.value(QStringLiteral("codec")).toString().isEmpty())
        args << "-c:a" << output.value(QStringLiteral("codec")).toString();
    if (output.value(QStringLiteral("rate")).toInt() > 0)
        args << "-ar" << output.value(QStringLiteral("rate")).toString();
    if (output.value(QStringLiteral("channels")).toInt() > 0)
        args << "-ac" << output.value(QStringLiteral("channels")).toString();
    args << "-t" << "0.05" << "-f" << "null" << "-" << "-print_graphs" << "-print_graphs_format" << "json";
    auto *job = new ProcessJob(FfTools::paths().ffmpeg, args, this);
    m_job = job;
    job->setBackground(true);
    job->setLog(m_log, QStringLiteral("graph"), QStringLiteral("error"));
    auto text = std::make_shared<QString>();
    connect(job, &ProcessJob::stderrLine, this, [text](const LogLine &line) {
        *text +=
            (line.context.isEmpty() || line.continuation ? QString() : u'[' + line.context + QStringLiteral("] ")) +
            line.text + u'\n';
    });
    connect(job, &ProcessJob::stdoutReady, this, [job] { job->readStdout(); });
    connect(job, &ProcessJob::finished, this, [this, job, text](bool ok) {
        auto graph = parse(*text);
        if (!ok)
            graph.error = job->problems(QStringLiteral("error")).join(u'\n');
        m_job = nullptr;
        job->deleteLater();
        emit finished(graph);
    });
    job->start();
}

NegotiatedGraph GraphProbe::parse(QString text)
{
    NegotiatedGraph result;
    static const QRegularExpression begin(QStringLiteral(R"(\{\s*"graphs"\s*:)"));
    const auto start = begin.match(text);
    if (!start.hasMatch()) {
        result.error = QStringLiteral("FFmpeg returned no negotiated graph");
        return result;
    }
    const int offset = start.capturedStart();
    int depth = 0, end = offset;
    bool quoted = false, escaped = false;
    for (; end < text.size(); ++end) {
        const auto c = text[end];
        if (quoted) {
            if (escaped)
                escaped = false;
            else if (c == u'\\')
                escaped = true;
            else if (c == u'"')
                quoted = false;
        } else {
            if (c == u'"')
                quoted = true;
            else if (c == u'{')
                ++depth;
            else if (c == u'}' && --depth == 0) {
                ++end;
                break;
            }
        }
    }
    QString json = text.mid(offset, end - offset);
    // This FFmpeg build emits the time base using the sample_rate key again.
    // Rename that occurrence before Qt's JSON parser discards the real rate.
    json.replace(QRegularExpression(QStringLiteral(R"re("sample_rate"\s*:\s*"(\d+/\d+)")re")),
                 QStringLiteral("\"time_base\":\"\\1\""));
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (!doc.isObject()) {
        result.error = error.errorString();
        return result;
    }
    QHash<QString, QPair<QVariantMap, QVariantMap>> conversions;
    static const QRegularExpression conv(QStringLiteral(
        R"(\[(auto_aresample_\d+)[^\]]*\].*?chl:(\S+) fmt:(\S+) r:(\d+)Hz ->.*?chl:(\S+) fmt:(\S+) r:(\d+)Hz)"));
    auto matches = conv.globalMatch(text);
    while (matches.hasNext()) {
        const auto m = matches.next();
        conversions.insert(m.captured(1),
                           {{{"layout", m.captured(2)}, {"fmt", m.captured(3)}, {"rate", m.captured(4).toInt()}},
                            {{"layout", m.captured(5)}, {"fmt", m.captured(6)}, {"rate", m.captured(7).toInt()}}});
    }
    for (const auto &g : doc.object().value(u"graphs").toArray())
        for (const auto &f : g.toObject().value(u"filters").toArray()) {
            const auto filter = f.toObject();
            const auto ins = filter.value(u"filter_inputs").toArray(), outs = filter.value(u"filter_outputs").toArray();
            const auto first = !ins.isEmpty()    ? ins.first().toObject()
                               : !outs.isEmpty() ? outs.first().toObject()
                                                 : QJsonObject();
            const QString id = first.value(u"filter_id").toString();
            auto format = [](const QJsonObject &pad) -> QVariantMap {
                return {{"rate", pad.value(u"sample_rate").toString().toInt()},
                        {"layout", pad.value(u"channel_layout").toString()},
                        {"fmt", QString()}};
            };
            QVariantMap in = format(first), out = !outs.isEmpty() ? format(outs.first().toObject()) : in;
            QString kind = QStringLiteral("filter");
            for (auto it = conversions.begin(); it != conversions.end(); ++it)
                if (id.endsWith(it.key())) {
                    in = it->first;
                    out = it->second;
                    kind = QStringLiteral("auto");
                    break;
                }
            result.nodes << QVariantMap{
                {"uid", id},
                {"label", filter.value(u"filter_name").toString()},
                {"kind", kind},
                {"from", in},
                {"to", out},
                {"inputs", ins.size()},
                {"outputs", outs.size()},
                {"source", first.value(u"source_filter_id").toString()},
                {"dest", outs.isEmpty() ? QString() : outs.first().toObject().value(u"dest_filter_id").toString()}};
        }
    // Formats are carried along edges from converters, including the sample
    // formats that print_graphs currently omits.
    for (int pass = 0; pass < result.nodes.size(); ++pass)
        for (auto &node : result.nodes) {
            if (node.value(QStringLiteral("kind")).toString() == u"auto")
                continue;
            for (const auto &source : std::as_const(result.nodes))
                if (source.value(QStringLiteral("uid")) == node.value(QStringLiteral("source"))) {
                    auto in = node.value(QStringLiteral("from")).toMap();
                    in.insert(QStringLiteral("fmt"),
                              source.value(QStringLiteral("to")).toMap().value(QStringLiteral("fmt")));
                    node.insert(QStringLiteral("from"), in);
                    auto out = node.value(QStringLiteral("to")).toMap();
                    out.insert(QStringLiteral("fmt"), in.value(QStringLiteral("fmt")));
                    for (const auto &dest : std::as_const(result.nodes))
                        if (dest.value(QStringLiteral("uid")) == node.value(QStringLiteral("dest")) &&
                            dest.value(QStringLiteral("kind")).toString() == u"auto")
                            out = dest.value(QStringLiteral("from")).toMap();
                    node.insert(QStringLiteral("to"), out);
                }
        }
    QList<QVariantMap> pending = result.nodes;
    result.nodes.clear();
    QSet<QString> ids, emitted;
    for (const auto &n : pending)
        ids.insert(n.value(QStringLiteral("uid")).toString());
    while (!pending.isEmpty()) {
        bool advanced = false;
        for (int i = 0; i < pending.size(); ++i) {
            const QString source = pending[i].value(QStringLiteral("source")).toString();
            if (ids.contains(source) && !emitted.contains(source))
                continue;
            auto node = pending.takeAt(i);
            emitted.insert(node.value(QStringLiteral("uid")).toString());
            result.nodes << node;
            advanced = true;
            break;
        }
        if (!advanced) {
            result.nodes << pending;
            break;
        }
    }
    return result;
}
} // namespace ffgui
