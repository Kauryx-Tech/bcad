add_library(bcad_properties STATIC
    PropertyMap.cpp
)

target_include_directories(bcad_properties PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(bcad_properties PUBLIC
    bcad_events
)

target_compile_features(bcad_properties PUBLIC cxx_std_20)

set_target_properties(bcad_properties PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    # Visibilite par defaut : partage des symboles avec l'executable hote et
    # les plugins (mediation ADR-005). Cf. docs/PLUGIN_ARCHITECTURE.md.
)