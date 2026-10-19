#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bcad::cadastre {

// Systemes de coordonnees proposes par pays (K-06), lus dans la donnee du
// module `cadastre/crs/systemes_coordonnees.json`. Ce sont des propositions
// sourcees, « a valider » par un geometre de chaque pays : le module ne
// choisit pas a la place de l'operateur et n'invente aucune regle (ADR-016).
struct SystemeCoordonnees {
    int epsg = 0;
    std::string nom;      // « WGS 84 / UTM zone 31N »
    std::string usage;    // « a l'est de 6°W »…
    std::string pays;     // code ISO : TG, BJ, CI…
    bool valide = false;  // confirme par un geometre du pays
};

// Liste lue dans un fichier ; vide s'il est absent, illisible ou d'un schema
// inconnu.
std::vector<SystemeCoordonnees> lireSystemesCoordonnees(const std::string& cheminJson);
// La meme, cherchee dans les repertoires de donnees du module.
std::vector<SystemeCoordonnees> systemesCoordonneesInstalles();

// Le code EPSG d'une saisie : « 32631 », « EPSG:32631 », « epsg 32631 ».
std::optional<int> codeEpsgSaisi(std::string_view saisie);

// Ce que le cartouche imprime : « WGS 84 / UTM zone 31N (EPSG:32631) », ou
// « EPSG:1234 (non répertorié) » pour un code absent de la liste.
std::string libelleSysteme(int epsg, const std::vector<SystemeCoordonnees>& connus);

} // namespace bcad::cadastre
