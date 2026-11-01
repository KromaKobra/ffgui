#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QVariantMap>

// A list model whose rows are maps with a fixed set of keys (the roles).
// Updates are fine-grained: update() emits dataChanged for the roles that
// actually changed, so delegates rebind only what moved. QML delegates read
// the roles as required properties; get() and ModelRow read whole rows.
class RowModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Models are owned by App")

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    // Bumped on every change, for bindings that read rows through get().
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    explicit RowModel(const QStringList &roles, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return int(m_rows.size()); }
    int revision() const { return m_revision; }

    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE QVariant value(int row, const QString &role) const;
    // The first row whose `role` equals `value`, or -1.
    Q_INVOKABLE int find(const QString &role, const QVariant &value) const;

    const QStringList &roles() const { return m_roles; }
    const QList<QVariantMap> &rows() const { return m_rows; }

    void setRows(const QList<QVariantMap> &rows);
    void insertAt(int row, const QVariantMap &values);
    void append(const QVariantMap &values) { insertAt(count(), values); }
    void update(int row, const QVariantMap &changes);
    void removeAt(int row);
    void move(int from, int to);
    void clear() { setRows({}); }

signals:
    void countChanged();
    void revisionChanged();

private:
    QVariantMap normalized(const QVariantMap &values) const;
    void bump();

    QStringList m_roles;
    QList<QVariantMap> m_rows;
    int m_revision = 0;
};
