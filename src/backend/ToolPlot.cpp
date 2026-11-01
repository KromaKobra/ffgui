#include "ToolPlot.h"
#include <QPainter>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <cmath>
void ToolPlot::setSource(ScopeState *s)
{
    if (m_source == s)
        return;
    disconnect(m_connection);
    m_source = s;
    if (s)
        m_connection = connect(s, &ScopeState::updated, this, &QQuickItem::update);
    emit changed();
    update();
}
QSGNode *ToolPlot::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    const int w = std::clamp(int(width()), 0, 2048), h = std::clamp(int(height()), 0, 1024);
    if (!w || !h || !window()) {
        delete old;
        return nullptr;
    }
    QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(m_grid, 1));
    for (int i = 1; i < 4; ++i)
        p.drawLine(0, i * h / 4, w, i * h / 4);
    const QVariantMap data = m_source ? m_source->values() : QVariantMap();
    const auto values = m_source ? data.value(m_kind).toList() : m_values;
    p.setPen(QPen(m_tone, 1));
    p.setBrush(m_tone);
    if (m_kind == u"vectorscope") {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(m_grid, 1));
        p.drawLine(w / 2, 0, w / 2, h);
        p.drawEllipse(QPointF(w / 2., h / 2.), w * .45, h * .45);
        p.setPen(QPen(m_tone, 1.5));
        for (const auto &v : values) {
            const auto point = v.toMap();
            p.drawPoint(QPointF(w / 2. + point.value("x").toDouble() * w * .32,
                                h / 2. - point.value("y").toDouble() * h * .32));
        }
    } else if (m_kind == u"phase") {
        const double phase = data.value("phase").toDouble();
        p.drawRect(QRectF(std::min(w / 2., w / 2. + phase * w * .45), h * .35, std::abs(phase) * w * .45,
                          std::max(2., h * .3)));
        p.setPen(QPen(m_grid, 1));
        p.drawLine(w / 2, 0, w / 2, h);
    } else if (m_kind == u"waveform") {
        p.setPen(QPen(m_tone, std::max(1., double(w) / std::max<qsizetype>(1, values.size()))));
        for (int i = 0; i < values.size(); ++i) {
            const double peak = std::clamp(values[i].toDouble(), 0., 1.);
            const double x = double(i) * w / std::max<qsizetype>(1, values.size());
            p.drawLine(QPointF(x, h / 2. - peak * h * .48), QPointF(x, h / 2. + peak * h * .48));
        }
    } else {
        const bool db = m_kind == u"spectrum" || m_kind == u"cqt";
        for (int i = 0; i < values.size(); ++i) {
            double n = values[i].toDouble();
            if (db)
                n = (n + 100) / 100;
            n = std::clamp(n, 0., 1.);
            const double bw = double(w) / std::max<qsizetype>(1, values.size());
            p.drawRect(QRectF(i * bw, h * (1 - n), std::max(.5, bw - 1), h * n));
        }
    }
    p.end();
    auto *node = static_cast<QSGSimpleTextureNode *>(old);
    if (!node) {
        node = new QSGSimpleTextureNode;
        node->setFiltering(QSGTexture::Linear);
    }
    auto *texture = window()->createTextureFromImage(image);
    auto *previous = node->texture();
    node->setOwnsTexture(false);
    node->setTexture(texture);
    node->setOwnsTexture(true);
    delete previous;
    node->setRect(boundingRect());
    return node;
}
