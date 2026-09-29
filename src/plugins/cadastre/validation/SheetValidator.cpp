#include "validation/SheetValidator.h"

#include "../Templates.h"
#include "../commands/ProfilCommand.h"
#include "../layout/CadastreSheet.h"

#include "bcad/core/Document.h"
#include "bcad/layout/Sheet.h"
#include "bcad/layout/Viewport.h"

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace bcad::cadastre {

namespace {

// Le profil que le dossier désigne, ou le défaut du module. Même chemin que
// `cadastre.set_profile` : répertoires mémorisés, code refusé = pas de profil,
// fichier absent = valeurs par défaut. Un dossier sans profil désigné est
// vérifié contre le défaut, et le libellé le dit.
CadastreTemplates gabaritEnVigueur(const core::Document& document) {
    const std::string code = document.properties().getString(kCleProfilDossier);
    if (!code.empty() && estCodeDeProfilValide(code)) {
        const CadastreTemplates gabarit = chargerGabaritDuProfil(code);
        if (!gabarit.source.empty()) return gabarit;
    }
    CadastreTemplates defaut;
    defaut.profile = kProfilParDefaut;
    return defaut;
}

std::vector<int> echellesEnVigueur(const CadastreTemplates& gabarit) {
    if (!gabarit.permittedScales.empty()) return gabarit.permittedScales;
    return defaultPermittedScales();
}

std::string listeDesEchelles(const std::vector<int>& echelles) {
    std::ostringstream ss;
    for (std::size_t i = 0; i < echelles.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << "1:" << echelles[i];
    }
    return ss.str();
}

std::string dimensions(double largeur, double hauteur) {
    std::ostringstream ss;
    ss.precision(1);
    ss << std::fixed << largeur << "×" << hauteur << " mm";
    return ss.str();
}

} // namespace

std::string SheetValidator::id() const { return "cadastre.mise_en_page"; }

std::string SheetValidator::label() const { return "Mise en page des feuilles"; }

std::vector<validation::Diagnostic> SheetValidator::validateDocument(
    const core::Document& document) const {
    std::vector<validation::Diagnostic> diagnostics;
    const CadastreTemplates gabarit = gabaritEnVigueur(document);
    const std::vector<int> echelles = echellesEnVigueur(gabarit);

    for (const auto& feuille : document.sheets()) {
        if (!feuille->formatIsKnown() || !feuille->orientationIsKnown()) {
            diagnostics.push_back({validation::Severity::Warning,
                                   "feuille « " + feuille->title() + " » : format « " +
                                       feuille->formatToken() + " » ou orientation inconnus de cet "
                                       "exécutable — dimensions non vérifiables, feuille conservée",
                                   {}});
            continue;
        }
        int vue = 0;
        for (const auto& vueFeuille : feuille->views()) {
            ++vue;
            const std::string ou =
                "feuille « " + feuille->title() + " », vue " + std::to_string(vue);
            const double echelle = vueFeuille.scale();
            if (echelle <= 0) {
                diagnostics.push_back({validation::Severity::Warning,
                                       ou + " : échelle non choisie — la composition ajustera, "
                                             "le livrable ne portera pas l'échelle du profil",
                                       {}});
                continue;
            }
            if (!vueFeuille.fitsIn(*feuille)) {
                diagnostics.push_back(
                    {validation::Severity::Error,
                     ou + " : déborde à 1:" + std::to_string(static_cast<int>(echelle)) +
                         " (" + dimensions(vueFeuille.widthOnSheet(), vueFeuille.heightOnSheet()) +
                         " pour " + dimensions(feuille->printableWidth(), feuille->printableHeight()) +
                         " imprimables) — changer de format ou d'échelle",
                     {}});
            }
            bool admise = false;
            for (const int permise : echelles) {
                if (std::abs(echelle - permise) < 0.5) {
                    admise = true;
                    break;
                }
            }
            if (!admise) {
                diagnostics.push_back(
                    {validation::Severity::Warning,
                     ou + " : échelle 1:" + std::to_string(static_cast<int>(echelle)) +
                         " hors de la liste du profil « " + gabarit.profile + " » (" +
                         listeDesEchelles(echelles) + ")",
                     {}});
            }
        }
    }
    return diagnostics;
}

} // namespace bcad::cadastre
