/*
 * This file is part of NWN Emitter Editor.
 * Copyright (C) 2025 Varenx
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <GLFW/glfw3.h>
#include <chrono>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <memory>
#include "camera.hpp"
#include "emitter.hpp"
#include "file_dialog.hpp"
#include "grab_mode.hpp"
#include "particle_system.hpp"
#include "property_editor.hpp"
#include "toast_manager.hpp"

void errorCallback(int error, const char* description)
{
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

// Global variables for mouse input
static Camera* g_camera = nullptr;
static bool g_middleMousePressed = false;
static bool g_shiftPressed = false;
static bool g_viewportHovered = false;
static bool g_cameraActive = false;
static ImVec2 g_viewportMin, g_viewportMax;

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_MIDDLE)
    {
        if (action == GLFW_PRESS)
        {
            // Start camera interaction only if mouse is over viewport
            if (g_viewportHovered)
            {
                g_middleMousePressed = true;
                g_cameraActive = true;
                g_shiftPressed = (mods & GLFW_MOD_SHIFT) != 0;

                // Initialize camera's mouse position to prevent jumping
                double mouseX, mouseY;
                glfwGetCursorPos(window, &mouseX, &mouseY);
                if (g_camera)
                {
                    g_camera->setLastMousePosition(mouseX, mouseY);
                }
            }
        }
        else
        {
            // Stop camera interaction
            g_middleMousePressed = false;
            g_cameraActive = false;
        }
    }
}

// Global variables for hotkeys
static bool g_ctrlS_pressed = false;
static bool g_ctrlO_pressed = false;
static bool g_ctrlQ_pressed = false;
static bool g_ctrlN_pressed = false;
static bool g_ctrlShiftS_pressed = false;
static bool g_shiftA_pressed = false;
static bool g_shiftD_pressed = false;
static bool g_x_pressed = false;
static bool g_y_pressed = false;
static bool g_z_pressed = false;
static bool g_g_pressed = false;
static bool g_s_pressed = false;
static bool g_r_pressed = false;
static bool g_esc_pressed = false;

// Grab mode state
static GrabMode g_grabMode = GrabMode::None;
static glm::vec3 g_grabStartPosition = glm::vec3(0.0f);
static glm::vec3 g_grabCurrentPosition = glm::vec3(0.0f);
static ImVec2 g_grabStartMouse = ImVec2(0.0f, 0.0f);
static int g_grabbedEmitter = -1;

// Scale mode state
static ScaleMode g_scaleMode = ScaleMode::None;
static glm::vec2 g_scaleStartSize = glm::vec2(0.0f);
static glm::vec2 g_scaleCurrentSize = glm::vec2(0.0f);
static ImVec2 g_scaleStartMouse = ImVec2(0.0f, 0.0f);
static int g_scaledEmitter = -1;

// Rotation mode state
static RotationMode g_rotationMode = RotationMode::None;
static glm::vec3 g_rotationStartRotation = glm::vec3(0.0f);
static glm::vec3 g_rotationCurrentRotation = glm::vec3(0.0f);
static ImVec2 g_rotationStartMouse = ImVec2(0.0f, 0.0f);
static int g_rotatedEmitter = -1;

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    g_shiftPressed = (mods & GLFW_MOD_SHIFT) != 0;

    // Handle hotkeys on key press
    if (action == GLFW_PRESS)
    {
        if (mods & GLFW_MOD_CONTROL)
        {
            switch (key)
            {
            case GLFW_KEY_S:
                if (mods & GLFW_MOD_SHIFT)
                {
                    g_ctrlShiftS_pressed = true;
                }
                else
                {
                    g_ctrlS_pressed = true;
                }
                break;
            case GLFW_KEY_O:
                g_ctrlO_pressed = true;
                break;
            case GLFW_KEY_Q:
                g_ctrlQ_pressed = true;
                break;
            case GLFW_KEY_N:
                g_ctrlN_pressed = true;
                break;
            }
        }
        else if (mods & GLFW_MOD_SHIFT)
        {
            switch (key)
            {
            case GLFW_KEY_A:
                g_shiftA_pressed = true;
                break;
            case GLFW_KEY_D:
                g_shiftD_pressed = true;
                break;
            case GLFW_KEY_X:
                g_x_pressed = true;
                break;
            case GLFW_KEY_Y:
                g_y_pressed = true;
                break;
            case GLFW_KEY_Z:
                g_z_pressed = true;
                break;
            }
        }
        else if (mods == 0)
        {
            switch (key)
            {
            case GLFW_KEY_X:
                g_x_pressed = true;
                break;
            case GLFW_KEY_G:
                g_g_pressed = true;
                break;
            case GLFW_KEY_S:
                g_s_pressed = true;
                break;
            case GLFW_KEY_R:
                g_r_pressed = true;
                break;
            case GLFW_KEY_ESCAPE:
                g_esc_pressed = true;
                break;
            case GLFW_KEY_Y:
                g_y_pressed = true;
                break;
            case GLFW_KEY_Z:
                g_z_pressed = true;
                break;
            }
        }
    }
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (g_camera && g_viewportHovered)
    {
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        g_camera->update(mouseX, mouseY, false, false, static_cast<float>(yoffset));
    }
}

int main()
{
    glfwSetErrorCallback(errorCallback);

    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // OpenGL 4.1 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(1600, 900, "NWN Emitter Editor", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Set up input callbacks
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetScrollCallback(window, scrollCallback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Setup ImGui context with docking
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    // Initialize application components
    EmitterEditor emitterEditor;
    ParticleRenderer particleRenderer;
    PropertyEditor propertyEditor;
    Camera camera;
    ToastManager toastManager;

    g_camera = &camera; // Set global pointer for callbacks

    particleRenderer.initialize();
    particleRenderer.setTextureDirectory(emitterEditor.getTextureDirectory());

    int selectedEmitter = 0;
    bool showMDLText = true;

    auto lastTime = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Update camera continuously when active (allows dragging outside viewport)
        if (g_cameraActive)
        {
            double mouseX, mouseY;
            glfwGetCursorPos(window, &mouseX, &mouseY);
            camera.update(mouseX, mouseY, g_middleMousePressed, g_shiftPressed, 0.0f);

            // Set appropriate cursor shape
            if (g_shiftPressed)
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll); // Pan cursor
            }
            else
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); // Rotate cursor
            }
        }
        else
        {
            // Reset cursor when not actively manipulating
            ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
        }

        // Calculate delta time
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Update toast manager
        toastManager.update(deltaTime);

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Get window size
        int windowWidth, windowHeight;
        glfwGetFramebufferSize(window, &windowWidth, &windowHeight);

        // Setup docking
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::Begin("DockSpace", nullptr, windowFlags);
        ImGui::PopStyleVar(2);

        // Menu bar
        static std::string loadFile, saveFile;
        static std::string currentFilePath; // Track current file for quick save
        static bool openLoadDialog = false;
        static bool openSaveDialog = false;
        static bool openSaveAsDialog = false;
        static bool showAboutModal = false;

        // Grab mode
        if (g_grabMode != GrabMode::None)
        {
            // Cancel
            if (g_esc_pressed)
            {
                // Restore original position
                if (g_grabbedEmitter >= 0 && g_grabbedEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
                {
                    emitterEditor.getEmitters()[g_grabbedEmitter].position = g_grabStartPosition;
                }
                g_grabMode = GrabMode::None;
                g_grabbedEmitter = -1;
                g_esc_pressed = false;
            }

            // Handle axis constraints during grab mode
            if (g_x_pressed)
            {
                if (g_shiftPressed)
                {
                    g_grabMode = GrabMode::YZ_Plane; // Move on Y-Z plane (exclude X)
                }
                else
                {
                    g_grabMode = GrabMode::X_Axis; // Move only on X axis
                }
                g_x_pressed = false;
            }
            if (g_y_pressed)
            {
                if (g_shiftPressed)
                {
                    g_grabMode = GrabMode::XZ_Plane; // Move on X-Z plane (exclude Y)
                }
                else
                {
                    g_grabMode = GrabMode::Y_Axis; // Move only on Y axis
                }
                g_y_pressed = false;
            }
            if (g_z_pressed)
            {
                if (g_shiftPressed)
                {
                    g_grabMode = GrabMode::XY_Plane; // Move on X-Y plane (exclude Z)
                }
                else
                {
                    g_grabMode = GrabMode::Z_Axis; // Move only on Z axis
                }
                g_z_pressed = false;
            }

            // Clear other hotkeys during grab mode to prevent normal actions
            g_ctrlS_pressed = false;
            g_ctrlO_pressed = false;
            g_ctrlQ_pressed = false;
            g_ctrlN_pressed = false;
            g_ctrlShiftS_pressed = false;
            g_shiftA_pressed = false;
            g_shiftD_pressed = false;
        }
        else
        {
            if (g_g_pressed)
            {
                if (g_viewportHovered && selectedEmitter >= 0 &&
                    selectedEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
                {
                    g_grabMode = GrabMode::Free;
                    g_grabbedEmitter = selectedEmitter;
                    g_grabStartPosition = emitterEditor.getEmitters()[selectedEmitter].position;
                    g_grabCurrentPosition = g_grabStartPosition;
                    ImVec2 mousePos = ImGui::GetMousePos();
                    g_grabStartMouse = mousePos;
                }
                g_g_pressed = false;
            }
        }

        if (g_scaleMode != ScaleMode::None)
        {
            if (g_esc_pressed)
            {
                if (g_scaledEmitter >= 0 && g_scaledEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
                {
                    emitterEditor.getEmitters()[g_scaledEmitter].xsize = g_scaleStartSize.x;
                    emitterEditor.getEmitters()[g_scaledEmitter].ysize = g_scaleStartSize.y;
                }
                g_scaleMode = ScaleMode::None;
                g_scaledEmitter = -1;
                g_esc_pressed = false;
            }

            g_ctrlS_pressed = false;
            g_ctrlO_pressed = false;
            g_ctrlQ_pressed = false;
            g_ctrlN_pressed = false;
            g_ctrlShiftS_pressed = false;
            g_shiftA_pressed = false;
            g_shiftD_pressed = false;
            g_x_pressed = false;
            g_y_pressed = false;
            g_z_pressed = false;
            g_g_pressed = false;
            g_r_pressed = false;
        }
        else
        {
            if (g_s_pressed)
            {
                if (g_viewportHovered && selectedEmitter >= 0 &&
                    selectedEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
                {
                    g_scaleMode = ScaleMode::Uniform;
                    g_scaledEmitter = selectedEmitter;
                    g_scaleStartSize = glm::vec2(emitterEditor.getEmitters()[selectedEmitter].xsize,
                                                 emitterEditor.getEmitters()[selectedEmitter].ysize);
                    g_scaleCurrentSize = g_scaleStartSize;

                    ImVec2 mousePos = ImGui::GetMousePos();
                    g_scaleStartMouse = mousePos;
                }
                g_s_pressed = false;
            }
        }

        if (g_rotationMode != RotationMode::None)
        {
            if (g_esc_pressed)
            {
                if (g_rotatedEmitter >= 0 && g_rotatedEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
                {
                    emitterEditor.getEmitters()[g_rotatedEmitter].rotationAngles = g_rotationStartRotation;
                }
                g_rotationMode = RotationMode::None;
                g_rotatedEmitter = -1;
                g_esc_pressed = false;
            }

            // Handle axis constraints during rotation mode
            if (g_x_pressed)
            {
                g_rotationMode = RotationMode::X_Axis;
                g_x_pressed = false;
            }
            if (g_y_pressed)
            {
                g_rotationMode = RotationMode::Y_Axis;
                g_y_pressed = false;
            }
            if (g_z_pressed)
            {
                g_rotationMode = RotationMode::Z_Axis;
                g_z_pressed = false;
            }

            g_ctrlS_pressed = false;
            g_ctrlO_pressed = false;
            g_ctrlQ_pressed = false;
            g_ctrlN_pressed = false;
            g_ctrlShiftS_pressed = false;
            g_shiftA_pressed = false;
            g_shiftD_pressed = false;
            g_g_pressed = false;
            g_s_pressed = false;
        }
        else
        {
            if (g_r_pressed)
            {
                if (g_viewportHovered && selectedEmitter >= 0 &&
                    selectedEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
                {
                    g_rotationMode = RotationMode::Free;
                    g_rotatedEmitter = selectedEmitter;
                    g_rotationStartRotation = emitterEditor.getEmitters()[selectedEmitter].rotationAngles;
                    g_rotationCurrentRotation = g_rotationStartRotation;
                    ImVec2 mousePos = ImGui::GetMousePos();
                    g_rotationStartMouse = mousePos;
                }
                g_r_pressed = false;
            }
        }

        if (g_grabMode == GrabMode::None && g_scaleMode == ScaleMode::None && g_rotationMode == RotationMode::None &&
            g_ctrlS_pressed)
        {
            if (!currentFilePath.empty())
            {
                // Quick save to existing file
                std::string modelName = FileDialog::extractModelName(currentFilePath);
                emitterEditor.setModelName(modelName);
                emitterEditor.saveToMDL(currentFilePath);
                toastManager.addToast("MDL Saved", currentFilePath);
            }
            else
            {
                // No file path, open save dialog
                openSaveDialog = true;
            }
            g_ctrlS_pressed = false;
        }
        if (g_grabMode == GrabMode::None && g_scaleMode == ScaleMode::None && g_rotationMode == RotationMode::None &&
            g_ctrlShiftS_pressed)
        {
            openSaveAsDialog = true;
            g_ctrlShiftS_pressed = false;
        }
        if (g_grabMode == GrabMode::None && g_scaleMode == ScaleMode::None && g_rotationMode == RotationMode::None &&
            g_ctrlO_pressed)
        {
            openLoadDialog = true;
            g_ctrlO_pressed = false;
        }
        if (g_grabMode == GrabMode::None && g_scaleMode == ScaleMode::None && g_rotationMode == RotationMode::None &&
            g_ctrlQ_pressed)
        {
            glfwSetWindowShouldClose(window, true);
            g_ctrlQ_pressed = false;
        }
        if (g_grabMode == GrabMode::None && g_scaleMode == ScaleMode::None && g_rotationMode == RotationMode::None &&
            g_ctrlN_pressed)
        {
            emitterEditor.resetToNew();
            camera.reset();
            selectedEmitter = 0;
            currentFilePath = ""; // Clear file path for new file
            g_ctrlN_pressed = false;
        }
        if (g_grabMode == GrabMode::None && g_scaleMode == ScaleMode::None && g_rotationMode == RotationMode::None &&
            g_shiftA_pressed)
        {
            if (g_viewportHovered)
            {
                emitterEditor.addEmitter("emitter_" + std::to_string(emitterEditor.getEmitters().size() + 1));
                selectedEmitter = static_cast<int>(emitterEditor.getEmitters().size() - 1);
            }
            g_shiftA_pressed = false;
        }
        if (g_grabMode == GrabMode::None && g_scaleMode == ScaleMode::None && g_rotationMode == RotationMode::None &&
            g_shiftD_pressed)
        {
            if (g_viewportHovered && selectedEmitter >= 0 &&
                selectedEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
            {
                emitterEditor.duplicateEmitter(selectedEmitter);
                selectedEmitter = static_cast<int>(emitterEditor.getEmitters().size() - 1);
            }
            g_shiftD_pressed = false;
        }
        if (g_grabMode == GrabMode::None && g_scaleMode == ScaleMode::None && g_rotationMode == RotationMode::None &&
            g_x_pressed)
        {
            if (g_viewportHovered && selectedEmitter >= 0 &&
                selectedEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
            {
                emitterEditor.removeEmitter(selectedEmitter);
                selectedEmitter = std::min(selectedEmitter, static_cast<int>(emitterEditor.getEmitters().size() - 1));
                if (selectedEmitter < 0 && !emitterEditor.getEmitters().empty())
                    selectedEmitter = 0;
            }
            g_x_pressed = false;
        }

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New MDL", "Ctrl+N"))
                {
                    emitterEditor.resetToNew();
                    camera.reset();
                    selectedEmitter = 0;
                    currentFilePath = ""; // Clear file path for new file
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Load MDL...", "Ctrl+O"))
                {
                    openLoadDialog = true;
                }
                if (ImGui::MenuItem("Save MDL", "Ctrl+S"))
                {
                    if (!currentFilePath.empty())
                    {
                        // Quick save to existing file
                        std::string modelName = FileDialog::extractModelName(currentFilePath);
                        emitterEditor.setModelName(modelName);
                        emitterEditor.saveToMDL(currentFilePath);
                        toastManager.addToast("MDL Saved", currentFilePath);
                    }
                    else
                    {
                        // No file path, open save dialog
                        openSaveDialog = true;
                    }
                }
                if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
                {
                    openSaveAsDialog = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Ctrl+Q"))
                {
                    glfwSetWindowShouldClose(window, true);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Add Emitter", "Shift+A"))
                {
                    emitterEditor.addEmitter("emitter_" + std::to_string(emitterEditor.getEmitters().size() + 1));
                    selectedEmitter = static_cast<int>(emitterEditor.getEmitters().size() - 1);
                }
                if (ImGui::MenuItem("Duplicate Emitter", "Shift+D", false,
                                    selectedEmitter >= 0 &&
                                        selectedEmitter < static_cast<int>(emitterEditor.getEmitters().size())))
                {
                    if (selectedEmitter >= 0 && selectedEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
                    {
                        emitterEditor.duplicateEmitter(selectedEmitter);
                        selectedEmitter = static_cast<int>(emitterEditor.getEmitters().size() - 1);
                    }
                }
                if (ImGui::MenuItem("Delete Emitter", "X", false,
                                    selectedEmitter >= 0 &&
                                        selectedEmitter < static_cast<int>(emitterEditor.getEmitters().size())))
                {
                    if (selectedEmitter >= 0 && selectedEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
                    {
                        emitterEditor.removeEmitter(selectedEmitter);
                        selectedEmitter =
                            std::min(selectedEmitter, static_cast<int>(emitterEditor.getEmitters().size() - 1));
                        if (selectedEmitter < 0 && !emitterEditor.getEmitters().empty())
                            selectedEmitter = 0;
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("MDL Text", nullptr, &showMDLText);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help"))
            {
                if (ImGui::MenuItem("About"))
                {
                    showAboutModal = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // Open popups based on flags
        if (openLoadDialog)
        {
            FileDialog::clearFileCache();
            ImGui::OpenPopup("Load MDL File");
            openLoadDialog = false;
        }
        if (openSaveDialog)
        {
            FileDialog::clearFileCache();
            ImGui::OpenPopup("Save MDL File");
            openSaveDialog = false;
        }
        if (openSaveAsDialog)
        {
            FileDialog::clearFileCache();
            ImGui::OpenPopup("Save As MDL File");
            openSaveAsDialog = false;
        }
        if (showAboutModal)
        {
            ImGui::OpenPopup("About");
            showAboutModal = false;
        }

        // Create docking space
        ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

        // File dialogs need to be rendered inside the docking space
        if (FileDialog::renderLoadDialog("Load MDL File", loadFile))
        {
            emitterEditor.loadFromMDL(loadFile);
            particleRenderer.setTextureDirectory(emitterEditor.getTextureDirectory());
            selectedEmitter = 0;
            currentFilePath = loadFile; // Remember loaded file path
            // Update the last saved filename when loading a file
            std::string modelName = FileDialog::extractModelName(loadFile);
            FileDialog::setLastSavedFilename(modelName);
        }

        if (FileDialog::renderSaveDialog("Save MDL File", saveFile))
        {
            std::string modelName = FileDialog::extractModelName(saveFile);
            emitterEditor.setModelName(modelName);
            emitterEditor.saveToMDL(saveFile);
            currentFilePath = saveFile; // Remember saved file path
            toastManager.addToast("MDL Saved", saveFile);
        }

        if (FileDialog::renderSaveAsDialog("Save As MDL File", saveFile, currentFilePath))
        {
            std::string modelName = FileDialog::extractModelName(saveFile);
            emitterEditor.setModelName(modelName);
            emitterEditor.saveToMDL(saveFile);
            currentFilePath = saveFile; // Update current file path to new location
            toastManager.addToast("MDL Saved", saveFile);
        }

        // About modal dialog
        if (ImGui::BeginPopupModal("About", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("NWN Emitter Editor");
            ImGui::Text("Copyright (C) 2025 Varenx");
            ImGui::Separator();

            ImGui::Text("This program is free software: you can redistribute it and/or modify");
            ImGui::Text("it under the terms of the GNU General Public License as published by");
            ImGui::Text("the Free Software Foundation, either version 3 of the License, or");
            ImGui::Text("(at your option) any later version.");
            ImGui::Separator();

            ImGui::Text("Special Thanks:");
            ImGui::BulletText("Neverwinter Vault Discord Community");
            ImGui::Text("   First and foremost, for keeping this game alive");
            ImGui::BulletText("Sean Barrett - STB Image library");
            ImGui::Text("   https://github.com/nothings");
            ImGui::BulletText("Omar Cornut - Dear ImGui");
            ImGui::Text("   https://github.com/ocornut");
            ImGui::BulletText("G-Truc Creation - GLM Mathematics Library");
            ImGui::Text("   https://github.com/g-truc/glm");
            ImGui::BulletText("Marcus Geelnard & Camilla Löwy and all contributors - GLFW");
            ImGui::Text("   https://www.glfw.org");

            ImGui::Separator();
            if (ImGui::Button("OK"))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::End();

        // Property Editor Panel
        propertyEditor.render(emitterEditor, selectedEmitter);

        // Particle Preview Panel
        ImGui::Begin("Particle Preview");
        glm::vec3 camPos = camera.getPosition();
        ImGui::Text("Camera: (%.1f, %.1f, %.1f)", camPos.x, camPos.y, camPos.z);
        ImGui::Text("Middle mouse: rotate | Shift+Middle: pan | Scroll: zoom");

        ImGui::Text("Emitter transformations: G: Grab | S: Scale | R: Rotate");
        ImGui::Text("While transforming: X/Y/Z to constrain to axis, Shift+X/Y/Z to exclude axis");

        // Simple controls for camera
        if (ImGui::Button("Reset Camera"))
        {
            camera.reset();
        }

        // Get available space for 3D viewport
        ImVec2 previewSize = ImGui::GetContentRegionAvail();
        if (previewSize.x > 50 && previewSize.y > 50)
        { // Minimum size check
            // Setup camera matrices
            glm::mat4 view = camera.getViewMatrix();
            glm::mat4 projection = camera.getProjectionMatrix(previewSize.x / previewSize.y);
            particleRenderer.setCamera(view, projection);

            // Render to framebuffer texture
            particleRenderer.renderToTexture(emitterEditor.getEmitters(), deltaTime, (int)previewSize.x,
                                             (int)previewSize.y, selectedEmitter);

            if (g_grabMode != GrabMode::None && g_grabbedEmitter >= 0 &&
                g_grabbedEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
            {

                glBindFramebuffer(GL_FRAMEBUFFER, particleRenderer.getFramebuffer());
                glViewport(0, 0, (int)previewSize.x, (int)previewSize.y);

                glm::vec3 emitterPos = emitterEditor.getEmitters()[g_grabbedEmitter].position;
                particleRenderer.renderGrabModeIndicator((int)previewSize.x, (int)previewSize.y, g_grabMode,
                                                         emitterPos);

                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }

            if (g_scaleMode != ScaleMode::None && g_scaledEmitter >= 0 &&
                g_scaledEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
            {

                glBindFramebuffer(GL_FRAMEBUFFER, particleRenderer.getFramebuffer());
                glViewport(0, 0, (int)previewSize.x, (int)previewSize.y);

                glm::vec3 emitterPos = emitterEditor.getEmitters()[g_scaledEmitter].position;
                glm::vec2 currentSize = glm::vec2(emitterEditor.getEmitters()[g_scaledEmitter].xsize,
                                                  emitterEditor.getEmitters()[g_scaledEmitter].ysize);
                particleRenderer.renderScaleModeIndicator((int)previewSize.x, (int)previewSize.y, g_scaleMode,
                                                          emitterPos, currentSize);

                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }

            if (g_rotationMode != RotationMode::None && g_rotatedEmitter >= 0 &&
                g_rotatedEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
            {

                glBindFramebuffer(GL_FRAMEBUFFER, particleRenderer.getFramebuffer());
                glViewport(0, 0, (int)previewSize.x, (int)previewSize.y);

                glm::vec3 emitterPos = emitterEditor.getEmitters()[g_rotatedEmitter].position;
                particleRenderer.renderRotationModeIndicator((int)previewSize.x, (int)previewSize.y, g_rotationMode,
                                                             emitterPos);

                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }

            // Display the texture in ImGui
            GLuint textureID = particleRenderer.getFramebufferTexture();
            if (textureID != 0)
            {
                ImVec2 imagePos = ImGui::GetCursorScreenPos();
                ImGui::Image(textureID, previewSize, ImVec2(0, 1), ImVec2(1, 0)); // Flip Y for OpenGL

                // Update hover state
                g_viewportHovered = ImGui::IsItemHovered();

                // Store viewport bounds for cursor clamping
                g_viewportMin = imagePos;
                g_viewportMax = ImVec2(imagePos.x + previewSize.x, imagePos.y + previewSize.y);

                if (g_grabMode != GrabMode::None)
                {
                    if (g_viewportHovered)
                    {
                        ImVec2 mousePos = ImGui::GetMousePos();
                        float relativeX = mousePos.x - imagePos.x;
                        float relativeY = mousePos.y - imagePos.y;

                        float mouseDeltaX = relativeX - (g_grabStartMouse.x - imagePos.x);
                        float mouseDeltaY = relativeY - (g_grabStartMouse.y - imagePos.y);
                        glm::vec3 constrainedDelta =
                            particleRenderer.mouseToCameraRelativeMovement(mouseDeltaX, mouseDeltaY, g_grabMode);

                        if (g_grabbedEmitter >= 0 &&
                            g_grabbedEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
                        {
                            emitterEditor.getEmitters()[g_grabbedEmitter].position =
                                g_grabStartPosition + constrainedDelta;
                        }
                    }

                    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                    {
                        g_grabMode = GrabMode::None;
                        g_grabbedEmitter = -1;
                    }
                    else if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                    {
                        if (g_grabbedEmitter >= 0 &&
                            g_grabbedEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
                        {
                            emitterEditor.getEmitters()[g_grabbedEmitter].position = g_grabStartPosition;
                        }
                        g_grabMode = GrabMode::None;
                        g_grabbedEmitter = -1;
                    }
                }
                else if (g_scaleMode != ScaleMode::None)
                {
                    if (g_viewportHovered)
                    {
                        ImVec2 mousePos = ImGui::GetMousePos();
                        float relativeX = mousePos.x - imagePos.x;
                        float relativeY = mousePos.y - imagePos.y;

                        float mouseDeltaX = relativeX - (g_scaleStartMouse.x - imagePos.x);
                        float mouseDeltaY = relativeY - (g_scaleStartMouse.y - imagePos.y);
                        glm::vec2 newSize =
                            particleRenderer.mouseToScale(mouseDeltaX, mouseDeltaY, g_scaleStartSize, g_scaleMode);

                        if (g_scaledEmitter >= 0 &&
                            g_scaledEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
                        {
                            emitterEditor.getEmitters()[g_scaledEmitter].xsize = newSize.x;
                            emitterEditor.getEmitters()[g_scaledEmitter].ysize = newSize.y;
                        }
                    }

                    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                    {
                        g_scaleMode = ScaleMode::None;
                        g_scaledEmitter = -1;
                    }
                    else if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                    {
                        if (g_scaledEmitter >= 0 &&
                            g_scaledEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
                        {
                            emitterEditor.getEmitters()[g_scaledEmitter].xsize = g_scaleStartSize.x;
                            emitterEditor.getEmitters()[g_scaledEmitter].ysize = g_scaleStartSize.y;
                        }
                        g_scaleMode = ScaleMode::None;
                        g_scaledEmitter = -1;
                    }
                }
                else if (g_rotationMode != RotationMode::None)
                {
                    if (g_viewportHovered)
                    {
                        ImVec2 mousePos = ImGui::GetMousePos();
                        float relativeX = mousePos.x - imagePos.x;
                        float relativeY = mousePos.y - imagePos.y;

                        float mouseDeltaX = relativeX - (g_rotationStartMouse.x - imagePos.x);
                        float mouseDeltaY = relativeY - (g_rotationStartMouse.y - imagePos.y);
                        glm::vec3 rotationDelta =
                            particleRenderer.mouseToRotation(mouseDeltaX, mouseDeltaY, g_rotationMode);

                        if (g_rotatedEmitter >= 0 &&
                            g_rotatedEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
                        {
                            emitterEditor.getEmitters()[g_rotatedEmitter].rotationAngles =
                                g_rotationStartRotation + rotationDelta;
                        }
                    }

                    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                    {
                        g_rotationMode = RotationMode::None;
                        g_rotatedEmitter = -1;
                    }
                    else if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                    {
                        if (g_rotatedEmitter >= 0 &&
                            g_rotatedEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
                        {
                            emitterEditor.getEmitters()[g_rotatedEmitter].rotationAngles = g_rotationStartRotation;
                        }
                        g_rotationMode = RotationMode::None;
                        g_rotatedEmitter = -1;
                    }
                }
                else
                {
                    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                    {
                        ImVec2 mousePos = ImGui::GetMousePos();
                        float relativeX = mousePos.x - imagePos.x;
                        float relativeY = mousePos.y - imagePos.y;

                        if (relativeX >= 0 && relativeX < previewSize.x && relativeY >= 0 && relativeY < previewSize.y)
                        {
                            int pickedEmitter =
                                particleRenderer.pickEmitter(emitterEditor.getEmitters(), relativeX, relativeY,
                                                             (int)previewSize.x, (int)previewSize.y);
                            if (pickedEmitter >= 0)
                            {
                                selectedEmitter = pickedEmitter;
                            }
                        }
                    }
                }

                // Get axis endpoint positions that follow camera rotation
                auto axisPositions =
                    particleRenderer.getAxisGizmoScreenPositions((int)previewSize.x, (int)previewSize.y);

                // Draw axis labels at the projected 3D positions
                ImDrawList* drawList = ImGui::GetWindowDrawList();

                const char* axisLabels[] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
                ImU32 axisColors[] = {
                    IM_COL32(255, 80, 80, 255), // +X bright red
                    IM_COL32(128, 40, 40, 255), // -X dark red
                    IM_COL32(80, 255, 80, 255), // +Y bright green
                    IM_COL32(40, 128, 40, 255), // -Y dark green
                    IM_COL32(80, 80, 255, 255), // +Z bright blue
                    IM_COL32(40, 40, 128, 255) // -Z dark blue
                };

                for (size_t i = 0; i < axisPositions.size() && i < 6; ++i)
                {
                    ImVec2 textPos = ImVec2(imagePos.x + axisPositions[i].x,
                                            imagePos.y + (previewSize.y - axisPositions[i].y) // Flip Y coordinate
                    );

                    // Add small background for better text visibility
                    ImVec2 textSize = ImGui::CalcTextSize(axisLabels[i]);
                    drawList->AddRectFilled(ImVec2(textPos.x - 2, textPos.y - 2),
                                            ImVec2(textPos.x + textSize.x + 2, textPos.y + textSize.y + 2),
                                            IM_COL32(0, 0, 0, 150));

                    drawList->AddText(textPos, axisColors[i], axisLabels[i]);
                }

                // Display particle counters above orientation gizmo
                int activeParticleCount =
                    (selectedEmitter >= 0 && selectedEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
                    ? particleRenderer.getActiveParticleCount(selectedEmitter)
                    : 0;
                int totalParticleCount = particleRenderer.getTotalActiveParticleCount();

                // Position counters above the gizmo area (top-right corner)
                ImVec2 counterPos = ImVec2(imagePos.x + previewSize.x - 120.0f, imagePos.y + 10.0f);

                // Create counter text
                std::string activeCountText = "Active: " + std::to_string(activeParticleCount);
                std::string totalCountText = "Total: " + std::to_string(totalParticleCount);

                // Draw active emitter particle count
                ImVec2 activeTextSize = ImGui::CalcTextSize(activeCountText.c_str());
                drawList->AddRectFilled(
                    ImVec2(counterPos.x - 4, counterPos.y - 2),
                    ImVec2(counterPos.x + activeTextSize.x + 4, counterPos.y + activeTextSize.y + 2),
                    IM_COL32(0, 0, 0, 180));
                drawList->AddText(counterPos, IM_COL32(255, 255, 255, 255), activeCountText.c_str());

                // Draw total particle count below the active count
                ImVec2 totalCounterPos = ImVec2(counterPos.x, counterPos.y + activeTextSize.y + 4);
                ImVec2 totalTextSize = ImGui::CalcTextSize(totalCountText.c_str());
                drawList->AddRectFilled(
                    ImVec2(totalCounterPos.x - 4, totalCounterPos.y - 2),
                    ImVec2(totalCounterPos.x + totalTextSize.x + 4, totalCounterPos.y + totalTextSize.y + 2),
                    IM_COL32(0, 0, 0, 180));
                drawList->AddText(totalCounterPos, IM_COL32(200, 200, 255, 255), totalCountText.c_str());
            }
            else
            {
                ImGui::Dummy(previewSize);
                ImGui::Text("No texture rendered");
                g_viewportHovered = false; // Reset hover state when no texture
                g_cameraActive = false; // Reset camera active state
            }
        }
        else
        {
            g_viewportHovered = false; // Reset hover state when viewport too small
            g_cameraActive = false; // Reset camera active state
        }

        ImGui::End();

        // MDL Text View Panel
        if (showMDLText)
        {
            ImGui::Begin("MDL Text View", &showMDLText);

            std::string mdlText = emitterEditor.generateMDLText();
            ImGui::TextUnformatted(mdlText.c_str());

            if (ImGui::Button("Copy to Clipboard"))
            {
                ImGui::SetClipboardText(mdlText.c_str());
            }

            ImGui::End();
        }

        // Render toast notifications (should be rendered last to appear on top)
        toastManager.render();

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


        glfwSwapBuffers(window);
    }

    particleRenderer.cleanup();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
