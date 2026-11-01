#pragma once

#include <QQmlEngine>

// Text formatting for QML: timecodes, dB with a real minus sign, sizes, rates
// and note names. Wraps ffgui::format.
class FormatApi : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Format)
    QML_SINGLETON

public:
    explicit FormatApi(QObject *parent = nullptr);

    Q_INVOKABLE QString timecode(double seconds, bool withMs = false) const;
    // NaN when the text isn't a time.
    Q_INVOKABLE double parseTime(const QString &text) const;
    Q_INVOKABLE QString number(double value, int digits = 1) const;
    Q_INVOKABLE QString signedNumber(double value, int digits = 1) const;
    // Linear amplitude -> "−12.3".
    Q_INVOKABLE QString db(double linear, int digits = 1) const;
    // A value already in dB -> "−12.3", "−∞" below -200.
    Q_INVOKABLE QString dbValue(double decibels, int digits = 1) const;
    Q_INVOKABLE QString bytes(double bytes) const;
    Q_INVOKABLE QString sampleRate(int hertz) const;
    Q_INVOKABLE QString frequency(double hertz) const;
    Q_INVOKABLE QString bitrate(double bitsPerSecond) const;
    Q_INVOKABLE QString note(double hertz) const;
    // "48 kHz · fltp · stereo" from { rate, fmt, layout }.
    Q_INVOKABLE QString signal(const QVariantMap &format) const;
};
