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

#include "camera.hpp"
#include "emitter.hpp"
#include "file_dialog.hpp"
#include "particle_system.hpp"
#include "property_editor.hpp"
#include "toast_manager.hpp"
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
        } else
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
                    } else
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
        } else if (mods & GLFW_MOD_SHIFT)
        {
            switch (key)
            {
                case GLFW_KEY_A:
                    g_shiftA_pressed = true;
                    break;
                case GLFW_KEY_D:
                    g_shiftD_pressed = true;
                    break;
            }
        } else if (mods == 0)
        {
            switch (key)
            {
                case GLFW_KEY_X:
                    g_x_pressed = true;
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
    glfwSwapInterval(1);// Enable vsync

    // Set up input callbacks
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetScrollCallback(window, scrollCallback);

    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress))
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

    g_camera = &camera;// Set global pointer for callbacks

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
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);// Pan cursor
            } else
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);// Rotate cursor
            }
        } else
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

        ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::Begin("DockSpace", nullptr, windowFlags);
        ImGui::PopStyleVar(2);

        // Menu bar
        static std::string loadFile, saveFile;
        static std::string currentFilePath;// Track current file for quick save
        static bool openLoadDialog = false;
        static bool openSaveDialog = false;
        static bool openSaveAsDialog = false;
        static bool showAboutModal = false;

        // Handle hotkeys
        if (g_ctrlS_pressed)
        {
            if (!currentFilePath.empty())
            {
                // Quick save to existing file
                std::string modelName = FileDialog::extractModelName(currentFilePath);
                emitterEditor.setModelName(modelName);
                emitterEditor.saveToMDL(currentFilePath);
                toastManager.addToast("MDL Saved", currentFilePath);
            } else
            {
                // No file path, open save dialog
                openSaveDialog = true;
            }
            g_ctrlS_pressed = false;
        }
        if (g_ctrlShiftS_pressed)
        {
            openSaveAsDialog = true;
            g_ctrlShiftS_pressed = false;
        }
        if (g_ctrlO_pressed)
        {
            openLoadDialog = true;
            g_ctrlO_pressed = false;
        }
        if (g_ctrlQ_pressed)
        {
            glfwSetWindowShouldClose(window, true);
            g_ctrlQ_pressed = false;
        }
        if (g_ctrlN_pressed)
        {
            emitterEditor.resetToNew();
            camera.reset();
            selectedEmitter = 0;
            currentFilePath = "";// Clear file path for new file
            g_ctrlN_pressed = false;
        }
        if (g_shiftA_pressed)
        {
            emitterEditor.addEmitter("emitter_" +
                                     std::to_string(emitterEditor.getEmitters().size() + 1));
            selectedEmitter = static_cast<int>(emitterEditor.getEmitters().size() - 1);
            g_shiftA_pressed = false;
        }
        if (g_shiftD_pressed)
        {
            if (selectedEmitter >= 0 &&
                selectedEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
            {
                emitterEditor.duplicateEmitter(selectedEmitter);
                selectedEmitter = static_cast<int>(emitterEditor.getEmitters().size() - 1);
            }
            g_shiftD_pressed = false;
        }
        if (g_x_pressed)
        {
            if (selectedEmitter >= 0 &&
                selectedEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
            {
                emitterEditor.removeEmitter(selectedEmitter);
                selectedEmitter = std::min(
                    selectedEmitter, static_cast<int>(emitterEditor.getEmitters().size() - 1));
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
                    currentFilePath = "";// Clear file path for new file
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
                    } else
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
                    emitterEditor.addEmitter(
                        "emitter_" + std::to_string(emitterEditor.getEmitters().size() + 1));
                    selectedEmitter = static_cast<int>(emitterEditor.getEmitters().size() - 1);
                }
                if (ImGui::MenuItem("Duplicate Emitter", "Shift+D", false,
                                    selectedEmitter >= 0 &&
                                        selectedEmitter <
                                            static_cast<int>(emitterEditor.getEmitters().size())))
                {
                    if (selectedEmitter >= 0 &&
                        selectedEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
                    {
                        emitterEditor.duplicateEmitter(selectedEmitter);
                        selectedEmitter = static_cast<int>(emitterEditor.getEmitters().size() - 1);
                    }
                }
                if (ImGui::MenuItem("Delete Emitter", "X", false,
                                    selectedEmitter >= 0 &&
                                        selectedEmitter <
                                            static_cast<int>(emitterEditor.getEmitters().size())))
                {
                    if (selectedEmitter >= 0 &&
                        selectedEmitter < static_cast<int>(emitterEditor.getEmitters().size()))
                    {
                        emitterEditor.removeEmitter(selectedEmitter);
                        selectedEmitter =
                            std::min(selectedEmitter,
                                     static_cast<int>(emitterEditor.getEmitters().size() - 1));
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
            ImGui::OpenPopup("Load MDL File");
            openLoadDialog = false;
        }
        if (openSaveDialog)
        {
            ImGui::OpenPopup("Save MDL File");
            openSaveDialog = false;
        }
        if (openSaveAsDialog)
        {
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
            currentFilePath = loadFile;// Remember loaded file path
            // Update the last saved filename when loading a file
            std::string modelName = FileDialog::extractModelName(loadFile);
            FileDialog::setLastSavedFilename(modelName);
        }

        if (FileDialog::renderSaveDialog("Save MDL File", saveFile))
        {
            std::string modelName = FileDialog::extractModelName(saveFile);
            emitterEditor.setModelName(modelName);
            emitterEditor.saveToMDL(saveFile);
            currentFilePath = saveFile;// Remember saved file path
            toastManager.addToast("MDL Saved", saveFile);
        }

        if (FileDialog::renderSaveAsDialog("Save As MDL File", saveFile, currentFilePath))
        {
            std::string modelName = FileDialog::extractModelName(saveFile);
            emitterEditor.setModelName(modelName);
            emitterEditor.saveToMDL(saveFile);
            currentFilePath = saveFile;// Update current file path to new location
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
            ImGui::BulletText("Sean Barrett - STB Image library");
            ImGui::Text("   https://github.com/nothings");
            ImGui::BulletText("Omar Cornut - Dear ImGui");
            ImGui::Text("   https://github.com/ocornut");
            ImGui::BulletText("Neverwinter Vault Discord Community");

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

        // Simple controls for camera
        if (ImGui::Button("Reset Camera"))
        {
            camera.reset();
        }

        // Get available space for 3D viewport
        ImVec2 previewSize = ImGui::GetContentRegionAvail();
        if (previewSize.x > 50 && previewSize.y > 50)
        {// Minimum size check
            // Setup camera matrices
            glm::mat4 view = camera.getViewMatrix();
            glm::mat4 projection = camera.getProjectionMatrix(previewSize.x / previewSize.y);
            particleRenderer.setCamera(view, projection);

            // Render to framebuffer texture
            particleRenderer.renderToTexture(emitterEditor.getEmitters(), deltaTime,
                                             (int) previewSize.x, (int) previewSize.y,
                                             selectedEmitter);

            // Display the texture in ImGui
            GLuint textureID = particleRenderer.getFramebufferTexture();
            if (textureID != 0)
            {
                ImVec2 imagePos = ImGui::GetCursorScreenPos();
                ImGui::Image(textureID, previewSize, ImVec2(0, 1),
                             ImVec2(1, 0));// Flip Y for OpenGL

                // Update hover state
                g_viewportHovered = ImGui::IsItemHovered();

                // Store viewport bounds for cursor clamping
                g_viewportMin = imagePos;
                g_viewportMax = ImVec2(imagePos.x + previewSize.x, imagePos.y + previewSize.y);

                // Get axis endpoint positions that follow camera rotation
                auto axisPositions = particleRenderer.getAxisGizmoScreenPositions(
                    (int) previewSize.x, (int) previewSize.y);

                // Draw axis labels at the projected 3D positions
                ImDrawList* drawList = ImGui::GetWindowDrawList();

                const char* axisLabels[] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
                ImU32 axisColors[] = {
                    IM_COL32(255, 80, 80, 255),// +X bright red
                    IM_COL32(128, 40, 40, 255),// -X dark red
                    IM_COL32(80, 255, 80, 255),// +Y bright green
                    IM_COL32(40, 128, 40, 255),// -Y dark green
                    IM_COL32(80, 80, 255, 255),// +Z bright blue
                    IM_COL32(40, 40, 128, 255) // -Z dark blue
                };

                for (size_t i = 0; i < axisPositions.size() && i < 6; ++i)
                {
                    ImVec2 textPos =
                        ImVec2(imagePos.x + axisPositions[i].x,
                               imagePos.y + (previewSize.y - axisPositions[i].y)// Flip Y coordinate
                        );

                    // Add small background for better text visibility
                    ImVec2 textSize = ImGui::CalcTextSize(axisLabels[i]);
                    drawList->AddRectFilled(
                        ImVec2(textPos.x - 2, textPos.y - 2),
                        ImVec2(textPos.x + textSize.x + 2, textPos.y + textSize.y + 2),
                        IM_COL32(0, 0, 0, 150));

                    drawList->AddText(textPos, axisColors[i], axisLabels[i]);
                }
            } else
            {
                ImGui::Dummy(previewSize);
                ImGui::Text("No texture rendered");
                g_viewportHovered = false;// Reset hover state when no texture
                g_cameraActive = false;   // Reset camera active state
            }
        } else
        {
            g_viewportHovered = false;// Reset hover state when viewport too small
            g_cameraActive = false;   // Reset camera active state
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
