#include "layout/CadastreSheet.h"

#include "ParcelOps.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/properties/PropertyMap.h"
#include "entities/ParcelEntity.h"

#include <cmath>
#include <cstdint>
#include <map>
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

// Un champ du cartouche et l'endroit où le module le range. Les deux noms de
// clé sont du vocabulaire cadastral : ni `src/layout` ni `src/app` ne les
// voient (ADR-016 §4).
struct ChampCartouche {
    const char* dossier;                                  // cadastre.dossier.<dossier>
    const char* parcelle;                                 // cadastre.<parcelle>, ou nullptr
    std::string layout::Cartouche::*const champ;
};

// `parcelle` n'est rempli que pour les cinq champs que le module écrit sur une
// parcelle. La nature d'une parcelle est un énuméré, pas une chaîne : elle ne
// peut pas remonter dans un cartouche sans que le module la nomme, ce que
// l'ADR-017 renvoie à un gabarit.
const std::vector<ChampCartouche>& champsCartouche() {
    static const std::vector<ChampCartouche> champs{
        {"projet", nullptr, &layout::Cartouche::projectName},
        {"numero_projet", nullptr, &layout::Cartouche::projectNumber},
        {"phase", nullptr, &layout::Cartouche::phase},
        {"lot", nullptr, &layout::Cartouche::lotNumber},
        {"commune", "commune", &layout::Cartouche::commune},
        {"section", "section", &layout::Cartouche::section},
        {"numero", "numero", &layout::Cartouche::numero},
        {"contenance", "contenance", &layout::Cartouche::contenance},
        {"code_commune", nullptr, &layout::Cartouche::communeCode},
        {"date", nullptr, &layout::Cartouche::date},
        {"geometre", nullptr, &layout::Cartouche::geometre},
        {"dossier", nullptr, &layout::Cartouche::dossier},
        {"proprietaire", "proprietaire", &layout::Cartouche::proprietaire},
        {"nature", nullptr, &layout::Cartouche::nature},
        {"reference_plan", nullptr, &layout::Cartouche::referencePlan},
        {"revision", nullptr, &layout::Cartouche::revision},
        {"auteur", nullptr, &layout::Cartouche::auteur},
        {"verifie_par", nullptr, &layout::Cartouche::verifiePar},
        {"approuve_par", nullptr, &layout::Cartouche::approuvePar},
        {"date_creation", nullptr, &layout::Cartouche::dateCreation},
        {"date_modification", nullptr, &layout::Cartouche::dateModification},
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

} // namespace

layout::Cartouche buildCartouche(const core::Document& document) {
    layout::Cartouche cartouche;
    const auto& dossier = document.properties();

    std::vector<const geom::Entity*> parcelles;
    for (const auto& entity : document.entities())
        if (isCadastreParcel(entity.get())) parcelles.push_back(entity.get());

    for (const auto& [cleDossier, cleParcelle, champ] : champsCartouche()) {
        std::string valeur =
            cleDossier ? dossier.getString(std::string("cadastre.dossier.") + cleDossier)
                       : std::string();
        if (valeur.empty() && cleParcelle)
            valeur = valeurCommune(parcelles, cleParcelle);
        if (!valeur.empty()) cartouche.*champ = valeur;
    }
    // `echelle` volontairement absent de la table : c'est la composition qui le
    // pose, et un cartouche dont la seule donnée serait une échelle ne doit pas
    // apparaître sur une feuille où l'opérateur n'a rien saisi.
    return cartouche;
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
        const std::string commune = properties.getString("cadastre.commune");
        const auto& vertices = parcel.vertices();
        if (vertices.size() < 3) continue;

        furniture.labels.push_back(
            layout::Label::forParcel(section, numero, contenance, vertices));
        furniture.table.add({section, numero, contenance, commune, parcelArea(parcel)});

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
