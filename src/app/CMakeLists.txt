add_executable(bcad
    main.cpp
    # MainWindow est une seule classe, repartie sur six unites de traduction
    # par responsabilite (fenetre, outils, menus, modules metiers, document,
    # import). L'en-tete Q_OBJECT reste unique : seuls les corps changent de fichier.
    MainWindow.cpp
    MainWindowTools.cpp
    MainWindowMenus.cpp
    MainWindowPlugins.cpp
    MainWindowDocument.cpp
    MainWindowImport.cpp
    # Viewport est une seule classe, repartie sur sept unites de traduction par
    # responsabilite (rendu et camera, entrees, surimpression, operations sur la
    # selection, outils de trace, outils de transformation, outils de coupe).
    # Memes raisons que MainWindow : l'en-tete Q_OBJECT reste unique, seuls les
    # corps changent de fichier.
    Viewport.cpp
    ViewportInput.cpp
    ViewportOverlay.cpp
    ViewportEditing.cpp
    ViewportDrawTools.cpp
    ViewportTransformTools.cpp
    ViewportCutTools.cpp
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
        $<IF:$<BOOL:${BCAD_ENABLE_RUST}>,bcad_io_ffi,>
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

if(BCAD_ENABLE_RUST)
    # Le bridge Rust n'existe que si BCAD_ENABLE_RUST est ON (target bcad_io_ffi,
    # volontairement hors du SDK installe, ADR-006). Sans lui, MainWindowDocument
    # utilise le lecteur et l'ecrivain natifs : le code ne doit donc pas
    # referencer le bridge inconditionnellement, ou le lien echoue.
    target_compile_definitions(bcad PRIVATE BCAD_HAVE_DXF_BRIDGE=1)
    target_link_libraries(bcad PRIVATE bcad_io_ffi)
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

# Install public headers
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/bcad/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bcad
        FILES_MATCHING PATTERN "*.h")
# BCADConfig.cmake n'est pas installe ici : il est genere a la racine de
# l'arbre binaire (configure_package_config_file), pas dans celui de l'application.
# Chercher le fichier dans ce repertoire fait echouer tout cmake --install.
