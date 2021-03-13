project(imgui LANGUAGES CXX)

cmake_minimum_required(VERSION 3.0)

include_directories(../glfw/include)
include_directories(../../include) # for glad
include_directories(./)

add_compile_definitions(IMGUI_IMPL_OPENGL_LOADER_GLAD)

add_library(imgui imgui_demo.cpp
                  imgui_draw.cpp
                  imgui_tables.cpp
                  imgui_widgets.cpp
                  imgui.cpp
                  backends/imgui_impl_opengl3.cpp
                  backends/imgui_impl_win32.cpp
                  backends/imgui_impl_glfw.cpp)