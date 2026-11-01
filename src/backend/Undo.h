#pragma once

#include <QQmlEngine>
#include <QUndoStack>

// Undo and redo for every project change (design principle 7). The stack
// belongs to the project; this is its face for the header bar and shortcuts.
class Undo : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by App")

    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    Q_PROPERTY(QString undoText READ undoText NOTIFY changed)
    Q_PROPERTY(QString redoText READ redoText NOTIFY changed)
    Q_PROPERTY(bool clean READ clean NOTIFY changed)

public:
    explicit Undo(QUndoStack *stack, QObject *parent = nullptr);

    bool canUndo() const { return m_stack->canUndo(); }
    bool canRedo() const { return m_stack->canRedo(); }
    QString undoText() const { return m_stack->undoText(); }
    QString redoText() const { return m_stack->redoText(); }
    bool clean() const { return m_stack->isClean(); }

    Q_INVOKABLE void undo() { m_stack->undo(); }
    Q_INVOKABLE void redo() { m_stack->redo(); }

signals:
    void changed();

private:
    QUndoStack *m_stack;
};
