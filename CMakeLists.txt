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

# Option Rust
option(BCAD_ENABLE_RUST "Enable Rust components (requires Rust toolchain)" OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets OpenGL Gui PrintSupport)
find_package(CGAL REQUIRED)
find_package(OpenGL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

if(BCAD_ENABLE_RUST)
    find_program(CARGO cargo)
    if(NOT CARGO)
        message(FATAL_ERROR
            "BCAD_ENABLE_RUST=ON but cargo was not found. Install a Rust toolchain "
            "(https://rustup.rs) or configure with -DBCAD_ENABLE_RUST=OFF.")
    endif()

    # Cargo builds into its own target directory, shared by every build tree of
    # this source tree. Overridable so a packaging build can keep it out of the
    # source directory.
    set(BCAD_RUST_TARGET_DIR "${CMAKE_SOURCE_DIR}/rust/target"
        CACHE PATH "Cargo target directory used by the Rust workspace")
    set(BCAD_FFI_STATICLIB "${BCAD_RUST_TARGET_DIR}/release/libbcad_ffi.a")

    # The C++ libraries that make up the SDK are deliberately *not* linked
    # against this. install(EXPORT) records a private link dependency of a
    # static library as $<LINK_ONLY:...>, so an absolute path to the Cargo
    # target directory would end up in BCADTargets.cmake and `find_package(BCAD)`
    # would only work on the machine that built it (ADR-006). The Rust ABI is an
    # implementation detail of the bridge, not a public SDK contract.
    #
    # crates: bcad-format bcad-dxf bcad-validation bcad-db bcad-export
    #         bcad-ffi bcad-doctor
    add_custom_target(bcad_rust ALL
        COMMAND ${CARGO} build --workspace --release
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/rust
        COMMENT "Building the Rust workspace (release)"
        VERBATIM
    )
    set_target_properties(bcad_rust PROPERTIES
        # Cargo has its own locking and fingerprinting; CMake must not second-
        # guess it, and the archive it produces is large.
        BYPRODUCTS "${BCAD_FFI_STATICLIB}")

    message(STATUS "Rust enabled: ${CARGO}, archive ${BCAD_FFI_STATICLIB}")
else()
    message(STATUS "Rust disabled (set BCAD_ENABLE_RUST=ON to enable)")
endif()

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
add_subdirectory(src/layout)
add_subdirectory(src/plugins)
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
            bcad_core bcad_registry bcad_serialization bcad_commands bcad_events bcad_plugin bcad_properties bcad_layout
        EXPORT BCADTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Install plugins
install(DIRECTORY ${CMAKE_BINARY_DIR}/plugins/
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/bcad/plugins
        FILES_MATCHING PATTERN "*.so" PATTERN "*.dll" PATTERN "*.dylib")

# Export targets for find_package
install(EXPORT BCADTargets
        FILE BCADTargets.cmake
        NAMESPACE BCAD::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BCAD)
