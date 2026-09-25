// La resolution du cartouche est la regle qui decide ce que la feuille affirme
// (ADR-017). Avant elle, le module intitulait « Plan cadastral » une feuille ou
// l'operateur n'avait rien saisi, et reprenait la commune de la premiere
// parcelle rencontree : l'ordre de stockage du document devenait une donnee du
// livrable. Ces assertions tiennent les deux regles et leurs deux refus.
//
// Les appels a effet de bord sont hors des assert() : en RelWithDebInfo NDEBUG
// est defini et assert() n'evalue pas son argument.

#include "layout/CadastreSheet.h"

#include "entities/ParcelEntity.h"

#include "bcad/core/Document.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/layout/Cartouche.h"
#include "bcad/properties/PropertyMap.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace bcad;
using namespace bcad::cadastre;

namespace {

std::unique_ptr<ParcelEntity> makeParcel(const std::string& commune,
                                         const std::string& proprietaire,
                                         const std::string& section = "A",
                                         const std::string& numero = "1") {
    auto parcel = std::make_unique<ParcelEntity>(std::vector<geom::Point2>{
        {0, 0}, {10, 0}, {10, 10}, {0, 10}});
    parcel->properties().setString("cadastre.commune", commune);
    parcel->properties().setString("cadastre.proprietaire", proprietaire);
    parcel->properties().setString("cadastre.section", section);
    parcel->properties().setString("cadastre.numero", numero);
    return parcel;
}

core::Document& withAttributes(core::Document& document, const std::string& cle,
                               const std::string& valeur) {
    document.properties().setString(std::string("cadastre.dossier.") + cle, valeur);
    return document;
}

// Une saisie du dossier prime, meme quand les parcelles diraient autre chose :
// c'est l'operateur qui engage le livrable, pas le leve.
void checkLeDossierPrime() {
    core::Document document;
    document.addEntity(makeParcel("Lome", "Kossi"));
    withAttributes(document, "commune", "Blover");

    const layout::Cartouche cartouche = buildCartouche(document);
    assert(cartouche.commune == "Blover");
    // Un champ que le dossier ne nomme pas remonte neanmoins des parcelles.
    assert(cartouche.proprietaire == "Kossi");
}

// La valeur commune est une donnee de la feuille : elle n'existe que si TOUTES
// les parcelles la portent.
void checkLaValeurCommuneRemonte() {
    core::Document document;
    document.addEntity(makeParcel("Lome", "Kossi"));
    document.addEntity(makeParcel("Lome", "Afiavi"));

    const layout::Cartouche cartouche = buildCartouche(document);
    assert(cartouche.commune == "Lome");
    // Des parcelles qui ne disent pas la meme chose ne font pas une affirmation
    // pour la feuille : le champ reste vide, la nomenclature porte la difference.
    assert(cartouche.proprietaire.empty());
}

// La premiere parcelle lue ne dicte rien. C'est l'assertion qui manquait : le
// defaut rendait « Anne » visible sur une feuille dont aucune parcelle ne
// portait ce nom en commun.
void checkLaPremiereParcelleNeDicteRien() {
    core::Document document;
    document.addEntity(makeParcel("Lome", "Anne", "A", "1"));
    document.addEntity(makeParcel("Kpalime", "Kossi", "B", "2"));

    const layout::Cartouche cartouche = buildCartouche(document);
    assert(cartouche.commune.empty());
    assert(cartouche.proprietaire.empty());
    // Les parcelles d'une feuille sont justement censees differer sur ces deux
    // champs : la nomenclature les porte, le cartouche non.
    assert(cartouche.section.empty());
    assert(cartouche.numero.empty());
}

// Un champ invente faute de saisie est une faussete, pas un arrangement. Le
// titre « Plan cadastral » est passe par la : il doit revenir vide, et le
// cartouche doit etre juge invalide, donc ni reserve ni peint.
void checkRienNestInvente() {
    core::Document vide;
    const layout::Cartouche cartouche = buildCartouche(vide);
    assert(!cartouche.isValid());
    assert(cartouche.projectName.empty());
    assert(cartouche.commune.empty());
    assert(cartouche.echelle.empty());

    // Une cle que le module ne connait pas ne devient pas un champ : elle est
    // ignoree, et le cartouche ne se remplit pas d'un fourre-tout.
    core::Document inconnu;
    inconnu.properties().setString("cadastre.dossier.champ_inexistant", "valeur");
    assert(!buildCartouche(inconnu).isValid());

    // Le cartouche n'est pas le lieu de l'echelle : c'est la composition qui la
    // pose, et une feuille sans donnee saisie ne doit pas en porter une.
    core::Document seule;
    seule.addEntity(makeParcel("Lome", "Kossi", "A", "7"));
    const layout::Cartouche resolue = buildCartouche(seule);
    assert(resolue.commune == "Lome");
    assert(resolue.section == "A");
    assert(resolue.numero == "7");
    assert(resolue.echelle.empty());
    assert(resolue.isValid());
}

// Le champ recherche d'abord dans le dossier, puis dans les parcelles : les
// deux regles coexistent, elles ne s'annulent pas.
void checkLesDeuxSourcesCoexistent() {
    core::Document document;
    document.addEntity(makeParcel("Lome", "Kossi"));
    document.addEntity(makeParcel("Lome", "Kossi"));
    withAttributes(document, "geometre", "Cabinet Adjo");
    withAttributes(document, "dossier", "D-2026-118");

    const layout::Cartouche cartouche = buildCartouche(document);
    assert(cartouche.geometre == "Cabinet Adjo");
    assert(cartouche.dossier == "D-2026-118");
    assert(cartouche.commune == "Lome");
    assert(cartouche.proprietaire == "Kossi");
}

} // namespace

int main() {
    checkLeDossierPrime();
    checkLaValeurCommuneRemonte();
    checkLaPremiereParcelleNeDicteRien();
    checkRienNestInvente();
    checkLesDeuxSourcesCoexistent();

    std::cout << "resolution du cartouche OK\n";
    return 0;
}
