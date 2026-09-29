// La resolution du cartouche est la regle qui decide ce que la feuille affirme
// (ADR-017). Avant elle, le module intitulait « Plan cadastral » une feuille ou
// l'operateur n'avait rien saisi, et reprenait la commune de la premiere
// parcelle rencontree : l'ordre de stockage du document devenait une donnee du
// livrable. Ces assertions tiennent les deux regles et leurs deux refus — sur
// des champs déclaratifs résolus depuis un gabarit, plus sur des membres C++.
//
// Les appels a effet de bord sont hors des assert() : en RelWithDebInfo NDEBUG
// est defini et assert() n'evalue pas son argument.

#include "layout/CadastreSheet.h"

#include "entities/ParcelEntity.h"

#include "bcad/core/Document.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/properties/PropertyMap.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <variant>
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

layout::ResolvedFurniture resoudre(core::Document& document,
                                   std::vector<validation::Diagnostic>& diagnostics) {
    return buildCartoucheFurniture(document, defaultCartoucheTemplate(), diagnostics);
}

std::string valeur(const layout::ResolvedFurniture& resolu, const std::string& cle) {
    for (const auto& champ : resolu.fields) {
        if (champ.key != cle) continue;
        if (const auto* texte = std::get_if<std::string>(&champ.value.value)) return *texte;
        return {};
    }
    return {};
}

bool manqueSignale(const std::vector<validation::Diagnostic>& diagnostics,
                   const std::string& cle) {
    for (const auto& diag : diagnostics) {
        if (diag.message.find(cle) != std::string::npos) return true;
    }
    return false;
}

// Une saisie du dossier prime, meme quand les parcelles diraient autre chose :
// c'est l'operateur qui engage le livrable, pas le leve.
void checkLeDossierPrime() {
    core::Document document;
    document.addEntity(makeParcel("Lome", "Kossi"));
    withAttributes(document, "commune", "Blover");
    std::vector<validation::Diagnostic> diagnostics;

    const auto resolu = resoudre(document, diagnostics);
    assert(valeur(resolu, "cadastre.dossier.commune") == "Blover");
    // Un champ que le dossier ne nomme pas remonte neanmoins des parcelles.
    assert(valeur(resolu, "cadastre.dossier.proprietaire") == "Kossi");
}

// La valeur commune est une donnee de la feuille : elle n'existe que si TOUTES
// les parcelles la portent.
void checkLaValeurCommuneRemonte() {
    core::Document document;
    document.addEntity(makeParcel("Lome", "Kossi"));
    document.addEntity(makeParcel("Lome", "Afiavi"));
    std::vector<validation::Diagnostic> diagnostics;

    const auto resolu = resoudre(document, diagnostics);
    assert(valeur(resolu, "cadastre.dossier.commune") == "Lome");
    // Des parcelles qui ne disent pas la meme chose ne font pas une affirmation
    // pour la feuille : le champ reste vide, la nomenclature porte la difference.
    assert(valeur(resolu, "cadastre.dossier.proprietaire").empty());
    assert(manqueSignale(diagnostics, "cadastre.dossier.proprietaire"));
}

// La premiere parcelle lue ne dicte rien. C'est l'assertion qui manquait : le
// defaut rendait « Anne » visible sur une feuille dont aucune parcelle ne
// portait ce nom en commun.
void checkLaPremiereParcelleNeDicteRien() {
    core::Document document;
    document.addEntity(makeParcel("Lome", "Anne", "A", "1"));
    document.addEntity(makeParcel("Kpalime", "Kossi", "B", "2"));
    std::vector<validation::Diagnostic> diagnostics;

    const auto resolu = resoudre(document, diagnostics);
    assert(valeur(resolu, "cadastre.dossier.commune").empty());
    assert(valeur(resolu, "cadastre.dossier.proprietaire").empty());
    // Les parcelles d'une feuille sont justement censees differer sur ces deux
    // champs : la nomenclature les porte, le cartouche non.
    assert(valeur(resolu, "cadastre.dossier.section").empty());
    assert(valeur(resolu, "cadastre.dossier.numero").empty());
}

// Un champ invente faute de saisie est une faussete, pas un arrangement. Le
// titre « Plan cadastral » est passe par la : il doit revenir vide, et aucun
// champ ne doit porter de valeur sur une feuille ou l'operateur n'a rien saisi.
void checkRienNestInvente() {
    core::Document vide;
    std::vector<validation::Diagnostic> diagnostics;
    const auto resolu = resoudre(vide, diagnostics);
    for (const auto& champ : resolu.fields) assert(!champ.hasValue());
    assert(valeur(resolu, "cadastre.dossier.projet").empty());
    assert(valeur(resolu, "cadastre.dossier.commune").empty());

    // Une cle que le gabarit ne nomme pas ne devient pas un champ : le module
    // ne resout que ce que le gabarit declare, il ne se remplit pas d'un
    // fourre-tout.
    core::Document inconnu;
    inconnu.properties().setString("cadastre.dossier.champ_inexistant", "valeur");
    std::vector<validation::Diagnostic> diagnosticsInconnu;
    const auto resoluInconnu = resoudre(inconnu, diagnosticsInconnu);
    for (const auto& champ : resoluInconnu.fields) assert(!champ.hasValue());

    // Le cartouche n'est pas le lieu de l'echelle : une feuille sans donnee
    // saisie ne doit pas en porter une — le littéral arrive avec la
    // composition, pas avec la résolution.
    core::Document seule;
    seule.addEntity(makeParcel("Lome", "Kossi", "A", "7"));
    std::vector<validation::Diagnostic> diagnosticsSeule;
    const auto resolue = resoudre(seule, diagnosticsSeule);
    assert(valeur(resolue, "cadastre.dossier.commune") == "Lome");
    assert(valeur(resolue, "cadastre.dossier.section") == "A");
    assert(valeur(resolue, "cadastre.dossier.numero") == "7");
    assert(valeur(resolue, "").empty());
}

// Le champ recherche d'abord dans le dossier, puis dans les parcelles : les
// deux regles coexistent, elles ne s'annulent pas.
void checkLesDeuxSourcesCoexistent() {
    core::Document document;
    document.addEntity(makeParcel("Lome", "Kossi"));
    document.addEntity(makeParcel("Lome", "Kossi"));
    withAttributes(document, "geometre", "Cabinet Adjo");
    withAttributes(document, "dossier", "D-2026-118");
    std::vector<validation::Diagnostic> diagnostics;

    const auto resolu = resoudre(document, diagnostics);
    assert(valeur(resolu, "cadastre.dossier.geometre") == "Cabinet Adjo");
    assert(valeur(resolu, "cadastre.dossier.dossier") == "D-2026-118");
    assert(valeur(resolu, "cadastre.dossier.commune") == "Lome");
    assert(valeur(resolu, "cadastre.dossier.proprietaire") == "Kossi");
}

// Un littéral du gabarit traverse la résolution tel quel : c'est par lui que
// l'échelle « 1:n » posée par l'appelant rejoint le cartouche.
void checkLeLiteralTraverse() {
    core::Document document;
    document.addEntity(makeParcel("Lome", "Kossi"));
    auto gabarit = defaultCartoucheTemplate();
    for (auto& attendu : gabarit.fields) {
        if (attendu.key.empty()) attendu.literal = "1:500";
    }
    std::vector<validation::Diagnostic> diagnostics;
    const auto resolu = buildCartoucheFurniture(document, gabarit, diagnostics);
    assert(valeur(resolu, "") == "1:500");
}

} // namespace

int main() {
    checkLeDossierPrime();
    checkLaValeurCommuneRemonte();
    checkLaPremiereParcelleNeDicteRien();
    checkRienNestInvente();
    checkLesDeuxSourcesCoexistent();
    checkLeLiteralTraverse();

    std::cout << "resolution du cartouche OK\n";
    return 0;
}
