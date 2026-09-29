// La validité d'une feuille est une validation, pas une exception (ADR-017,
// décision 5). Une vue qui déborde est une ERREUR (le livrable est faux), une
// échelle hors de la liste du profil un AVERTISSEMENT (le plan s'imprime, non
// conforme). Le profil en vigueur est celui que le dossier désigne, à défaut
// les valeurs du module.
//
// Les appels a effet de bord sont hors des assert() : en RelWithDebInfo NDEBUG
// est defini et assert() n'evalue pas son argument.

#include "validation/SheetValidator.h"

#include "bcad/core/Document.h"
#include "bcad/layout/Sheet.h"
#include "bcad/layout/Viewport.h"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace bcad;
using namespace bcad::cadastre;

namespace {

layout::Sheet& feuilleA3(core::Document& document, const std::string& titre) {
    layout::Sheet* feuille = document.addSheet(titre);
    assert(feuille != nullptr);
    feuille->setFormatToken("A3");
    feuille->setOrientationToken("Paysage");
    return *feuille;
}

void poserVue(layout::Sheet& feuille, double largeurMonde, double hauteurMonde, double echelle) {
    layout::Viewport vue;
    geom::BoundingBox source;
    source.minX = 0;
    source.minY = 0;
    source.maxX = largeurMonde;
    source.maxY = hauteurMonde;
    vue.setSource(source);
    vue.setScale(echelle);
    feuille.views().push_back(vue);
}

bool contient(const std::vector<validation::Diagnostic>& diagnostics,
              validation::Severity severite, const std::string& extrait) {
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.severity == severite &&
            diagnostic.message.find(extrait) != std::string::npos)
            return true;
    }
    return false;
}

// Un îlot de 300×200 m à 1:500 fait 600×400 mm sur 400×277 imprimables : le
// refus est une erreur qui nomme la feuille, la vue et les dimensions.
void checkLeDebordementEstUneErreur() {
    core::Document document;
    layout::Sheet& feuille = feuilleA3(document, "Foncier Nord");
    poserVue(feuille, 300, 200, 500);

    SheetValidator regle;
    assert(regle.id() == "cadastre.mise_en_page");
    const auto diagnostics = regle.validateDocument(document);
    assert(contient(diagnostics, validation::Severity::Error, "Foncier Nord"));
    assert(contient(diagnostics, validation::Severity::Error, "600"));
    assert(contient(diagnostics, validation::Severity::Error, "déborde"));
}

// 100×60 m à 1:1000 tient sur A3 paysage : aucun constat.
void checkUneVueQuiTientNeDitRien() {
    core::Document document;
    layout::Sheet& feuille = feuilleA3(document, "Foncier Sud");
    poserVue(feuille, 100, 60, 1000);

    SheetValidator regle;
    const auto diagnostics = regle.validateDocument(document);
    assert(diagnostics.empty());
}

// 1:300 n'est ni dans le défaut du module ni dans aucun profil : avertissement
// qui nomme l'échelle et la liste admise, pas erreur — le plan s'imprime.
void checkHorsListeEstUnAvertissement() {
    core::Document document;
    layout::Sheet& feuille = feuilleA3(document, "Foncier Est");
    poserVue(feuille, 10, 10, 300);

    SheetValidator regle;
    const auto diagnostics = regle.validateDocument(document);
    assert(!contient(diagnostics, validation::Severity::Error, "300"));
    assert(contient(diagnostics, validation::Severity::Warning, "1:300"));
    assert(contient(diagnostics, validation::Severity::Warning, "hors de la liste"));
}

// Sans échelle choisie, pas de mensonge : avertissement, et surtout pas
// d'erreur de débordement calculée sur une échelle inventée.
void checkSansEchellePasDeDebordementInvente() {
    core::Document document;
    layout::Sheet& feuille = feuilleA3(document, "Foncier Ouest");
    poserVue(feuille, 300, 200, 0);

    SheetValidator regle;
    const auto diagnostics = regle.validateDocument(document);
    assert(!contient(diagnostics, validation::Severity::Error, "déborde"));
    assert(contient(diagnostics, validation::Severity::Warning, "non choisie"));
}

// Un format que cet exécutable ne connaît pas ne fait pas sauter la règle :
// constat qui le dit, dimensions non vérifiables, feuille conservée.
void checkFormatInconnuSignaleConserve() {
    core::Document document;
    layout::Sheet* feuille = document.addSheet("Format national");
    assert(feuille != nullptr);
    feuille->setFormatToken("A3-TG");
    feuille->setOrientationToken("Paysage");
    poserVue(*feuille, 10, 10, 500);

    SheetValidator regle;
    const auto diagnostics = regle.validateDocument(document);
    assert(contient(diagnostics, validation::Severity::Warning, "A3-TG"));
    assert(!contient(diagnostics, validation::Severity::Error, "déborde"));
}

// La place décidée donne l'ancre, l'échelle la taille : quand les deux ne
// s'accordent pas, le dire plutôt que rogner en silence.
void checkPlaceEtEchelleDesaccordeesSignalees() {
    core::Document document;
    layout::Sheet& feuille = feuilleA3(document, "Foncier Centre");
    layout::Viewport vue;
    geom::BoundingBox source;
    source.minX = 0;
    source.minY = 0;
    source.maxX = 40;
    source.maxY = 30;
    vue.setSource(source);
    vue.setScale(500); // 80×60 mm de plan...
    vue.setPaper({10.0, 10.0, 100.0, 100.0}); // ...pour 100×100 réservés
    feuille.views().push_back(vue);

    SheetValidator regle;
    const auto diagnostics = regle.validateDocument(document);
    assert(!contient(diagnostics, validation::Severity::Error, "déborde"));
    assert(contient(diagnostics, validation::Severity::Warning, "place réservée"));
}

// Une place accordée à l'échelle ne dit rien.
void checkPlaceAccordeeSilencieuse() {
    core::Document document;
    layout::Sheet& feuille = feuilleA3(document, "Foncier Calé");
    layout::Viewport vue;
    geom::BoundingBox source;
    source.minX = 0;
    source.minY = 0;
    source.maxX = 40;
    source.maxY = 30;
    vue.setSource(source);
    vue.setScale(500);
    vue.setPaper({10.0, 10.0, 80.0, 60.0});
    feuille.views().push_back(vue);

    SheetValidator regle;
    const auto diagnostics = regle.validateDocument(document);
    assert(diagnostics.empty());
}

} // namespace

int main() {
    checkLeDebordementEstUneErreur();
    checkUneVueQuiTientNeDitRien();
    checkHorsListeEstUnAvertissement();
    checkSansEchellePasDeDebordementInvente();
    checkFormatInconnuSignaleConserve();
    checkPlaceEtEchelleDesaccordeesSignalees();
    checkPlaceAccordeeSilencieuse();

    std::cout << "mise en page des feuilles OK\n";
    return 0;
}
