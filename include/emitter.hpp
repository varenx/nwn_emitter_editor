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

#ifndef EMITTER_HPP
#define EMITTER_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <string>
#include <vector>

struct AnimationKeyframe {
    float time;
    glm::vec3 value;

    AnimationKeyframe() : time(0.0f), value(0.0f) {}
    AnimationKeyframe(float t, const glm::vec3 &v) : time(t), value(v) {}
};

struct AnimationTrack {
    std::vector<AnimationKeyframe> keyframes;

    glm::vec3 getValueAtTime(float time) const
    {
        if (keyframes.empty())
            return glm::vec3(0.0f);
        if (keyframes.size() == 1)
            return keyframes[0].value;

        // Find the two keyframes to interpolate between
        for (size_t i = 0; i < keyframes.size() - 1; ++i) {
            if (time >= keyframes[i].time && time <= keyframes[i + 1].time) {
                float t = (time - keyframes[i].time) / (keyframes[i + 1].time - keyframes[i].time);
                return glm::mix(keyframes[i].value, keyframes[i + 1].value, t);
            }
        }

        // If time is outside range, return closest value
        if (time < keyframes[0].time)
            return keyframes[0].value;
        return keyframes.back().value;
    }
};

enum class UpdateType { Fountain, Single, Explosion, Lightning };

enum class RenderType {
    Normal,
    Linked,
    Billboard_to_Local_Z,
    Billboard_to_World_Z,
    Aligned_to_World_Z,
    Aligned_to_Particle_Direction,
    Motion_Blur
};

enum class BlendType { Normal, Punch_Through, Lighten };

enum class SpawnType { Normal = 0, Trail = 1 };

struct EmitterNode {
    std::string name = "emitter";
    std::string parent = "NULL";

    // Core properties
    bool p2p = false;
    int p2p_sel = 1;
    bool affectedByWind = false;
    bool m_isTinted = false;
    bool bounce = false;
    bool random = false;
    bool inherit = true;
    bool inheritvel = false;
    bool inherit_local = false;
    bool splat = false;
    bool inherit_part = false;
    int renderorder = 0;
    SpawnType spawntype = SpawnType::Normal;

    UpdateType update = UpdateType::Fountain;
    RenderType render = RenderType::Normal;
    BlendType blend = BlendType::Normal;

    // Texture properties
    std::string texture = ""; // Display name (filename without extension)
    std::string texturePath = ""; // Full file path for loading
    int xgrid = 1;
    int ygrid = 1;
    bool loop = false;
    float deadspace = 0.0f;
    bool twosidedtex = false;

    // Blast properties
    float blastRadius = 0.0f;
    float blastLength = 0.0f;

    // Transform
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    glm::vec3 rotationAngles = {0.0f, 0.0f, 0.0f}; // X, Y, Z axis rotations in degrees

    // Get quaternion for rendering (converted from stored angles)
    glm::quat getOrientation() const { return glm::quat(glm::radians(rotationAngles)); }

    // Emitter geometry
    float xsize = 0.0f;
    float ysize = 0.0f;

    // Particle behavior
    float birthrate = 1.0f;
    float lifeExp = 1.0f;
    float velocity = 1.0f;
    float spread = 0.0f;
    float mass = 1.0f;
    float particleRot = 0.0f;

    // Color and opacity
    glm::vec3 colorStart = {1.0f, 1.0f, 1.0f};
    glm::vec3 colorEnd = {1.0f, 1.0f, 1.0f};
    float alphaStart = 1.0f;
    float alphaEnd = 1.0f;

    // Size
    float sizeStart = 1.0f;
    float sizeEnd = 1.0f;
    float sizeStart_y = 0.0f;
    float sizeEnd_y = 0.0f;

    // Advanced properties
    float grav = 0.0f;
    float drag = 0.0f;
    float threshold = 0.0f;
    float fps = 0.0f;
    float frameStart = 0.0f;
    float frameEnd = 0.0f;

    // Bounce properties
    float bounce_co = 0.0f;

    // Additional properties for complete coverage
    float combinetime = 0.0f;
    float blurlength = 0.0f;
    float lightningDelay = 0.0f;
    float lightningRadius = 0.0f;
    float lightningScale = 0.0f;
    float lightningSubDiv = 0.0f;
    float lightningZigZag = 0.0f;


    // Animation tracks
    AnimationTrack positionKeys;
    AnimationTrack orientationKeys;

    // Get animated values at specific time
    glm::vec3 getAnimatedPosition(float time) const
    {
        if (positionKeys.keyframes.empty())
            return position;
        return positionKeys.getValueAtTime(time);
    }

    glm::vec3 getAnimatedOrientation(float time) const
    {
        if (orientationKeys.keyframes.empty())
            return glm::vec3(0.0f);
        return orientationKeys.getValueAtTime(time);
    }

    // Coordinate conversion helpers (MDL uses Y-up, editor uses Z-up)
    static glm::vec3 convertMDLToGame(const glm::vec3 &mdlPos)
    {
        // MDL Y-up → Editor Z-up: X→X, Y→Z, Z→Y
        return glm::vec3(mdlPos.x, mdlPos.z, mdlPos.y);
    }

    static glm::vec3 convertGameToMDL(const glm::vec3 &gamePos)
    {
        // Editor Z-up → MDL Y-up: X→X, Z→Y, Y→Z
        return glm::vec3(gamePos.x, gamePos.z, gamePos.y);
    }

    static glm::quat convertMDLToGameOrientation(const glm::quat &mdlQuat)
    {
        // Convert Y-up quaternion to Z-up quaternion
        // To rotate Y-up to Z-up: rotate around Y-axis
        glm::quat coordTransform = glm::angleAxis(glm::radians(-90.0f), glm::vec3(0, 1, 0));
        return coordTransform * mdlQuat;
    }

    static glm::quat convertGameToMDLOrientation(const glm::quat &gameQuat)
    {
        // Convert Z-up quaternion back to Y-up quaternion
        // Apply inverse transformation: rotate around Y-axis
        glm::quat coordTransform = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0));
        return coordTransform * gameQuat;
    }
};

class EmitterEditor {
public:
    EmitterEditor();
    ~EmitterEditor();

    void addEmitter(const std::string &name = "emitter");
    void removeEmitter(int index);
    void duplicateEmitter(int index);
    void resetToNew();

private:
    EmitterNode createDefaultEmitter();

public:
    void loadFromMDL(const std::string &filename);
    void saveToMDL(const std::string &filename);
    void setModelName(const std::string &name);

    std::vector<EmitterNode> &getEmitters() { return emitters; }
    const std::vector<EmitterNode> &getEmitters() const { return emitters; }

    std::string generateMDLText() const;
    std::string getTextureDirectory() const { return textureDirectory; }

private:
    std::vector<EmitterNode> emitters;
    std::string modelName = "emitter_model";
    std::string textureDirectory;
};

#endif // EMITTER_HPP
