#pragma once

#include <QColor>
#include <QPointer>
#include <QQuickItem>
#include <QVariantList>

class MediaState;
class Analysis;

// Draws one channel's waveform (peak outline and RMS body) for the visible
// window, from the peak pyramid, or sample by sample when zoomed in that far.
class WaveformItem : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(MediaState *media READ media WRITE setMedia NOTIFY mediaChanged)
    Q_PROPERTY(int channel READ channel WRITE setChannel NOTIFY viewChanged)          // -1 = all channels
    Q_PROPERTY(double viewStart READ viewStart WRITE setViewStart NOTIFY viewChanged)
    Q_PROPERTY(double viewEnd READ viewEnd WRITE setViewEnd NOTIFY viewChanged)
    Q_PROPERTY(double amplitude READ amplitude WRITE setAmplitude NOTIFY viewChanged)  // vertical zoom
    Q_PROPERTY(bool showRms READ showRms WRITE setShowRms NOTIFY viewChanged)
    Q_PROPERTY(QColor peakColor READ peakColor WRITE setPeakColor NOTIFY viewChanged)
    Q_PROPERTY(QColor rmsColor READ rmsColor WRITE setRmsColor NOTIFY viewChanged)

public:
    explicit WaveformItem(QQuickItem *parent = nullptr);

    MediaState *media() const { return m_media; }
    void setMedia(MediaState *media);
    int channel() const { return m_channel; }
    void setChannel(int c);
    double viewStart() const { return m_viewStart; }
    void setViewStart(double t);
    double viewEnd() const { return m_viewEnd; }
    void setViewEnd(double t);
    double amplitude() const { return m_amplitude; }
    void setAmplitude(double a);
    bool showRms() const { return m_showRms; }
    void setShowRms(bool on);
    QColor peakColor() const { return m_peakColor; }
    void setPeakColor(const QColor &c);
    QColor rmsColor() const { return m_rmsColor; }
    void setRmsColor(const QColor &c);

signals:
    void mediaChanged();
    void viewChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;

private:
    QPointer<MediaState> m_media;
    QMetaObject::Connection m_connection;
    int m_channel = 0;
    double m_viewStart = 0;
    double m_viewEnd = 1;
    double m_amplitude = 1;
    bool m_showRms = true;
    QColor m_peakColor = QColor(0x3d, 0x82, 0xcc);
    QColor m_rmsColor = QColor(0x8f, 0xcb, 0xff);
};

// Draws one channel's spectrogram for the visible window, with a linear or
// logarithmic frequency axis and a colour map.
class SpectrogramItem : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(MediaState *media READ media WRITE setMedia NOTIFY mediaChanged)
    Q_PROPERTY(int channel READ channel WRITE setChannel NOTIFY viewChanged)
    Q_PROPERTY(double viewStart READ viewStart WRITE setViewStart NOTIFY viewChanged)
    Q_PROPERTY(double viewEnd READ viewEnd WRITE setViewEnd NOTIFY viewChanged)
    Q_PROPERTY(double minFrequency READ minFrequency WRITE setMinFrequency NOTIFY viewChanged)
    Q_PROPERTY(double maxFrequency READ maxFrequency WRITE setMaxFrequency NOTIFY viewChanged)  // 0 = Nyquist
    Q_PROPERTY(bool logScale READ logScale WRITE setLogScale NOTIFY viewChanged)
    // [[position, r, g, b], ...], positions 0..1.
    Q_PROPERTY(QVariantList colorStops READ colorStops WRITE setColorStops NOTIFY viewChanged)

public:
    explicit SpectrogramItem(QQuickItem *parent = nullptr);

    MediaState *media() const { return m_media; }
    void setMedia(MediaState *media);
    int channel() const { return m_channel; }
    void setChannel(int c);
    double viewStart() const { return m_viewStart; }
    void setViewStart(double t);
    double viewEnd() const { return m_viewEnd; }
    void setViewEnd(double t);
    double minFrequency() const { return m_minFrequency; }
    void setMinFrequency(double f);
    double maxFrequency() const { return m_maxFrequency; }
    void setMaxFrequency(double f);
    bool logScale() const { return m_logScale; }
    void setLogScale(bool on);
    QVariantList colorStops() const { return m_colorStops; }
    void setColorStops(const QVariantList &stops);

signals:
    void mediaChanged();
    void viewChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;

private:
    void buildLut();

    QPointer<MediaState> m_media;
    QMetaObject::Connection m_connection;
    int m_channel = 0;
    double m_viewStart = 0;
    double m_viewEnd = 1;
    double m_minFrequency = 20;
    double m_maxFrequency = 0;
    bool m_logScale = true;
    QVariantList m_colorStops;
    QRgb m_lut[256];
};

// A time series from the analysis (short-term loudness, phase) over the
// visible window: a line, or bars coloured by value.
class CurveItem : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(Analysis *analysis READ analysis WRITE setAnalysis NOTIFY analysisChanged)
    Q_PROPERTY(QString series READ series WRITE setSeries NOTIFY viewChanged)
    Q_PROPERTY(double viewStart READ viewStart WRITE setViewStart NOTIFY viewChanged)
    Q_PROPERTY(double viewEnd READ viewEnd WRITE setViewEnd NOTIFY viewChanged)
    Q_PROPERTY(double minValue READ minValue WRITE setMinValue NOTIFY viewChanged)
    Q_PROPERTY(double maxValue READ maxValue WRITE setMaxValue NOTIFY viewChanged)
    Q_PROPERTY(bool bars READ bars WRITE setBars NOTIFY viewChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY viewChanged)
    // Bars: below badBelow -> badColor, below warnBelow -> warnColor, else color.
    Q_PROPERTY(double warnBelow READ warnBelow WRITE setWarnBelow NOTIFY viewChanged)
    Q_PROPERTY(double badBelow READ badBelow WRITE setBadBelow NOTIFY viewChanged)
    Q_PROPERTY(QColor warnColor READ warnColor WRITE setWarnColor NOTIFY viewChanged)
    Q_PROPERTY(QColor badColor READ badColor WRITE setBadColor NOTIFY viewChanged)

public:
    explicit CurveItem(QQuickItem *parent = nullptr);

    Analysis *analysis() const { return m_analysis; }
    void setAnalysis(Analysis *analysis);
    QString series() const { return m_series; }
    void setSeries(const QString &s);
    double viewStart() const { return m_viewStart; }
    void setViewStart(double t);
    double viewEnd() const { return m_viewEnd; }
    void setViewEnd(double t);
    double minValue() const { return m_min; }
    void setMinValue(double v);
    double maxValue() const { return m_max; }
    void setMaxValue(double v);
    bool bars() const { return m_bars; }
    void setBars(bool b);
    QColor color() const { return m_color; }
    void setColor(const QColor &c);
    double warnBelow() const { return m_warnBelow; }
    void setWarnBelow(double v);
    double badBelow() const { return m_badBelow; }
    void setBadBelow(double v);
    QColor warnColor() const { return m_warnColor; }
    void setWarnColor(const QColor &c);
    QColor badColor() const { return m_badColor; }
    void setBadColor(const QColor &c);

signals:
    void analysisChanged();
    void viewChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;

private:
    QPointer<Analysis> m_analysis;
    QMetaObject::Connection m_connection;
    QString m_series;
    double m_viewStart = 0;
    double m_viewEnd = 1;
    double m_min = -60;
    double m_max = 0;
    bool m_bars = false;
    QColor m_color = Qt::green;
    double m_warnBelow = -2;
    double m_badBelow = -2;
    QColor m_warnColor = Qt::yellow;
    QColor m_badColor = Qt::red;
};
