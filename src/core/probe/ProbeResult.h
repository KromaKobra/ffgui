#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// What ffprobe reports about an input, in ffgui's terms.
namespace ffgui {

struct AudioTrack
{
    int index = 0;              // stream index in the file
    int audioIndex = 0;         // n in -map 0:a:n
    QString codec;
    QString codecLong;
    QString profile;
    QString sampleFmt;
    int sampleRate = 0;
    int channels = 0;
    QString layout;             // as reported, or ffmpeg's default for the count
    QStringList channelNames;
    int bitsPerSample = 0;      // 0 when the codec doesn't store one (lossy)
    qint64 bitRate = 0;         // bits/s, 0 when unknown
    double duration = 0;
    qint64 frames = 0;          // nb_frames, 0 when unknown
    QString language;
    QString title;
    bool isDefault = false;
    QVariantMap disposition;    // the flags that are set
    QStringList sideData;       // "ReplayGain: track gain -6.2 dB", ...
    QVariantMap tags;
};

struct Chapter
{
    double start = 0;
    double end = 0;
    QString title;
};

struct ProbeResult
{
    QString path;
    QString formatName;         // "mp3", "mov,mp4,m4a,3gp,3g2,mj2"
    QString formatLong;
    double duration = 0;        // seconds, 0 when unknown (live streams)
    double startTime = 0;
    qint64 size = 0;            // bytes
    qint64 bitRate = 0;
    QVariantMap tags;
    QList<AudioTrack> tracks;
    QList<Chapter> chapters;
    int coverStream = -1;       // stream index of the attached picture
    QString coverCodec;
    int coverWidth = 0;
    int coverHeight = 0;

    // A short container label for badges: WAV, MP3, FLAC, M4A, MKA, ...
    QString containerLabel() const;
    int defaultTrack() const;   // index into tracks
};

} // namespace ffgui
