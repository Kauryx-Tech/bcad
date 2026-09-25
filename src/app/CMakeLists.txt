add_executable(bcad
    main.cpp
    # MainWindow est une seule classe, repartie sur cinq unites de traduction
    # par responsabilite (fenetre, outils, menus, modules metiers, document).
    # L'en-tete Q_OBJECT reste unique : seuls les corps changent de fichier.
    MainWindow.cpp
    MainWindowTools.cpp
    MainWindowMenus.cpp
    MainWindowPlugins.cpp
    MainWindowDocument.cpp
    Viewport.cpp
    LayerPanel.cpp
    TessellationWorker.cpp
    SnapEngine.cpp
    Commands.cpp
    RibbonBar.cpp
    PropertiesPanel.cpp
    # Les en-têtes portant un Q_OBJECT doivent être vus par AUTOMOC. La plupart
    # sont appariés par nom de base à leur .cpp ; QtCommandAdapter.h n'a pas de
    # .cpp du même nom et ne serait jamais moc sans cette liste.
    MainWindow.h
    Viewport.h
    LayerPanel.h
    TessellationWorker.h
    RibbonBar.h
    PropertiesPanel.h
    QtCommandAdapter.h
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
