#pragma once

// Macro d'export de la bibliotheque hote (libbcad_plugin). Seul l'API publique
// est exportee : le reste est compile avec -fvisibility=hidden.
//
// Header separe pour que tout en-tete du module `plugin` (PluginRegistry,
// Plugin, Workbench) puisse l'utiliser sans cycle d'inclusion.

#if defined(_WIN32)
#  if defined(BCAD_PLUGIN_BUILDING)
#    define BCAD_PLUGIN_API __declspec(dllexport)
#  else
#    define BCAD_PLUGIN_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#  define BCAD_PLUGIN_API __attribute__((visibility("default")))
#else
#  define BCAD_PLUGIN_API
#endif
