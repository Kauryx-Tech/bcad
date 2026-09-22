add_library(bcad_plugin SHARED
    PluginManager.cpp
)

target_include_directories(bcad_plugin PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(bcad_plugin PUBLIC
    bcad_geometry
    bcad_core
    bcad_commands
    bcad_serialization
    bcad_registry
    ${CMAKE_DL_LIBS}
)

target_compile_features(bcad_plugin PUBLIC cxx_std_20)

set_target_properties(bcad_plugin PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    SOVERSION ${BCAD_VERSION_MAJOR}
    VERSION ${BCAD_VERSION}
)

# Install
install(TARGETS bcad_plugin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    ${CMAKE_SOURCE_DIR}/include/bcad/plugin/Plugin.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bcad/plugin
)