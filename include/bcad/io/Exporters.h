#pragma once

namespace bcad::io {

// Enregistre les formats d'échange génériques du noyau (GeoJSON, CSV de
// coordonnées, DXF) dans le registre des exporteurs. Idempotent : un second
// appel ne remplace ni ne duplique rien. Ces formats tombent dans le même
// registre que ceux déclarés par un module métier, donc l'hôte dresse son menu
// sans nommer aucun format (ADR-016).
void initializeNativeFileExporters();

} // namespace bcad::io
