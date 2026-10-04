#pragma once

// Adaptateurs `plugin::IValidator` des regles cadastrales (ADR-003/005/016 : le
// core ne connait AUCUNE regle, c'est le plugin qui les declare et qui en
// redige les libelles). Chaque adaptateur transforme un lot de geom::Entity en
// diagnostics ; les utilitaires de verification restent les classes existantes
// (ParcelOverlapValidator, ParcelIdentifierValidator).

#include "ParcelIdentifierValidator.h"

#include "bcad/plugin/Validator.h"

#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace bcad::cadastre {

struct CadastreTemplates;

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
//
// Le profil n'est pas fige au chargement : l'operateur en designe un pour son
// dossier, et `appliquerProfil` remplace les motifs en vigueur. L'hote detient
// l'instance mais ne sait pas ce qu'un profil veut dire — c'est le module qui
// change sa propre regle. Le libelle du lot porte le nom du profil applique :
// un profil herite d'un dossier precedent se lit alors dans le panneau, au lieu
// de faire passer une reponse fausse pour une reponse juste.
class ParcelIdentifierRuleValidator : public plugin::IValidator {
public:
    explicit ParcelIdentifierRuleValidator(
        std::string sectionPattern = "^[A-Z]{1,3}$",
        std::string numberPattern = "^[0-9]+$");

    // La regle telle que la livre un gabarit de profil.
    explicit ParcelIdentifierRuleValidator(const CadastreTemplates& gabarit);

    std::string id() const override;
    std::string label() const override;
    std::vector<std::string> applicableTypes() const override;
    std::vector<validation::Diagnostic> validate(
        const std::vector<geom::Entity*>& entities) const override;

    // Le profil en vigueur, tel que le libelle du lot le nomme.
    void appliquerProfil(const CadastreTemplates& gabarit) const;
    std::string profil() const;

    // L'etat en vigueur, tel qu'un changement de profil devrait le retrouver.
    CadastreTemplates profilEnVigueur() const;

private:
    // Le profil en vigueur est lu a chaque verification, jamais fige a la
    // construction : c'est ce qui rend le changement de profil effectif sur une
    // regle deja enregistree par l'hote.
    struct Motifs {
        const std::string profil;
        const std::string source;
        const ParcelIdentifierValidator regle;
        Motifs(std::string p, std::string s, std::string section, std::string numero)
            : profil(std::move(p)),
              source(std::move(s)),
              regle(std::move(section), std::move(numero)) {}
    };

    std::shared_ptr<const Motifs> motifs() const;

    mutable std::mutex mutex_;
    // mutable : changer de profil est une operation constante vue de l'hote, qui
    // ne tient qu'une liste de `const IValidator*`.
    mutable std::shared_ptr<const Motifs> motifs_;
};

// Contenance calculee contre contenance declaree (K-09). La surface est celle
// du contour moins ses trous ; l'ecart admis est le perimetre multiplie par la
// tolerance lineaire du profil (`survey_tolerance.default_m`) — hypothese de
// travail a faire valider par un geometre de chaque pays. Le profil peut
// changer en cours de dossier (`appliquerProfil`), comme pour l'identification.
class ParcelAreaRuleValidator : public plugin::IValidator {
public:
    explicit ParcelAreaRuleValidator(const CadastreTemplates& gabarit);

    std::string id() const override;
    std::string label() const override;
    std::vector<std::string> applicableTypes() const override;
    std::vector<validation::Diagnostic> validate(
        const std::vector<geom::Entity*>& entities) const override;

    void appliquerProfil(const CadastreTemplates& gabarit) const;
    double toleranceM() const;

private:
    mutable std::mutex mutex_;
    mutable double toleranceM_;
    mutable std::string profil_;
};

} // namespace bcad::cadastre
