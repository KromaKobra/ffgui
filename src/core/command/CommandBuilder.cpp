#include "CommandBuilder.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <cmath>
#include <metadata/Metadata.h>
#include <tools/FfTools.h>
#include <tools/ShellQuote.h>

namespace ffgui
{
namespace
{
QString value(const QVariant &v)
{
    if (v.metaType().id() == QMetaType::Bool)
        return v.toBool() ? QStringLiteral("1") : QStringLiteral("0");
    bool ok = false;
    const double n = v.toDouble(&ok);
    if (ok && std::isfinite(n))
        return QString::number(n, 'g', 12);
    QString s = v.toString();
    s.replace(u'\\', QStringLiteral("\\\\\\\\"));
    s.replace(u'\'', QStringLiteral("'\\\\\\''"));
    return u'\'' + s + u'\'';
}
QStringList splitFilters(const QString &text)
{
    QStringList out;
    int start = 0;
    bool quoted = false, escaped = false;
    for (int i = 0; i < text.size(); ++i) {
        if (escaped) {
            escaped = false;
            continue;
        }
        if (text[i] == u'\\') {
            escaped = true;
            continue;
        }
        if (text[i] == u'\'')
            quoted = !quoted;
        if (!quoted && text[i] == u',') {
            out << text.mid(start, i - start);
            start = i + 1;
        }
    }
    out << text.mid(start);
    return out;
}
QString named(QString filter, const QString &uid, int index = 0)
{
    const int eq = filter.indexOf(u'=');
    const QString name = eq < 0 ? filter : filter.left(eq);
    return name + u'@' + uid + (index ? u'_' + QString::number(index) : QString()) +
           (eq < 0 ? QString() : filter.mid(eq));
}
QString num(double n) { return QString::number(n, 'g', 12); }
} // namespace

QString StepBuilders::optionValue(const QVariant &v) { return value(v); }

QString StepBuilders::build(const StepType &type, const QVariantMap &node, int channels, QString *error)
{
    QVariantMap params = type.defaults();
    const auto overrides = node.value(QStringLiteral("params")).toMap();
    for (auto it = overrides.begin(); it != overrides.end(); ++it)
        params.insert(it.key(), it.value());
    QString uid = node.value(QStringLiteral("uid")).toString();
    uid.replace(QRegularExpression(QStringLiteral("[^a-zA-Z0-9_]")), QStringLiteral("_"));
    QStringList filters;
    if (type.key == u"gain" && params.value(QStringLiteral("mute")).toBool())
        filters << QStringLiteral("volume=0");
    else if (type.builder == u"trim") {
        const double start = params.value(QStringLiteral("start")).toDouble(),
                     end = params.value(QStringLiteral("end")).toDouble();
        if (params.value(QStringLiteral("cut")).toBool())
            filters << QStringLiteral("aselect='not(between(t,%1,%2))'").arg(num(start), num(end))
                    << QStringLiteral("asetpts=N/SR/TB");
        else {
            filters << QStringLiteral("atrim=start=%1").arg(num(start)) +
                           (end > start ? QStringLiteral(":end=%1").arg(num(end)) : QString());
            filters << QStringLiteral("asetpts=PTS-STARTPTS");
        }
    } else if (type.builder == u"dehum") {
        for (int i = 1; i <= std::clamp(params.value(QStringLiteral("harmonics")).toInt(), 1, 20); ++i)
            filters << QStringLiteral("bandreject=f=%1:width_type=h:w=%2")
                           .arg(num(params.value(QStringLiteral("f")).toDouble() * i),
                                value(params.value(QStringLiteral("w"))));
    } else if (type.builder == u"pitch") {
        filters << QStringLiteral("rubberband=pitch=%1")
                       .arg(num(std::pow(2.0, params.value(QStringLiteral("semitones")).toDouble() / 12.0)));
    } else if (type.builder == u"mono") {
        QStringList terms;
        for (int c = 0; c < std::max(1, channels); ++c)
            terms << num(1.0 / std::max(1, channels)) + QStringLiteral("*c%1").arg(c);
        filters << QStringLiteral("pan=mono|c0=") + terms.join(u'+');
    } else if (type.builder == u"swap") {
        if (channels < 2) {
            if (error)
                *error = QStringLiteral("Swap channels needs at least two channels");
            return {};
        }
        QStringList terms;
        for (int c = 0; c < channels; ++c)
            terms << QStringLiteral("c%1=c%2").arg(c).arg(c < 2 ? 1 - c : c);
        filters << QStringLiteral("pan=%1c|").arg(channels) + terms.join(u'|');
    } else if (type.builder == u"mix") {
        filters << QStringLiteral("pan=") + value(params.value(QStringLiteral("matrix")));
    } else {
        QString text = type.tmpl;
        static const QRegularExpression placeholder(QStringLiteral(R"(\{([a-zA-Z_0-9]+)\})"));
        int offset = 0;
        while (true) {
            const auto match = placeholder.match(text, offset);
            if (!match.hasMatch())
                break;
            const auto v = params.value(match.captured(1));
            if (!v.isValid()) {
                if (error)
                    *error = QStringLiteral("Missing parameter: %1").arg(match.captured(1));
                return {};
            }
            const QString replacement = value(v);
            text.replace(match.capturedStart(), match.capturedLength(), replacement);
            offset = match.capturedStart() + replacement.size();
        }
        filters = splitFilters(text);
    }
    // Uncurated help options belong to the primary filter, including named
    // builders such as pitch and EQ. Recipe-only switches are not AVOptions.
    if (!filters.isEmpty()) {
        for (auto it = overrides.begin(); it != overrides.end(); ++it)
            if (it.key() != u"mute" && it.key() != u"cut" && !type.defaults().contains(it.key()) &&
                !type.tmpl.contains(u'{' + it.key() + u'}'))
                filters[0] += (filters[0].contains(u'=') ? u':' : u'=') + it.key() + u'=' + value(it.value());
    }
    for (int i = 0; i < filters.size(); ++i)
        filters[i] = named(filters[i], uid, i);
    return filters.join(u',');
}

ChainResult ChainCompiler::compile(const QList<QVariantMap> &nodes, const Catalog &catalog, const BuildInfo &build,
                                   int channels, bool preview, bool copy)
{
    ChainResult result;
    QStringList filters;
    for (const auto &node : nodes) {
        if (node.value(QStringLiteral("bypass")).toBool())
            continue;
        const auto *type = catalog.step(node.value(QStringLiteral("type")).toString());
        if (!type) {
            result.error = QStringLiteral("Unknown processing step");
            return result;
        }
        const auto p = node.value(QStringLiteral("params")).toMap();
        if (type->timeline == u"speed")
            result.alignment = false;
        if (type->timeline == u"trim" && !p.value(QStringLiteral("cut")).toBool()) {
            result.hasTrim = true;
            const double start = std::max(0.0, p.value(QStringLiteral("start")).toDouble());
            const double end = p.value(QStringLiteral("end")).toDouble();
            const double oldEnd = result.end;
            const double previousStart = result.start;
            result.start += start;
            if (end > start)
                result.end = oldEnd > 0 ? std::min(oldEnd, previousStart + end) : previousStart + end;
            if (result.end > 0 && result.start >= result.end) {
                result.error = QStringLiteral("The trim range is empty");
                return result;
            }
        }
        if (preview && !type->timeline.isEmpty())
            continue;
        if (copy && type->timeline == u"trim")
            continue;
        if (result.hasTrim && type->timeline == u"trim" && result.start < 0) {
            result.error = QStringLiteral("Trim starts before the input");
            return result;
        }
        if (!build.version.isEmpty())
            for (const QString &f : type->filters)
                if (!build.filters.contains(f)) {
                    result.error = QStringLiteral("Needs %1, which this build lacks").arg(f);
                    return result;
                }
        QString text = StepBuilders::build(*type, node, channels, &result.error);
        if (!result.error.isEmpty())
            return result;
        if (node.value(QStringLiteral("hasRange")).toBool()) {
            bool supported = !type->filters.isEmpty();
            for (const QString &f : type->filters)
                supported &= build.filters.value(f).timeline;
            if (!supported) {
                result.error = QStringLiteral("Range not supported by %1").arg(type->primaryFilter());
                return result;
            }
            const QString enable = QStringLiteral(":enable='between(t,%1,%2)'")
                                       .arg(num(node.value(QStringLiteral("rangeStart")).toDouble()),
                                            num(node.value(QStringLiteral("rangeEnd")).toDouble()));
            // Every filter in a composite step is gated over the same interval.
            auto parts = splitFilters(text);
            for (auto &part : parts)
                part += enable;
            text = parts.join(u',');
        }
        if (!text.isEmpty())
            filters << text;
    }
    result.filter = filters.join(u',');
    return result;
}

double ChainCompiler::duration(const QList<QVariantMap> &nodes, const Catalog &catalog, double length)
{
    for (const auto &node : nodes) {
        if (node.value(QStringLiteral("bypass")).toBool())
            continue;
        const auto *type = catalog.step(node.value(QStringLiteral("type")).toString());
        if (!type)
            continue;
        const auto p = node.value(QStringLiteral("params")).toMap();
        if (type->timeline == u"trim") {
            const double start = std::max(0.0, p.value(QStringLiteral("start")).toDouble());
            const double end = p.value(QStringLiteral("end")).toDouble();
            const double removed =
                std::max(0.0, std::min(length, end > start ? end : length) - std::min(start, length));
            length = p.value(QStringLiteral("cut")).toBool() ? length - removed : removed;
        } else if (type->timeline == u"pad")
            length += p.value(QStringLiteral("pad_dur"), 2).toDouble();
        else if (type->timeline == u"speed")
            length /= std::max(0.01, p.value(QStringLiteral("tempo"), 1.25).toDouble());
    }
    return std::max(0.0, length);
}

CopyAdvice CopyAdvisor::advise(const QVariantMap &track, const QVariantMap &output, const QList<QVariantMap> &nodes,
                               const Catalog &catalog)
{
    CopyAdvice a;
    const auto *codec = catalog.codec(output.value(QStringLiteral("codec")).toString());
    const QString source = track.value(QStringLiteral("codec")).toString();
    const QStringList lossless{"flac", "alac", "wavpack", "ape", "tta"};
    a.lossyToLossy = codec && codec->lossy && !source.startsWith(u"pcm_") && !lossless.contains(source);
    if (!codec || codec->codec != source)
        a.reason = QStringLiteral("Codec changes");
    for (const auto &s : nodes) {
        if (s.value(QStringLiteral("bypass")).toBool())
            continue;
        const auto *t = catalog.step(s.value(QStringLiteral("type")).toString());
        if (!t || t->timeline != u"trim" ||
            s.value(QStringLiteral("params")).toMap().value(QStringLiteral("cut")).toBool())
            a.reason = QStringLiteral("Processing chain changes the samples");
    }
    for (const QString &key : {QStringLiteral("rate"), QStringLiteral("channels"), QStringLiteral("depth")}) {
        const int target = output.value(key).toInt();
        if (target > 0 && target != track.value(key).toInt())
            a.reason = QStringLiteral("Signal format changes");
    }
    if (!output.value(QStringLiteral("encoderOptions")).toMap().isEmpty())
        a.reason = QStringLiteral("Encoder options are set");
    // Selecting an encoder quality is an explicit request to encode.
    if (codec && codec->hasQuality && output.value(QStringLiteral("forceEncode"), true).toBool())
        a.reason = QStringLiteral("Encoder quality is selected");
    a.copy = a.reason.isEmpty();
    if (a.copy)
        a.lossyToLossy = false;
    a.trimLabel = a.copy ? QStringLiteral("Snaps to nearest packet") : QStringLiteral("Sample-exact");
    return a;
}

double SizeEstimator::kbps(const CodecSpec &codec, const QVariantMap &output, const QVariantMap &track)
{
    const int rate = output.value(QStringLiteral("rate")).toInt() > 0
                         ? output.value(QStringLiteral("rate")).toInt()
                         : track.value(QStringLiteral("rate"), 48000).toInt();
    const int ch = output.value(QStringLiteral("channels")).toInt() > 0
                       ? output.value(QStringLiteral("channels")).toInt()
                       : track.value(QStringLiteral("channels"), 2).toInt();
    const int depth = codec.depth > 0 ? codec.depth
                      : output.value(QStringLiteral("depth")).toInt() > 0
                          ? output.value(QStringLiteral("depth")).toInt()
                          : std::max(16, track.value(QStringLiteral("depth")).toInt());
    return codec.estimateKbps(output.value(QStringLiteral("quality")).toDouble(), double(rate) * ch * depth / 1000);
}
qint64 SizeEstimator::bytes(double kbps, double duration) { return qint64(std::max(0.0, kbps * 125 * duration)); }

QString CommandBuilder::outputPath(const QString &input, const QVariantMap &output, const Catalog &catalog)
{
    const auto *container = catalog.container(output.value(QStringLiteral("container")).toString());
    QString pattern = output.value(QStringLiteral("pattern"), QStringLiteral("{name}-processed")).toString();
    const QUrl url(input);
    const bool remote = !url.scheme().isEmpty() && !url.isLocalFile();
    QString base = remote ? QFileInfo(url.path()).completeBaseName() : QFileInfo(input).completeBaseName();
    if (base.isEmpty())
        base = QStringLiteral("audio");
    pattern.replace(QStringLiteral("{name}"), base);
    const QString folder = output.value(QStringLiteral("folder")).toString();
    const QString besideInput =
        remote ? QStandardPaths::writableLocation(QStandardPaths::MusicLocation) : QFileInfo(input).absolutePath();
    return QDir(folder.isEmpty() ? besideInput : folder)
        .absoluteFilePath(pattern + u'.' + (container ? container->extension : QStringLiteral("wav")));
}

bool BuiltCommand::prepare(QString *error) const
{
    for (auto it = sidecars.begin(); it != sidecars.end(); ++it) {
        if (QFileInfo::exists(it.key()))
            continue;
        if (!QDir().mkpath(QFileInfo(it.key()).absolutePath())) {
            if (error)
                *error = QStringLiteral("Couldn't create metadata cache");
            return false;
        }
        QSaveFile file(it.key());
        if (!file.open(QIODevice::WriteOnly) || file.write(it.value()) != it.value().size() ||
            !file.commit()) {
            if (error)
                *error = file.errorString();
            return false;
        }
    }
    return true;
}

BuiltCommand CommandBuilder::build(const QString &input, int audioIndex, const QVariantMap &track, double duration,
                                   const QList<QVariantMap> &nodes, const QList<QVariantMap> &outputs,
                                   const Catalog &catalog, const BuildInfo &caps, double selectionStart,
                                   double selectionEnd)
{
    BuiltCommand result;
    result.duration = duration;
    if (input.isEmpty() || outputs.isEmpty()) {
        result.error = QStringLiteral("Choose an input and output");
        return result;
    }
    auto group = [&](const QStringList &g) {
        result.args << g;
        result.groups << shellJoin(g);
    };
    result.groups << shellQuote(FfTools::paths().ffmpeg.isEmpty() ? QStringLiteral("ffmpeg") : FfTools::paths().ffmpeg);
    group({"-hide_banner", "-loglevel", "level+info", "-n", "-progress", "pipe:1", "-nostats"});
    const auto graph = track.value(QStringLiteral("graphPlan")).toMap();
    const bool graphMode = !graph.isEmpty();
    if (graphMode && !graph.value(QStringLiteral("error")).toString().isEmpty()) {
        result.error = graph.value(QStringLiteral("error")).toString();
        return result;
    }
    if (graphMode)
        group(graph.value(QStringLiteral("inputs")).toStringList());
    else
        group({"-i", input});
    const int metadataSource =
        graphMode ? std::max(0, int(graph.value(QStringLiteral("inputs")).toStringList().indexOf(input)) / 2) : 0;
    const auto edits = track.value(QStringLiteral("metadataEdits")).toMap();
    int metadataInput = -1, coverInput = -1,
        nextInput =
            graphMode ? graph.value(QStringLiteral("inputs")).toStringList().count(QStringLiteral("-i")) : 1;
    bool editing = false;
    for (const auto &out : outputs)
        editing |= out.value(QStringLiteral("metadata")).toString() == u"edit";
    auto sidecar = [&](const QByteArray &bytes, const QString &suffix) {
        const QString path =
            QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/metadata/") +
            QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) + suffix;
        result.sidecars.insert(path, bytes);
        return path;
    };
    if (editing && !edits.isEmpty()) {
        const auto bytes = ffmetadata(edits.value(QStringLiteral("tags")).toMap(),
                                      edits.value(QStringLiteral("chapters")).toList());
        metadataInput = nextInput++;
        group({"-f", "ffmetadata", "-i", sidecar(bytes, QStringLiteral(".ffmeta"))});
        if (edits.value(QStringLiteral("coverAction")).toString() == u"replace") {
            coverInput = nextInput++;
            group({"-i", sidecar(QByteArray::fromBase64(
                                     edits.value(QStringLiteral("coverData")).toString().toLatin1()),
                                 QStringLiteral(".png"))});
        }
    }

    const auto chain = graphMode ? ChainResult()
                                 : ChainCompiler::compile(nodes, catalog, caps,
                                                          track.value(QStringLiteral("channels"), 2).toInt());
    if (!chain.error.isEmpty()) {
        result.error = chain.error;
        return result;
    }
    double start = chain.hasTrim ? chain.start : 0,
           end = chain.hasTrim && chain.end > chain.start ? chain.end : duration;
    if (selectionEnd > selectionStart && selectionStart >= 0) {
        start = std::max(start, selectionStart);
        end = std::min(end, selectionEnd);
    }
    result.duration =
        selectionStart < 0 ? ChainCompiler::duration(nodes, catalog, duration) : std::max(0.0, end - start);
    if (graphMode)
        result.duration = graph.value(QStringLiteral("duration"), duration).toDouble();
    if (duration > 0 && result.duration <= 0) {
        result.error = QStringLiteral("The selected range is empty");
        return result;
    }
    QList<CopyAdvice> advice;
    int encodes = 0;
    for (const auto &out : outputs) {
        advice << CopyAdvisor::advise(track, out, nodes, catalog);
        if (graphMode) {
            advice.last().copy = false;
            advice.last().reason = QStringLiteral("Graph processing requires encoding");
        }
        encodes += !advice.last().copy;
    }
    QString filter = chain.filter;
    if (!graphMode && selectionStart >= 0 && selectionEnd > selectionStart) {
        // Selection coordinates refer to the source timeline; trim precedes
        // processing and the recipe's own trim is intersected with it.
        QList<QVariantMap> selectionNodes;
        for (const auto &n : nodes) {
            const auto *t = catalog.step(n.value(QStringLiteral("type")).toString());
            if (!t || t->timeline != u"trim")
                selectionNodes << n;
        }
        const auto rest =
            ChainCompiler::compile(selectionNodes, catalog, caps, track.value(QStringLiteral("channels"), 2).toInt());
        filter = QStringLiteral("atrim=start=%1:end=%2,asetpts=PTS-STARTPTS").arg(num(start), num(end));
        if (!rest.filter.isEmpty())
            filter += u',' + rest.filter;
    }
    if (!graphMode && encodes > 0 && outputs.size() > 1) {
        QString graph =
            QStringLiteral("[0:a:%1]").arg(audioIndex) + (filter.isEmpty() ? QStringLiteral("anull") : filter);
        if (encodes > 1)
            graph += QStringLiteral(",asplit=%1").arg(encodes);
        for (int i = 0; i < encodes; ++i)
            graph += QStringLiteral("[a%1]").arg(i);
        group({"-filter_complex", graph});
    }
    if (graphMode) {
        QString graphText = graph.value(QStringLiteral("filter")).toString();
        if (selectionStart >= 0 && selectionEnd > selectionStart) {
            const auto pads = graph.value(QStringLiteral("outputs")).toMap();
            for (const auto &out : outputs) {
                const auto p = pads.value(out.value(QStringLiteral("uid")).toString()).toString();
                graphText += QStringLiteral(";[%1]atrim=start=%2:end=%3,asetpts=PTS-STARTPTS[%1_selected]")
                                 .arg(p)
                                 .arg(selectionStart)
                                 .arg(selectionEnd);
            }
            result.duration = selectionEnd - selectionStart;
        }
        group({"-filter_complex", graphText});
    }
    int pad = 0;
    for (int i = 0; i < outputs.size(); ++i) {
        const auto &out = outputs[i];
        const auto *codec = catalog.codec(out.value(QStringLiteral("codec")).toString());
        const auto *container = catalog.container(out.value(QStringLiteral("container")).toString());
        if (!codec || !container || !container->codecs.contains(codec->key)) {
            result.error = QStringLiteral("The container does not support this codec");
            return result;
        }
        const int requestedDepth = out.value(QStringLiteral("depth")).toInt();
        if (codec->depth > 0 && requestedDepth > 0 && codec->depth != requestedDepth) {
            result.error = QStringLiteral("The selected codec uses %1-bit audio; choose another bit depth or codec").arg(codec->depth);
            return result;
        }
        if (!caps.version.isEmpty() &&
            (!caps.audioEncoders.contains(codec->key) || !caps.muxers.contains(container->muxer))) {
            result.error = QStringLiteral("Encoder or container unavailable in this FFmpeg build");
            return result;
        }
        QString path = outputPath(input, out, catalog);
        if (QFileInfo(path).absoluteFilePath() == QFileInfo(input).absoluteFilePath() ||
            result.paths.contains(path) ||
            (graphMode && graph.value(QStringLiteral("inputs")).toStringList().contains(path))) {
            result.error = QStringLiteral("Output paths must be unique and different from the input");
            return result;
        }
        result.paths << path;
        if (graphMode) {
            const auto p = graph.value(QStringLiteral("outputs"))
                               .toMap()
                               .value(out.value(QStringLiteral("uid")).toString())
                               .toString();
            if (p.isEmpty()) {
                result.error = QStringLiteral("Output has no graph connection");
                return result;
            }
            group({"-map",
                   '[' + p +
                       (selectionStart >= 0 && selectionEnd > selectionStart ? QStringLiteral("_selected")
                                                                             : QString()) +
                       ']'});
        } else
            group({"-map", advice[i].copy || outputs.size() == 1 ? QStringLiteral("0:a:%1").arg(audioIndex)
                                                                 : QStringLiteral("[a%1]").arg(pad++)});
        if (advice[i].copy) {
            if (start > 0)
                group({"-ss", num(start)});
            if (end > start && (chain.hasTrim || selectionStart >= 0))
                group({"-t", num(end - start)});
            group({"-c:a", "copy"});
        } else {
            if (!graphMode && outputs.size() == 1 && !filter.isEmpty())
                group({"-af", filter});
            group({"-c:a", codec->key});
            if (codec->hasQuality) {
                QString q = num(out.value(QStringLiteral("quality"), codec->quality.defaultValue).toDouble());
                if (codec->quality.kind == u"bitrate")
                    q += u'k';
                group({codec->quality.flag, q});
            }
            if (out.value(QStringLiteral("rate")).toInt() > 0)
                group({"-ar", out.value(QStringLiteral("rate")).toString()});
            if (out.value(QStringLiteral("channels")).toInt() > 0)
                group({"-ac", out.value(QStringLiteral("channels")).toString()});
            const int depth = out.value(QStringLiteral("depth")).toInt();
            if ((codec->key == u"flac" || codec->key == u"alac") && depth > 0) {
                if (depth > 24) {
                    result.error = QStringLiteral("This codec supports 16-bit or 24-bit output");
                    return result;
                }
                group({"-sample_fmt",
                       codec->key == u"alac" ? (depth > 16 ? "s32p" : "s16p") : (depth > 16 ? "s32" : "s16"),
                       "-bits_per_raw_sample", QString::number(depth)});
            }
        }
        const bool strip = out.value(QStringLiteral("metadata")).toString() == u"strip";
        const bool edit = out.value(QStringLiteral("metadata")).toString() == u"edit";
        const int cover = track.value(QStringLiteral("coverStream"), -1).toInt();
        const QString coverAction = edits.value(QStringLiteral("coverAction")).toString();
        if (!strip && !(edit && coverAction == u"remove") &&
            QStringList{"mp3", "flac", "ipod", "mp4", "mov"}.contains(container->muxer)) {
            if (edit && coverAction == u"replace" && coverInput >= 0)
                group({"-map", QString::number(coverInput) + ":v:0", "-c:v", "copy", "-disposition:v",
                       "attached_pic"});
            else if (cover >= 0)
                group({"-map", QStringLiteral("%1:%2").arg(metadataSource).arg(cover), "-c:v", "copy",
                       "-disposition:v", "attached_pic"});
        }
        const QString meta = strip                        ? QStringLiteral("-1")
                             : edit && metadataInput >= 0 ? QString::number(metadataInput)
                                                          : QString::number(metadataSource);
        group({"-map_metadata", meta, "-map_chapters", meta});
        if (edit) {
            const auto changes =
                edits.value(QStringLiteral("tracks")).toMap().value(QString::number(audioIndex)).toMap();
            if (changes.contains(QStringLiteral("title")))
                group({"-metadata:s:a:0",
                       QStringLiteral("title=") + changes.value(QStringLiteral("title")).toString()});
            if (changes.contains(QStringLiteral("lang")))
                group({"-metadata:s:a:0",
                       QStringLiteral("language=") + changes.value(QStringLiteral("lang")).toString()});
            if (changes.contains(QStringLiteral("isDefault")))
                group({"-disposition:a:0",
                       changes.value(QStringLiteral("isDefault")).toBool() ? "default" : "0"});
        }
        for (const auto &key : {QStringLiteral("encoderOptions"), QStringLiteral("muxerOptions")}) {
            const auto opts = out.value(key).toMap();
            for (auto it = opts.begin(); it != opts.end(); ++it)
                group({u'-' + it.key(), it.value().metaType().id() == QMetaType::Bool
                                            ? (it.value().toBool() ? "1" : "0")
                                            : it.value().toString()});
        }
        group({"-f", container->muxer, path});
    }
    return result;
}
} // namespace ffgui
