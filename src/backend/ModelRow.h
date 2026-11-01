#pragma once

#include <QAbstractItemModel>
#include <QPointer>
#include <QQmlEngine>
#include <QVariantMap>

// One row of a model as a map that updates when the row does:
//
//     ModelRow { id: step; model: App.project.steps; row: App.selection.index }
//     Label { text: step.values.name }
//
// For property sheets that show a single item of a list model.
class ModelRow : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QAbstractItemModel *model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(int row READ row WRITE setRow NOTIFY rowChanged)
    Q_PROPERTY(QVariantMap values READ values NOTIFY valuesChanged)
    Q_PROPERTY(bool valid READ valid NOTIFY valuesChanged)

public:
    explicit ModelRow(QObject *parent = nullptr);

    QAbstractItemModel *model() const { return m_model; }
    void setModel(QAbstractItemModel *model);
    int row() const { return m_row; }
    void setRow(int row);
    QVariantMap values() const { return m_values; }
    bool valid() const { return m_valid; }

signals:
    void modelChanged();
    void rowChanged();
    void valuesChanged();

private:
    void refresh();

    QPointer<QAbstractItemModel> m_model;
    int m_row = -1;
    QVariantMap m_values;
    bool m_valid = false;
    QList<QMetaObject::Connection> m_connections;
};
