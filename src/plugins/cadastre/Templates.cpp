// Lecture des gabarits du module (C3). Le fichier est un donnee installee, pas
// du code : seule la valeur change, la regle reste ecrite ici.

#include "Templates.h"

#include "bcad/plugin/PluginRegistry.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <utility>

namespace bcad::cadastre {

namespace {

// Version de gabarit comprise par ce module. Un gabarit d'une version
// inconnue n'est pas applique : ses cles pourraient signifier autre chose.
constexpr int kSchemaVersion = 1;

void replaceIfPresent(const QJsonObject& object, const char* key, std::string& target) {
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isString()) return;
    const QString text = value.toString();
    if (text.isEmpty()) return;
    target = text.toStdString();
}

} // namespace

CadastreTemplates loadCadastreTemplates(const plugin::PluginRegistry& registry,
                                        const std::string& profile) {
    CadastreTemplates templates;
    const std::string path =
        registry.resolveDataFile("cadastre/templates/" + profile + ".json");
    if (path.empty()) return templates;

    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly)) return templates;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return templates;

    const QJsonObject root = document.object();
    if (root.value(QLatin1String("schema_version")).toInt() != kSchemaVersion)
        return templates;

    replaceIfPresent(root.value(QLatin1String("parcel_identifier")).toObject(),
                     "section_pattern", templates.sectionPattern);
    replaceIfPresent(root.value(QLatin1String("parcel_identifier")).toObject(),
                     "number_pattern", templates.numberPattern);
    templates.source = path;
    return templates;
}

} // namespace bcad::cadastre
