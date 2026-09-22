add_library(bcad_render STATIC
    GlRenderer.cpp
)

target_include_directories(bcad_render PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(bcad_render PUBLIC
    bcad_core
    bcad_geometry
    Qt6::Widgets
    Qt6::OpenGLWidgets
    Qt6::OpenGL
    Qt6::Gui
    OpenGL::GL
)

set_target_properties(bcad_render PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
