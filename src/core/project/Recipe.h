#pragma once
#include <QVariantMap>
#include <QList>
#include <QJsonObject>

namespace ffgui
{
// Nodes use the same durable fields as the row projection. Links are explicit
// so a linear recipe can grow into a branched graph without a format change.
struct Recipe {
    QString name = QStringLiteral("Untitled project");
    QList<QVariantMap> inputs, nodes, links, outputs, markers;
    QVariantMap metadata, edits;
};

class RecipeJson
{
public:
    static QJsonObject encode(const Recipe &recipe);
    static bool decode(const QJsonObject &json, Recipe *recipe, QString *error);
    static bool load(const QString &path, Recipe *recipe, QString *error);
    static bool save(const QString &path, const Recipe &recipe, QString *error);
};
} // namespace ffgui
