#pragma once
#include <QUndoCommand>
#include <QVariantMap>
#include <QDateTime>
#include <functional>

namespace ffgui
{
class SnapshotCommand : public QUndoCommand
{
public:
    SnapshotCommand(const QString &label, const QString &key, QVariantMap before, QVariantMap after,
                    std::function<void(const QVariantMap &)> apply)
        : QUndoCommand(label), m_key(key), m_before(std::move(before)), m_after(std::move(after)),
          m_apply(std::move(apply))
    {
    }
    int id() const override { return m_key.isEmpty() ? -1 : 1; }
    bool mergeWith(const QUndoCommand *other) override
    {
        const auto *next = dynamic_cast<const SnapshotCommand *>(other);
        if (!next || next->m_key != m_key || next->m_time - m_time > 500 || m_after != next->m_before)
            return false;
        m_after = next->m_after;
        m_time = next->m_time;
        return true;
    }
    void undo() override { m_apply(m_before); }
    void redo() override
    {
        if (m_first)
            m_first = false;
        else
            m_apply(m_after);
    }

private:
    QString m_key;
    QVariantMap m_before, m_after;
    std::function<void(const QVariantMap &)> m_apply;
    qint64 m_time = QDateTime::currentMSecsSinceEpoch();
    bool m_first = true;
};
} // namespace ffgui
