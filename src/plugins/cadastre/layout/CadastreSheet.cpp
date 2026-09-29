#include "layout/CadastreSheet.h"

#include "ParcelOps.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/layout/FieldResolution.h"
#include "bcad/properties/PropertyMap.h"
#include "entities/ParcelEntity.h"

#include <cmath>
#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace bcad::cadastre {

namespace {

// Clé de déduplication des bornes : deux sommets au même endroit sont la meme
// borne, pas deux. Un centième de millimetre suffit — les coordonnées du
// parcellaire sont en metres et issues de levés bien plus larges.
std::int64_t borneKey(const geom::Point2& p) {
    constexpr double kGrid = 100000.0;
    return static_cast<std::int64_t>(std::llround(p.x_ * kGrid)) * 1000003LL +
           static_cast<std::int64_t>(std::llround(p.y_ * kGrid));
}

// Un champ du cartouche et l'endroit où le module le range : la clé du dossier
// (`cadastre.dossier.<dossier>`), la clé de parcelle (`cadastre.<parcelle>`, ou
// nullptr), et la place qu'il tient dans le gabarit par défaut. Les trois noms
// sont du vocabulaire cadastral : ni `src/layout` ni `src/app` ne les voient
// (ADR-016 §4).
struct ChampCartouche {
    const char* dossier;
    const char* parcelle;
    const char* label;
    int slot;
};

// `parcelle` n'est rempli que pour les cinq champs que le module écrit sur une
// parcelle. La nature d'une parcelle est un énuméré, pas une chaîne : elle ne
// peut pas remonter dans un cartouche sans que le module la nomme, ce que le
// gabarit du profil national déclare.
const std::vector<ChampCartouche>& champsCartouche() {
    static const std::vector<ChampCartouche> champs{
        {"projet", nullptr, "Projet", 0},
        {"numero_projet", nullptr, "N° projet", 1},
        {"phase", nullptr, "Phase", 2},
        {"lot", nullptr, "Lot", 3},
        {"commune", "commune", "Commune", 4},
        {"section", "section", "Section", 5},
        {"numero", "numero", "N° parcelle", 6},
        {"contenance", "contenance", "Contenance", 7},
        {"code_commune", nullptr, "Code commune", 8},
        {"date", nullptr, "Date", 9},
        {"geometre", nullptr, "Géomètre", 10},
        {"dossier", nullptr, "Dossier", 11},
        {"proprietaire", "proprietaire", "Propriétaire", 12},
        {"nature", nullptr, "Nature", 13},
        {"reference_plan", nullptr, "Réf. plan", 14},
        {"revision", nullptr, "Révision", 15},
        {"auteur", nullptr, "Auteur", 16},
        {"verifie_par", nullptr, "Vérifié par", 17},
        {"approuve_par", nullptr, "Approuvé par", 18},
        {"date_creation", nullptr, "Créé le", 19},
        {"date_modification", nullptr, "Modifié le", 20},
    };
    return champs;
}

// La valeur que TOUTES les parcelles de la feuille portent, ou rien. Une valeur
// qui diffère d'une parcelle à l'autre n'est pas une donnée de la feuille :
// c'est une donnée de la parcelle, et la nomenclature la porte déjà. Rendre la
// première lue aurait fait du livrable une affirmation que personne n'a saisie.
std::string valeurCommune(const std::vector<const geom::Entity*>& parcelles,
                          const char* cleParcelle) {
    std::string valeur;
    for (const auto* parcelle : parcelles) {
        const std::string candidate =
            parcelle->properties().getString(std::string("cadastre.") + cleParcelle);
        if (candidate.empty()) return {};
        if (valeur.empty()) valeur = candidate;
        else if (valeur != candidate) return {};
    }
    return valeur;
}

std::vector<const geom::Entity*> parcellesDe(const core::Document& document) {
    std::vector<const geom::Entity*> parcelles;
    for (const auto& entity : document.entities())
        if (isCadastreParcel(entity.get())) parcelles.push_back(entity.get());
    return parcelles;
}

std::string aireEnTexte(double aire) {
    std::ostringstream ss;
    ss.precision(2);
    ss << std::fixed << aire;
    return ss.str() + " m²";
}

} // namespace

layout::FurnitureTemplate defaultCartoucheTemplate() {
    layout::FurnitureTemplate gabarit;
    gabarit.id = "cadastre.cartouche.defaut";
    gabarit.nature = "cadastre.cartouche";
    gabarit.rows = 6;
    gabarit.columns = 4;
    gabarit.reservedZone = {0, 0, 0, 25.0};
    gabarit.borderWidth = 0.5;
    gabarit.fontName = "Standard";
    gabarit.fontSizeMm = 2.5;
    for (const auto& champ : champsCartouche()) {
        layout::TemplateField attendu;
        attendu.role = "attribut";
        attendu.label = champ.label;
        attendu.key = std::string("cadastre.dossier.") + champ.dossier;
        attendu.slot = champ.slot;
        gabarit.fields.push_back(std::move(attendu));
    }
    // L'échelle n'est pas un attribut du dossier : elle arrive en littéral,
    // posée par l'appelant une fois la composition connue.
    layout::TemplateField echelle;
    echelle.role = "calcule";
    echelle.label = "Échelle";
    echelle.slot = 21;
    gabarit.fields.push_back(std::move(echelle));
    return gabarit;
}

layout::FurnitureTemplate defaultNomenclatureTemplate() {
    layout::FurnitureTemplate gabarit;
    gabarit.id = "cadastre.nomenclature.defaut";
    gabarit.nature = "cadastre.nomenclature";
    gabarit.columns = 3;
    gabarit.columnLabels = {"Section", "N°", "Contenance"};
    gabarit.reservedZone = {0, 0, 55.0, 0};
    gabarit.borderWidth = 0.5;
    gabarit.fontName = "Standard";
    gabarit.fontSizeMm = 2.0;
    return gabarit;
}

layout::FurnitureTemplate defaultSignaturesTemplate() {
    layout::FurnitureTemplate gabarit;
    gabarit.id = "cadastre.signatures.defaut";
    gabarit.nature = "cadastre.signatures";
    gabarit.columns = 4;
    gabarit.columnLabels = {"Nom", "Rôle", "Date", "Signature"};
    gabarit.reservedZone = {0, 0, 0, 0};
    gabarit.borderWidth = 0.5;
    gabarit.fontName = "Standard";
    gabarit.fontSizeMm = 2.0;
    return gabarit;
}

std::vector<int> defaultPermittedScales() {
    return {500, 1000, 1250, 2000, 2500, 5000};
}

layout::ResolvedFurniture buildCartoucheFurniture(
    const core::Document& document, const layout::FurnitureTemplate& gabarit,
    std::vector<validation::Diagnostic>& diagnostics) {
    layout::ResolvedFurniture resolu;
    resolu.gabarit = gabarit;
    const auto& dossier = document.properties();
    const auto parcelles = parcellesDe(document);

    std::vector<const properties::PropertyMap*> propsParcelles;
    for (const auto* parcelle : parcelles) propsParcelles.push_back(&parcelle->properties());

    // Dossier-clé -> parcelle-clé : la correspondance vit ici, pas dans
    // l'hôte, qui ne résout que des clés opaques une par une.
    std::map<std::string, std::string> versParcelle;
    for (const auto& champ : champsCartouche()) {
        if (champ.parcelle)
            versParcelle[std::string("cadastre.dossier.") + champ.dossier] =
                std::string("cadastre.") + champ.parcelle;
    }

    for (const auto& attendu : gabarit.fields) {
        layout::Field champ;
        champ.role = attendu.role;
        champ.label = attendu.label;
        champ.key = attendu.key;
        champ.format = attendu.format;
        champ.slot = attendu.slot;

        if (attendu.key.empty()) {
            champ.value.value = attendu.literal;
            resolu.fields.push_back(std::move(champ));
            continue;
        }
        const layout::FieldScope auDossier{&dossier, {}};
        const layout::FieldAnswer dossierDit = layout::resolveField(attendu.key, auDossier);
        if (dossierDit.isUnambiguous() && !layout::isBlank(dossierDit.values.front())) {
            champ.value = dossierDit.values.front();
            resolu.fields.push_back(std::move(champ));
            continue;
        }
        const auto correspondance = versParcelle.find(attendu.key);
        if (correspondance != versParcelle.end()) {
            const layout::FieldScope auxParcelles{nullptr, propsParcelles};
            const layout::FieldAnswer parcellesDisent =
                layout::resolveField(correspondance->second, auxParcelles);
            if (parcellesDisent.isUnambiguous()) {
                champ.value = parcellesDisent.values.front();
                resolu.fields.push_back(std::move(champ));
                continue;
            }
            if (parcellesDisent.values.size() > 1) {
                diagnostics.push_back({validation::Severity::Warning,
                                       "champ « " + attendu.label + " » (cle « " + attendu.key +
                                           " ») : " +
                                           std::to_string(parcellesDisent.values.size()) +
                                           " valeurs distinctes sur les parcelles — la"
                                           " nomenclature les porte, le cartouche non",
                                       {}});
                resolu.fields.push_back(std::move(champ));
                continue;
            }
        }
        diagnostics.push_back({validation::Severity::Warning,
                               "champ « " + attendu.label + " » : aucune valeur sous la cle « " +
                                   attendu.key + " »",
                               {}});
        resolu.fields.push_back(std::move(champ));
    }
    return resolu;
}

layout::ResolvedFurniture buildNomenclatureFurniture(
    const core::Document& document, const layout::FurnitureTemplate& gabarit) {
    layout::ResolvedFurniture resolu;
    resolu.gabarit = gabarit;
    const int columns = std::max(1, gabarit.columns);
    double surfaceTotale = 0.0;
    int rang = 0;

    for (const auto& entity : document.entities()) {
        if (!isCadastreParcel(entity.get())) continue;
        const auto& parcel = static_cast<const geom::PolylineEntity&>(*entity);
        if (parcel.vertices().size() < 3) continue;
        const auto& properties = parcel.properties();
        const double aire = parcelArea(parcel);
        surfaceTotale += aire;
        std::string contenance = properties.getString("cadastre.contenance");
        if (contenance.empty() && aire > 0.0) contenance = aireEnTexte(aire);

        const std::string valeurs[3] = {properties.getString("cadastre.section"),
                                        properties.getString("cadastre.numero"), contenance};
        for (int col = 0; col < 3 && col < columns; ++col) {
            layout::Field champ;
            champ.role = "attribut";
            champ.slot = rang * columns + col;
            champ.value.type = properties::PropertyType::String;
            champ.value.value = valeurs[col];
            resolu.fields.push_back(std::move(champ));
        }
        ++rang;
    }

    if (rang > 0 && surfaceTotale > 0.0) {
        layout::Field total;
        total.role = "calcule";
        total.label = "Total";
        total.slot = rang * columns;
        total.value.type = properties::PropertyType::String;
        total.value.value = std::string("Total");
        resolu.fields.push_back(std::move(total));
        layout::Field aire;
        aire.role = "calcule";
        aire.slot = rang * columns + std::min(2, columns - 1);
        aire.value.type = properties::PropertyType::String;
        aire.value.value = aireEnTexte(surfaceTotale);
        resolu.fields.push_back(std::move(aire));
    }
    return resolu;
}

layout::ResolvedFurniture buildSignaturesFurniture(
    const core::Document& document, const layout::FurnitureTemplate& gabarit) {
    layout::ResolvedFurniture resolu;
    resolu.gabarit = gabarit;
    const int columns = std::max(1, gabarit.columns);
    const auto& dossier = document.properties();

    for (int i = 0; i < 64; ++i) {
        const std::string base = "cadastre.dossier.signature." + std::to_string(i) + ".";
        const std::string nom = dossier.getString(base + "nom");
        if (nom.empty()) break; // sans nom, la ligne n'identifie personne
        const std::string valeurs[4] = {nom, dossier.getString(base + "role"),
                                        dossier.getString(base + "date"),
                                        dossier.getString(base + "image")};
        for (int col = 0; col < 4 && col < columns; ++col) {
            layout::Field champ;
            champ.role = "attribut";
            champ.slot = i * columns + col;
            champ.value.type = properties::PropertyType::String;
            champ.value.value = valeurs[col];
            if (col == 3) champ.format = "image";
            resolu.fields.push_back(std::move(champ));
        }
    }
    return resolu;
}

SheetFurniture buildSheetFurniture(const core::Document& document) {
    SheetFurniture furniture;
    std::map<std::int64_t, std::string> bornes;
    int nextBorne = 1;

    for (const auto& entity : document.entities()) {
        if (!isCadastreParcel(entity.get())) continue;
        const auto& parcel = static_cast<const geom::PolylineEntity&>(*entity);
        const auto& properties = parcel.properties();
        const std::string section = properties.getString("cadastre.section");
        const std::string numero = properties.getString("cadastre.numero");
        const std::string contenance = properties.getString("cadastre.contenance");
        const auto& vertices = parcel.vertices();
        if (vertices.size() < 3) continue;

        furniture.labels.push_back(
            layout::Label::forParcel(section, numero, contenance, vertices));

        for (const auto& vertex : vertices) {
            const auto [it, inserted] =
                bornes.emplace(borneKey(vertex), std::to_string(nextBorne));
            if (inserted) {
                furniture.bornes.push_back({vertex, it->second});
                ++nextBorne;
            }
        }
    }
    return furniture;
}

} // namespace bcad::cadastre
