#pragma once

#include <QQmlEngine>
#include <QVariantMap>

// "One selection drives everything" (design principle 4): the Inspector, the
// Media View and the command preview all follow this object.
//
//   kind     none | input | track | step | range | output
//   index    the input, step or output (input and track: the input)
//   track    the track within the input
//   start/end, channel   the time range (channel -1 = all channels)
class Selection : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by App")

    Q_PROPERTY(QString kind READ kind NOTIFY changed)
    Q_PROPERTY(int index READ index NOTIFY changed)
    Q_PROPERTY(int track READ track NOTIFY changed)
    Q_PROPERTY(double start READ start NOTIFY changed)
    Q_PROPERTY(double end READ end NOTIFY changed)
    Q_PROPERTY(int channel READ channel NOTIFY changed)
    Q_PROPERTY(bool isRange READ isRange NOTIFY changed)

public:
    explicit Selection(QObject *parent = nullptr);

    QString kind() const { return m_kind; }
    int index() const { return m_index; }
    int track() const { return m_track; }
    double start() const { return m_start; }
    double end() const { return m_end; }
    int channel() const { return m_channel; }
    bool isRange() const { return m_kind == u"range"; }

    // select("step", { index: 2 }), select("range", { start: 1.5, end: 3 }), ...
    Q_INVOKABLE void select(const QString &kind, const QVariantMap &props = {});
    Q_INVOKABLE void clear() { select(QStringLiteral("none")); }
    Q_INVOKABLE bool isSelected(const QString &kind, int index = -1) const;

    // Keeps a step/output/input selection pointing at the same item when the
    // list it belongs to changes.
    void itemRemoved(const QString &kind, int index);
    void itemMoved(const QString &kind, int from, int to);
    void itemInserted(const QString &kind, int index);

signals:
    void changed();

private:
    QString m_kind = QStringLiteral("none");
    int m_index = -1;
    int m_track = -1;
    double m_start = 0;
    double m_end = 0;
    int m_channel = -1;
};
