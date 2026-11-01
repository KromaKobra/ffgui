#include "Format.h"

#include <QStringList>

#include <cmath>
#include <limits>

namespace ffgui::format {

namespace {

const QChar kMinus(0x2212);

QString withMinus(QString s)
{
    if (s.startsWith(u'-'))
        s[0] = kMinus;
    return s;
}

QString pad(qint64 value, int width)
{
    return QString::number(value).rightJustified(width, u'0');
}

} // namespace

QString timecode(double seconds, bool withMs)
{
    if (!std::isfinite(seconds))
        seconds = 0;
    const double t = std::max(0.0, seconds);
    // Round to whole milliseconds first so 59.9996 shows as 01:00.000, not 00:59.1000.
    const qint64 ms = std::llround(t * 1000.0);
    const qint64 totalSeconds = withMs ? ms / 1000 : static_cast<qint64>(std::floor(t));
    const qint64 h = totalSeconds / 3600;
    const qint64 m = totalSeconds % 3600 / 60;
    const qint64 s = totalSeconds % 60;
    QString out = h > 0 ? QString::number(h) + u':' + pad(m, 2) : pad(m, 2);
    out += u':' + pad(s, 2);
    if (withMs)
        out += u'.' + pad(ms % 1000, 3);
    return out;
}

double parseTimecode(const QString &text)
{
    const QString trimmed = text.trimmed().replace(kMinus, u'-');
    if (trimmed.isEmpty())
        return std::numeric_limits<double>::quiet_NaN();
    const QStringList parts = trimmed.split(u':');
    if (parts.size() > 3)
        return std::numeric_limits<double>::quiet_NaN();
    double total = 0;
    for (int i = 0; i < parts.size(); ++i) {
        bool ok = false;
        const double v = parts[i].toDouble(&ok);
        if (!ok || v < 0 || (i > 0 && v >= 60))
            return std::numeric_limits<double>::quiet_NaN();
        total = total * 60 + v;
    }
    return total;
}

QString number(double value, int digits)
{
    if (std::isnan(value))
        return QStringLiteral("—");
    if (std::isinf(value))
        return value < 0 ? QString(kMinus) + u'∞' : QStringLiteral("∞");
    QString s = QString::number(value, 'f', digits);
    // "-0.0" reads as a sign error; show it as 0.
    if (s.startsWith(u'-') && QString::number(-value, 'f', digits).toDouble() == 0)
        s.remove(0, 1);
    return withMinus(s);
}

QString signedNumber(double value, int digits)
{
    const QString s = number(value, digits);
    return value > 0 && s.toDouble() != 0 ? u'+' + s : s;
}

QString decibels(double linear, int digits)
{
    if (!(linear > 0.00001))
        return QString(kMinus) + u'∞';
    return number(20.0 * std::log10(linear), digits);
}

QString bytes(double bytes)
{
    if (!(bytes >= 0))
        return QStringLiteral("—");
    if (bytes < 1000)
        return QString::number(std::llround(bytes)) + QStringLiteral(" B");
    const double kb = bytes / 1000.0;
    if (kb < 1000)
        return QString::number(kb, 'f', kb < 10 ? 1 : 0) + QStringLiteral(" kB");
    const double mb = kb / 1000.0;
    if (mb < 1000)
        return QString::number(mb, 'f', mb < 10 ? 1 : 0) + QStringLiteral(" MB");
    return QString::number(mb / 1000.0, 'f', 2) + QStringLiteral(" GB");
}

QString sampleRate(int hertz)
{
    if (hertz <= 0)
        return QStringLiteral("—");
    const double khz = hertz / 1000.0;
    return QString::number(khz, 'f', std::fmod(hertz, 1000) == 0 ? 0 : 1) + QStringLiteral(" kHz");
}

QString frequency(double hertz)
{
    if (!(hertz > 0))
        return QStringLiteral("—");
    if (hertz >= 1000)
        return QString::number(hertz / 1000.0, 'f', 2) + QStringLiteral(" kHz");
    return QString::number(std::llround(hertz)) + QStringLiteral(" Hz");
}

QString bitrate(double bitsPerSecond)
{
    if (!(bitsPerSecond > 0))
        return QStringLiteral("—");
    return QString::number(std::llround(bitsPerSecond / 1000.0)) + QStringLiteral(" kb/s");
}

QString noteName(double hertz)
{
    if (!(hertz > 0))
        return QStringLiteral("—");
    static const char *names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const long midi = std::lround(69 + 12 * std::log2(hertz / 440.0));
    const long octave = static_cast<long>(std::floor(midi / 12.0)) - 1;
    return QString::fromLatin1(names[((midi % 12) + 12) % 12]) + QString::number(octave);
}

} // namespace ffgui::format
