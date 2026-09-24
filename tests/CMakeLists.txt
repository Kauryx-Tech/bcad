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
endif()
