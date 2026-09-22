add_library(bcad_registry STATIC
    EntityRegistry.cpp
    NativeEntityRegistration.cpp
)

target_include_directories(bcad_registry PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(bcad_registry PUBLIC
    bcad_geometry
)

target_compile_features(bcad_registry PUBLIC cxx_std_20)

set_target_properties(bcad_registry PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    # Visibilite par defaut : EntityRegistry est un point d'entree du systeme
    # de plugins (mediation par l'hote). Son unique instance vit dans
    # l'executable hote et doit rester resolvable par libbcad_plugin.so.
)