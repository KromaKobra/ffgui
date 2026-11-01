#include "Transcript.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
namespace ffgui
{
namespace
{
double time(const QString &text)
{
    const auto parts = text.trimmed().split(u':');
    if (parts.size() != 3)
        return -1;
    bool ok = false;
    const double sec = QString(parts[2]).replace(u',', u'.').toDouble(&ok);
    return ok ? parts[0].toDouble() * 3600 + parts[1].toDouble() * 60 + sec : -1;
}
QString stamp(double seconds)
{
    const qint64 ms = qRound64(seconds * 1000);
    return QStringLiteral("%1:%2:%3,%4")
        .arg(ms / 3600000, 2, 10, u'0')
        .arg(ms / 60000 % 60, 2, 10, u'0')
        .arg(ms / 1000 % 60, 2, 10, u'0')
        .arg(ms % 1000, 3, 10, u'0');
}
} // namespace
bool Transcript::valid(const QVariantMap &s)
{
    bool a = false, b = false;
    const double start = s.value("start").toDouble(&a), end = s.value("end").toDouble(&b);
    return a && b && std::isfinite(start) && std::isfinite(end) && start >= 0 && end > start &&
           s.value("text").metaType().id() == QMetaType::QString;
}
bool Transcript::parse(const QByteArray &data, const QString &format, QList<QVariantMap> *segments,
                       QString *error)
{
    QList<QVariantMap> parsed;
    auto fail = [&](const QString &s) {
        if (error)
            *error = s;
        return false;
    };
    QString text = QString::fromUtf8(data);
    text.remove(QChar(0xfeff));
    text.replace("\r\n", "\n");
    if (format == u"json") {
        const auto doc = QJsonDocument::fromJson(data);
        if (!doc.isArray())
            return fail(QStringLiteral("Expected a JSON array of transcript segments"));
        for (const auto &v : doc.array()) {
            if (!v.isObject())
                return fail(QStringLiteral("Invalid transcript entry"));
            parsed << v.toObject().toVariantMap();
        }
    } else if (format == u"srt") {
        const auto blocks = text.split(QRegularExpression(QStringLiteral("\n[ \t]*\n")), Qt::SkipEmptyParts);
        static const QRegularExpression timing(
            QStringLiteral(R"(^\s*(\d+:\d{2}:\d{2}[,.]\d+)\s*-->\s*(\d+:\d{2}:\d{2}[,.]\d+).*$)"));
        for (const auto &block : blocks) {
            auto lines = block.trimmed().split(u'\n');
            if (lines.size() < 2)
                return fail(QStringLiteral("Invalid SRT block"));
            if (!lines[0].contains("-->"))
                lines.removeFirst();
            if (lines.isEmpty())
                return fail(QStringLiteral("Missing SRT timestamp"));
            const auto match = timing.match(lines.takeFirst());
            if (!match.hasMatch())
                return fail(QStringLiteral("Invalid SRT timestamp"));
            parsed << QVariantMap{{"start", time(match.captured(1))},
                                  {"end", time(match.captured(2))},
                                  {"text", lines.join(u'\n')}};
        }
    } else if (format == u"txt") {
        double at = 0;
        for (const auto &line : text.split(u'\n'))
            if (!line.trimmed().isEmpty()) {
                parsed << QVariantMap{{"start", at}, {"end", at + 5}, {"text", line}};
                at += 5;
            }
    } else
        return fail(QStringLiteral("Use SRT, TXT or JSON"));
    if (parsed.size() > 100000)
        return fail(QStringLiteral("Transcript has too many segments"));
    for (const auto &s : parsed)
        if (!valid(s))
            return fail(
                QStringLiteral("Segments need finite start/end times with end after start, and text"));
    std::stable_sort(parsed.begin(), parsed.end(), [](const auto &a, const auto &b) {
        return a.value("start").toDouble() < b.value("start").toDouble();
    });
    *segments = parsed;
    return true;
}
QByteArray Transcript::encode(const QList<QVariantMap> &segments, const QString &format)
{
    if (format == u"json") {
        QJsonArray array;
        for (const auto &s : segments)
            array << QJsonObject::fromVariantMap(s);
        return QJsonDocument(array).toJson();
    }
    QString result;
    int index = 0;
    for (const auto &s : segments) {
        if (format == u"srt")
            result += QString::number(++index) + u'\n' + stamp(s.value("start").toDouble()) + " --> " +
                      stamp(s.value("end").toDouble()) + u'\n';
        result += s.value("text").toString() + u'\n';
        if (format == u"srt")
            result += u'\n';
    }
    return result.toUtf8();
}
} // namespace ffgui
