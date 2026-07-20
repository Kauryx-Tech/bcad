cmake_minimum_required(VERSION 3.20)
project(bcad LANGUAGES CXX)

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

add_subdirectory(src/geometry)
add_subdirectory(src/layers)
add_subdirectory(src/io)
add_subdirectory(src/render)
add_subdirectory(src/core)
add_subdirectory(src/app)

enable_testing()
add_subdirectory(tests)
