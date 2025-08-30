cmake_minimum_required(VERSION 3.20)
project(nwn_emitter_editor)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(PkgConfig REQUIRED)

# Set up directories
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
set(VENDOR_DIR ${CMAKE_SOURCE_DIR}/vendor)

# Include directories
include_directories(${INCLUDE_DIR})
include_directories(${VENDOR_DIR})
include_directories(${VENDOR_DIR}/imgui)
include_directories(${VENDOR_DIR}/imgui/backends)
include_directories(${VENDOR_DIR}/glad/include)
include_directories(${VENDOR_DIR}/stb)

# ImGui sources
set(IMGUI_SOURCES
    ${VENDOR_DIR}/imgui/imgui.cpp
    ${VENDOR_DIR}/imgui/imgui_demo.cpp
    ${VENDOR_DIR}/imgui/imgui_draw.cpp
    ${VENDOR_DIR}/imgui/imgui_tables.cpp
    ${VENDOR_DIR}/imgui/imgui_widgets.cpp
    ${VENDOR_DIR}/imgui/backends/imgui_impl_glfw.cpp
    ${VENDOR_DIR}/imgui/backends/imgui_impl_opengl3.cpp
)

# GLAD sources
set(GLAD_SOURCES
    ${VENDOR_DIR}/glad/src/glad.c
)

# Project sources
set(PROJECT_SOURCES
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/camera.cpp
    ${SRC_DIR}/emitter.cpp
    ${SRC_DIR}/file_dialog.cpp
    ${SRC_DIR}/particle_system.cpp
    ${SRC_DIR}/property_editor.cpp
    ${SRC_DIR}/toast_manager.cpp
    ${SRC_DIR}/stb_dds.cpp
    ${INCLUDE_DIR}/camera.hpp
    ${INCLUDE_DIR}/emitter.hpp
    ${INCLUDE_DIR}/file_dialog.hpp
    ${INCLUDE_DIR}/particle_system.hpp
    ${INCLUDE_DIR}/property_editor.hpp
    ${INCLUDE_DIR}/toast_manager.hpp
    ${INCLUDE_DIR}/stb_dds.hpp
)

# Create executable
add_executable(${PROJECT_NAME} 
    ${PROJECT_SOURCES}
    ${IMGUI_SOURCES}
    ${GLAD_SOURCES}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    OpenGL::GL
    glfw
    ${CMAKE_DL_LIBS}
)

# Compiler flags
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)
