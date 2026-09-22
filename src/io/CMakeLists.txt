add_library(bcad_io STATIC
    DxfWriter.cpp
    DxfReader.cpp
    Database.cpp
)

target_include_directories(bcad_io PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(bcad_io PUBLIC
    bcad_core
    bcad_serialization
    SQLite::SQLite3
)

set_target_properties(bcad_io PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
