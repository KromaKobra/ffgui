#include "ShellQuote.h"

#include <QRegularExpression>

namespace ffgui {

QString shellQuote(const QString &arg)
{
    if (arg.isEmpty())
        return QStringLiteral("''");
    static const QRegularExpression unsafe(QStringLiteral(R"([\s'"|;&()<>\[\]*?$`\\!#~{}])"));
    if (!unsafe.match(arg).hasMatch())
        return arg;
    static const QRegularExpression needsSingle(QStringLiteral(R"([$`\\"!])"));
    if (!needsSingle.match(arg).hasMatch())
        return u'"' + arg + u'"';
    QString s = arg;
    s.replace(u'\'', QStringLiteral("'\\''"));
    return u'\'' + s + u'\'';
}

QString shellJoin(const QStringList &args)
{
    QStringList quoted;
    quoted.reserve(args.size());
    for (const QString &a : args)
        quoted << shellQuote(a);
    return quoted.join(u' ');
}

} // namespace ffgui
