#pragma once

#include <QString>

// Text formatting shared by the UI and the logs: timecodes, decibels with a
// real minus sign, sizes, rates and note names. Pure functions.
namespace ffgui::format {

// "mm:ss" below an hour, "h:mm:ss" above; withMs appends ".mmm".
QString timecode(double seconds, bool withMs = false);

// Parses "1:02:03.5", "02:03", "75.2" or "75". Returns NaN when it can't.
double parseTimecode(const QString &text);

// A number with a real minus sign (U+2212), e.g. "−12.3".
QString number(double value, int digits = 1);

// Like number(), with a leading "+" for positive values.
QString signedNumber(double value, int digits = 1);

// A linear amplitude as dB, "−∞" at or below 1e-5.
QString decibels(double linear, int digits = 1);

// Decimal units, as file managers show them: "417 B", "12 kB", "731 MB", "1.40 GB".
QString bytes(double bytes);

// "48 kHz", "44.1 kHz", "192 kHz".
QString sampleRate(int hertz);

// "440 Hz", "1.20 kHz".
QString frequency(double hertz);

// "2304 kb/s" from bits per second.
QString bitrate(double bitsPerSecond);

// Nearest equal-tempered note, e.g. 440 -> "A4", 261.6 -> "C4".
QString noteName(double hertz);

} // namespace ffgui::format
