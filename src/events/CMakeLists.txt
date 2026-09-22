add_library(bcad_events STATIC
)

target_include_directories(bcad_events PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(bcad_events PUBLIC
    bcad_core
    bcad_geometry
)

target_compile_features(bcad_events PUBLIC cxx_std_20)

set_target_properties(bcad_events PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)