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
# Construit contre le SDK ; lie bcad_geometry pour resoudre les symboles des
# entites manipulees (PointEntity/Property). L'enregistrement des extensions
# passe par le PluginRegistry (ADR-005), mediatise par l'hote libbcad_plugin.
add_library(bcad_hello_plugin MODULE plugin/hello_plugin.cpp)
target_link_libraries(bcad_hello_plugin PRIVATE BCAD::bcad_geometry)
set_target_properties(bcad_hello_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugin)

# --- Loader (Phase 10) : charge le plugin, verifie metadonnees + enregistrements
# effectifs dans les registres globaux, puis decharge ---
# L'executable est l'HOTE : il porte les registres (registry/commands/
# serialization, en visibilite par defaut) et libbcad_plugin.so (DT_NEEDED)
# y resout ses references non definies -> une seule instance des registres,
# partagee avec le plugin charge. Tout hote qui charge des plugins doit lier
# ces trois bibliotheques.
add_executable(bcad_plugin_loader loader/plugin_loader.cpp)
target_link_libraries(bcad_plugin_loader PRIVATE
    BCAD::bcad_plugin BCAD::bcad_registry BCAD::bcad_commands BCAD::bcad_serialization)
set_target_properties(bcad_plugin_loader PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/loader)