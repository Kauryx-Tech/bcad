#pragma once

#include "bcad/core/Document.h"
#include <string>

namespace bcad::io {

// Formats d'échange génériques. Ils ne connaissent que le modèle d'entité du
// noyau (géométrie + PropertyMap) : les champs d'un module métier sortent sans
// qu'aucune ligne du noyau ne nomme un de ces champs (ADR-016).

// FeatureCollection GeoJSON. Une entité dont la géométrie n'est pas un type du
// noyau est tessellée : elle reste dans le fichier au lieu d'en disparaître en
// silence.
std::string toGeoJson(const core::Document& document);

// Une ligne par point tessellé : identifiant, index, coordonnées.
std::string toCoordinateCsv(const core::Document& document);

// Écrit `text` en binaire à `path`. Faux sinon, message dans `error`.
bool writeTextFile(const std::string& path, const std::string& text, std::string* error);

} // namespace bcad::io
