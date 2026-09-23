add_library(bcad_cadastre STATIC ParcelOps.cpp)
target_include_directories(bcad_cadastre PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
target_link_libraries(bcad_cadastre PUBLIC bcad_geometry)
set_target_properties(bcad_cadastre PROPERTIES POSITION_INDEPENDENT_CODE ON)
