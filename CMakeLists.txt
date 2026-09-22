cmake_minimum_required(VERSION 3.20)
project(bcad LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets OpenGL Gui)
find_package(CGAL REQUIRED)
find_package(OpenGL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

# Installation prefix for SDK
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Install prefix" FORCE)
endif()

add_subdirectory(src/geometry)
add_subdirectory(src/layers)
add_subdirectory(src/index)
add_subdirectory(src/io)
add_subdirectory(src/render)
add_subdirectory(src/core)
add_subdirectory(src/registry)
add_subdirectory(src/serialization)
add_subdirectory(src/commands)
add_subdirectory(src/events)
add_subdirectory(src/plugin)
add_subdirectory(src/properties)
add_subdirectory(src/app)

enable_testing()
add_subdirectory(tests)

# Install public headers
install(DIRECTORY include/bcad/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bcad
        FILES_MATCHING PATTERN "*.h")

# Generate and install BCADConfig.cmake (relocatable via configure_package_config_file)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
configure_package_config_file(cmake/BCADConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/BCADConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BCAD
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/BCADConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/BCADConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/BCADConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BCAD)

# Install targets
install(TARGETS bcad_geometry bcad_layers bcad_index bcad_io bcad_render
            bcad_core bcad_registry bcad_serialization bcad_commands bcad_events bcad_plugin bcad_properties
        EXPORT BCADTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Export targets for find_package
install(EXPORT BCADTargets
        FILE BCADTargets.cmake
        NAMESPACE BCAD::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BCAD)
