#pragma once

// Seule constante que la répartition de Viewport en unités de traduction
// doive partager : la tolérance de pointage, exprimée en pixels d'écran, sert
// à la fois quand la souris choisit une entité (ViewportInput) et quand un
// outil remplace une entité pointée ou joint deux extrémités qui se touchent
// (ViewportEditing, par `pickModifiableEntity`).
//
// `inline constexpr` plutôt qu'un objet anonyme recopié par unité : un scalaire
// sans état ne risque rien à être dupliqué, mais une définition unique écarte
// ici le seul décalage que la découpe pourrait introduire — un outil qui
// pointerait plus loin qu'un autre. Comparez avec `kTools` de MainWindow, dont
// une copie par unité aurait perdu la synchronisation menu/ruban sans erreur
// de compilation.

namespace bcad::app {

inline constexpr double kPickToleranceScreenPx = 6.0;

} // namespace bcad::app
