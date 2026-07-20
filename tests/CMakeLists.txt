add_executable(bcad_smoke_test smoke_test.cpp)

target_link_libraries(bcad_smoke_test PRIVATE
    bcad_geometry
    bcad_layers
    bcad_core
    bcad_io
)

add_test(NAME smoke_test COMMAND bcad_smoke_test)
