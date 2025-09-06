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

#ifndef PROPERTY_EDITOR_HPP
#define PROPERTY_EDITOR_HPP

#include <imgui.h>
#include "emitter.hpp"

class PropertyEditor
{
public:
    PropertyEditor();
    ~PropertyEditor();

    void render(EmitterEditor& editor, int& selectedEmitter);
    bool hasChanges() const { return propertiesChanged; }
    void resetChangeFlag() { propertiesChanged = false; }

private:
    void renderOutliner(EmitterEditor& editor, int& selectedEmitter);
    void renderEmitterProperties(EmitterNode& emitter);

    bool propertiesChanged;

    void renderEnumCombo(const char* label, int& value, const char* const* items, int itemCount);
    void renderUpdateTypeCombo(UpdateType& updateType);
    void renderRenderTypeCombo(RenderType& renderType);
    void renderBlendTypeCombo(BlendType& blendType);
    void renderSpawnTypeCombo(SpawnType& spawnType);

    bool renderColorEdit(const char* label, glm::vec3& color);
    bool renderVec3Edit(const char* label, glm::vec3& vec);
    bool renderQuatEdit(const char* label, glm::quat& quat);
    bool renderEditableFloat(const char* label, float& value, float speed = 0.1f, float min = 0.0f, float max = 0.0f);
    bool renderEditableInt(const char* label, int& value, int min = 0, int max = 0);
};

#endif // PROPERTY_EDITOR_HPP
