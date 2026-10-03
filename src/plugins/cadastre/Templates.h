#pragma once

#include <string>
#include <string_view>
#include <vector>

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
    // Code du profil demande. Il est garde meme quand `source` est vide : une
    // regle qui s'applique sans gabarit trouve doit le dire, sinon l'operateur
    // croit verifier son dossier contre un profil qui n'existe pas.
    std::string profile;
    // Échelles admises (1:n) par le profil national. Vide = le profil n'en
    // donne aucune : le module applique ses valeurs par défaut
    // (`defaultPermittedScales`). En dernier pour ne pas casser les
    // constructions positionnelles existantes.
    std::vector<int> permittedScales;
};

// Le profil que le module applique quand le dossier n'en nomme aucun. C'est une
// donnee du module, pas de l'hote : src/app ne voit jamais ce nom (ADR-016).
constexpr const char* kProfilParDefaut = "cadastre_togo";

// Un code de profil devient un composant de chemin (`templates/<code>.json`) sous
// la saisie de l'operateur : il doit donc rester un nom, pas un chemin. Sans
// lettre au debut, sans `..`, sans separateur — tout le reste n'est pas un
// profil, et le refuser ici vaut mieux qu'ouvrir un fichier hors des donnees du
// module.
bool estCodeDeProfilValide(std::string_view code);

// Ouvre `cadastre/templates/<profile>.json` dans les repertoires de donnees que
// l'hote a proposes au module : le module nomme un fichier, il ne nomme jamais
// un chemin d'installation (ADR-016).
//
// `profile` est le nom du profil sans chemin ni extension ; le module choisit
// son profil par defaut tant que l'operateur n'a rien designe.
//
// Un fichier retenu remplace les valeurs qu'il donne, et seulement elles : une
// cle absente ou vide laisse la valeur par defaut du module. Une donnee mal
// ecrite ne doit pas desarmer la verification, elle doit la rendre visible dans
// les diagnostics.
CadastreTemplates loadCadastreTemplates(const plugin::PluginRegistry& registry,
                                        const std::string& profile = kProfilParDefaut);

// Les repertoires que l'hote a proposes au module, copies pendant
// bcad_plugin_init. Le module y range une liste de chemins, jamais une facon de
// les construire : c'est l'hote qui les choisit, et la regle de recherche reste
// « le premier repertoire qui contient le fichier ».
//
// Sans cette copie, le module ne pourrait ouvrir un gabarit QUE pendant son
// initialisation : `PluginRegistry` meurt a la sortie de `bcad_plugin_init` et
// les factories de commande sont des pointeurs de fonction, donc incapables de
// le capter. Or l'operateur designe son profil apres.
void memoriserRepertoiresDeDonnees(const std::vector<std::string>& repertoires);

// Premier fichier `<repertoire>/<cheminRelatif>` existant parmi les repertoires
// memorises, en chemin absolu ; vide s'il n'existe nulle part. Le chemin est
// fixe par le module (ex. "cadastre/icons/parcel-new.svg"), jamais saisi.
std::string trouverFichierDeDonnees(const std::string& cheminRelatif);

// Le gabarit du profil nomme, cherche dans les repertoires memorises. Un code
// refuse, un profil absent ou un fichier illisible rendent un gabarit dont
// `source` est vide : l'appelant voit la difference entre « trouve » et « valeurs
// par defaut du module », il ne la devine pas.
CadastreTemplates chargerGabaritDuProfil(const std::string& codeDuProfil);

} // namespace bcad::cadastre
