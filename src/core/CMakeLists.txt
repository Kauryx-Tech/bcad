add_library(bcad_core STATIC
    Document.cpp
)

target_include_directories(bcad_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(bcad_core PUBLIC
    bcad_geometry
    bcad_layers
    bcad_index
    bcad_events
)

set_target_properties(bcad_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
