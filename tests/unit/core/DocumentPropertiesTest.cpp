// Les attributs du dossier sont une donnee du document, pas du dessin (ADR-017,
// tranche 1). Sans eux, le cartouche ne peut rien afficher que ne lui dicte une
// parcelle, et le livrable porte des affirmations que personne n'a saisies. Ce
// test tient les trois proprietes qui rendent le conteneur utilisable : il
// porte ce qu'on y met, il ne se mele pas de la geometrie, et il se vide avec
// le document — un « Nouveau » qui laisserait le projet precedent accroche
// serait le pire des defauts, parce qu'il produit un plan faux et croyable.
//
// Les appels a effet de bord sont hors des assert() : en RelWithDebInfo NDEBUG
// est defini et assert() n'evalue pas son argument.

#include "bcad/core/Document.h"
#include "bcad/geometry/Point.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/properties/PropertyMap.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace bcad;

namespace {

std::unique_ptr<geom::Entity> makeParcel() {
    return std::make_unique<geom::PolylineEntity>(std::vector<geom::Point2>{
        {0, 0}, {10, 0}, {10, 10}, {0, 10}});
}

void checkPorteCeQuOnYMet() {
    core::Document document;
    assert(document.properties().listNames().empty());
    assert(document.properties().getString("cadastre.dossier.projet").empty());

    document.properties().setString("cadastre.dossier.projet", "Zones A et B");
    document.properties().setString("cadastre.dossier.geometre", "K. Mensah");

    assert(document.properties().getString("cadastre.dossier.projet") == "Zones A et B");
    assert(document.properties().has("cadastre.dossier.geometre"));
    // Une cle pose par une saisie ulterieure remplace la precedente, elle ne
    // s'ajoute pas a cote : deux valeurs du meme champ du cartouche n'ont pas
    // de sens.
    document.properties().setString("cadastre.dossier.projet", "Zone A");
    assert(document.properties().getString("cadastre.dossier.projet") == "Zone A");
    assert(document.properties().listNames().size() == 2);
}

// Les attributs ne passent par aucun chemin geometrique : ni entite, ni index,
// ni emprise. C'est ce qui les distingue d'une entite de texte posee sur la
// feuille — et ce qui rend leur absence de persistance presente sans risque
// pour le dessin.
void checkNeSeMelePasDeGeometrie() {
    core::Document document;
    document.addEntity(makeParcel());
    const geom::BoundingBox avant = document.extents();

    for (int i = 0; i < 30; ++i)
        document.properties().setString("cle_" + std::to_string(i), "valeur " + std::to_string(i));

    assert(document.entities().size() == 1);
    const geom::BoundingBox apres = document.extents();
    assert(apres.minX == avant.minX && apres.maxX == avant.maxX);
    assert(apres.minY == avant.minY && apres.maxY == avant.maxY);
    assert(document.entitiesInRegion(apres).size() == 1);
}

// « Nouveau », et tout import qui remplace le contenu, passent par clear().
void checkSeVideAvecLeDocument() {
    core::Document document;
    document.addEntity(makeParcel());
    document.properties().setString("cadastre.dossier.projet", "Dossier precedent");
    document.properties().setString("cadastre.dossier.profil", "cadastre_togo");

    document.clear();

    assert(document.entities().empty());
    assert(document.properties().listNames().empty());
    assert(!document.properties().has("cadastre.dossier.projet"));
    assert(!document.properties().has("cadastre.dossier.profil"));
}

} // namespace

int main() {
    checkPorteCeQuOnYMet();
    checkNeSeMelePasDeGeometrie();
    checkSeVideAvecLeDocument();

    std::cout << "attributs du dossier OK\n";
    return 0;
}
