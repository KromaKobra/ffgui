#include "Selection.h"

#include <algorithm>

Selection::Selection(QObject *parent)
    : QObject(parent)
{
}

void Selection::select(const QString &kind, const QVariantMap &props)
{
    static const QStringList kinds { "none", "input", "track", "step", "range", "output" };
    const QString k = kinds.contains(kind) ? kind : QStringLiteral("none");
    const int index = props.value(QStringLiteral("index"), -1).toInt();
    const int track = props.value(QStringLiteral("track"), -1).toInt();
    double start = props.value(QStringLiteral("start"), 0.0).toDouble();
    double end = props.value(QStringLiteral("end"), 0.0).toDouble();
    if (start > end)
        std::swap(start, end);
    const int channel = props.value(QStringLiteral("channel"), -1).toInt();

    if (k == m_kind && index == m_index && track == m_track && start == m_start && end == m_end
        && channel == m_channel)
        return;
    m_kind = k;
    m_index = index;
    m_track = track;
    m_start = start;
    m_end = end;
    m_channel = channel;
    emit changed();
}

bool Selection::isSelected(const QString &kind, int index) const
{
    return m_kind == kind && (index < 0 || m_index == index);
}

void Selection::itemRemoved(const QString &kind, int index)
{
    const bool owner = m_kind == kind || (kind == u"input" && m_kind == u"track");
    if (!owner)
        return;
    if (m_index == index)
        clear();
    else if (m_index > index)
        select(m_kind, { { "index", m_index - 1 }, { "track", m_track } });
}

void Selection::itemMoved(const QString &kind, int from, int to)
{
    if (m_kind != kind)
        return;
    int i = m_index;
    if (i == from)
        i = to;
    else if (from < i && i <= to)
        --i;
    else if (to <= i && i < from)
        ++i;
    if (i != m_index)
        select(m_kind, { { "index", i }, { "track", m_track } });
}

void Selection::itemInserted(const QString &kind, int index)
{
    const bool owner = m_kind == kind || (kind == u"input" && m_kind == u"track");
    if (owner && m_index >= index)
        select(m_kind, { { "index", m_index + 1 }, { "track", m_track } });
}
