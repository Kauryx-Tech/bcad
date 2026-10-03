# Les sources de l'application, hors point d'entree, forment une bibliotheque
# d'objets : l'executable et les tests de bout en bout de la fenetre partagent
# exactement le meme code, sans recopier la liste. OBJECT et non STATIC : les
# ressources Qt (icones, theme) s'enregistrent par un initialiseur statique
# qu'une bibliotheque statique laisserait tomber au lien.
add_library(bcad_app_objects OBJECT
    # MainWindow est une seule classe, repartie sur huit unites de traduction
    # par responsabilite (fenetre, outils, menus, modules metiers, actions de
    # workbench, document, onglets des dessins, import). L'en-tete Q_OBJECT
    # reste unique : seuls les corps changent de fichier.
    MainWindow.cpp
    MainWindowTools.cpp
    MainWindowMenus.cpp
    MainWindowPlugins.cpp
    MainWindowWorkbench.cpp
    MainWindowDocument.cpp
    MainWindowImport.cpp
    MainWindowSessions.cpp
    DocumentSessions.cpp
    # Icones SVG et feuille de style embarquees dans l'executable : identiques
    # hors-ligne sur tout poste (ADR-016), aucun fichier a installer a cote.
    resources.qrc
    # Viewport est une seule classe, repartie sur huit unites de traduction par
    # responsabilite (rendu et camera, entrees, surimpression, operations sur la
    # selection, outils de trace, outils de transformation, outils de coupe,
    # consignes d'etape).
    # Memes raisons que MainWindow : l'en-tete Q_OBJECT reste unique, seuls les
    # corps changent de fichier.
    Viewport.cpp
    ViewportInput.cpp
    ViewportOverlay.cpp
    ViewportEditing.cpp
    ViewportDrawTools.cpp
    ViewportTransformTools.cpp
    ViewportCutTools.cpp
    ViewportPrompts.cpp
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

target_include_directories(bcad_app_objects PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(bcad_app_objects PUBLIC
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

if(BCAD_ENABLE_RUST)
    # Le bridge Rust n'existe que si BCAD_ENABLE_RUST est ON (target bcad_io_ffi,
    # volontairement hors du SDK installe, ADR-006). Sans lui, MainWindowDocument
    # utilise le lecteur et l'ecrivain natifs : le code ne doit donc pas
    # referencer le bridge inconditionnellement, ou le lien echoue.
    target_compile_definitions(bcad_app_objects PUBLIC BCAD_HAVE_DXF_BRIDGE=1)
    target_link_libraries(bcad_app_objects PUBLIC bcad_io_ffi)
endif()

# Un executable qui porte la fenetre est un hote de modules : il conserve les
# bibliotheques statiques du noyau en entier et exporte leurs symboles BCAD vers
# les DSO des plugins, comme les loaders de preuve SDK. L'application et les
# tests de bout en bout de la fenetre passent par la meme fonction.
function(bcad_link_as_plugin_host target)
    set_target_properties(${target} PROPERTIES ENABLE_EXPORTS TRUE)
    target_link_libraries(${target} PRIVATE bcad_app_objects)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        foreach(lib IN ITEMS bcad_geometry bcad_properties bcad_core
                     bcad_registry bcad_commands bcad_serialization)
            target_link_options(${target} PRIVATE
                "/WHOLEARCHIVE:$<TARGET_FILE:${lib}>")
        endforeach()
    else()
        target_link_libraries(${target} PRIVATE
            -Wl,--whole-archive
            bcad_geometry
            bcad_properties
            bcad_core
            bcad_registry
            bcad_commands
            bcad_serialization
            $<IF:$<BOOL:${BCAD_ENABLE_RUST}>,bcad_io_ffi,>
            -Wl,--no-whole-archive)
        target_link_options(${target} PRIVATE
            $<$<CXX_COMPILER_ID:GNU,Clang>:
                -Wl,--export-dynamic-symbol=_ZN4bcad*
                -Wl,--export-dynamic-symbol=_ZNK4bcad*
                -Wl,--export-dynamic-symbol=_ZTVN4bcad*
                -Wl,--export-dynamic-symbol=_ZTIN4bcad*
                -Wl,--export-dynamic-symbol=_ZTSN4bcad*
                -Wl,--export-dynamic-symbol=_ZGVN4bcad*>)
    endif()
endfunction()

add_executable(bcad main.cpp)
bcad_link_as_plugin_host(bcad)

install(TARGETS bcad
    RUNTIME DESTINATION bin)

# Install public headers
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/bcad/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bcad
        FILES_MATCHING PATTERN "*.h")
# BCADConfig.cmake n'est pas installe ici : il est genere a la racine de
# l'arbre binaire (configure_package_config_file), pas dans celui de l'application.
# Chercher le fichier dans ce repertoire fait echouer tout cmake --install.
