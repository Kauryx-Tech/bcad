add_library(bcad_commands STATIC
    CommandsModule.cpp
    CommandRegistry.cpp
)

target_include_directories(bcad_commands PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(bcad_commands PUBLIC
    bcad_core
    bcad_geometry
)

target_compile_features(bcad_commands PUBLIC cxx_std_20)

set_target_properties(bcad_commands PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)