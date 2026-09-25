# La mediation par l'hote exige QU'UN SEUL exemplaire des registres existe :
# bcad_plugin.so ne doit PAS lier les bibliotheques statiques qu'il mediatise
# (registres/commands/serialization/geometry/core). Le DSO compile contre les
# headers publics (include/) ; leurs symboles restent NON DEFINIS dans le .so
# et sont resolus au chargement vers l'instance portee par l'executable hote
# (qui exporte ses symboles, ex. -rdynamic sur Linux). Les consommateurs qui
# ont besoin des registres/entites doivent lier eux-memes les bibliotheques
# statiques (regle d'usage du SDK, documentee dans PLUGIN_ARCHITECTURE).
add_library(bcad_plugin SHARED
    PluginManager.cpp
)

target_include_directories(bcad_plugin PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(bcad_plugin PRIVATE
    ${CMAKE_DL_LIBS}
)

target_compile_definitions(bcad_plugin PRIVATE BCAD_PLUGIN_BUILDING)

target_compile_features(bcad_plugin PUBLIC cxx_std_20)

set_target_properties(bcad_plugin PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    # PROJECT_VERSION_* : `project(bcad VERSION 1.0.0)` ne definit PAS
    # BCAD_VERSION, et un SOVERSION vide avalait le jeton `VERSION` suivant
    # (le DSO etait installe sous le nom `libbcad_plugin.so.VERSION`).
    SOVERSION ${PROJECT_VERSION_MAJOR}
    VERSION ${PROJECT_VERSION}
)

# Install
install(TARGETS bcad_plugin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    ${CMAKE_SOURCE_DIR}/include/bcad/plugin/Api.h
    ${CMAKE_SOURCE_DIR}/include/bcad/plugin/FileExporter.h
    ${CMAKE_SOURCE_DIR}/include/bcad/plugin/Plugin.h
    ${CMAKE_SOURCE_DIR}/include/bcad/plugin/PluginRegistry.h
    ${CMAKE_SOURCE_DIR}/include/bcad/plugin/Validator.h
    ${CMAKE_SOURCE_DIR}/include/bcad/plugin/Workbench.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bcad/plugin
)