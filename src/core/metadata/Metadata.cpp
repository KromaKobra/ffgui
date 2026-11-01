#include "Metadata.h"
namespace ffgui
{
namespace
{
QString escape(QString s)
{
    s.replace(u'\\', QStringLiteral("\\\\"));
    for (QChar c : QStringLiteral("=;#\n"))
        s.replace(c, QString(u'\\') + c);
    return s;
}
} // namespace
QByteArray ffmetadata(const QVariantMap &tags, const QVariantList &chapters)
{
    QString text = QStringLiteral(";FFMETADATA1\n");
    for (auto it = tags.begin(); it != tags.end(); ++it)
        text += escape(it.key()) + u'=' + escape(it.value().toString()) + u'\n';
    for (const auto &entry : chapters) {
        const auto c = entry.toMap();
        text += QStringLiteral("[CHAPTER]\nTIMEBASE=1/1000\nSTART=%1\nEND=%2\ntitle=%3\n")
                    .arg(qRound64(c.value("start").toDouble() * 1000))
                    .arg(qRound64(c.value("end").toDouble() * 1000))
                    .arg(escape(c.value("title").toString()));
    }
    return text.toUtf8();
}
} // namespace ffgui
