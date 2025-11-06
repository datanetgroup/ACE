cmake_minimum_required(VERSION 3.20)
project(ACE LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(External/glfw)
add_subdirectory(Engine)
add_subdirectory(Editor)
add_subdirectory(Tools/Launcher)
