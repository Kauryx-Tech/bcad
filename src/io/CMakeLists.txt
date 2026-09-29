add_library(bcad_io STATIC
    DxfWriter.cpp
    DxfReader.cpp
    Database.cpp
    Exchange.cpp
    Exporters.cpp
)

target_include_directories(bcad_io PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(bcad_io PUBLIC
    bcad_core
    bcad_serialization
    # Les exporteurs natifs s'enregistrent dans le registre porté par
    # libbcad_plugin (l'hôte, et lui seul, médiatise les registres).
    bcad_plugin
    SQLite::SQLite3
)

if(BCAD_ENABLE_RUST)
    # Kept out of `bcad_io` on purpose. `bcad_io` is part of the installed,
    # exported SDK, and linking the Cargo archive into it would write an
    # absolute path into BCADTargets.cmake, breaking `find_package(BCAD)` for
    # anyone but this machine (ADR-006). The bridge is an internal target: it
    # proves the ABI links, and is deliberately not installed.
    add_library(bcad_io_ffi STATIC DxfBridge.cpp)

    target_link_libraries(bcad_io_ffi PUBLIC bcad_core bcad_io)
    target_include_directories(bcad_io_ffi PRIVATE
        ${CMAKE_SOURCE_DIR}/rust/crates/bcad-ffi/include
    )
    # DxfBridge.cpp garde l'en-tete FFI sous `#ifdef BCAD_ENABLE_RUST` : sans
    # cette definition, BcString, BcErrorCode et BCAD_OK restent inconnus et la
    # cible ne compile pas, meme configuree avec BCAD_ENABLE_RUST=ON.
    target_compile_definitions(bcad_io_ffi PRIVATE BCAD_ENABLE_RUST)
    target_link_libraries(bcad_io_ffi PRIVATE
        ${BCAD_FFI_STATICLIB}
        Threads::Threads
        ${CMAKE_DL_LIBS}
        m
    )
    add_dependencies(bcad_io_ffi bcad_rust)

    set_target_properties(bcad_io_ffi PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

set_target_properties(bcad_io PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
