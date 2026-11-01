#include "FormatApi.h"

#include <util/Format.h>

#include <cmath>

using namespace ffgui;

FormatApi::FormatApi(QObject *parent)
    : QObject(parent)
{
}

QString FormatApi::timecode(double seconds, bool withMs) const { return format::timecode(seconds, withMs); }
double FormatApi::parseTime(const QString &text) const { return format::parseTimecode(text); }
QString FormatApi::number(double value, int digits) const { return format::number(value, digits); }
QString FormatApi::signedNumber(double value, int digits) const { return format::signedNumber(value, digits); }
QString FormatApi::db(double linear, int digits) const { return format::decibels(linear, digits); }
QString FormatApi::bytes(double bytes) const { return format::bytes(bytes); }
QString FormatApi::sampleRate(int hertz) const { return format::sampleRate(hertz); }
QString FormatApi::frequency(double hertz) const { return format::frequency(hertz); }
QString FormatApi::bitrate(double bitsPerSecond) const { return format::bitrate(bitsPerSecond); }
QString FormatApi::note(double hertz) const { return format::noteName(hertz); }

QString FormatApi::dbValue(double decibels, int digits) const
{
    if (!(decibels > -200))
        return format::number(-INFINITY);
    return format::number(decibels, digits);
}

QString FormatApi::signal(const QVariantMap &f) const
{
    QStringList parts;
    if (const int rate = f.value(QStringLiteral("rate")).toInt(); rate > 0)
        parts << format::sampleRate(rate);
    if (const QString fmt = f.value(QStringLiteral("fmt")).toString(); !fmt.isEmpty())
        parts << fmt;
    if (const QString layout = f.value(QStringLiteral("layout")).toString(); !layout.isEmpty())
        parts << layout;
    return parts.join(QStringLiteral(" · "));
}
