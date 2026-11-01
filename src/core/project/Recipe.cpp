#include "Recipe.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>

namespace ffgui
{
namespace
{
QJsonArray array(const QList<QVariantMap> &rows)
{
    QJsonArray a;
    for (const auto &row : rows)
        a.append(QJsonObject::fromVariantMap(row));
    return a;
}
} // namespace
QJsonObject RecipeJson::encode(const Recipe &r)
{
    return {{"version", 1},
            {"name", r.name},
            {"inputs", array(r.inputs)},
            {"nodes", array(r.nodes)},
            {"links", array(r.links)},
            {"outputs", array(r.outputs)},
            {"markers", array(r.markers)},
            {"metadata", QJsonObject::fromVariantMap(r.metadata)}};
}

bool RecipeJson::decode(const QJsonObject &json, Recipe *recipe, QString *error)
{
    auto fail = [&](const QString &text) {
        if (error)
            *error = text;
        return false;
    };
    if (json.value(u"version").toInt() != 1)
        return fail(QStringLiteral("Unsupported project version"));
    Recipe r;
    r.name = json.value(u"name").toString(r.name);
    for (const auto &entry : {std::pair{"inputs", &r.inputs},
                              {"nodes", &r.nodes},
                              {"links", &r.links},
                              {"outputs", &r.outputs},
                              {"markers", &r.markers}}) {
        const auto v = json.value(QLatin1String(entry.first));
        if (!v.isArray())
            return fail(QStringLiteral("Invalid project list: %1").arg(QLatin1String(entry.first)));
        QSet<QString> ids;
        for (const auto &item : v.toArray()) {
            if (!item.isObject())
                return fail(QStringLiteral("Project entries must be objects"));
            const auto row = item.toObject().toVariantMap();
            if (entry.second != &r.links && entry.second != &r.markers) {
                const QString id = row.value(QStringLiteral("uid")).toString();
                if (id.isEmpty() || ids.contains(id))
                    return fail(QStringLiteral("Missing or duplicate project id"));
                ids.insert(id);
            }
            entry.second->append(row);
        }
    }
    QSet<QString> nodes;
    for (const auto &n : r.nodes) {
        if (n.value(QStringLiteral("type")).toString().isEmpty())
            return fail(QStringLiteral("Missing step type"));
        nodes.insert(n.value(QStringLiteral("uid")).toString());
    }
    for (const auto &link : r.links)
        if (!nodes.contains(link.value(QStringLiteral("from")).toString()) ||
            !nodes.contains(link.value(QStringLiteral("to")).toString()))
            return fail(QStringLiteral("Graph link refers to a missing node"));
    r.metadata = json.value(u"metadata").toObject().toVariantMap();
    *recipe = r;
    return true;
}

bool RecipeJson::load(const QString &path, Recipe *recipe, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parse);
    if (!doc.isObject()) {
        if (error)
            *error = parse.errorString();
        return false;
    }
    return decode(doc.object(), recipe, error);
}

bool RecipeJson::save(const QString &path, const Recipe &recipe, QString *error)
{
    QSaveFile file(path);
    const QByteArray data = QJsonDocument(encode(recipe)).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}
} // namespace ffgui
