#pragma once

#include <string>
#include <vector>

namespace bcad::core {
class Document;
}

namespace bcad::cadastre {

// Proces-verbal de bornage contradictoire (K-07) : ce que le dessin et le
// dossier savent deja, mis en forme pour etre imprime, complete et signe sur
// le terrain. Rien n'est invente : un champ non saisi reste une ligne vide a
// remplir a la main, une presence non renseignee des cases a cocher. L'etat
// des bornes (posee, reconnue, manquante) se constate au bornage : il est
// imprime en cases, pas devine.
//
// Memes numeros de borne, memes riverains que le plan (riverainRows,
// coordinateRows) : le PV et le plan ne peuvent pas se contredire.
struct PvBorne {
    std::string numero;
    double x = 0.0;
    double y = 0.0;
};

struct PvLimite {
    std::string de;
    std::string vers;
    std::string riverain;   // « — » sans voisin leve ni nom saisi
    std::string presence;   // vide si non renseignee
};

struct PvParcelle {
    std::string designation;
    std::string proprietaire;
    std::string commune;
    std::string contenance;   // contenance de l'acte, telle que saisie
    double surface = 0.0;     // surface nette calculee, m²
    std::vector<PvBorne> bornes;
    std::vector<PvLimite> limites;
};

struct ProcesVerbal {
    std::string date;
    std::string geometre;
    std::string agrement;
    std::string dossier;
    std::string commune;
    std::string systeme;
    std::vector<PvParcelle> parcelles;
    // Parties a faire signer, apres le geometre : le proprietaire de chaque
    // parcelle (un riverain leve en est un), puis chaque riverain non leve
    // nomme a la main, une seule fois. Un nom vide est une ligne a remplir.
    struct Signataire {
        std::string qualite;
        std::string nom;
    };
    std::vector<Signataire> signataires;
};

ProcesVerbal buildProcesVerbal(const core::Document& document);

// Le PV en HTML (sous-ensemble rendu par QTextDocument), toutes les saisies
// echappees : un nom ne peut pas devenir du balisage.
std::string procesVerbalHtml(const ProcesVerbal& pv);

std::string htmlEscape(const std::string& text);

} // namespace bcad::cadastre
