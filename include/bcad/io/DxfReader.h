#pragma once

#include "bcad/core/Document.h"
#include <string>

namespace bcad::io {

// Lit le sous-ensemble DXF produit par writeDxf(), et est suffisamment
// permissif pour importer une géométrie LINE/CIRCLE/ARC/LWPOLYLINE/POLYLINE/
// TEXT/MTEXT simple depuis des fichiers exportés par d'autres outils de CAO
// (AutoCAD, LibreCAD, QCAD).
//
// Les XDATA BCAD_PROPS sont relus et rendus au PropertyMap de l'entité avec
// leur type ; l'ancien appid BCAD_CADASTRE (paires clé/valeur) est lu par
// compatibilité, préfixé « cadastre. », mais n'est plus écrit. Une XDATA d'un
// appid inconnu est ignorée, jamais refusée.
//
// Remplit outDoc sur place (en le vidant d'abord) plutôt que de retourner un
// Document par valeur : Document contient un std::shared_mutex protégeant
// son index spatial, ce qui le rend volontairement non copiable/non
// déplaçable.
bool readDxf(const std::string& path, core::Document& outDoc);

} // namespace bcad::io
