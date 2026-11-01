#include "SceneItems.h"
#include "Analysis.h"
#include "MediaState.h"

#include <QPainter>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>

#include <algorithm>
#include <cmath>

using namespace ffgui;

#define FFGUI_SETTER(Class, Name, Type, Member, Signal)  \
    void Class::Name(Type v)                              \
    {                                                     \
        if (Member == v)                                  \
            return;                                       \
        Member = v;                                       \
        emit Signal();                                    \
        update();                                         \
    }

namespace {

double dpr(const QQuickItem *item)
{
    return item->window() ? item->window()->effectiveDevicePixelRatio() : 1.0;
}

// The items draw into images shown as textures: that works with every
// scene graph backend, including the software renderer (offscreen, VMs).
QSGNode *textureNode(QQuickItem *item, QSGNode *old, const QImage &image)
{
    auto *node = static_cast<QSGSimpleTextureNode *>(old);
    if (image.isNull() || !item->window()) {
        delete node;
        return nullptr;
    }
    QSGTexture *texture = item->window()->createTextureFromImage(image, QQuickWindow::TextureHasAlphaChannel);
    if (!node) {
        node = new QSGSimpleTextureNode;
        node->setFiltering(QSGTexture::Linear);
    }
    QSGTexture *previous = node->texture();
    node->setOwnsTexture(false);
    node->setTexture(texture);
    node->setOwnsTexture(true);
    delete previous;
    node->setRect(item->boundingRect());
    return node;
}

QImage blankImage(const QQuickItem *item, int maxWidth = 4096, int maxHeight = 2048)
{
    const double scale = dpr(item);
    const int w = std::clamp(int(std::ceil(item->width() * scale)), 0, maxWidth);
    const int h = std::clamp(int(std::ceil(item->height() * scale)), 0, maxHeight);
    if (w <= 0 || h <= 0)
        return {};
    QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    return image;
}

} // namespace

// ----------------------------------------------------------------------------
// WaveformItem

WaveformItem::WaveformItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
}

void WaveformItem::setMedia(MediaState *media)
{
    if (m_media == media)
        return;
    disconnect(m_connection);
    m_media = media;
    if (media)
        m_connection = connect(media, &MediaState::updated, this, &QQuickItem::update);
    emit mediaChanged();
    update();
}

FFGUI_SETTER(WaveformItem, setChannel, int, m_channel, viewChanged)
FFGUI_SETTER(WaveformItem, setViewStart, double, m_viewStart, viewChanged)
FFGUI_SETTER(WaveformItem, setViewEnd, double, m_viewEnd, viewChanged)
FFGUI_SETTER(WaveformItem, setAmplitude, double, m_amplitude, viewChanged)
FFGUI_SETTER(WaveformItem, setShowRms, bool, m_showRms, viewChanged)
FFGUI_SETTER(WaveformItem, setPeakColor, const QColor &, m_peakColor, viewChanged)
FFGUI_SETTER(WaveformItem, setRmsColor, const QColor &, m_rmsColor, viewChanged)

QSGNode *WaveformItem::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    const std::shared_ptr<MediaData> data = m_media ? m_media->data() : nullptr;
    const double span = m_viewEnd - m_viewStart;
    QImage image = blankImage(this, 8192);
    if (!data || image.isNull() || span <= 0) {
        delete old;
        return nullptr;
    }
    const int cols = image.width(), h = image.height();
    const double mid = h / 2.0, amp = h / 2.0 * m_amplitude;
    auto yOf = [&](double v) { return std::clamp(mid - v * amp, 0.0, double(h - 1)); };
    const int rate = data->sampleRate();
    QPainter painter(&image);

    // Zoomed in to single samples: a line through them.
    std::vector<float> samples;
    qint64 first = 0;
    if (span * rate / cols < 1.5 && data->windowSamples(m_channel, m_viewStart, m_viewEnd, &samples, &first)) {
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(m_rmsColor, 1.5));
        QPolygonF line;
        line.reserve(qsizetype(samples.size()));
        for (size_t k = 0; k < samples.size(); ++k)
            line << QPointF((double(first + qint64(k)) / rate - m_viewStart) / span * cols, yOf(samples[k]));
        painter.drawPolyline(line);
        if (span * rate / cols < 0.25) {
            painter.setBrush(m_rmsColor);
            painter.setPen(Qt::NoPen);
            for (const QPointF &p : std::as_const(line))
                painter.drawEllipse(p, 2.0, 2.0);
        }
        painter.end();
        return textureNode(this, old, image);
    }

    // One column per device pixel: the peak range, the RMS body over it.
    std::vector<PeakColumn> columns(static_cast<size_t>(cols));
    data->peakColumns(m_channel, m_viewStart, m_viewEnd, cols, columns.data());
    painter.setPen(QPen(m_peakColor, 1));
    for (int x = 0; x < cols; ++x) {
        const PeakColumn &c = columns[size_t(x)];
        if (c.valid)
            painter.drawLine(QPointF(x + 0.5, yOf(c.max)), QPointF(x + 0.5, yOf(c.min) + 0.01));
    }
    if (m_showRms) {
        painter.setPen(QPen(m_rmsColor, 1));
        for (int x = 0; x < cols; ++x) {
            const PeakColumn &c = columns[size_t(x)];
            if (c.valid && c.rms * amp >= 0.5)
                painter.drawLine(QPointF(x + 0.5, yOf(c.rms)), QPointF(x + 0.5, yOf(-c.rms)));
        }
    }
    painter.end();
    return textureNode(this, old, image);
}

// ----------------------------------------------------------------------------
// SpectrogramItem

SpectrogramItem::SpectrogramItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
    buildLut();
}

void SpectrogramItem::setMedia(MediaState *media)
{
    if (m_media == media)
        return;
    disconnect(m_connection);
    m_media = media;
    if (media)
        m_connection = connect(media, &MediaState::updated, this, &QQuickItem::update);
    emit mediaChanged();
    update();
}

FFGUI_SETTER(SpectrogramItem, setChannel, int, m_channel, viewChanged)
FFGUI_SETTER(SpectrogramItem, setViewStart, double, m_viewStart, viewChanged)
FFGUI_SETTER(SpectrogramItem, setViewEnd, double, m_viewEnd, viewChanged)
FFGUI_SETTER(SpectrogramItem, setMinFrequency, double, m_minFrequency, viewChanged)
FFGUI_SETTER(SpectrogramItem, setMaxFrequency, double, m_maxFrequency, viewChanged)
FFGUI_SETTER(SpectrogramItem, setLogScale, bool, m_logScale, viewChanged)

void SpectrogramItem::setColorStops(const QVariantList &stops)
{
    if (m_colorStops == stops)
        return;
    m_colorStops = stops;
    buildLut();
    emit viewChanged();
    update();
}

void SpectrogramItem::buildLut()
{
    struct Stop { double pos, r, g, b; };
    std::vector<Stop> stops;
    for (const QVariant &v : std::as_const(m_colorStops)) {
        const QVariantList s = v.toList();
        if (s.size() >= 4)
            stops.push_back({ s[0].toDouble(), s[1].toDouble(), s[2].toDouble(), s[3].toDouble() });
    }
    if (stops.size() < 2)
        stops = { { 0, 0, 0, 0 }, { 1, 255, 255, 255 } };
    for (int k = 0; k < 256; ++k) {
        const double v = k / 255.0;
        size_t j = 1;
        while (j + 1 < stops.size() && stops[j].pos < v)
            ++j;
        const Stop &a = stops[j - 1], &b = stops[j];
        const double f = b.pos > a.pos ? std::clamp((v - a.pos) / (b.pos - a.pos), 0.0, 1.0) : 0.0;
        m_lut[k] = qRgb(int(a.r + (b.r - a.r) * f), int(a.g + (b.g - a.g) * f), int(a.b + (b.b - a.b) * f));
    }
}

QSGNode *SpectrogramItem::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    const std::shared_ptr<MediaData> data = m_media ? m_media->data() : nullptr;
    QImage image = blankImage(this);
    if (!data || image.isNull() || m_viewEnd <= m_viewStart) {
        delete old;
        return nullptr;
    }
    data->renderSpectrogram(m_channel, m_viewStart, m_viewEnd, m_minFrequency, m_maxFrequency, m_logScale, m_lut, &image);
    return textureNode(this, old, image);
}

// ----------------------------------------------------------------------------
// CurveItem

CurveItem::CurveItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
}

void CurveItem::setAnalysis(Analysis *analysis)
{
    if (m_analysis == analysis)
        return;
    disconnect(m_connection);
    m_analysis = analysis;
    if (analysis)
        m_connection = connect(analysis, &Analysis::seriesChanged, this, &QQuickItem::update);
    emit analysisChanged();
    update();
}

FFGUI_SETTER(CurveItem, setSeries, const QString &, m_series, viewChanged)
FFGUI_SETTER(CurveItem, setViewStart, double, m_viewStart, viewChanged)
FFGUI_SETTER(CurveItem, setViewEnd, double, m_viewEnd, viewChanged)
FFGUI_SETTER(CurveItem, setMinValue, double, m_min, viewChanged)
FFGUI_SETTER(CurveItem, setMaxValue, double, m_max, viewChanged)
FFGUI_SETTER(CurveItem, setBars, bool, m_bars, viewChanged)
FFGUI_SETTER(CurveItem, setColor, const QColor &, m_color, viewChanged)
FFGUI_SETTER(CurveItem, setWarnBelow, double, m_warnBelow, viewChanged)
FFGUI_SETTER(CurveItem, setBadBelow, double, m_badBelow, viewChanged)
FFGUI_SETTER(CurveItem, setWarnColor, const QColor &, m_warnColor, viewChanged)
FFGUI_SETTER(CurveItem, setBadColor, const QColor &, m_badColor, viewChanged)

QSGNode *CurveItem::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    const std::shared_ptr<const Analysis::Series> series = m_analysis ? m_analysis->series(m_series) : nullptr;
    const double span = m_viewEnd - m_viewStart;
    QImage image = blankImage(this);
    if (!series || series->times.isEmpty() || image.isNull() || span <= 0 || m_max <= m_min) {
        delete old;
        return nullptr;
    }
    const int cols = image.width(), h = image.height();

    // One value per pixel column: the highest (lines) or lowest (bars) there.
    std::vector<float> values(size_t(cols), NAN);
    const QVector<double> &times = series->times;
    const QVector<float> &vals = series->values;
    auto it = std::lower_bound(times.begin(), times.end(), m_viewStart - span / cols);
    for (qsizetype k = it - times.begin(); k < times.size() && k < vals.size() && times[k] <= m_viewEnd + span / cols; ++k) {
        const int col = std::clamp(int((times[k] - m_viewStart) / span * cols), 0, cols - 1);
        float &slot = values[size_t(col)];
        slot = std::isnan(slot) ? vals[k] : m_bars ? std::min(slot, vals[k]) : std::max(slot, vals[k]);
    }
    // Zoomed in past the series' resolution: hold each value until the next.
    float last = NAN;
    for (float &v : values) {
        if (std::isnan(v))
            v = last;
        else
            last = v;
    }
    auto yOf = [&](float v) { return h - std::clamp((v - m_min) / (m_max - m_min), 0.0, 1.0) * (h - 1) - 0.5; };

    QPainter painter(&image);
    if (m_bars) {
        for (int x = 0; x < cols; ++x) {
            const float v = values[size_t(x)];
            if (!std::isnan(v))
                painter.fillRect(x, 0, 1, h, v < m_badBelow ? m_badColor : v < m_warnBelow ? m_warnColor : m_color);
        }
    } else {
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(m_color, 1.5 * dpr(this)));
        QPolygonF line;
        for (int x = 0; x < cols; ++x) {
            const float v = values[size_t(x)];
            if (std::isnan(v) || v <= m_min) {
                if (line.size() > 1)
                    painter.drawPolyline(line);
                line.clear();
                continue;
            }
            line << QPointF(x + 0.5, yOf(v));
        }
        if (line.size() > 1)
            painter.drawPolyline(line);
    }
    painter.end();
    return textureNode(this, old, image);
}
