cmake_minimum_required(VERSION 3.20)
project(bcad_cadastre_plugin LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Plugin cadastre : module métier dynamique (ADR-005)
# Ne lie AUCUNE bibliothèque de types Core : les vtables/typeinfo/registres
# sont celles de l'hôte (exposées via --export-dynamic-symbol dans l'app).
# Les factories sont des pointeurs de fonction bruts ré-emballés côté hôte.

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core)

# Sources du plugin
set(PLUGIN_SOURCES
    # Entités
    entities/ParcelEntity.cpp
    entities/BoundaryEntity.cpp
    entities/SurveyMarkEntity.cpp
    entities/EasementEntity.cpp

    # Commandes
    commands/CreateParcelCommand.cpp
    commands/SplitParcelCommand.cpp
    commands/FindParcelCommand.cpp
    ParcelOps.cpp

    # Mise en page de la feuille
    layout/CadastreSheet.cpp

    # Validation
    validation/CadastreValidators.cpp
    validation/ParcelOverlapValidator.cpp
    validation/ParcelIdentifierValidator.cpp
    validation/SurveyToleranceValidator.cpp

    # IO
    io/GeoJsonSerializer.cpp
    io/GeoPackageSerializer.cpp
    io/GeoPackageExporter.cpp
    io/ArcGisAdapter.cpp

    # UI
    ui/CadastreWorkbench.cpp

    # Gabarits de profil (lus via les repertoires de donnees de l'hote)
    Templates.cpp

    # Point d'entrée plugin
    cadastre_plugin.cpp
)

add_library(bcad_cadastre_plugin MODULE ${PLUGIN_SOURCES})

target_include_directories(bcad_cadastre_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/entities
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(bcad_cadastre_plugin PRIVATE
    bcad_plugin
    bcad_layout
    SQLite::SQLite3
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core    # QJsonDocument des gabarits (Templates.cpp)
)

set_target_properties(bcad_cadastre_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    PREFIX ""
    SUFFIX ""
)

# Les donnees du module doivent etre atteignables dans l'arbre de build comme
# dans l'installation, sous la MEME forme relative (<racine>/share/bcad/plugins/
# cadastre/templates) : l'hote propose "<binaire>/../../share/bcad/plugins" et le
# module ne fait que nommer son fichier. Sans cette copie, le gabarit n'existe
# que dans l'installation et n'est jamais lu en developpement.
add_custom_command(TARGET bcad_cadastre_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/templates
        ${CMAKE_BINARY_DIR}/share/bcad/plugins/cadastre/templates
    COMMENT "Copie des gabarits du module dans l'arbre de build")

install(DIRECTORY templates/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/bcad/plugins/cadastre/templates
    FILES_MATCHING PATTERN "*.json"
)

# Installation
install(TARGETS bcad_cadastre_plugin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/bcad/plugins
)