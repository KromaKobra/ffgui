#pragma once

#include <QFile>
#include <QString>

// Reads a file captured from this machine's ffmpeg (tests/core/fixtures).
inline QByteArray fixture(const char *name)
{
    QFile f(QStringLiteral(FIXTURE_DIR "/") + QLatin1String(name));
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
