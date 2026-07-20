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
    # Q_OBJECT headers must be listed explicitly: they live under include/,
    # not next to their .cpp, so AUTOMOC's same-directory header pairing
    # can't find them by basename alone.
    ${CMAKE_SOURCE_DIR}/include/bcad/app/MainWindow.h
    ${CMAKE_SOURCE_DIR}/include/bcad/app/Viewport.h
    ${CMAKE_SOURCE_DIR}/include/bcad/app/LayerPanel.h
    ${CMAKE_SOURCE_DIR}/include/bcad/app/TessellationWorker.h
    ${CMAKE_SOURCE_DIR}/include/bcad/app/RibbonBar.h
    ${CMAKE_SOURCE_DIR}/include/bcad/app/PropertiesPanel.h
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
    Qt6::Widgets
    Qt6::OpenGLWidgets
    Qt6::OpenGL
    Qt6::Gui
)
