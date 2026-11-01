#pragma once

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

// What an ffmpeg build is and can do, parsed from -version, -filters,
// -encoders and -muxers.
namespace ffgui {

struct FilterInfo
{
    bool timeline = false;      // T: supports enable='between(t,a,b)'
    bool slice = false;         // S: slice threading
    QString io;                 // "A->A", "AA->A", "|->A", "A->N", ...
    QString description;
    bool audio() const { return io.contains(u'A') || io.contains(u'N'); }
};

struct LibraryInfo
{
    QString name;
    bool found = false;
    QString unlocks;
};

struct BuildInfo
{
    QString version;            // "9.0.2" (the "n" of git tags is dropped)
    QString configuration;
    QHash<QString, FilterInfo> filters;
    QHash<QString, QString> audioEncoders;   // name -> description
    QHash<QString, QString> muxers;          // name -> description
    QList<LibraryInfo> libraries;

    bool hasConfigFlag(const QString &flag) const { return configuration.contains(flag); }

    QJsonObject toJson() const;
    static BuildInfo fromJson(const QJsonObject &json);
};

namespace caps {

// "ffmpeg version n9.0.2 Copyright ..." and its "configuration:" line.
void parseVersion(const QString &output, BuildInfo *info);
QHash<QString, FilterInfo> parseFilters(const QString &output);
// Audio encoders only.
QHash<QString, QString> parseEncoders(const QString &output);
// Formats that can mux (E flag).
QHash<QString, QString> parseMuxers(const QString &output);
// The optional libraries ffgui cares about, and whether this build has them.
QList<LibraryInfo> evaluateLibraries(const BuildInfo &info);

} // namespace caps
} // namespace ffgui
