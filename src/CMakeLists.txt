cmake_minimum_required(VERSION 3.4.3)

set(CMAKE_CXX_FLAGS "-O2 -Wall -Wextra")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project (RoboticEye)
add_executable (render render.cpp)

target_link_libraries (render glfw GL GLU GLEW)

