cmake_minimum_required(VERSION 3.15)
project(close2gl-without-rasterization C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 11)

# GLFW
find_package(glfw3 REQUIRED)

# GLM
find_package(glm REQUIRED)

# GLAD
add_library(glad external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)

# ImGui
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.6
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/imgui
)
FetchContent_MakeAvailable(imgui)

add_library(imgui
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC external/imgui external/imgui/backends)
target_link_libraries(imgui glfw)

add_executable(app src/main.cpp)

target_compile_definitions(app PRIVATE SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders")
target_compile_definitions(app PRIVATE MODEL_DIR="${CMAKE_SOURCE_DIR}/models")

target_link_libraries(app
    glad
    glfw
    GL
    glm::glm
    imgui
)
