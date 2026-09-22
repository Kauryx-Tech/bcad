add_library(bcad_index STATIC
    QuadtreeIndex.cpp
    SpatialIndexFactory.cpp
)

target_include_directories(bcad_index PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(bcad_index PUBLIC
    bcad_geometry
)

set_target_properties(bcad_index PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
