cmake_minimum_required(VERSION 2.8)
project(glwfdemo)

set(CMAKE_CXX_STANDARD 11)

include_directories(/usr/local/include)

add_executable(${PROJECT_NAME} main.cpp)

find_library(lib_opengl OpenGL)
find_library(lib_glfw glfw)

target_link_libraries(${PROJECT_NAME} ${lib_opengl} ${lib_glfw})
