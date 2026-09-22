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
