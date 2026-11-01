#pragma once

#include <QStringList>

namespace ffgui {

// Quotes one argument for display in a POSIX shell: bare when safe, "double"
// when it has no $ ` \ ", 'single' otherwise. Commands always run from the
// argument list; this is only for showing and copying them.
QString shellQuote(const QString &arg);
QString shellJoin(const QStringList &args);

} // namespace ffgui
