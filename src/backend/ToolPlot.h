#pragma once
#include "Tools.h"
#include <QColor>
#include <QPointer>
#include <QQuickItem>
class ToolPlot : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(ScopeState *source READ source WRITE setSource NOTIFY changed)
    Q_PROPERTY(QVariantList values READ values WRITE setValues NOTIFY changed)
    Q_PROPERTY(QString kind READ kind WRITE setKind NOTIFY changed)
    Q_PROPERTY(QColor tone READ tone WRITE setTone NOTIFY changed)
    Q_PROPERTY(QColor gridColor READ gridColor WRITE setGridColor NOTIFY changed)
  public:
    explicit ToolPlot(QQuickItem *parent = nullptr) : QQuickItem(parent) { setFlag(ItemHasContents); }
    ScopeState *source() const { return m_source; }
    void setSource(ScopeState *s);
    QVariantList values() const { return m_values; }
    void setValues(const QVariantList &v)
    {
        m_values = v;
        emit changed();
        update();
    }
    QString kind() const { return m_kind; }
    void setKind(const QString &s)
    {
        m_kind = s;
        emit changed();
        update();
    }
    QColor tone() const { return m_tone; }
    void setTone(const QColor &v)
    {
        m_tone = v;
        emit changed();
        update();
    }
    QColor gridColor() const { return m_grid; }
    void setGridColor(const QColor &v)
    {
        m_grid = v;
        emit changed();
        update();
    }
  signals:
    void changed();

  protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;

  private:
    QPointer<ScopeState> m_source;
    QMetaObject::Connection m_connection;
    QVariantList m_values;
    QString m_kind = "spectrum";
    QColor m_tone = Qt::white, m_grid = Qt::gray;
};
