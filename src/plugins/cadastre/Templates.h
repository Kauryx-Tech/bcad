#pragma once

#include <string>

namespace bcad::plugin {
class PluginRegistry;
}

namespace bcad::cadastre {

// Profil cadastral lu dans le gabarit du module. Les valeurs par defaut sont
// celles du profil historique : sans gabarit installe, le module verifie
// exactement la meme chose qu'avant la mise en service des fichiers de profil.
struct CadastreTemplates {
    std::string sectionPattern = "^[A-Z]{1,3}$";
    std::string numberPattern = "^[0-9]+$";
    // Chemin du gabarit applique. Vide quand aucun fichier n'a ete retenu
    // (absent, illisible, JSON invalide, schema inconnu) : toutes les valeurs
    // restent alors celles du module.
    std::string source;
};

// Ouvre `cadastre/templates/<profile>.json` dans les repertoires de donnees que
// l'hote a proposes au module : le module nomme un fichier, il ne nomme jamais
// un chemin d'installation (ADR-016).
//
// `profile` est le nom du profil sans chemin ni extension ; le module choisit
// son profil par defaut tant qu'aucun reglage utilisateur n'existe.
//
// Un fichier retenu remplace les valeurs qu'il donne, et seulement elles : une
// cle absente ou vide laisse la valeur par defaut du module. Une donnee mal
// ecrite ne doit pas desarmer la verification, elle doit la rendre visible dans
// les diagnostics.
CadastreTemplates loadCadastreTemplates(const plugin::PluginRegistry& registry,
                                        const std::string& profile = "cadastre_togo");

} // namespace bcad::cadastre
