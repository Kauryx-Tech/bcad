add_library(bcad_io STATIC
    DxfWriter.cpp
    DxfReader.cpp
    Database.cpp
)

target_include_directories(bcad_io PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(bcad_io PUBLIC
    bcad_core
    SQLite::SQLite3
)
