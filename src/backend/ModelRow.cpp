#include "ModelRow.h"

ModelRow::ModelRow(QObject *parent)
    : QObject(parent)
{
}

void ModelRow::setModel(QAbstractItemModel *model)
{
    if (m_model == model)
        return;
    for (const auto &c : std::as_const(m_connections))
        disconnect(c);
    m_connections.clear();
    m_model = model;
    if (model) {
        auto refresh = [this] { this->refresh(); };
        m_connections << connect(model, &QAbstractItemModel::dataChanged, this,
                                 [this](const QModelIndex &a, const QModelIndex &b) {
                                     if (m_row >= a.row() && m_row <= b.row())
                                         this->refresh();
                                 })
                      << connect(model, &QAbstractItemModel::rowsInserted, this, refresh)
                      << connect(model, &QAbstractItemModel::rowsRemoved, this, refresh)
                      << connect(model, &QAbstractItemModel::rowsMoved, this, refresh)
                      << connect(model, &QAbstractItemModel::modelReset, this, refresh)
                      << connect(model, &QAbstractItemModel::layoutChanged, this, refresh);
    }
    emit modelChanged();
    refresh();
}

void ModelRow::setRow(int row)
{
    if (m_row == row)
        return;
    m_row = row;
    emit rowChanged();
    refresh();
}

void ModelRow::refresh()
{
    QVariantMap values;
    bool valid = false;
    if (m_model && m_row >= 0 && m_row < m_model->rowCount()) {
        valid = true;
        const QModelIndex index = m_model->index(m_row, 0);
        const auto names = m_model->roleNames();
        for (auto it = names.begin(); it != names.end(); ++it)
            values.insert(QString::fromUtf8(it.value()), m_model->data(index, it.key()));
    }
    if (values == m_values && valid == m_valid)
        return;
    m_values = values;
    m_valid = valid;
    emit valuesChanged();
}
