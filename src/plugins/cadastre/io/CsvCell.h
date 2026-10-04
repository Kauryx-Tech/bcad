#pragma once

// Ecriture CSV commune aux exports tabulaires du module, pour un champ TEXTE
// saisi par l'operateur ou venu d'un fichier recu (nom de parcelle…) :
//   - un texte commencant par = + - @ (ou tabulation, retour chariot) serait
//     execute comme une formule par le tableur a l'ouverture du CSV : il est
//     prefixe d'une apostrophe, qui le fait lire comme du texte ;
//   - un texte portant le separateur, un guillemet ou un saut de ligne est mis
//     entre guillemets doubles, pour ne pas casser la ligne.
// Les nombres calcules par le module ne passent pas par ici.

#include <string>

namespace bcad::cadastre {

inline std::string csvCell(const std::string& text) {
    std::string safe = text;
    if (!safe.empty() && std::string("=+-@\t\r").find(safe.front()) != std::string::npos)
        safe.insert(safe.begin(), '\'');
    if (safe.find_first_of(";\"\n\r") == std::string::npos) return safe;
    std::string quoted = "\"";
    for (char c : safe) quoted += (c == '"') ? std::string("\"\"") : std::string(1, c);
    return quoted + "\"";
}

} // namespace bcad::cadastre
