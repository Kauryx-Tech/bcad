add_library(bcad_serialization STATIC
    Serializer.cpp
    NativeSerializers.cpp
)

target_include_directories(bcad_serialization PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(bcad_serialization PUBLIC
    bcad_geometry
    bcad_registry
)

target_compile_features(bcad_serialization PUBLIC cxx_std_20)

set_target_properties(bcad_serialization PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)