add_library(bcad_layout INTERFACE)
target_include_directories(bcad_layout INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
