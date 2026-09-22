add_library(bcad_geometry STATIC
    BooleanOps.cpp
    Triangulation.cpp
    SnapGeometry.cpp
    Transform2D.cpp
    Polyline.cpp
    GeometryUtils2.cpp
)

target_include_directories(bcad_geometry PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(bcad_geometry PUBLIC
    bcad_properties
    CGAL::CGAL
)

target_compile_features(bcad_geometry PUBLIC cxx_std_20)

set_target_properties(bcad_geometry PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
