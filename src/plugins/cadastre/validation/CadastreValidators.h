#pragma once

// Adaptateurs `plugin::IValidator` des regles cadastrales (ADR-003/005/016 : le
// core ne connait AUCUNE regle, c'est le plugin qui les declare et qui en
// redige les libelles). Chaque adaptateur transforme un lot de geom::Entity en
// diagnostics ; les utilitaires de verification restent les classes existantes
// (ParcelOverlapValidator, ParcelIdentifierValidator).

#include "ParcelIdentifierValidator.h"

#include "bcad/plugin/Validator.h"

#include <string>
#include <vector>

namespace bcad::cadastre {

// Types a laquelle chaque regle s'applique : une parcelle est soit l'entite
// cadastre.parcel, soit une polyligne fermee portant les attributs cadastraux
// (import DXF). L'hote filtre la selection la-dessus, le detail est tranche par
// isCadastreParcel() dans l'adaptateur.
std::vector<std::string> parcelApplicableTypes();

// Topologie d'une parcelle : au moins 3 sommets, aire non nulle, polygone simple.
class ParcelTopologyValidator : public plugin::IValidator {
public:
    std::string id() const override;
    std::string label() const override;
    std::vector<std::string> applicableTypes() const override;
    std::vector<validation::Diagnostic> validate(
        const std::vector<geom::Entity*>& entities) const override;
};

// Deux parcelles ne doivent pas posseder d'emprise commune.
class ParcelOverlapRuleValidator : public plugin::IValidator {
public:
    std::string id() const override;
    std::string label() const override;
    std::vector<std::string> applicableTypes() const override;
    std::vector<validation::Diagnostic> validate(
        const std::vector<geom::Entity*>& entities) const override;
};

// Identification de parcelle : section et numero conformes au motif attendu.
// Les motifs viennent du gabarit du profil (Templates.cpp) ; les valeurs par
// defaut sont celles du profil historique, pour que la regle reste utilisable
// sans fichier installe.
class ParcelIdentifierRuleValidator : public plugin::IValidator {
public:
    explicit ParcelIdentifierRuleValidator(
        std::string sectionPattern = "^[A-Z]{1,3}$",
        std::string numberPattern = "^[0-9]+$");

    std::string id() const override;
    std::string label() const override;
    std::vector<std::string> applicableTypes() const override;
    std::vector<validation::Diagnostic> validate(
        const std::vector<geom::Entity*>& entities) const override;

private:
    ParcelIdentifierValidator identifier_;
};

} // namespace bcad::cadastre
