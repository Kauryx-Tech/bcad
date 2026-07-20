#pragma once

#include "bcad/core/Document.h"
#include <string>

namespace bcad::io {

// Format natif du projet .bcad : un unique fichier SQLite avec une table
// `layers` et une table `entities` (géométrie empaquetée sous forme de
// chaîne de paramètres compacte). Choisi plutôt qu'un format binaire maison
// pour que les dessins restent inspectables/interrogeables avec n'importe
// quel outil SQLite, et pour que les futures fonctionnalités (journal
// d'annulation, blocs, xrefs) puissent ajouter des tables sans avoir à gérer
// une migration de format personnalisée.
class Database {
public:
    static bool save(const std::string& path, const core::Document& doc);

    // Remplit outDoc sur place (en le vidant d'abord) plutôt que de retourner
    // un Document par valeur : Document contient un std::shared_mutex
    // protégeant son index spatial, ce qui le rend volontairement
    // non copiable/non déplaçable.
    static bool load(const std::string& path, core::Document& outDoc);
};

} // namespace bcad::io
