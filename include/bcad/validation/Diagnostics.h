#pragma once

// Resultat d'une regle de verification (ADR-003/005 : le core ne fournit que le
// MECANISME, les regles viennent d'un plugin). Ce type ne connait aucun metier :
// un message est une chaine fournie par celui qui la produit.

#include <string>
#include <vector>

namespace bcad::validation {

enum class Severity {
    Info,     // information sans consequence
    Warning,  // suspect, le document reste utilisable
    Error     // contraire a une regle, a corriger
};

// Une constatation, rattachee aux entites concernees (ids de geom::Entity::id(),
// 0 = ne porte sur aucune entite precise).
struct Diagnostic {
    Severity severity = Severity::Info;
    std::string message;
    std::vector<int> entityIds;
};

} // namespace bcad::validation
