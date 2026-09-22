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

target_include_directories(bcad PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(bcad PRIVATE
    bcad_core
    bcad_render
    bcad_io
    bcad_geometry
    bcad_layers
    bcad_commands
    Qt6::Widgets
    Qt6::OpenGLWidgets
    Qt6::OpenGL
    Qt6::Gui
)

install(TARGETS bcad
    RUNTIME DESTINATION bin)
