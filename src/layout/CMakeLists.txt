add_library(bcad_layout STATIC
    Cartouche.cpp
    PdfExport.cpp
    DimensionStyle.cpp
)

target_include_directories(bcad_layout PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(bcad_layout PUBLIC
    bcad_core
    Qt6::Gui
    Qt6::Widgets
    Qt6::PrintSupport
)

set_target_properties(bcad_layout PROPERTIES POSITION_INDEPENDENT_CODE ON)
