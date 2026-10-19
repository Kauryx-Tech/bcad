#pragma once

#include "bcad/commands/Command.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace bcad::cadastre {

// L'attribut du dossier qui porte le systeme de coordonnees, imprime au
// cartouche du plan.
inline constexpr const char* kCleSystemeCoordonnees = "cadastre.dossier.systeme_coordonnees";

// Le code EPSG numerique du meme systeme : les exports (GeoPackage, DXF,
// GeoJSON) ont besoin d'un entier, pas seulement du libelle imprime.
inline constexpr const char* kCleEpsgDossier = "cadastre.dossier.epsg";

// « Systeme de coordonnees… » (K-06) : l'operateur tape un code EPSG, le
// module l'inscrit au dossier avec son nom (liste sourcee du module) — ou
// « non repertorie » si la liste ne le connait pas. Annulable.
class SetCrsCommand final : public commands::Command {
public:
    explicit SetCrsCommand(std::string libelle, int epsg = 0)
        : libelle_(std::move(libelle)), epsg_(epsg) {}
    std::string_view text() const override { return "cadastre.set_crs"; }
    void execute(core::Document& doc) override;
    void undo(core::Document& doc) override;
    std::unique_ptr<commands::Command> clone() const override;
    const std::string& libelle() const { return libelle_; }

private:
    std::string libelle_;
    int epsg_ = 0;
    std::string precedent_;
    int precedentEpsg_ = 0;
    bool present_ = false;
    bool presentEpsg_ = false;
    bool pris_ = false;
};

// Un argument : le code tape. Refuse ce qui n'est pas un code EPSG.
std::unique_ptr<commands::Command> makeSetCrs(const std::vector<std::string>& args);

} // namespace bcad::cadastre
