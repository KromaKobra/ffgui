#pragma once
#include <QStringList>
namespace ffgui
{
// POSIX quoting for argument lists, without expansion or shell execution.
QStringList splitCommand(const QString &text, QString *error = nullptr);
} // namespace ffgui
