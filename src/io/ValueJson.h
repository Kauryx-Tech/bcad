#pragma once

// Le codage `value_json` d'une valeur typee. Interne a src/io, comme JsonText.h.
//
// Il etait ecrit dans l'espace anonyme de Database.cpp, ou il ne servait qu'a la
// table `entity_properties`. Il en sort sans changer de forme pour une raison :
// une valeur n'est pas que celle d'une entite. Les champs declaratifs d'un meuble
// de mise en page portent la meme exigence — un reel de 1250.42 doit revenir en
// Double et non en chaine — et ne doivent surtout pas se donner un second
// codage a versionner. Une seule grammaire, un seul lecteur.

#include "JsonText.h"

#include "bcad/geometry/Point.h"
#include "bcad/properties/PropertyMap.h"
#include "bcad/properties/PropertyTypes.h"

#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace bcad::io {

namespace value_json_detail {

inline bool consume(std::string_view& text, std::string_view prefix) {
    if (text.size() < prefix.size() || text.substr(0, prefix.size()) != prefix) return false;
    text = text.substr(prefix.size());
    return true;
}

// Nombre jusqu'au délimiteur donné (':' n'existe pas ici, on cherche '}' ou ',').
inline std::optional<double> numberUntil(std::string_view text, char stop) {
    const std::size_t end = text.find(stop);
    if (end == std::string_view::npos) return std::nullopt;
    double value = 0.0;
    if (!parseJsonNumber(text.substr(0, end), value)) return std::nullopt;
    return value;
}

// Liste des libellés d'un enum : « "a","b" » ou vide.
inline std::vector<std::string> labelsOf(std::string_view text) {
    std::vector<std::string> out;
    while (!text.empty()) {
        if (!consume(text, "\"")) return out;
        const std::size_t quote = text.find('"');
        if (quote == std::string_view::npos) return out;
        out.push_back(jsonUnescape(std::string(text.substr(0, quote))));
        text = quote + 1 < text.size() ? text.substr(quote + 1) : std::string_view{};
        if (!consume(text, ",")) break;
    }
    return out;
}

} // namespace value_json_detail

// Le type est une donnée de la ligne, pas une supposition du lecteur : un réel
// de 1250.42 doit revenir en Double et non en chaîne ni en entier. Le domaine
// d'une énumération voyage avec elle (`values`) : c'est la seule façon de
// reinjecter un index sans le mettre hors domaine.
inline std::string encodeValue(const properties::Property& prop) {
    std::ostringstream ss;
    ss.precision(17);
    ss << "{\"type\":\"";
    switch (prop.type()) {
        case properties::PropertyType::Double:
            return (ss << "double\",\"value\":" << prop.asDouble() << "}", ss.str());
        case properties::PropertyType::Int:
            return (ss << "int\",\"value\":" << prop.asInt() << "}", ss.str());
        case properties::PropertyType::Bool:
            return (ss << "bool\",\"value\":" << (prop.asBool() ? "true" : "false") << "}", ss.str());
        case properties::PropertyType::Color: {
            const auto c = prop.asColor();
            return (ss << "color\",\"value\":{\"r\":" << c.r << ",\"g\":" << c.g
                       << ",\"b\":" << c.b << ",\"a\":" << c.a << "}}", ss.str());
        }
        case properties::PropertyType::Enum: {
            ss << "enum\",\"value\":" << prop.asEnum() << ",\"values\":[";
            const auto& values = prop.enumValues();
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (i) ss << ',';
                ss << jsonString(values[i]);
            }
            return (ss << "]}", ss.str());
        }
        case properties::PropertyType::String:
            return (ss << "string\",\"value\":" << jsonString(prop.asString()) << "}", ss.str());
    }
    return {};
}

struct DecodedValue {
    properties::PropertyType type = properties::PropertyType::String;
    properties::PropertyValue value;
    std::vector<std::string> enumValues;
    int enumIndex = 0;
};

// Lecture stricte : la grammaire est exactement celle d'encodeValue. Les
// chaînes sont prises par bornes (première quote ouvrante, dernière quote
// fermante), jamais par mot-clé, pour qu'une valeur contenant « "value": » ne
// devienne pas une structure. Un texte qui ne colle pas est une donnée invalide,
// pas un cas à deviner.
inline std::optional<DecodedValue> decodeValue(std::string_view text) {
    namespace d = value_json_detail;
    using namespace properties;

    if (!d::consume(text, "{\"type\":\"")) return std::nullopt;
    const std::size_t tagEnd = text.find('"');
    if (tagEnd == std::string_view::npos) return std::nullopt;
    const std::string tag(text.substr(0, tagEnd));
    text = tagEnd + 1 < text.size() ? text.substr(tagEnd + 1) : std::string_view{};
    if (!d::consume(text, ",\"value\":")) return std::nullopt;

    DecodedValue out;
    if (tag == "double") {
        const auto value = d::numberUntil(text, '}');
        if (!value) return std::nullopt;
        out.type = PropertyType::Double;
        out.value = *value;
        return out;
    }
    if (tag == "int") {
        const auto value = d::numberUntil(text, '}');
        if (!value) return std::nullopt;
        out.type = PropertyType::Int;
        out.value = static_cast<int>(*value);
        return out;
    }
    if (tag == "bool") {
        if (d::consume(text, "true}")) out.value = true;
        else if (d::consume(text, "false}")) out.value = false;
        else return std::nullopt;
        out.type = PropertyType::Bool;
        return out;
    }
    if (tag == "string") {
        if (!d::consume(text, "\"")) return std::nullopt;
        const std::size_t close = text.rfind('"');
        if (close == std::string_view::npos || close + 1 >= text.size()) return std::nullopt;
        if (text.substr(close + 1) != "}") return std::nullopt;
        out.type = PropertyType::String;
        out.value = jsonUnescape(std::string(text.substr(0, close)));
        return out;
    }
    if (tag == "color") {
        if (!d::consume(text, "{\"r\":")) return std::nullopt;
        std::optional<double> r = d::numberUntil(text, ',');
        text = text.substr(text.find(',') + 1);
        if (!d::consume(text, "\"g\":")) return std::nullopt;
        std::optional<double> g = d::numberUntil(text, ',');
        text = text.substr(text.find(',') + 1);
        if (!d::consume(text, "\"b\":")) return std::nullopt;
        std::optional<double> b = d::numberUntil(text, ',');
        text = text.substr(text.find(',') + 1);
        if (!d::consume(text, "\"a\":")) return std::nullopt;
        std::optional<double> a = d::numberUntil(text, '}');
        if (!r || !g || !b || !a) return std::nullopt;
        out.type = PropertyType::Color;
        out.value = geom::Color{ static_cast<float>(*r), static_cast<float>(*g),
                                 static_cast<float>(*b), static_cast<float>(*a) };
        return out;
    }
    if (tag == "enum") {
        const auto index = d::numberUntil(text, ',');
        if (!index) return std::nullopt;
        text = text.substr(text.find(',') + 1);
        if (!d::consume(text, "\"values\":[")) return std::nullopt;
        const std::size_t close = text.rfind("]}");
        if (close == std::string_view::npos) return std::nullopt;
        if (text.substr(close) != "]}" && text.substr(close + 1) != "}") return std::nullopt;
        out.type = PropertyType::Enum;
        out.enumIndex = static_cast<int>(*index);
        out.value = EnumIndex(out.enumIndex);
        out.enumValues = d::labelsOf(text.substr(0, close));
        return out;
    }
    return std::nullopt;
}

// Règle de précédence, la même qu'en v1 où la table gagnait sur les paramètres,
// et la même qu'en DXF où la XDATA gagnait sur la géométrie :
//
//   - clé absente            : créée avec le type et la valeur du fichier ;
//   - clé déclarée, type concordant : la valeur du fichier gagne ;
//   - clé déclarée, type différent  : le schéma du module gagne. Un fichier ne
//     peut pas imposer une chaîne à une propriété que le module relit comme un
//     Enum (getEnum lèverait bad_variant_access) ; le module corrige le fichier
//     en réécrivant la propriété.
//
// Une propriété déclarée en lecture seule n'est pas touchée : sa valeur est
// recalculée par le module, pas archivée.
inline void applyStoredValue(properties::PropertyMap& props, const std::string& key,
                            const DecodedValue& stored) {
    using namespace properties;

    if (const Property* existing = props.find(key)) {
        if (existing->type() != stored.type || existing->isReadOnly()) return;
    }

    switch (stored.type) {
        case PropertyType::Double: props.setDouble(key, std::get<double>(stored.value)); break;
        case PropertyType::Int: props.setInt(key, std::get<int>(stored.value)); break;
        case PropertyType::String: props.setString(key, std::get<std::string>(stored.value)); break;
        case PropertyType::Bool: props.setBool(key, std::get<bool>(stored.value)); break;
        case PropertyType::Color: props.setColor(key, std::get<geom::Color>(stored.value)); break;
        case PropertyType::Enum: {
            Property* prop = props.get(key);
            if (!prop || prop->type() != PropertyType::Enum) {
                prop = props.addEnum(key, 0, stored.enumValues);
            }
            try {
                prop->setFromEnum(stored.enumIndex);
            } catch (const std::out_of_range&) {
                // Index reçu d'un domaine plus long que celui déclaré ici : on
                // garde la valeur de départ, jamais un index hors domaine.
            }
            break;
        }
    }
}

} // namespace bcad::io
