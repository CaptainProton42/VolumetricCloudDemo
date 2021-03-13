project(VolumetricCloudsDemo LANGUAGES CXX)
cmake_minimum_required(VERSION 3.0)

# Static linking of GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(3rdparty/glfw)
add_subdirectory(3rdparty/imgui)

# We need an OpenGL install
find_package(OpenGL REQUIRED)

# Include directory
include_directories(include)
include_directories(3rdparty/imgui)

add_executable(main main.cpp 3rdparty/glad.c)
target_link_libraries(main glfw)
target_link_libraries(main OpenGL::GL)
target_link_libraries(main imgui)