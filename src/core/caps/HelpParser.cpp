#include "HelpParser.h"

#include <QRegularExpression>
#include <cmath>

namespace ffgui
{
namespace
{
double number(QString text, double fallback)
{
    if (text == u"INT_MAX")
        return 2147483647;
    if (text == u"INT_MIN")
        return -2147483648.0;
    if (text == u"I64_MAX")
        return 9.223372036854776e18;
    if (text == u"I64_MIN")
        return -9.223372036854776e18;
    if (text == u"FLT_MAX" || text == u"DBL_MAX")
        return 1e12;
    if (text == u"-FLT_MAX" || text == u"-DBL_MAX")
        return -1e12;
    bool ok = false;
    const double value = text.toDouble(&ok);
    return ok && std::isfinite(value) ? value : fallback;
}
} // namespace

QList<OptionSpec> HelpParser::parse(const QString &text)
{
    static const QRegularExpression option(QStringLiteral(R"(^\s{2,}([^\s]+)\s+<([^>]+)>\s+([.A-Z]{8,})\s*(.*)$)"));
    static const QRegularExpression constant(
        QStringLiteral(R"(^\s{4,}(\S+)\s+(-?\d+|0x[0-9a-fA-F]+)\s+[.A-Z]{8,}.*$)"));
    static const QRegularExpression range(QStringLiteral(R"(\(from (\S+) to (\S+)\))"));
    static const QRegularExpression def(QStringLiteral(R"re(\(default ("[^"]*"|[^)]*)\))re"));
    QList<OptionSpec> result;
    for (const QString &line : text.split(u'\n')) {
        const auto match = option.match(line);
        if (!match.hasMatch()) {
            const auto choice = constant.match(line);
            if (!result.isEmpty() && choice.hasMatch() && result.last().type != u"string") {
                auto &o = result.last();
                o.type = QStringLiteral("choice");
                o.choices << choice.captured(1);
                if (o.defaultValue.toString() == choice.captured(2))
                    o.defaultValue = choice.captured(1);
            }
            continue;
        }
        OptionSpec o;
        o.key = match.captured(1);
        o.label = o.key;
        o.help = match.captured(4);
        o.runtime = match.captured(3).contains(u'T');
        const QString native = match.captured(2);
        const bool integer = native == u"int" || native == u"int64";
        o.type = native == u"boolean"                                   ? QStringLiteral("bool")
                 : integer || native == u"float" || native == u"double" ? QStringLiteral("number")
                                                                        : QStringLiteral("string");
        o.step = integer ? 1 : 0.01;
        const auto bounds = range.match(o.help);
        if (bounds.hasMatch()) {
            o.min = number(bounds.captured(1), -1e12);
            o.max = number(bounds.captured(2), 1e12);
        } else if (o.type == u"number") {
            o.min = -1e12;
            o.max = 1e12;
        }
        const auto d = def.match(o.help);
        QString value = d.captured(1);
        if (value.startsWith(u'"') && value.endsWith(u'"'))
            value = value.mid(1, value.size() - 2);
        if (o.type == u"bool")
            o.defaultValue = value == u"true" || value == u"1";
        else if (o.type == u"number") {
            bool ok = false;
            const double numeric = value.toDouble(&ok);
            o.defaultValue = ok ? QVariant(numeric) : QVariant(value);
        } else
            o.defaultValue = value;
        result << o;
    }
    for (auto &o : result) {
        if (o.type == u"number") {
            const double fallback = o.min <= 0 && o.max >= 0 ? 0 : o.min;
            o.defaultValue = number(o.defaultValue.toString(), fallback);
        }
    }
    return result;
}
} // namespace ffgui
