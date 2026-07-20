add_library(bcad_core STATIC
    Document.cpp
)

target_include_directories(bcad_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(bcad_core PUBLIC
    bcad_geometry
    bcad_layers
    bcad_render
)
