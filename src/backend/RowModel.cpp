#include "RowModel.h"

RowModel::RowModel(const QStringList &roles, QObject *parent)
    : QAbstractListModel(parent)
    , m_roles(roles)
{
}

int RowModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant RowModel::data(const QModelIndex &index, int role) const
{
    const int column = role - Qt::UserRole;
    if (!index.isValid() || index.row() >= count() || column < 0 || column >= m_roles.size())
        return {};
    return m_rows.at(index.row()).value(m_roles.at(column));
}

QHash<int, QByteArray> RowModel::roleNames() const
{
    QHash<int, QByteArray> names;
    for (int i = 0; i < m_roles.size(); ++i)
        names.insert(Qt::UserRole + i, m_roles.at(i).toUtf8());
    return names;
}

QVariantMap RowModel::get(int row) const
{
    return row >= 0 && row < count() ? m_rows.at(row) : QVariantMap();
}

QVariant RowModel::value(int row, const QString &role) const
{
    return get(row).value(role);
}

int RowModel::find(const QString &role, const QVariant &value) const
{
    for (int i = 0; i < count(); ++i)
        if (m_rows.at(i).value(role) == value)
            return i;
    return -1;
}

QVariantMap RowModel::normalized(const QVariantMap &values) const
{
    // Every row carries every role, so delegates' required properties never
    // see undefined.
    QVariantMap row;
    for (const QString &role : m_roles)
        row.insert(role, values.value(role));
    return row;
}

void RowModel::setRows(const QList<QVariantMap> &rows)
{
    const int before = count();
    beginResetModel();
    m_rows.clear();
    m_rows.reserve(rows.size());
    for (const QVariantMap &r : rows)
        m_rows << normalized(r);
    endResetModel();
    if (before != count())
        emit countChanged();
    bump();
}

void RowModel::insertAt(int row, const QVariantMap &values)
{
    row = std::clamp(row, 0, count());
    beginInsertRows({}, row, row);
    m_rows.insert(row, normalized(values));
    endInsertRows();
    emit countChanged();
    bump();
}

void RowModel::update(int row, const QVariantMap &changes)
{
    if (row < 0 || row >= count())
        return;
    QVariantMap &r = m_rows[row];
    QList<int> changed;
    for (auto it = changes.begin(); it != changes.end(); ++it) {
        const int column = int(m_roles.indexOf(it.key()));
        if (column < 0 || r.value(it.key()) == it.value())
            continue;
        r.insert(it.key(), it.value());
        changed << Qt::UserRole + column;
    }
    if (changed.isEmpty())
        return;
    const QModelIndex i = index(row);
    emit dataChanged(i, i, changed);
    bump();
}

void RowModel::removeAt(int row)
{
    if (row < 0 || row >= count())
        return;
    beginRemoveRows({}, row, row);
    m_rows.removeAt(row);
    endRemoveRows();
    emit countChanged();
    bump();
}

void RowModel::move(int from, int to)
{
    if (from < 0 || from >= count() || to < 0 || to >= count() || from == to)
        return;
    // beginMoveRows wants the destination as "insert before" in the old order.
    beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);
    m_rows.move(from, to);
    endMoveRows();
    bump();
}

void RowModel::bump()
{
    ++m_revision;
    emit revisionChanged();
}
