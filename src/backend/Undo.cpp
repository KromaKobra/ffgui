#include "Undo.h"

Undo::Undo(QUndoStack *stack, QObject *parent)
    : QObject(parent)
    , m_stack(stack)
{
    connect(stack, &QUndoStack::indexChanged, this, &Undo::changed);
    connect(stack, &QUndoStack::cleanChanged, this, &Undo::changed);
    connect(stack, &QUndoStack::undoTextChanged, this, &Undo::changed);
    connect(stack, &QUndoStack::redoTextChanged, this, &Undo::changed);
}
