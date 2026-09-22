add_library(bcad_layers INTERFACE)

target_include_directories(bcad_layers INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(bcad_layers INTERFACE
    bcad_geometry
)
