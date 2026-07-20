add_library(bcad_geometry STATIC
    BooleanOps.cpp
    Triangulation.cpp
    SnapGeometry.cpp
)

target_include_directories(bcad_geometry PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(bcad_geometry PUBLIC
    CGAL::CGAL
)

target_compile_features(bcad_geometry PUBLIC cxx_std_20)
