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

#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
public:
    Camera();
    void update(double mouseX, double mouseY, bool middlePressed, bool shiftPressed,
                float scrollOffset);
    void reset();
    void setLastMousePosition(double mouseX, double mouseY);
    [[nodiscard]] glm::mat4 getViewMatrix() const;
    [[nodiscard]] glm::mat4 getProjectionMatrix(float aspect) const;
    [[nodiscard]] glm::vec3 getPosition() const
    {
        return position;
    }
    [[nodiscard]] glm::vec3 getTarget() const
    {
        return target;
    }

private:
    glm::vec3 position;
    glm::vec3 target;
    glm::vec3 up;
    float distance;
    float yaw;
    float pitch;
    double lastMouseX, lastMouseY;
    bool firstMouse;

    void updatePosition();
};

#endif// CAMERA_HPP