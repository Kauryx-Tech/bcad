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

    # Test Sheet (mise en page G1)
    add_executable(bcad_sheet_test unit/layout/SheetTest.cpp)
    target_link_libraries(bcad_sheet_test PRIVATE bcad_layout)
    add_test(NAME sheet_test COMMAND bcad_sheet_test)

    add_executable(bcad_viewport_test unit/layout/ViewportTest.cpp)
    target_link_libraries(bcad_viewport_test PRIVATE bcad_layout bcad_geometry)
    add_test(NAME viewport_test COMMAND bcad_viewport_test)

    add_executable(bcad_label_test unit/layout/LabelTest.cpp)
    target_link_libraries(bcad_label_test PRIVATE bcad_layout bcad_geometry)
    add_test(NAME label_test COMMAND bcad_label_test)

    add_executable(bcad_cartouche_test unit/layout/CartoucheTest.cpp)
    target_link_libraries(bcad_cartouche_test PRIVATE bcad_layout)
    add_test(NAME cartouche_test COMMAND bcad_cartouche_test)

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

    add_executable(bcad_borne_test unit/layout/BorneTest.cpp)
    target_link_libraries(bcad_borne_test PRIVATE bcad_layout bcad_geometry)
    add_test(NAME borne_test COMMAND bcad_borne_test)

    add_executable(bcad_parceltable_test unit/layout/ParcelTableTest.cpp)
    target_link_libraries(bcad_parceltable_test PRIVATE bcad_layout)
    add_test(NAME parceltable_test COMMAND bcad_parceltable_test)

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

    add_executable(bcad_dxf_cadastre_test unit/io/DxfCadastreRoundtripTest.cpp)
    target_link_libraries(bcad_dxf_cadastre_test PRIVATE bcad_io bcad_core bcad_geometry)
    add_test(NAME dxf_cadastre_test COMMAND bcad_dxf_cadastre_test)

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
