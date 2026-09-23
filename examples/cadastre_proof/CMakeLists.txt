cmake_minimum_required(VERSION 3.20)
project(bcad_cadastre_proof LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Squelette de MODULE METIER cadastre (preparation terrain, Phase 13).
# Meme recette que examples/sdk_proof : un module domaine est un plugin
# externe consomme via find_package(BCAD). L'entite `cadastre.parcel`
# (parcelle = polygone ferme + donnees cadastrales par proprietes) n'existe
# que dans ce module : elle est inconnue du Core (mediation par l'hote, ADR-005).
find_package(BCAD CONFIG REQUIRED)

# --- Consumer externe (Phase 9) : demonte une parcelle via le SDK ---
add_executable(bcad_cadastre_consumer consumer/consumer.cpp)
target_link_libraries(bcad_cadastre_consumer PRIVATE BCAD::bcad_core BCAD::bcad_geometry)
set_target_properties(bcad_cadastre_consumer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- Module metier cadastre (plugin) : entite + commande + serializer ---
# Construit contre les headers du SDK ; ne lie AUCUNE bibliotheque de types :
# les entites/typeinfo/vtables sont celles de l'hote. Les factories passees
# par le plugin sont des pointeurs de fonction re-emballes par l'hote (aucun
# std::function du plugin ne survit a un dlclose, cf. CommentairePhase).
add_library(bcad_cadastre_plugin MODULE plugin/cadastre_plugin.cpp)
target_link_libraries(bcad_cadastre_plugin PRIVATE BCAD::bcad_plugin)
set_target_properties(bcad_cadastre_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugin)

# --- Loader hote (Phase 10) : charge le module metier, verifie que ses
# enregistrements (entite cadastre.parcel, commande, serializer) ont bien
# abouti dans les registres globaux, puis decharge proprement ---
add_executable(bcad_cadastre_loader loader/cadastre_loader.cpp)
target_link_libraries(bcad_cadastre_loader PRIVATE BCAD::bcad_plugin)
# Meme strategie d'export de symboles hote que la preuve SDK (voir
# examples/sdk_proof/CMakeLists.txt) : l'hote porte et exporte les
# vtables/typeinfo/registres pour que le plugin dlopen partage les types.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_link_libraries(bcad_cadastre_loader PRIVATE
        BCAD::bcad_geometry BCAD::bcad_properties BCAD::bcad_core
        BCAD::bcad_registry BCAD::bcad_commands BCAD::bcad_serialization)
    foreach(lib IN ITEMS bcad_geometry bcad_properties bcad_core
                 bcad_registry bcad_commands bcad_serialization)
        target_link_options(bcad_cadastre_loader PRIVATE
            "/WHOLEARCHIVE:$<TARGET_FILE:BCAD::${lib}>")
    endforeach()
else()
    target_link_libraries(bcad_cadastre_loader PRIVATE
        -Wl,--whole-archive
        BCAD::bcad_geometry
        BCAD::bcad_properties
        BCAD::bcad_core
        BCAD::bcad_registry
        BCAD::bcad_commands
        BCAD::bcad_serialization
        -Wl,--no-whole-archive)
endif()
# Export cible des seuls symboles bcad (meme jeu que la preuve SDK, cf ADR-005)
target_link_options(bcad_cadastre_loader PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:
        -Wl,--export-dynamic-symbol=_ZN4bcad*
        -Wl,--export-dynamic-symbol=_ZNK4bcad*
        -Wl,--export-dynamic-symbol=_ZTVN4bcad*
        -Wl,--export-dynamic-symbol=_ZTIN4bcad*
        -Wl,--export-dynamic-symbol=_ZTSN4bcad*
        -Wl,--export-dynamic-symbol=_ZGVN4bcad*>)
set_target_properties(bcad_cadastre_loader PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/loader)
