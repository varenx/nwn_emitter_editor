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
#include <algorithm>

Camera::Camera() :
    target(0.0f, 0.0f, 0.0f), up(0.0f, 0.0f, 1.0f), distance(5.0f), yaw(180.0f), pitch(0.0f), lastMouseX(0.0),
    lastMouseY(0.0), firstMouse(true)
{
    updatePosition();
}

void Camera::update(double mouseX, double mouseY, bool middlePressed, bool shiftPressed, float scrollOffset)
{
    // Handle scroll (dollying)
    if (scrollOffset != 0.0f)
    {
        distance *= (1.0f - scrollOffset * 0.1f);
        distance = std::clamp(distance, 0.1f, 50.0f);
        updatePosition();
    }

    // Handle mouse input
    if (middlePressed)
    {
        if (firstMouse)
        {
            lastMouseX = mouseX;
            lastMouseY = mouseY;
            firstMouse = false;
            return;
        }

        double deltaX = mouseX - lastMouseX;
        double deltaY = mouseY - lastMouseY;

        if (shiftPressed)
        {
            // Panning (shift + middle mouse)
            float sensitivity = 0.01f;
            glm::vec3 right = glm::normalize(glm::cross(position - target, up));
            glm::vec3 localUp = glm::normalize(glm::cross(right, position - target));

            target += right * static_cast<float>(deltaX * sensitivity * distance);
            target += localUp * static_cast<float>(deltaY * sensitivity * distance);
            updatePosition();
        }
        else
        {
            // Turntable rotation (middle mouse)
            float sensitivity = 0.5f;
            yaw += static_cast<float>(deltaX * sensitivity);
            pitch += static_cast<float>(deltaY * sensitivity);

            // Clamp pitch to avoid flipping
            pitch = std::clamp(pitch, -89.0f, 89.0f);

            updatePosition();
        }
    }
    else
    {
        firstMouse = true;
    }

    lastMouseX = mouseX;
    lastMouseY = mouseY;
}

void Camera::updatePosition()
{
    // Convert spherical coordinates to cartesian (right-handed: X=left-to-right, Y=away-from-viewer, Z=up)
    float x = distance * cos(glm::radians(pitch)) * sin(glm::radians(yaw));
    float y = distance * cos(glm::radians(pitch)) * cos(glm::radians(yaw));
    float z = distance * sin(glm::radians(pitch));

    position = target + glm::vec3(x, y, z);
}

glm::mat4 Camera::getViewMatrix() const { return glm::lookAt(position, target, up); }

glm::mat4 Camera::getProjectionMatrix(float aspect) const
{
    return glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
}

void Camera::reset()
{
    target = glm::vec3(0.0f, 0.0f, 0.0f);
    distance = 5.0f;
    yaw = 180.0f; // Position camera at negative Y (toward viewer)
    pitch = 0.0f; // Level with the ground plane
    firstMouse = true; // Reset mouse tracking
    updatePosition();
}

void Camera::setLastMousePosition(double mouseX, double mouseY)
{
    lastMouseX = mouseX;
    lastMouseY = mouseY;
    firstMouse = false; // Mark that we have a valid mouse position
}
