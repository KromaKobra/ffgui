#include "Command.h"

Command::Command(QObject *parent)
    : QObject(parent)
{
}

void Command::set(const QStringList &args, const QStringList &groups)
{
    m_args = args;
    m_text = groups.join(u' ');
    m_multiline = groups.join(QStringLiteral(" \\\n    "));
    m_placeholder.clear();
    emit changed();
}

void Command::clear(const QString &placeholder)
{
    if (m_args.isEmpty() && m_placeholder == placeholder)
        return;
    m_args.clear();
    m_text.clear();
    m_multiline.clear();
    m_placeholder = placeholder;
    emit changed();
}
