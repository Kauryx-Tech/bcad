#include "io/CadastreStyleProvider.h"

#include "Templates.h"

#include "bcad/plugin/PluginRegistry.h"
#include "bcad/plugin/StyleProvider.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QString>

#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace bcad::cadastre {

namespace {

std::optional<std::string> readJsonValue(const QJsonObject& obj, const char* key) {
    const QJsonValue val = obj.value(QLatin1String(key));
    if (!val.isString()) return std::nullopt;
    const QString str = val.toString();
    if (str.isEmpty()) return std::nullopt;
    return str.toStdString();
}

std::optional<int> readJsonInt(const QJsonObject& obj, const char* key) {
    const QJsonValue val = obj.value(QLatin1String(key));
    if (!val.isDouble()) return std::nullopt;
    return val.toInt();
}

std::optional<double> readJsonDouble(const QJsonObject& obj, const char* key) {
    const QJsonValue val = obj.value(QLatin1String(key));
    if (!val.isDouble()) return std::nullopt;
    return val.toDouble();
}

std::optional<bool> readJsonBool(const QJsonObject& obj, const char* key) {
    const QJsonValue val = obj.value(QLatin1String(key));
    if (!val.isBool()) return std::nullopt;
    return val.toBool();
}

std::vector<double> readJsonArrayDouble(const QJsonObject& obj, const char* key) {
    std::vector<double> result;
    const QJsonValue val = obj.value(QLatin1String(key));
    if (!val.isArray()) return result;
    for (const QJsonValue& v : val.toArray()) {
        if (v.isDouble()) result.push_back(v.toDouble());
    }
    return result;
}

std::string readHexColor(const QJsonObject& obj, const char* key, const std::string& def) {
    auto v = readJsonValue(obj, key);
    if (!v) return def;
    // S'assurer que la couleur commence par #
    if (!v->empty() && v->front() != '#') return "#" + *v;
    return *v;
}

} // namespace

std::vector<bcad::plugin::LayerStyle> CadastreStyleProvider::layerStyles() const {
    std::vector<plugin::LayerStyle> styles;
    CadastreTemplates gabarit = loadCadastreTemplates(/* registry vide pour défaut */ plugin::PluginRegistry{});
    // Si un gabarit a des calques déclarés, on les utilise ; sinon on tombe sur les défauts
    // Pour l'instant, on retourne les défauts du module (le gabarit TOGO n'a pas de calques spécifiques)
    return {
        {"cadastre.parcels", "Parcelles", 3, "CONTINUOUS", 0.25, true, false, true},
        {"cadastre.boundaries", "Limites", 1, "CONTINUOUS", 0.18, true, false, true},
        {"cadastre.survey_marks", "Bornes", 5, "CONTINUOUS", 0.18, true, false, true},
        {"cadastre.easements", "Servitudes", 6, "CONTINUOUS", 0.18, true, false, true},
    };
}

std::vector<bcad::plugin::PlotStyle> CadastreStyleProvider::plotStyles() const {
    // Lecture du fichier plot_styles.json si présent
    // Pour l'instant, valeurs par défaut du module
    return {
        {"cadastre.parcel", "#202020", 0.25, "none", {}},
        {"cadastre.boundary", "#505050", 0.18, "none", {}},
        {"cadastre.easement", "#406080", 0.18, "none", {2.0, 1.0}},
    };
}

std::vector<bcad::plugin::TextStyle> CadastreStyleProvider::textStyles() const {
    return {
        {"cadastre.parcel_label", "Sans", 2.5, "#202020"},
        {"cadastre.reference_label", "Sans", 2.0, "#505050"},
    };
}

} // namespace bcad::cadastre