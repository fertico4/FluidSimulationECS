cmake_minimum_required(VERSION 3.24)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(FluidSimulationECS VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (APPLE)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -mcpu=apple-m4 -ffast-math")
    add_compile_definitions(GL_SILENCE_DEPRECATION)
endif ()

# Automatic Dependencies Management
include(FetchContent)

# ImGUI
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG         v1.90.4
)

# GLM
FetchContent_Declare(
        glm
        GIT_REPOSITORY  https://github.com/g-truc/glm.git
        GIT_TAG         1.0.1
)

# GLFW
FetchContent_Declare(
        glfw
        GIT_REPOSITORY  https://github.com/glfw/glfw.git
        GIT_TAG         3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# EnTT
FetchContent_Declare(
        entt
        GIT_REPOSITORY  https://github.com/skypjack/entt.git
        GIT_TAG         v3.13.0
)

# GLAD
FetchContent_Declare(
        glad
        GIT_REPOSITORY  https://github.com/Dav1dde/glad.git
        GIT_TAG         v2.0.6
        SOURCE_SUBDIR   cmake
)

FetchContent_MakeAvailable(imgui glm glfw entt glad)

if (APPLE)
    glad_add_library(glad_graphics STATIC REPRODUCIBLE LOADER API gl:core=4.1)
else ()
    glad_add_library(glad_graphics STATIC REPRODUCIBLE DISEXT LOADER API gl:core=4.3)
endif ()

# Project Launcher Config

set(SOURCE_FILES
        src/main.cpp
        src/Core/Engine.h
        src/Core/Engine.cpp
        src/ECS/Components.h
        src/ECS/Systems.h
        src/Render/Renderer.h
        src/Render/Renderer.cpp
        src/Physics/SpatialGrid.cpp
        src/Physics/SpatialGrid.h

        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE
        src
        ${entt_SOURCE_DIR}/src
        ${glfw_SOURCE_DIR}/include
        ${glm_SOURCE_DIR}
        ${CMAKE_BINARY_DIR}/_deps/glad-build/include
        ${CMAKE_BINARY_DIR}/_deps/glad-generated/include
        glad_graphics
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)

if (APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
            EnTT::EnTT
            glm::glm
            glfw
            glad_graphics
            "-framework Cocoa"
            "-framework IOKit"
            "-framework CoreVideo"
            "-framework OpenGL"
    )
else ()
    find_package(OpenGL REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE EnTT::EnTT glm::glm glfw glad_graphics OpenGL::GL)
endif ()

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/shaders"
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")