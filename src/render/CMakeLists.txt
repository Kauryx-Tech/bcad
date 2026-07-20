add_library(bcad_render STATIC
    Quadtree.cpp
    GlRenderer.cpp
)

target_include_directories(bcad_render PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(bcad_render PUBLIC
    bcad_geometry
    Qt6::Widgets
    Qt6::OpenGLWidgets
    Qt6::OpenGL
    Qt6::Gui
    OpenGL::GL
)
