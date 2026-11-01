#pragma once
#include <QVariantMap>
namespace ffgui
{
class Transcript
{
  public:
    static bool parse(const QByteArray &data, const QString &format, QList<QVariantMap> *segments,
                      QString *error);
    static QByteArray encode(const QList<QVariantMap> &segments, const QString &format);
    static bool valid(const QVariantMap &segment);
};
} // namespace ffgui
