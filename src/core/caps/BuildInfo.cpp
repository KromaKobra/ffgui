#include "BuildInfo.h"

#include <QJsonArray>
#include <QRegularExpression>

namespace ffgui {

namespace caps {

void parseVersion(const QString &output, BuildInfo *info)
{
    static const QRegularExpression version(QStringLiteral(R"(version\s+n?([0-9][^\s]*))"));
    if (const auto m = version.match(output); m.hasMatch())
        info->version = m.captured(1);
    for (const QString &line : output.split(u'\n')) {
        if (line.startsWith(QStringLiteral("configuration:"))) {
            info->configuration = line.mid(14).trimmed();
            break;
        }
    }
}

namespace {

// The lines after the "---" separator that ends each list's legend.
QStringList body(const QString &output)
{
    QStringList lines = output.split(u'\n');
    qsizetype start = 0;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (lines[i].trimmed().startsWith(QStringLiteral("---"))) {
            start = i + 1;
            break;
        }
    }
    return lines.mid(start);
}

} // namespace

QHash<QString, FilterInfo> parseFilters(const QString &output)
{
    // " TS afftdn            A->A       Denoise audio samples using FFT."
    static const QRegularExpression re(QStringLiteral(R"(^\s([T.])([S.])\s+(\S+)\s+(\S+->\S+)\s+(.*)$)"));
    QHash<QString, FilterInfo> out;
    for (const QString &line : body(output)) {
        const auto m = re.match(line);
        if (!m.hasMatch())
            continue;
        out.insert(m.captured(3), { m.captured(1) == u"T", m.captured(2) == u"S", m.captured(4), m.captured(5).trimmed() });
    }
    return out;
}

QHash<QString, QString> parseEncoders(const QString &output)
{
    // " A....D libmp3lame           libmp3lame MP3 (MPEG audio layer 3) (codec mp3)"
    static const QRegularExpression re(QStringLiteral(R"(^\s([VAS])[.\w]{5}\s+(\S+)\s+(.*)$)"));
    QHash<QString, QString> out;
    for (const QString &line : body(output)) {
        const auto m = re.match(line);
        if (m.hasMatch() && m.captured(1) == u"A")
            out.insert(m.captured(2), m.captured(3).trimmed());
    }
    return out;
}

QHash<QString, QString> parseMuxers(const QString &output)
{
    // "  E  mp3             MP3 (MPEG audio layer 3)"  (D, E, d flag columns)
    static const QRegularExpression re(QStringLiteral(R"(^\s([D ])([E ])([d ])\s+(\S+)\s+(.*)$)"));
    QHash<QString, QString> out;
    for (const QString &line : body(output)) {
        const auto m = re.match(line);
        if (!m.hasMatch() || m.captured(2) != u"E")
            continue;
        // Some muxers are listed as "name1,name2".
        for (const QString &name : m.captured(4).split(u','))
            out.insert(name, m.captured(5).trimmed());
    }
    return out;
}

QList<LibraryInfo> evaluateLibraries(const BuildInfo &b)
{
    auto encoder = [&](const char *n) { return b.audioEncoders.contains(QString::fromLatin1(n)); };
    auto filter = [&](const char *n) { return b.filters.contains(QString::fromLatin1(n)); };
    auto muxer = [&](const char *n) { return b.muxers.contains(QString::fromLatin1(n)); };
    auto config = [&](const char *n) { return b.hasConfigFlag(QStringLiteral("--enable-") + QString::fromLatin1(n)); };
    return {
        { QStringLiteral("libmp3lame"), encoder("libmp3lame"), QStringLiteral("MP3 encoding") },
        { QStringLiteral("libopus"), encoder("libopus"), QStringLiteral("Opus encoding") },
        { QStringLiteral("libvorbis"), encoder("libvorbis"), QStringLiteral("Vorbis encoding") },
        { QStringLiteral("libfdk_aac"), encoder("libfdk_aac"), QStringLiteral("Fraunhofer AAC: better AAC at low bitrates") },
        { QStringLiteral("libsoxr"), config("libsoxr"), QStringLiteral("High-quality resampling (soxr)") },
        { QStringLiteral("librubberband"), filter("rubberband"), QStringLiteral("Effects › Pitch shift") },
        { QStringLiteral("libbs2b"), filter("bs2b"), QStringLiteral("Channels › Headphone crossfeed") },
        { QStringLiteral("libmysofa"), filter("sofalizer"), QStringLiteral("Spatial audio for headphones (sofalizer)") },
        { QStringLiteral("whisper"), filter("whisper"), QStringLiteral("Speech › Transcribe, the transcript lane") },
        { QStringLiteral("flite"), filter("flite"), QStringLiteral("Generate › Speech") },
        { QStringLiteral("chromaprint"), muxer("chromaprint"), QStringLiteral("Audio fingerprints in Compare") },
        { QStringLiteral("libzmq"), filter("azmq"), QStringLiteral("Live parameter changes during a render") },
        { QStringLiteral("libpulse"), config("libpulse"), QStringLiteral("Record from PulseAudio / PipeWire") },
        { QStringLiteral("libjack"), config("libjack"), QStringLiteral("Record from JACK") },
        { QStringLiteral("libsrt"), config("libsrt"), QStringLiteral("SRT in Stream out") },
        { QStringLiteral("ladspa"), filter("ladspa"), QStringLiteral("LADSPA plugins as steps") },
        { QStringLiteral("lv2"), filter("lv2"), QStringLiteral("LV2 plugins as steps") },
    };
}

} // namespace caps

QJsonObject BuildInfo::toJson() const
{
    QJsonObject filtersJson;
    for (auto it = filters.begin(); it != filters.end(); ++it)
        filtersJson.insert(it.key(), QJsonArray { it->timeline, it->slice, it->io, it->description });
    auto map = [](const QHash<QString, QString> &h) {
        QJsonObject o;
        for (auto it = h.begin(); it != h.end(); ++it)
            o.insert(it.key(), it.value());
        return o;
    };
    return { { "version", version }, { "configuration", configuration }, { "filters", filtersJson },
             { "encoders", map(audioEncoders) }, { "muxers", map(muxers) } };
}

BuildInfo BuildInfo::fromJson(const QJsonObject &json)
{
    BuildInfo b;
    b.version = json.value(u"version").toString();
    b.configuration = json.value(u"configuration").toString();
    const QJsonObject f = json.value(u"filters").toObject();
    for (auto it = f.begin(); it != f.end(); ++it) {
        const QJsonArray a = it.value().toArray();
        b.filters.insert(it.key(), { a.at(0).toBool(), a.at(1).toBool(), a.at(2).toString(), a.at(3).toString() });
    }
    const QJsonObject e = json.value(u"encoders").toObject();
    for (auto it = e.begin(); it != e.end(); ++it)
        b.audioEncoders.insert(it.key(), it.value().toString());
    const QJsonObject m = json.value(u"muxers").toObject();
    for (auto it = m.begin(); it != m.end(); ++it)
        b.muxers.insert(it.key(), it.value().toString());
    b.libraries = caps::evaluateLibraries(b);
    return b;
}

} // namespace ffgui
