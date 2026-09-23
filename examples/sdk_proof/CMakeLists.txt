cmake_minimum_required(VERSION 3.20)
project(bcad_sdk_proof LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Phase 9 proof: SDK usable from an external project via find_package(BCAD)
find_package(BCAD CONFIG REQUIRED)

# --- Consumer (Phase 9) : lien contre les targets importes ---
add_executable(bcad_sdk_consumer consumer/consumer.cpp)
target_link_libraries(bcad_sdk_consumer PRIVATE BCAD::bcad_core BCAD::bcad_geometry)
set_target_properties(bcad_sdk_consumer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- Minimal external plugin (Phase 10) : module charge via dlopen ---
# Construit contre les headers du SDK ; ne lie AUCUNE bibliotheque de types :
# les entites/typeinfo/vtables sont celles de l'hote (exposees via
# --export-dynamic-symbol, voir loader). L'enregistrement des extensions passe
# par le PluginRegistry (ADR-005), mediatise par l'hote. Les factories passees
# par le plugin sont des POINTEURS DE FONCTION : l'hote les re-emballe dans des
# std::function definis cote hote, donc aucun manager/invoker du plugin ne
# survit a un dlclose (pas de SEGV au teardown).
add_library(bcad_hello_plugin MODULE plugin/hello_plugin.cpp)
target_link_libraries(bcad_hello_plugin PRIVATE BCAD::bcad_plugin)
set_target_properties(bcad_hello_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugin)

# --- Loader (Phase 10) : charge le plugin, verifie metadonnees + enregistrements
# effectifs dans les registres globaux, puis decharge ---
# L'executable est l'HOTE : il porte les registres (registry/commands/
# serialization) et les types (core/geometry/...), et exporte ses symboles
# (--export-dynamic-symbol, substitut de -rdynamic) pour que libbcad_plugin.so
# et le plugin charge (dlopen) resolvent leurs references vers l'instance de
# l'hote : une seule instance des registres ET une seule copie des
# vtables/typeinfo (dynamic_cast inter-DSO fonctionnel). -rdynamic n'est pas
# utilisable : il entraine un crash de teardown deteministe.
add_executable(bcad_plugin_loader loader/plugin_loader.cpp)
target_link_libraries(bcad_plugin_loader PRIVATE
    BCAD::bcad_plugin)
# L'hote embarque TOUS les objets des libs de types/services (whole-archive)
# pour porter et exporter l'ensemble des symboles bcad (vtables/typeinfo des
# entites, registres, ...) : le plugin charge (dlopen) ne lie aucune bibliotheque.
# NOTE portabilite : les options -Wl,--whole-archive et --export-dynamic-symbol
# sont propres a GNU/Clang (ld binutils). Sur MSVC, l'equivalent est
# /WHOLEARCHIVE:<lib> (option de lien par bibliotheque) et l'export de symboles
# se fait via __declspec(dllexport) sur les classes, pas via une option de lien.
# La preuve cible uniquement les chaines GNU/Clang sur Linux.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_link_libraries(bcad_plugin_loader PRIVATE
        BCAD::bcad_geometry BCAD::bcad_properties BCAD::bcad_core
        BCAD::bcad_registry BCAD::bcad_commands BCAD::bcad_serialization)
    foreach(lib IN ITEMS bcad_geometry bcad_properties bcad_core
                 bcad_registry bcad_commands bcad_serialization)
        target_link_options(bcad_plugin_loader PRIVATE
            "/WHOLEARCHIVE:$<TARGET_FILE:BCAD::${lib}>")
    endforeach()
else()
    target_link_libraries(bcad_plugin_loader PRIVATE
        -Wl,--whole-archive
        BCAD::bcad_geometry
        BCAD::bcad_properties
        BCAD::bcad_core
        BCAD::bcad_registry
        BCAD::bcad_commands
        BCAD::bcad_serialization
        -Wl,--no-whole-archive)
endif()
# Export cible des seuls symboles bcad (GNU/Clang, binutils >= 2.25). Les globs
# couvrent les fonctions (ZN/ZNK), les vtables (ZTV), les typeinfo (ZTI), les
# strings de typeid (ZTS) et les initialisateurs de typeinfo (ZGV). Ne pas
# exporter les autres symboles evite les collisions de weak symbols entre DSO.
# (Equivalence Windows : non fournie par option de lien ; les classes hote
# partagees devraient etre marquees __declspec(dllexport) — hors perimetre de
# la preuve, cible GNU/Clang sur Linux uniquement.)
target_link_options(bcad_plugin_loader PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:
        -Wl,--export-dynamic-symbol=_ZN4bcad*
        -Wl,--export-dynamic-symbol=_ZNK4bcad*
        -Wl,--export-dynamic-symbol=_ZTVN4bcad*
        -Wl,--export-dynamic-symbol=_ZTIN4bcad*
        -Wl,--export-dynamic-symbol=_ZTSN4bcad*
        -Wl,--export-dynamic-symbol=_ZGVN4bcad*>)
set_target_properties(bcad_plugin_loader PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/loader)