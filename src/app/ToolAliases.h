// Alias de commande clavier (C-01) : la seule table. Taper l'alias au repos
// dans la ligne de commande (ou directement dans le canevas) lance l'outil,
// comme les alias d'AutoCAD. Header-only : le canevas (et son test, qui ne
// lie pas la fenetre) comme la fenetre lisent la meme table.

#pragma once

#include "Viewport.h"

#include <QRegularExpression>
#include <QString>

#include <optional>

namespace bcad::app {

struct ToolAlias {
    const char* alias;
    ToolMode mode;
};

inline constexpr ToolAlias kToolAliases[] = {
    // Dessin
    {"L", ToolMode::Line},        {"PL", ToolMode::Polyline},     {"C", ToolMode::Circle},
    {"A", ToolMode::Arc3P},         {"REC", ToolMode::Rectangle},   {"POL", ToolMode::Polygon},
    {"PO", ToolMode::Point},      {"T", ToolMode::Text},          {"H", ToolMode::Hatch},
    // Modification
    {"M", ToolMode::Move},        {"CO", ToolMode::Copy},         {"RO", ToolMode::Rotate},
    {"SC", ToolMode::Scale},      {"MI", ToolMode::Mirror},       {"TR", ToolMode::Trim},
    {"EX", ToolMode::Extend},     {"BR", ToolMode::Break},        {"O", ToolMode::Offset},
    {"S", ToolMode::Stretch},     {"CHA", ToolMode::Chamfer},     {"AR", ToolMode::Array},
    {"RA", ToolMode::Fillet},     {"RAC", ToolMode::Fillet},      {"F", ToolMode::Fillet},
    {"J", ToolMode::Join},        {"X", ToolMode::Explode},       {"E", ToolMode::Erase},
    // Cotation
    {"DLI", ToolMode::DimensionLinear}, {"DAL", ToolMode::DimensionAligned},
    {"DAN", ToolMode::DimensionAngular}, {"DRA", ToolMode::DimensionRadius},
    {"DDI", ToolMode::DimensionDiameter},
    // Renseignements
    {"DI", ToolMode::Measure},    {"MEA", ToolMode::Measure},     {"AA", ToolMode::Area},
    {"ID", ToolMode::Coordinates},
    // Affichage
    {"ZF", ToolMode::ZoomWindow}, {"P", ToolMode::Pan},
    // Noms complets des commandes d'AutoCAD en francais (sans accents, comme
    // on les tape) : la ligne de commande les accepte et les propose.
    {"LIGNE", ToolMode::Line},     {"POLYLIGN", ToolMode::Polyline},   {"CERCLE", ToolMode::Circle},
    {"ARC", ToolMode::Arc3P},      {"RECTANG", ToolMode::Rectangle},   {"POLYGONE", ToolMode::Polygon},
    {"POINT", ToolMode::Point},    {"TEXTE", ToolMode::Text},          {"HACHURES", ToolMode::Hatch},
    {"DEPLACER", ToolMode::Move},  {"COPIER", ToolMode::Copy},         {"ROTATION", ToolMode::Rotate},
    {"ECHELLE", ToolMode::Scale},  {"MIROIR", ToolMode::Mirror},       {"AJUSTER", ToolMode::Trim},
    {"PROLONGE", ToolMode::Extend}, {"COUPURE", ToolMode::Break},      {"DECALER", ToolMode::Offset},
    {"ETIRER", ToolMode::Stretch}, {"CHANFREIN", ToolMode::Chamfer},   {"RACCORD", ToolMode::Fillet},
    {"RESEAU", ToolMode::Array},   {"JOINDRE", ToolMode::Join},        {"DECOMPOS", ToolMode::Explode},
    {"EFFACER", ToolMode::Erase},  {"COTLIN", ToolMode::DimensionLinear}, {"COTALI", ToolMode::DimensionAligned},
    {"COTANG", ToolMode::DimensionAngular}, {"COTRAYON", ToolMode::DimensionRadius},
    {"COTDIA", ToolMode::DimensionDiameter}, {"DIST", ToolMode::Measure}, {"AIRE", ToolMode::Area},
    {"PAN", ToolMode::Pan},
};

// Outils pas encore livres : leur alias annonce la tache au lieu de ne rien
// faire. Un alias passe dans kToolAliases le jour de la livraison.
struct FutureAlias {
    const char* alias;
    const char* detail;
};

inline constexpr FutureAlias kFutureAliases[] = {
    {"B", "Créer un bloc (D-09)"},
    {"I", "Insérer un bloc (D-09)"},
    {"LE", "Ligne de repère (A-05)"},
    {"TB", "Tableau (A-08)"},
    {"MA", "Copier les propriétés (M-10)"},
};

// Premier mot de la saisie, en majuscules.
inline QString aliasToken(const QString& text) {
    return text.trimmed().section(QRegularExpression(QStringLiteral("\\s+")), 0, 0).toUpper();
}

inline std::optional<ToolMode> toolModeForAlias(const QString& text) {
    const QString token = aliasToken(text);
    if (token.isEmpty()) return std::nullopt;
    for (const auto& a : kToolAliases)
        if (token == QLatin1String(a.alias)) return a.mode;
    return std::nullopt;
}

inline const char* futureToolForAlias(const QString& text) {
    const QString token = aliasToken(text);
    for (const auto& a : kFutureAliases)
        if (token == QLatin1String(a.alias)) return a.detail;
    return nullptr;
}

} // namespace bcad::app
