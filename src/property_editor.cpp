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

#include "property_editor.hpp"
#include "file_dialog.hpp"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

PropertyEditor::PropertyEditor() : propertiesChanged(false)
{
}

PropertyEditor::~PropertyEditor() = default;

void PropertyEditor::render(EmitterEditor& editor, int& selectedEmitter)
{
    // Render outliner window
    renderOutliner(editor, selectedEmitter);

    // Render property editor window
    ImGui::Begin("Property Editor");

    auto& emitters = editor.getEmitters();
    if (selectedEmitter >= 0 && selectedEmitter < static_cast<int>(emitters.size()))
    {
        renderEmitterProperties(emitters[selectedEmitter]);
    } else
    {
        ImGui::Text("No emitter selected");
        ImGui::Text("Select an emitter from the Outliner to edit its properties.");
    }

    ImGui::End();
}

void PropertyEditor::renderOutliner(EmitterEditor& editor, int& selectedEmitter)
{
    ImGui::Begin("Outliner");

    auto& emitters = editor.getEmitters();
    ImGui::Text("Emitters (%zu)", emitters.size());

    if (ImGui::Button("Add Emitter"))
    {
        editor.addEmitter("emitter_" + std::to_string(emitters.size() + 1));
        selectedEmitter = static_cast<int>(emitters.size() - 1);
        propertiesChanged = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Remove Emitter") && selectedEmitter >= 0 &&
        selectedEmitter < static_cast<int>(emitters.size()))
    {
        editor.removeEmitter(selectedEmitter);
        selectedEmitter = std::min(selectedEmitter, static_cast<int>(emitters.size() - 1));
        if (selectedEmitter < 0 && !emitters.empty())
            selectedEmitter = 0;
        propertiesChanged = true;
    }

    ImGui::Separator();

    // List emitters
    for (int i = 0; i < static_cast<int>(emitters.size()); ++i)
    {
        bool isSelected = (i == selectedEmitter);
        if (ImGui::Selectable(emitters[i].name.c_str(), isSelected))
        {
            selectedEmitter = i;
        }
    }

    ImGui::End();
}


void PropertyEditor::renderEmitterProperties(EmitterNode& emitter)
{
    ImGui::Text("Emitter: %s", emitter.name.c_str());

    if (ImGui::CollapsingHeader("Basic Properties", ImGuiTreeNodeFlags_DefaultOpen))
    {
        char nameBuf[256];
        strcpy(nameBuf, emitter.name.c_str());
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
        {
            std::string newName = nameBuf;
            if (newName.empty() || newName.find_first_not_of(" \t\n\r") == std::string::npos)
            {
                // Empty or whitespace-only name, use default
                emitter.name = "default_emitter";
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                                   "Warning: Empty name replaced with default");
            } else
            {
                emitter.name = newName;
            }
            propertiesChanged = true;
        }

        char parentBuf[256];
        strcpy(parentBuf, emitter.parent.c_str());
        if (ImGui::InputText("Parent", parentBuf, sizeof(parentBuf)))
        {
            emitter.parent = parentBuf;
            propertiesChanged = true;
        }

        renderUpdateTypeCombo(emitter.update);
        renderRenderTypeCombo(emitter.render);
        renderBlendTypeCombo(emitter.blend);
        renderSpawnTypeCombo(emitter.spawntype);
    }

    if (ImGui::CollapsingHeader("Texture Properties"))
    {
        ImGui::Text("Texture: %s", emitter.texture.empty() ? "(none)" : emitter.texture.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Browse...##texture"))
        {
            ImGui::OpenPopup("Select Texture");
        }

        // Texture selection dialog
        static std::string selectedTexturePath;
        if (FileDialog::renderTextureDialog("Select Texture", selectedTexturePath))
        {
            emitter.texturePath = selectedTexturePath;
            emitter.texture = std::filesystem::path(selectedTexturePath).stem().string();
            propertiesChanged = true;
        }

        if (renderEditableInt("X Grid", emitter.xgrid, 1, 16))
            propertiesChanged = true;
        if (renderEditableInt("Y Grid", emitter.ygrid, 1, 16))
            propertiesChanged = true;
        if (ImGui::Checkbox("Loop", &emitter.loop))
            propertiesChanged = true;
        if (renderEditableFloat("Dead Space", emitter.deadspace, 0.01f, 0.0f, 1.0f))
            propertiesChanged = true;
        if (ImGui::Checkbox("Two-sided Texture", &emitter.twosidedtex))
            propertiesChanged = true;
        if (renderEditableFloat("FPS", emitter.fps, 1.0f))
            propertiesChanged = true;
        if (renderEditableFloat("Frame Start", emitter.frameStart, 1.0f))
            propertiesChanged = true;
        if (renderEditableFloat("Frame End", emitter.frameEnd, 1.0f))
            propertiesChanged = true;
    }

    if (ImGui::CollapsingHeader("Transform"))
    {
        if (renderVec3Edit("Position", emitter.position))
            propertiesChanged = true;
        if (renderVec3Edit("Orientation (°)", emitter.eulerAngles))
            propertiesChanged = true;
    }

    if (ImGui::CollapsingHeader("Emitter Geometry"))
    {
        if (renderEditableFloat("X Size", emitter.xsize, 0.1f))
            propertiesChanged = true;
        if (renderEditableFloat("Y Size", emitter.ysize, 0.1f))
            propertiesChanged = true;
    }

    if (ImGui::CollapsingHeader("Particle Behavior", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (renderEditableFloat("Birth Rate", emitter.birthrate, 0.1f, 0.0f, 500.0f))
            propertiesChanged = true;
        if (renderEditableFloat("Life Expectancy", emitter.lifeExp, 0.1f, 0.1f))
            propertiesChanged = true;
        if (renderEditableFloat("Velocity", emitter.velocity, 0.1f))
            propertiesChanged = true;
        if (renderEditableFloat("Spread", emitter.spread, 1.0f, 0.0f, 360.0f))
            propertiesChanged = true;
        if (renderEditableFloat("Mass", emitter.mass, 0.1f))
            propertiesChanged = true;
        if (renderEditableFloat("Particle Rotation", emitter.particleRot, 0.1f))
            propertiesChanged = true;
        if (renderEditableFloat("Gravity", emitter.grav, 0.1f))
            propertiesChanged = true;
        if (renderEditableFloat("Drag", emitter.drag, 0.01f, 0.0f, 1.0f))
            propertiesChanged = true;
        if (renderEditableFloat("Threshold", emitter.threshold, 1.0f))
            propertiesChanged = true;
    }

    if (ImGui::CollapsingHeader("Color and Alpha", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (renderColorEdit("Color Start", emitter.colorStart))
            propertiesChanged = true;
        if (renderColorEdit("Color End", emitter.colorEnd))
            propertiesChanged = true;
        if (renderEditableFloat("Alpha Start", emitter.alphaStart, 0.01f, 0.0f, 1.0f))
            propertiesChanged = true;
        if (renderEditableFloat("Alpha End", emitter.alphaEnd, 0.01f, 0.0f, 1.0f))
            propertiesChanged = true;
    }

    if (ImGui::CollapsingHeader("Size"))
    {
        if (renderEditableFloat("Size Start", emitter.sizeStart, 0.1f))
            propertiesChanged = true;
        if (renderEditableFloat("Size End", emitter.sizeEnd, 0.1f))
            propertiesChanged = true;
        if (renderEditableFloat("Size Start Y", emitter.sizeStart_y, 0.1f))
            propertiesChanged = true;
        if (renderEditableFloat("Size End Y", emitter.sizeEnd_y, 0.1f))
            propertiesChanged = true;
    }


    if (ImGui::CollapsingHeader("Blast Properties"))
    {
        if (renderEditableFloat("Blast Radius", emitter.blastRadius, 1.0f))
            propertiesChanged = true;
        if (renderEditableFloat("Blast Length", emitter.blastLength, 1.0f))
            propertiesChanged = true;
    }

    if (ImGui::CollapsingHeader("Lightning Properties"))
    {
        if (renderEditableFloat("Lightning Delay", emitter.lightningDelay, 0.1f))
            propertiesChanged = true;
        if (renderEditableFloat("Lightning Radius", emitter.lightningRadius, 1.0f))
            propertiesChanged = true;
        if (renderEditableFloat("Lightning Scale", emitter.lightningScale, 0.1f))
            propertiesChanged = true;
        if (renderEditableFloat("Lightning Sub Div", emitter.lightningSubDiv, 0.1f))
            propertiesChanged = true;
        if (renderEditableFloat("Lightning Zig Zag", emitter.lightningZigZag, 0.1f))
            propertiesChanged = true;
    }

    if (ImGui::CollapsingHeader("Advanced Properties"))
    {
        if (ImGui::Checkbox("P2P", &emitter.p2p))
            propertiesChanged = true;
        if (renderEditableInt("P2P Selection", emitter.p2p_sel, 1, 10))
            propertiesChanged = true;
        if (ImGui::Checkbox("Affected by Wind", &emitter.affectedByWind))
            propertiesChanged = true;
        if (ImGui::Checkbox("Is Tinted", &emitter.m_isTinted))
            propertiesChanged = true;
        if (ImGui::Checkbox("Bounce", &emitter.bounce))
            propertiesChanged = true;
        if (ImGui::Checkbox("Random", &emitter.random))
            propertiesChanged = true;
        if (ImGui::Checkbox("Inherit", &emitter.inherit))
            propertiesChanged = true;
        if (ImGui::Checkbox("Inherit Velocity", &emitter.inheritvel))
            propertiesChanged = true;
        if (ImGui::Checkbox("Inherit Local", &emitter.inherit_local))
            propertiesChanged = true;
        if (ImGui::Checkbox("Splat", &emitter.splat))
            propertiesChanged = true;
        if (ImGui::Checkbox("Inherit Part", &emitter.inherit_part))
            propertiesChanged = true;
        if (renderEditableInt("Render Order", emitter.renderorder))
            propertiesChanged = true;
        if (renderEditableFloat("Bounce Coefficient", emitter.bounce_co, 0.01f, 0.0f, 1.0f))
            propertiesChanged = true;
        if (renderEditableFloat("Combine Time", emitter.combinetime, 0.1f))
            propertiesChanged = true;
        if (renderEditableFloat("Blur Length", emitter.blurlength, 0.1f))
            propertiesChanged = true;
    }
}

void PropertyEditor::renderUpdateTypeCombo(UpdateType& updateType)
{
    const char* updateTypes[] = {"Fountain", "Single", "Explosion", "Lightning"};
    int currentUpdate = static_cast<int>(updateType);

    if (ImGui::Combo("Update Type", &currentUpdate, updateTypes, 4))
    {
        updateType = static_cast<UpdateType>(currentUpdate);
        propertiesChanged = true;
    }
}

void PropertyEditor::renderRenderTypeCombo(RenderType& renderType)
{
    const char* renderTypes[] = {"Normal",
                                 "Linked",
                                 "Billboard to Local Z",
                                 "Billboard to World Z",
                                 "Aligned to World Z",
                                 "Aligned to Particle Direction",
                                 "Motion Blur"};
    int currentRender = static_cast<int>(renderType);

    if (ImGui::Combo("Render Type", &currentRender, renderTypes, 7))
    {
        renderType = static_cast<RenderType>(currentRender);
        propertiesChanged = true;
    }
}

void PropertyEditor::renderBlendTypeCombo(BlendType& blendType)
{
    const char* blendTypes[] = {"Normal", "Punch-Through", "Lighten"};
    int currentBlend = static_cast<int>(blendType);

    if (ImGui::Combo("Blend Type", &currentBlend, blendTypes, 3))
    {
        blendType = static_cast<BlendType>(currentBlend);
        propertiesChanged = true;
    }
}

void PropertyEditor::renderSpawnTypeCombo(SpawnType& spawnType)
{
    const char* spawnTypes[] = {"Normal", "Trail"};
    int currentSpawn = static_cast<int>(spawnType);

    if (ImGui::Combo("Spawn Type", &currentSpawn, spawnTypes, 2))
    {
        spawnType = static_cast<SpawnType>(currentSpawn);
        propertiesChanged = true;
    }
}

bool PropertyEditor::renderColorEdit(const char* label, glm::vec3& color)
{
    float colorArray[3] = {color.r, color.g, color.b};
    bool changed = ImGui::ColorEdit3(label, colorArray);
    if (changed)
    {
        color.r = colorArray[0];
        color.g = colorArray[1];
        color.b = colorArray[2];
    }
    return changed;
}

bool PropertyEditor::renderVec3Edit(const char* label, glm::vec3& vec)
{
    float vecArray[3] = {vec.x, vec.y, vec.z};
    bool changed = ImGui::DragFloat3(label, vecArray, 0.1f);
    if (changed)
    {
        vec.x = vecArray[0];
        vec.y = vecArray[1];
        vec.z = vecArray[2];
    }
    return changed;
}

bool PropertyEditor::renderQuatEdit(const char* label, glm::quat& quat)
{
    // Convert quaternion to euler angles for editing
    glm::vec3 euler = glm::degrees(glm::eulerAngles(quat));
    float eulerArray[3] = {euler.x, euler.y, euler.z};

    bool changed = ImGui::DragFloat3(label, eulerArray, 1.0f, -360.0f, 360.0f);
    if (changed)
    {
        euler.x = glm::radians(eulerArray[0]);
        euler.y = glm::radians(eulerArray[1]);
        euler.z = glm::radians(eulerArray[2]);
        quat = glm::quat(euler);
    }
    return changed;
}

bool PropertyEditor::renderEditableFloat(const char* label, float& value, float speed, float min,
                                         float max)
{
    // Use InputFloat which allows typing and double-click editing
    if (min == 0.0f && max == 0.0f)
    {
        // No limits - use InputFloat for direct text editing
        return ImGui::InputFloat(label, &value, speed, speed * 10.0f, "%.3f");
    } else
    {
        // With limits - use DragFloat but with much wider range
        return ImGui::DragFloat(label, &value, speed, min, max);
    }
}

bool PropertyEditor::renderEditableInt(const char* label, int& value, int min, int max)
{
    // Use InputInt which allows typing and editing
    if (min == 0 && max == 0)
    {
        // No limits
        return ImGui::InputInt(label, &value);
    } else
    {
        // With limits - use DragInt
        return ImGui::DragInt(label, &value, 1.0f, min, max);
    }
}