#pragma once

#include "bcad/core/Document.h"
#include <string>
#include <vector>

namespace bcad::io {

// Format natif du projet .bcad : un unique fichier SQLite avec une table
// `layers`, une table `entities` (géométrie empaquetée sous forme de chaîne de
// paramètres compacte, avec le TypeId réel de l'entité) et une table
// `entity_properties` (une ligne par propriété : entité, clé, valeur typée en
// JSON). Choisi plutôt qu'un format binaire maison pour que les dessins restent
// inspectables/interrogeables avec n'importe quel outil SQLite, et pour que les
// futures fonctionnalités (journal d'annulation, blocs, xrefs) puissent ajouter
// des tables sans avoir à gérer une migration de format personnalisée.
//
// Compatibilité, dans l'ordre :
//   - v1 est lue (et ses six colonnes cadastrales deviennent des propriétés
//     génériques) ;
//   - v2 est lue (sans attributs du dossier ni feuilles : elle n'en portait
//     pas, la migration ne peut rien inventer) ;
//   - v3 est la seule écrite : entités, propriétés, attributs du dossier
//     (`document_properties`), feuilles, vues, meubles et champs déclaratifs
//     (`sheets`, `sheet_views`, `furniture`, `furniture_fields`) ;
//   - une version supérieure à la connue est refusée, le fichier n'est pas
//     touché ;
//   - aucune propriété lue n'est écrasée par une valeur que l'hôte ne comprend
//     pas : ce qu'il ne comprend pas est conservé tel quel.
class Database {
public:
    static bool save(const std::string& path, const core::Document& doc);

    // Remplit outDoc sur place (en le vidant d'abord) plutôt que de retourner
    // un Document par valeur : Document contient un std::shared_mutex
    // protégeant son index spatial, ce qui le rend volontairement
    // non copiable/non déplaçable.
    // diagnostics (optionnel) : si non nul, reçoit un message par type inconnu
    // conservé en UnknownEntity et par valeur de propriété non déchiffrable.
    // Permet à l'hôte d'avertir l'utilisateur sans bloquer le chargement.
    static bool load(const std::string& path, core::Document& outDoc,
                     std::vector<std::string>* diagnostics = nullptr);

    // Version de schéma du fichier, ou -1 si ce n'est pas une base lisible.
    static int schemaVersion(const std::string& path);

    // Monte un fichier v1 ou v2 sur place, dans une transaction unique (tout ou
    // rien). Idempotente sur du v3, refus sur une version inconnue du futur.
    // Un ouvrir/enregistrer produit déjà du v3 : cette voie sert à mettre
    // niveau un fichier sans le charger dans une session.
    static bool migrateSchema(const std::string& path);
};

} // namespace bcad::io
