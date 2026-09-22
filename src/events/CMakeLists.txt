add_library(bcad_events INTERFACE)

target_include_directories(bcad_events INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(bcad_events INTERFACE
    # Header-only (aucun .cpp) : EventBus.h n'utilise de core/geometry que des
    # types et references d'en-tete, sans symbole de lien. Aucune dependance de
    # lien pour eviter le cycle geometry -> properties -> events -> geometry.
)

target_compile_features(bcad_events INTERFACE cxx_std_20)

set_target_properties(bcad_events PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)