#include "ProbeParser.h"
#include "ChannelLayouts.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace ffgui {

namespace {

double num(const QJsonValue &v)
{
    // ffprobe prints most numbers as strings.
    if (v.isDouble())
        return v.toDouble();
    bool ok = false;
    const double d = v.toString().toDouble(&ok);
    return ok ? d : 0;
}

QString sideDataText(const QJsonObject &sd)
{
    const QString type = sd.value(u"side_data_type").toString();
    if (type.contains(QStringLiteral("Replay"), Qt::CaseInsensitive)) {
        QStringList parts;
        const auto gain = [&](const char *key, const char *label) {
            if (sd.contains(QLatin1String(key)))
                parts << QStringLiteral("%1 %2").arg(QLatin1String(label)).arg(num(sd.value(QLatin1String(key))) / 100000.0, 0, 'f', 2);
        };
        gain("track_gain", "track gain");
        gain("album_gain", "album gain");
        return QStringLiteral("ReplayGain: ") + (parts.isEmpty() ? QStringLiteral("present") : parts.join(QStringLiteral(", ")) + QStringLiteral(" dB"));
    }
    return type;
}

} // namespace

QString ProbeResult::containerLabel() const
{
    const QString ext = QFileInfo(path).suffix().toLower();
    const QString first = formatName.section(u',', 0, 0);
    static const QHash<QString, QString> known {
        { "wav", "WAV" }, { "mp3", "MP3" }, { "flac", "FLAC" }, { "ogg", "OGG" }, { "opus", "OPUS" },
        { "matroska", "MKA" }, { "aiff", "AIFF" }, { "w64", "W64" }, { "caf", "CAF" }, { "ac3", "AC3" },
        { "eac3", "EAC3" }, { "dts", "DTS" }, { "aac", "AAC" }, { "ape", "APE" }, { "wv", "WV" },
        { "asf", "WMA" }, { "lavfi", "lavfi" }, { "hls", "HLS" }, { "pulse", "PULSE" },
    };
    if (formatName.contains(QStringLiteral("mp4")))
        return ext.isEmpty() ? QStringLiteral("MP4") : ext.toUpper();
    if (first == u"matroska" && (ext == u"mkv" || ext == u"webm"))
        return ext.toUpper();
    return known.value(first, first.toUpper());
}

int ProbeResult::defaultTrack() const
{
    for (int i = 0; i < tracks.size(); ++i)
        if (tracks[i].isDefault)
            return i;
    return tracks.isEmpty() ? -1 : 0;
}

QStringList probeArgs(const QString &input)
{
    return { QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("level+warning"),
             QStringLiteral("-of"), QStringLiteral("json"), QStringLiteral("-show_format"),
             QStringLiteral("-show_streams"), QStringLiteral("-show_chapters"), QStringLiteral("-i"), input };
}

bool parseProbe(const QByteArray &json, ProbeResult *r, QString *error)
{
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (!doc.isObject() || !doc.object().contains(u"format")) {
        if (error)
            *error = doc.isObject() ? QStringLiteral("ffprobe found no format") : pe.errorString();
        return false;
    }
    const QJsonObject root = doc.object();
    const QJsonObject format = root.value(u"format").toObject();
    r->formatName = format.value(u"format_name").toString();
    r->formatLong = format.value(u"format_long_name").toString();
    r->duration = num(format.value(u"duration"));
    r->startTime = num(format.value(u"start_time"));
    r->size = qint64(num(format.value(u"size")));
    r->bitRate = qint64(num(format.value(u"bit_rate")));
    r->tags = format.value(u"tags").toObject().toVariantMap();
    if (r->path.isEmpty())
        r->path = format.value(u"filename").toString();

    int audioIndex = 0;
    for (const QJsonValue &sv : root.value(u"streams").toArray()) {
        const QJsonObject s = sv.toObject();
        const QString type = s.value(u"codec_type").toString();
        const QJsonObject disposition = s.value(u"disposition").toObject();
        if (type == u"video" && disposition.value(u"attached_pic").toInt() == 1 && r->coverStream < 0) {
            r->coverStream = s.value(u"index").toInt();
            r->coverCodec = s.value(u"codec_name").toString();
            r->coverWidth = s.value(u"width").toInt();
            r->coverHeight = s.value(u"height").toInt();
            continue;
        }
        if (type != u"audio")
            continue;
        AudioTrack t;
        t.index = s.value(u"index").toInt();
        t.audioIndex = audioIndex++;
        t.codec = s.value(u"codec_name").toString();
        t.codecLong = s.value(u"codec_long_name").toString();
        t.profile = s.value(u"profile").toString();
        t.sampleFmt = s.value(u"sample_fmt").toString();
        t.sampleRate = int(num(s.value(u"sample_rate")));
        t.channels = s.value(u"channels").toInt();
        t.layout = s.value(u"channel_layout").toString();
        if (t.layout.isEmpty())
            t.layout = layouts::defaultLayout(t.channels);
        t.channelNames = layouts::channelNames(t.layout, t.channels);
        t.bitsPerSample = s.value(u"bits_per_raw_sample").isString() ? int(num(s.value(u"bits_per_raw_sample")))
                                                                     : s.value(u"bits_per_sample").toInt();
        if (t.bitsPerSample == 0)
            t.bitsPerSample = s.value(u"bits_per_sample").toInt();
        t.bitRate = qint64(num(s.value(u"bit_rate")));
        t.duration = num(s.value(u"duration"));
        t.frames = qint64(num(s.value(u"nb_frames")));
        const QJsonObject tags = s.value(u"tags").toObject();
        t.tags = tags.toVariantMap();
        t.language = tags.value(u"language").toString(QStringLiteral("und"));
        t.title = tags.value(u"title").toString();
        t.isDefault = disposition.value(u"default").toInt() == 1;
        for (auto it = disposition.begin(); it != disposition.end(); ++it)
            if (it.value().toInt() == 1)
                t.disposition.insert(it.key(), true);
        for (const QJsonValue &sd : s.value(u"side_data_list").toArray())
            t.sideData << sideDataText(sd.toObject());
        r->tracks << t;
    }
    // Files with one track often mark none as default; the first one is.
    if (!r->tracks.isEmpty() && r->defaultTrack() == 0 && !r->tracks[0].isDefault)
        r->tracks[0].isDefault = true;
    if (r->duration <= 0)
        for (const AudioTrack &t : std::as_const(r->tracks))
            r->duration = std::max(r->duration, t.duration);

    for (const QJsonValue &cv : root.value(u"chapters").toArray()) {
        const QJsonObject c = cv.toObject();
        r->chapters << Chapter { num(c.value(u"start_time")), num(c.value(u"end_time")),
                                 c.value(u"tags").toObject().value(u"title").toString() };
    }
    return true;
}

} // namespace ffgui
