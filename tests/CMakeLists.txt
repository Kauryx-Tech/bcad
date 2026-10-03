# Les tests sont ecrits avec assert(), mais RelWithDebInfo et Release definissent
# NDEBUG : un assert() n'evalue alors meme pas son argument et la verification
# disparait sans bruit. On le leve pour toute la sous-direction. Le faire par
# cible laissait filer les nouvelles cibles oubliees de l'appel explicite —
# 15 d'entre elles tournaient sans verifier quoi que ce soit.
if(MSVC)
    add_compile_options(/UNDEBUG)
else()
    add_compile_options(-UNDEBUG)
endif()

add_executable(bcad_smoke_test smoke_test.cpp)

# Le test de fumee exerce le parseur de coordonnees de l'hote, qui est prive
# (src/app/CoordinateInput.h) : include/bcad/ ne contient que l'API installee.
# Chemin limite a src/app plutot qu'a src/ entier, pour que le test ne puisse
# pas atteindre les en-tetes prives des autres modules par accident.
target_include_directories(bcad_smoke_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/app
)

target_link_libraries(bcad_smoke_test PRIVATE
    bcad_geometry
    bcad_layers
    bcad_index
    bcad_core
    bcad_io
)

add_test(NAME smoke_test COMMAND bcad_smoke_test)

# Architecture test - verifies core has no render dependency
add_executable(bcad_arch_test arch_test.cpp)

target_link_libraries(bcad_arch_test PRIVATE
    bcad_core
    bcad_geometry
    bcad_layers
    bcad_index
    bcad_io
)

add_test(NAME arch_test COMMAND bcad_arch_test)

# TypeId stability test
add_executable(bcad_typeid_test typeid_stability_test.cpp)

target_link_libraries(bcad_typeid_test PRIVATE
    bcad_geometry
    bcad_registry
    bcad_serialization
    bcad_core
)

add_test(NAME typeid_stability_test COMMAND bcad_typeid_test)

# Round-trip serialization test per native entity
add_executable(bcad_roundtrip_test roundtrip_test.cpp)

target_link_libraries(bcad_roundtrip_test PRIVATE
    bcad_geometry
    bcad_registry
    bcad_serialization
    bcad_core
)

add_test(NAME roundtrip_test COMMAND bcad_roundtrip_test)

# Command system test
add_executable(bcad_command_test command_test.cpp)

target_link_libraries(bcad_command_test PRIVATE
    bcad_commands
    bcad_core
    bcad_geometry
)

add_test(NAME command_test COMMAND bcad_command_test)

# SDK ABI test - verifies external project can use installed SDK
add_executable(bcad_sdk_abi_test sdk_abi_test.cpp)

target_link_libraries(bcad_sdk_abi_test PRIVATE
    bcad_core
    bcad_geometry
    bcad_registry
    bcad_serialization
    bcad_events
    bcad_commands
)

add_test(NAME sdk_abi_test COMMAND bcad_sdk_abi_test)

# Command pattern reference test
add_executable(bcad_command_pattern_test unit/commands/CommandPatternTest.cpp)

target_link_libraries(bcad_command_pattern_test PRIVATE
    bcad_commands
    bcad_core
    bcad_geometry
    bcad_registry
    bcad_serialization
)

add_test(NAME command_pattern_test COMMAND bcad_command_pattern_test)

# GeometryUtils2 test
add_executable(bcad_geometry_utils2_test unit/geometry/GeometryUtils2Test.cpp)

target_link_libraries(bcad_geometry_utils2_test PRIVATE
    bcad_geometry
    bcad_core
)

add_test(NAME geometry_utils2_test COMMAND bcad_geometry_utils2_test)

# Hole test (polygons with holes) — round-trip via SerializerRegistry
add_executable(bcad_hole_test unit/geometry/HoleTest.cpp)

target_link_libraries(bcad_hole_test PRIVATE
    bcad_geometry
    bcad_registry
    bcad_serialization
    bcad_core
)

add_test(NAME hole_test COMMAND bcad_hole_test)

# SDK installable (Phase 9) + plugin externe minimal (Phase 10) :
# installe BCAD, compile un projet externe via find_package(BCAD CONFIG REQUIRED)
# et execute le consommateur puis le plugin.
if(NOT CMAKE_CROSSCOMPILING)
    add_test(NAME sdk_external_test
        COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/prove_sdk.sh
                ${CMAKE_COMMAND} ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
    set_tests_properties(sdk_external_test PROPERTIES TIMEOUT 900)

    add_test(NAME cadastre_external_test
        COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/prove_cadastre.sh
                ${CMAKE_COMMAND} ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
    set_tests_properties(cadastre_external_test PROPERTIES TIMEOUT 900)

    # Test d'intégration draw → save → load → undo/redo
    add_executable(bcad_roundtrip_undo_test integration/document_roundtrip_undo_test.cpp)
    target_link_libraries(bcad_roundtrip_undo_test PRIVATE
        bcad_core bcad_io bcad_commands bcad_geometry bcad_events)
    add_test(NAME roundtrip_undo_test COMMAND bcad_roundtrip_undo_test)

    add_executable(bcad_drawing_print_roundtrip_test
        integration/drawing_print_roundtrip_test.cpp)
    target_link_libraries(bcad_drawing_print_roundtrip_test PRIVATE
        bcad_layout bcad_io bcad_core bcad_geometry bcad_properties
        bcad_registry bcad_serialization Qt6::Gui Qt6::Widgets Qt6::PrintSupport)
    add_test(NAME drawing_print_roundtrip_test
        COMMAND bcad_drawing_print_roundtrip_test)

    # Test PropertyMap
    add_executable(bcad_propertymap_test unit/properties/PropertyMapTest.cpp)
    target_link_libraries(bcad_propertymap_test PRIVATE bcad_properties bcad_core bcad_geometry)
    add_test(NAME propertymap_test COMMAND bcad_propertymap_test)

    # Phase 6 : PropertyChanged multi-abonnés
    add_executable(bcad_propertychanged_multi_test unit/events/PropertyChangedMultiSubscriberTest.cpp)
    target_link_libraries(bcad_propertychanged_multi_test PRIVATE bcad_events bcad_core bcad_geometry bcad_properties)
    add_test(NAME propertychanged_multi_test COMMAND bcad_propertychanged_multi_test)

    # Phase 6 : SetEntityPropertyCommand undo/redo/clone/merge
    add_executable(bcad_set_entity_property_command_test unit/commands/SetEntityPropertyCommandTest.cpp)
    target_link_libraries(bcad_set_entity_property_command_test PRIVATE bcad_commands bcad_core bcad_geometry bcad_properties)
    add_test(NAME set_entity_property_command_test COMMAND bcad_set_entity_property_command_test)

    # Phase 6 : PropertiesPanel multi-type + multi-selection (logique sans UI)
    add_executable(bcad_properties_panel_multi_test unit/app/PropertiesPanelMultiTypeTest.cpp)
    target_link_libraries(bcad_properties_panel_multi_test PRIVATE bcad_core bcad_geometry bcad_properties)
    add_test(NAME properties_panel_multi_test COMMAND bcad_properties_panel_multi_test)

    # Les tests qui passent par le bridge Rust n'existent que si le bridge
    # existe : bcad_io_ffi n'est construit que sous BCAD_ENABLE_RUST. Les lier
    # sans condition donne « cannot find -lbcad_io_ffi » sur une build sans Rust.
    if(BCAD_ENABLE_RUST)
        # Phase 6 : Round-trip DXF via Rust (read -> write -> read)
        add_executable(bcad_dxf_roundtrip_rust_test unit/io/DxfRoundtripRustTest.cpp)
        target_link_libraries(bcad_dxf_roundtrip_rust_test PRIVATE bcad_io_ffi bcad_io bcad_core bcad_geometry)
        add_test(NAME dxf_roundtrip_rust_test COMMAND bcad_dxf_roundtrip_rust_test)

        # Phase 3 : Validation bridge test
        add_executable(bcad_validation_bridge_test unit/io/ValidationBridgeTest.cpp)
        target_link_libraries(bcad_validation_bridge_test PRIVATE
            -Wl,--whole-archive
            bcad_io_ffi
            -Wl,--no-whole-archive
            bcad_io bcad_core bcad_geometry)
        add_test(NAME validation_bridge_test COMMAND bcad_validation_bridge_test)
    endif()

    # Les attributs du dossier : un document porte des donnees qui ne sont pas
    # du dessin, et qui doivent mourir avec lui quand « Nouveau » vide la feuille.
    add_executable(bcad_document_properties_test unit/core/DocumentPropertiesTest.cpp)
    target_link_libraries(bcad_document_properties_test PRIVATE
        bcad_core bcad_geometry bcad_properties)
    add_test(NAME document_properties_test COMMAND bcad_document_properties_test)

    # Test Sheet (mise en page G1)
    add_executable(bcad_sheet_test unit/layout/SheetTest.cpp)
    target_link_libraries(bcad_sheet_test PRIVATE bcad_layout)
    add_test(NAME sheet_test COMMAND bcad_sheet_test)

    add_executable(bcad_viewport_test unit/layout/ViewportTest.cpp)
    target_link_libraries(bcad_viewport_test PRIVATE bcad_layout bcad_geometry)
    add_test(NAME viewport_test COMMAND bcad_viewport_test)

    add_executable(bcad_furniture_paint_test unit/layout/FurniturePaintTest.cpp)
    target_link_libraries(bcad_furniture_paint_test PRIVATE bcad_layout)
    add_test(NAME furniture_paint_test COMMAND bcad_furniture_paint_test)

    add_executable(bcad_pdfexport_test unit/layout/PdfExportTest.cpp)
    target_link_libraries(bcad_pdfexport_test PRIVATE bcad_layout)
    add_test(NAME pdfexport_test COMMAND bcad_pdfexport_test)

    add_executable(bcad_dimension_test unit/layout/DimensionTest.cpp)
    target_link_libraries(bcad_dimension_test PRIVATE bcad_layout bcad_geometry)
    add_test(NAME dimension_test COMMAND bcad_dimension_test)

    add_executable(bcad_scale_test unit/layout/ScaleTest.cpp)
    target_link_libraries(bcad_scale_test PRIVATE bcad_layout)
    add_test(NAME scale_test COMMAND bcad_scale_test)

    add_executable(bcad_northarrow_test unit/layout/NorthArrowTest.cpp)
    target_link_libraries(bcad_northarrow_test PRIVATE bcad_layout bcad_geometry)
    add_test(NAME northarrow_test COMMAND bcad_northarrow_test)

    # Nomenclature (ADR-017) : tableau générique à en-têtes déclarés — voir
    # FurniturePaintTest et le test du module (nomenclature en champs).

    # Decoupage de la feuille : mathematique pure, verifiee sans QPrinter.
    add_executable(bcad_composition_test unit/layout/CompositionTest.cpp)
    target_link_libraries(bcad_composition_test PRIVATE bcad_layout)
    add_test(NAME composition_test COMMAND bcad_composition_test)

    # Le decoupage juste ne sert a rien si le peintre se trompe de repere : la
    # feuille est mesuree a l'encre (le bug etait un dessin ~47 fois trop petit).
    add_executable(bcad_sheet_render_test unit/layout/SheetRenderTest.cpp)
    target_link_libraries(bcad_sheet_render_test PRIVATE
        bcad_layout bcad_core bcad_geometry bcad_properties
        Qt6::Gui Qt6::Widgets Qt6::PrintSupport)
    add_test(NAME sheet_render_test COMMAND bcad_sheet_render_test)

    # SPIKE (ADR-017) : mesure de ce que l'API de mise en page refuse — echelle
    # ecrasee, vue unique et aneree, garde fitsIn sans appelant, mobilier devine
    # du contenu, vocabulaire de cartouche ferme. Ne decide rien ; a retirer ou
    # convertir en test de contrat quand le layout sera un objet du document.
    add_executable(bcad_layout_spike_test unit/layout/LayoutSpikeTest.cpp)
    target_link_libraries(bcad_layout_spike_test PRIVATE
        bcad_layout bcad_core bcad_geometry bcad_properties
        Qt6::Gui Qt6::Widgets)
    add_test(NAME layout_spike_test COMMAND bcad_layout_spike_test)

    add_executable(bcad_dxf_cadastre_test unit/io/DxfCadastreRoundtripTest.cpp)
    target_link_libraries(bcad_dxf_cadastre_test PRIVATE bcad_io bcad_core bcad_geometry)
    add_test(NAME dxf_cadastre_test COMMAND bcad_dxf_cadastre_test)

    # Ce que src/io doit restituer sans rien comprendre : proprietes typées de
    # module inconnu, texte, XDATA heritee d'un autre emetteur.
    # Le bridge Rust. Cible interne, non installee : c'est l'ABI qui est verifiee,
    # pas un contrat du SDK (ADR-006).
    if(BCAD_ENABLE_RUST)
        add_executable(bcad_dxf_bridge_test unit/io/DxfBridgeTest.cpp)
        target_link_libraries(bcad_dxf_bridge_test PRIVATE
            bcad_io_ffi bcad_layers bcad_core)
        add_test(NAME dxf_bridge_test COMMAND bcad_dxf_bridge_test)
    endif()

    add_executable(bcad_dxf_properties_test unit/io/DxfPropertiesRoundtripTest.cpp)
    target_link_libraries(bcad_dxf_properties_test PRIVATE
        bcad_io bcad_core bcad_geometry bcad_properties)
    target_compile_definitions(bcad_dxf_properties_test PRIVATE
        BCAD_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
    add_test(NAME dxf_properties_test COMMAND bcad_dxf_properties_test)

    # Le format natif .bcad face a ce qu'il ne comprend pas : proprietes typées
    # d'un module inconnu, fichiers de la version 1 (table metier + type entier),
    # et un ouvrir/enregistrer fait sans le module. PluginManager.cpp est
    # compile dans l'executable pour que le test puisse declarer lui-meme le
    # serializer du module Cadastre, comme le fait son plugin.
    add_executable(bcad_bcad_schema_test
        unit/io/BcadSchemaTest.cpp
        ${CMAKE_SOURCE_DIR}/src/plugin/PluginManager.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/entities/ParcelEntity.cpp)
    target_include_directories(bcad_bcad_schema_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre)
    target_link_libraries(bcad_bcad_schema_test PRIVATE
        bcad_io bcad_core bcad_geometry bcad_layers bcad_index bcad_properties
        bcad_registry bcad_serialization bcad_events bcad_commands
        SQLite::SQLite3 ${CMAKE_DL_LIBS})
    target_compile_definitions(bcad_bcad_schema_test PRIVATE
        BCAD_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
    add_test(NAME bcad_schema_test COMMAND bcad_bcad_schema_test)

    add_executable(bcad_cadastre_io_test
        unit/io/CadastreIoTest.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/io/GeoJsonSerializer.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/io/GeoPackageSerializer.cpp)
    target_include_directories(bcad_cadastre_io_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/io
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre)
    target_link_libraries(bcad_cadastre_io_test PRIVATE
        bcad_geometry bcad_core SQLite::SQLite3)
    add_test(NAME cadastre_io_test COMMAND bcad_cadastre_io_test)

    add_executable(bcad_parcelops_test
        unit/plugins/cadastre/ParcelOpsTest.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/ParcelOps.cpp)
    target_include_directories(bcad_parcelops_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre)
    target_link_libraries(bcad_parcelops_test PRIVATE bcad_geometry)
    add_test(NAME parcelops_test COMMAND bcad_parcelops_test)

    # Cycle de vie des workbenches (lot A). PluginManager.cpp est compile dans
    # l'executable : le test porte lui-meme le singleton de registre, comme
    # l'hote, au lieu de dupliquer celui du DSO.
    add_executable(bcad_workbench_test
        unit/plugin/WorkbenchTest.cpp
        ${CMAKE_SOURCE_DIR}/src/plugin/PluginManager.cpp)
    target_link_libraries(bcad_workbench_test PRIVATE
        bcad_geometry bcad_registry bcad_commands bcad_serialization ${CMAKE_DL_LIBS})
    add_test(NAME workbench_test COMMAND bcad_workbench_test)

    # Decouverte des modules : l'hote ne nomme plus de plugin (ADR-016).
    add_executable(bcad_discovery_test
        unit/plugin/DiscoveryTest.cpp
        ${CMAKE_SOURCE_DIR}/src/plugin/PluginManager.cpp)
    target_link_libraries(bcad_discovery_test PRIVATE
        bcad_geometry bcad_registry bcad_commands bcad_serialization ${CMAKE_DL_LIBS})
    add_test(NAME discovery_test COMMAND bcad_discovery_test)

    # Cycle de vie des validateurs declares par un plugin : l'hote detruit
    # l'objet AVANT dlclose (meme regle que les workbenches et serializers).
    add_executable(bcad_validator_test
        unit/plugin/ValidatorTest.cpp
        ${CMAKE_SOURCE_DIR}/src/plugin/PluginManager.cpp)
    target_link_libraries(bcad_validator_test PRIVATE
        bcad_geometry bcad_properties bcad_registry bcad_commands bcad_serialization ${CMAKE_DL_LIBS})
    add_test(NAME validator_test COMMAND bcad_validator_test)

    # Calques déclarés par IStyleProvider créés dans un LayerManager (§7.1) :
    # premier test prouvant que le point d'extension est consommé.
    add_executable(bcad_style_provider_test
        unit/plugin/StyleProviderTest.cpp
        ${CMAKE_SOURCE_DIR}/src/plugin/PluginManager.cpp)
    target_link_libraries(bcad_style_provider_test PRIVATE
        bcad_geometry bcad_layers bcad_properties bcad_registry bcad_commands bcad_serialization ${CMAKE_DL_LIBS})
    add_test(NAME style_provider_test COMMAND bcad_style_provider_test)

    # Regles cadastrales rendues executables par le point d'extension de
    # validation : sans ce test, elles redeviennent du code mort.
    add_executable(bcad_cadastre_validators_test
        unit/plugins/cadastre/CadastreValidatorsTest.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/validation/CadastreValidators.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/validation/ParcelOverlapValidator.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/validation/ParcelIdentifierValidator.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/validation/SurveyToleranceValidator.cpp)
    # ParcelEntity.cpp est exclu : il n'apporte que l'enregistrement du
    # serializer, qui fait dependre le test du DSO bcad_plugin sans rapport
    # avec la validation geometrique verifiee ici.
    target_include_directories(bcad_cadastre_validators_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre)
    target_link_libraries(bcad_cadastre_validators_test PRIVATE
        bcad_geometry bcad_properties)
    add_test(NAME cadastre_validators_test COMMAND bcad_cadastre_validators_test)

    # Gabarits de profil : le chemin complet, de la donnee installee jusqu'a la
    # regle gardee par l'hote. Templates.cpp a besoin de Qt (QJsonDocument) et
    # PluginManager.cpp porte la resolution de chemin, contrairement au test des
    # regles ci-dessus qui reste sans Qt.
    add_executable(bcad_cadastre_templates_test
        unit/plugins/cadastre/CadastreTemplatesTest.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/Templates.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/validation/CadastreValidators.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/validation/ParcelIdentifierValidator.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/validation/ParcelOverlapValidator.cpp
        ${CMAKE_SOURCE_DIR}/src/plugin/PluginManager.cpp)
    target_include_directories(bcad_cadastre_templates_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre)
    target_compile_definitions(bcad_cadastre_templates_test PRIVATE
        BCAD_CADASTRE_DATA_DIR="${CMAKE_BINARY_DIR}/share/bcad/plugins")
    target_link_libraries(bcad_cadastre_templates_test PRIVATE
        bcad_geometry bcad_properties bcad_registry bcad_commands
        bcad_serialization Qt6::Core ${CMAKE_DL_LIBS})
    # Le test lit le gabarit copie dans l'arbre de build : la copie doit avoir eu lieu.
    add_dependencies(bcad_cadastre_templates_test bcad_cadastre_plugin)
    add_test(NAME cadastre_templates_test COMMAND bcad_cadastre_templates_test)

    # Meuble de feuille : le plugin est le seul a connaitre les proprietes
    # cadastrales, src/layout ne dessine que ce qu'on lui donne (ADR-016).
    add_executable(bcad_cadastre_sheet_test
        unit/plugins/cadastre/CadastreSheetTest.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/layout/CadastreSheet.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/ParcelOps.cpp)
    target_include_directories(bcad_cadastre_sheet_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre)
    target_link_libraries(bcad_cadastre_sheet_test PRIVATE
        bcad_layout bcad_core bcad_geometry bcad_properties)
    add_test(NAME cadastre_sheet_test COMMAND bcad_cadastre_sheet_test)

    # Etiquette de parcelle : vocabulaire cadastral, construite par le module
    # (sortie de layout::Label::forParcel de l'API publique).
    add_executable(bcad_parcel_label_test unit/plugins/cadastre/ParcelLabelTest.cpp)
    target_include_directories(bcad_parcel_label_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre)
    target_link_libraries(bcad_parcel_label_test PRIVATE bcad_layout bcad_geometry)
    add_test(NAME parcel_label_test COMMAND bcad_parcel_label_test)

    # Resolution du cartouche : ce que la feuille affirme, et ce qu'elle tait.
    # Les deux regles (le dossier prime, les parcelles ne comptent que si
    # elles sont unanimes) et les deux refus (aucun titre invente, aucune premiere
    # parcelle lue).
    add_executable(bcad_cadastre_cartouche_test
        unit/plugins/cadastre/CartoucheResolutionTest.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/layout/CadastreSheet.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/ParcelOps.cpp)
    target_include_directories(bcad_cadastre_cartouche_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre)
    target_link_libraries(bcad_cadastre_cartouche_test PRIVATE
        bcad_layout bcad_core bcad_geometry bcad_properties)
    add_test(NAME cadastre_cartouche_test COMMAND bcad_cadastre_cartouche_test)

    # Mise en page (ADR-017 décision 5) : le débordement est une erreur, le
    # hors-liste un avertissement, l'inconnu un constat — jamais une exception.
    add_executable(bcad_cadastre_sheet_validator_test
        unit/plugins/cadastre/SheetValidatorTest.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/validation/SheetValidator.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/Templates.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/layout/CadastreSheet.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/ParcelOps.cpp)
    target_include_directories(bcad_cadastre_sheet_validator_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre)
    target_link_libraries(bcad_cadastre_sheet_validator_test PRIVATE
        bcad_layout bcad_core bcad_geometry bcad_properties bcad_plugin
        bcad_commands bcad_registry bcad_serialization
        Qt6::Core ${CMAKE_DL_LIBS})
    add_test(NAME cadastre_sheet_validator_test COMMAND bcad_cadastre_sheet_validator_test)

    # La commande qui produit le livrable (PDF) doit etre executee par quelque
    # chose : c'est le seul chemin par lequel l'utilisateur obtient sa feuille.
    add_executable(bcad_cadastre_plan_sheet_test
        unit/plugins/cadastre/PlanSheetCommandTest.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/commands/SplitParcelCommand.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/layout/CadastreSheet.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/ParcelOps.cpp)
    target_include_directories(bcad_cadastre_plan_sheet_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre)
    target_link_libraries(bcad_cadastre_plan_sheet_test PRIVATE
        bcad_layout bcad_core bcad_geometry bcad_properties bcad_commands
        Qt6::Gui Qt6::Widgets Qt6::PrintSupport)
    add_test(NAME cadastre_plan_sheet_test COMMAND bcad_cadastre_plan_sheet_test)

    # Recherche par reference cadastrale : supprimee avec le noyau cadastre lors
    # de la migration vers le module, elle n'avait jamais ete reimplementee. Ce
    # test est son appelant (et la preuve que la regle de saisie est bien du
    # cote du module, pas de src/app).
    add_executable(bcad_cadastre_search_test
        unit/plugins/cadastre/ParcelSearchTest.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/commands/FindParcelCommand.cpp)
    target_include_directories(bcad_cadastre_search_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre)
    target_link_libraries(bcad_cadastre_search_test PRIVATE
        bcad_core bcad_geometry bcad_properties bcad_registry)
    add_test(NAME cadastre_search_test COMMAND bcad_cadastre_search_test)

    # Le profil cadastral du dossier, de la saisie de l'operateur a la regle
    # enregistree chez l'hote : code assume comme un nom, gabarit resolve dans
    # les repertoires memorises, motif applique puis annule.
    add_executable(bcad_cadastre_profil_test
        unit/plugins/cadastre/ProfilCommandTest.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/commands/ProfilCommand.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/Templates.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/validation/CadastreValidators.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/validation/ParcelIdentifierValidator.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/validation/ParcelOverlapValidator.cpp
        ${CMAKE_SOURCE_DIR}/src/plugin/PluginManager.cpp)
    target_include_directories(bcad_cadastre_profil_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre)
    target_link_libraries(bcad_cadastre_profil_test PRIVATE
        bcad_core bcad_geometry bcad_properties bcad_registry bcad_commands
        bcad_serialization Qt6::Core ${CMAKE_DL_LIBS})
    add_test(NAME cadastre_profil_test COMMAND bcad_cadastre_profil_test)

    # Ecrivains d'echange du noyau : une proprietes inconnue de l'hote doit
    # sortir du GeoJSON, et une geometrie sans equivalent GeoJSON doit tesseler
    # au lieu de disparaitre silencieusement.
    add_executable(bcad_exchange_test unit/io/ExchangeTest.cpp)
    target_link_libraries(bcad_exchange_test PRIVATE bcad_io bcad_core bcad_geometry)
    add_test(NAME exchange_test COMMAND bcad_exchange_test)

    # Cycle de vie des exporteurs de fichier declares par un module : l'hote
    # detient l'objet et doit le detruire AVANT dlclose (meme regle que les
    # workbenches et les validateurs).
    add_executable(bcad_file_exporter_test
        unit/plugin/FileExporterTest.cpp
        ${CMAKE_SOURCE_DIR}/src/plugin/PluginManager.cpp)
    target_link_libraries(bcad_file_exporter_test PRIVATE
        bcad_geometry bcad_properties bcad_registry bcad_commands
        bcad_serialization bcad_core ${CMAKE_DL_LIBS})
    add_test(NAME file_exporter_test COMMAND bcad_file_exporter_test)

    add_executable(bcad_file_importer_test
        unit/plugin/FileImporterTest.cpp
        ${CMAKE_SOURCE_DIR}/src/plugin/PluginManager.cpp)
    target_link_libraries(bcad_file_importer_test PRIVATE
        bcad_geometry bcad_properties bcad_registry bcad_commands
        bcad_serialization bcad_core ${CMAKE_DL_LIBS})
    add_test(NAME file_importer_test COMMAND bcad_file_importer_test)
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/io/GeoPackageSerializer.cpp)
    # Le GeoPackage du module atteint par le contrat IFileExporter, pas par son
    # serializer : c'est le seul chemin par lequel l'utilisateur le voit.
    add_executable(bcad_cadastre_export_test
        unit/plugins/cadastre/GeoPackageExportTest.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/io/GeoPackageExporter.cpp
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/io/GeoPackageSerializer.cpp)
    target_include_directories(bcad_cadastre_export_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre/io
        ${CMAKE_SOURCE_DIR}/src/plugins/cadastre)
    target_link_libraries(bcad_cadastre_export_test PRIVATE
        bcad_geometry bcad_core SQLite::SQLite3)
    add_test(NAME cadastre_export_test COMMAND bcad_cadastre_export_test)
endif()
