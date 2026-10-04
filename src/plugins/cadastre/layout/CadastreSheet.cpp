#include "layout/CadastreSheet.h"
#include "layout/ParcelLabel.h"

#include "ParcelOps.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/layout/FieldResolution.h"
#include "bcad/properties/PropertyMap.h"
#include "entities/ParcelEntity.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
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

namespace {

// Numerotation des bornes de la feuille : une borne par position, numerotee
// dans l'ordre de rencontre des sommets de parcelle. Le plan et le tableau des
// coordonnees la partagent.
struct Bornage {
    std::map<std::int64_t, std::string> numeros;
    std::vector<layout::PointMarker> reperes;
};

Bornage numeroterBornes(const core::Document& document) {
    Bornage bornage;
    int suivante = 1;
    for (const auto& entity : document.entities()) {
        if (!isCadastreParcel(entity.get())) continue;
        const auto& parcel = static_cast<const geom::PolylineEntity&>(*entity);
        if (parcel.vertices().size() < 3) continue;
        for (const auto& vertex : parcel.vertices()) {
            // « B1, B2… » : le meme libelle sur le plan et dans le tableau.
            const auto [it, nouvelle] =
                bornage.numeros.emplace(borneKey(vertex), "B" + std::to_string(suivante));
            if (nouvelle) {
                bornage.reperes.push_back({vertex, it->second});
                ++suivante;
            }
        }
    }
    return bornage;
}

std::string designation(const geom::Entity& parcel) {
    const auto& props = parcel.properties();
    const std::string section = props.getString("cadastre.section");
    const std::string numero = props.getString("cadastre.numero");
    if (section.empty() && numero.empty()) return "id " + std::to_string(parcel.id());
    return section.empty() ? numero : (numero.empty() ? section : section + " " + numero);
}

} // namespace

SheetFurniture buildSheetFurniture(const core::Document& document) {
    SheetFurniture furniture;
    for (const auto& entity : document.entities()) {
        if (!isCadastreParcel(entity.get())) continue;
        const auto& parcel = static_cast<const geom::PolylineEntity&>(*entity);
        const auto& properties = parcel.properties();
        if (parcel.vertices().size() < 3) continue;
        furniture.labels.push_back(parcelLabel(properties.getString("cadastre.section"),
                                               properties.getString("cadastre.numero"),
                                               properties.getString("cadastre.contenance"),
                                               parcel.vertices()));
    }
    furniture.bornes = numeroterBornes(document).reperes;
    return furniture;
}

std::vector<CoordinateRow> coordinateRows(const core::Document& document) {
    const Bornage bornage = numeroterBornes(document);
    std::vector<CoordinateRow> rows;
    for (const auto& entity : document.entities()) {
        if (!isCadastreParcel(entity.get())) continue;
        const auto& parcel = static_cast<const geom::PolylineEntity&>(*entity);
        const auto& v = parcel.vertices();
        if (v.size() < 3) continue;
        const std::string nom = designation(parcel);
        for (std::size_t i = 0; i < v.size(); ++i) {
            const auto& a = v[i];
            const auto& b = v[(i + 1) % v.size()];
            const double dx = b.x_ - a.x_;
            const double dy = b.y_ - a.y_;
            // Gisement : angle depuis le nord (axe Y), sens horaire, en grades.
            double gisement = std::atan2(dx, dy) * 200.0 / std::numbers::pi;
            if (gisement < 0) gisement += 400.0;
            CoordinateRow row;
            row.parcelle = nom;
            row.borne = bornage.numeros.at(borneKey(a));
            row.x = a.x_;
            row.y = a.y_;
            row.vers = bornage.numeros.at(borneKey(b));
            row.gisementGrades = gisement;
            row.distance = std::hypot(dx, dy);
            rows.push_back(std::move(row));
        }
    }
    return rows;
}

namespace {

// 2S par la methode des coordonnees : somme de X(i) x (Y(i+1) - Y(i-1)).
double doubleAreaOf(const std::vector<geom::Point2>& v, std::vector<SurfaceStep>* steps,
                    const std::map<std::int64_t, std::string>* numeros) {
    double sum = 0.0;
    const std::size_t n = v.size();
    for (std::size_t i = 0; i < n; ++i) {
        const auto& prev = v[(i + n - 1) % n];
        const auto& next = v[(i + 1) % n];
        const double deltaY = next.y_ - prev.y_;
        const double produit = v[i].x_ * deltaY;
        sum += produit;
        if (steps) {
            SurfaceStep step;
            step.borne = numeros ? numeros->at(borneKey(v[i])) : std::string();
            step.x = v[i].x_;
            step.y = v[i].y_;
            step.deltaY = deltaY;
            step.produit = produit;
            steps->push_back(step);
        }
    }
    return sum;
}

} // namespace

std::vector<SurfaceComputation> surfaceComputations(const core::Document& document) {
    const Bornage bornage = numeroterBornes(document);
    std::vector<SurfaceComputation> out;
    for (const auto& entity : document.entities()) {
        if (!isCadastreParcel(entity.get())) continue;
        const auto& parcel = static_cast<const geom::PolylineEntity&>(*entity);
        if (parcel.vertices().size() < 3) continue;
        SurfaceComputation calc;
        calc.parcelle = designation(parcel);
        calc.doubleArea = doubleAreaOf(parcel.vertices(), &calc.steps, &bornage.numeros);
        calc.outerArea = std::abs(calc.doubleArea) / 2.0;
        for (const auto& hole : parcel.holes())
            if (hole.size() >= 3) calc.holesArea += std::abs(doubleAreaOf(hole, nullptr, nullptr)) / 2.0;
        calc.netArea = calc.outerArea - calc.holesArea;
        out.push_back(std::move(calc));
    }
    return out;
}

std::string formatDecimal(double value, int decimals) {
    std::ostringstream ss;
    ss.precision(decimals);
    ss << std::fixed << value;
    std::string text = ss.str();
    for (char& c : text)
        if (c == '.') c = ',';
    return text;
}

layout::FurnitureTemplate defaultCoordinatesTemplate() {
    layout::FurnitureTemplate gabarit;
    gabarit.id = "cadastre.coordonnees.defaut";
    gabarit.nature = "cadastre.coordonnees";
    gabarit.columns = 6;
    gabarit.columnLabels = {"Borne", "X (m)", "Y (m)", "Vers", "Gisement (gr)", "Distance (m)"};
    gabarit.reservedZone = {0, 0, 95.0, 0};
    gabarit.borderWidth = 0.35;
    gabarit.fontName = "Standard";
    gabarit.fontSizeMm = 1.8;
    return gabarit;
}

layout::ResolvedFurniture buildCoordinatesFurniture(
    const core::Document& document, const layout::FurnitureTemplate& gabarit) {
    layout::ResolvedFurniture resolu;
    resolu.gabarit = gabarit;
    const int columns = std::max(1, gabarit.columns);
    int rang = 0;
    for (const auto& row : coordinateRows(document)) {
        const std::string valeurs[6] = {row.borne, formatDecimal(row.x, 3), formatDecimal(row.y, 3),
                                        row.vers, formatDecimal(row.gisementGrades, 4),
                                        formatDecimal(row.distance, 2)};
        for (int col = 0; col < 6 && col < columns; ++col) {
            layout::Field champ;
            champ.role = "calcule";
            champ.slot = rang * columns + col;
            champ.value.type = properties::PropertyType::String;
            champ.value.value = valeurs[col];
            resolu.fields.push_back(std::move(champ));
        }
        ++rang;
    }
    return resolu;
}

} // namespace bcad::cadastre
