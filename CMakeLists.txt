cmake_minimum_required(VERSION 3.20)
project(nwn_emitter_editor)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use local vendor copies
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(vendor/glfw3)
add_subdirectory(vendor/glm)


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
        glfw
        glm::glm
)

# Define GLM_ENABLE_EXPERIMENTAL for GTX extensions
target_compile_definitions(${PROJECT_NAME} PRIVATE GLM_ENABLE_EXPERIMENTAL GLFW_INCLUDE_NONE)

