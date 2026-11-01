#include "CommandLine.h"
namespace ffgui
{
QStringList splitCommand(const QString &text, QString *error)
{
    QStringList args;
    QString word;
    QChar quote;
    bool escaped = false, started = false;
    auto fail = [&](const QString &message) {
        if (error)
            *error = message;
        return QStringList();
    };
    for (QChar c : text) {
        if (escaped) {
            if (c != u'\n') {
                if (quote == u'\"' && !QStringLiteral("$`\\\"").contains(c))
                    word += u'\\';
                word += c;
                started = true;
            }
            escaped = false;
            continue;
        }
        if (c == u'\\' && quote != u'\'') {
            escaped = true;
            continue;
        }
        if (!quote.isNull()) {
            if (c == quote)
                quote = {};
            else
                word += c;
            continue;
        }
        if (c == u'\'' || c == u'"') {
            quote = c;
            started = true;
            continue;
        }
        if (c.isSpace()) {
            if (started) {
                args << word;
                word.clear();
                started = false;
            }
            continue;
        }
        if (QStringLiteral("|;&<>`").contains(c))
            return fail(QStringLiteral("Shell operators are not supported; enter one FFmpeg argument list"));
        word += c;
        started = true;
    }
    if (escaped || !quote.isNull())
        return fail(QStringLiteral("Unclosed quote or escape in command"));
    if (started)
        args << word;
    return args;
}
} // namespace ffgui
