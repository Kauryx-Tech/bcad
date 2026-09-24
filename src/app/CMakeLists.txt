add_executable(bcad
    main.cpp
    MainWindow.cpp
    Viewport.cpp
    LayerPanel.cpp
    TessellationWorker.cpp
    SnapEngine.cpp
    Commands.cpp
    RibbonBar.cpp
    PropertiesPanel.cpp
    # Les en-têtes Q_OBJECT doivent être listés explicitement : ils se trouvent
    # sous include/, pas à côté de leur .cpp, donc l'appariement par répertoire
    # d'AUTOMOC ne peut pas les trouver par simple nom de base.
    ${CMAKE_SOURCE_DIR}/include/bcad/app/MainWindow.h
    ${CMAKE_SOURCE_DIR}/include/bcad/app/Viewport.h
    ${CMAKE_SOURCE_DIR}/include/bcad/app/LayerPanel.h
    ${CMAKE_SOURCE_DIR}/include/bcad/app/TessellationWorker.h
    ${CMAKE_SOURCE_DIR}/include/bcad/app/RibbonBar.h
    ${CMAKE_SOURCE_DIR}/include/bcad/app/PropertiesPanel.h
    ${CMAKE_SOURCE_DIR}/include/bcad/app/QtCommandAdapter.h
)

set_target_properties(bcad PROPERTIES ENABLE_EXPORTS TRUE)

target_include_directories(bcad PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(bcad PRIVATE
    bcad_render
    bcad_io
    bcad_layers
    bcad_layout
    bcad_plugin
    Qt6::Widgets
    Qt6::OpenGLWidgets
    Qt6::OpenGL
    Qt6::Gui
    Qt6::PrintSupport
)

# L'hôte porte les registres et les types utilisés par les plugins. Les
# bibliothèques statiques doivent être entièrement conservées puis leurs
# symboles BCAD exportés vers les DSO, comme pour les loaders de preuve SDK.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    foreach(lib IN ITEMS bcad_geometry bcad_properties bcad_core
                 bcad_registry bcad_commands bcad_serialization)
        target_link_options(bcad PRIVATE
            "/WHOLEARCHIVE:$<TARGET_FILE:${lib}>")
    endforeach()
else()
    target_link_libraries(bcad PRIVATE
        -Wl,--whole-archive
        bcad_geometry
        bcad_properties
        bcad_core
        bcad_registry
        bcad_commands
        bcad_serialization
        -Wl,--no-whole-archive)
    target_link_options(bcad PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:
            -Wl,--export-dynamic-symbol=_ZN4bcad*
            -Wl,--export-dynamic-symbol=_ZNK4bcad*
            -Wl,--export-dynamic-symbol=_ZTVN4bcad*
            -Wl,--export-dynamic-symbol=_ZTIN4bcad*
            -Wl,--export-dynamic-symbol=_ZTSN4bcad*
            -Wl,--export-dynamic-symbol=_ZGVN4bcad*>)
endif()

install(TARGETS bcad
    RUNTIME DESTINATION bin)
