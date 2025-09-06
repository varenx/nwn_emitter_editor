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

#ifndef PARTICLE_SYSTEM_HPP
#define PARTICLE_SYSTEM_HPP

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <random>
#include <unordered_map>
#include <vector>
#include "emitter.hpp"
#include "grab_mode.hpp"

struct Particle
{
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec4 color;
    float size;
    float life;
    float maxLife;
    float rotation;
    float mass;
    bool active;

    float getAge() const { return maxLife - life; }

    Particle() :
        position(0.0f), velocity(0.0f), color(1.0f), size(1.0f), life(0.0f), maxLife(1.0f), rotation(0.0f), mass(1.0f),
        active(false)
    {
    }
};

struct ParticleSystemState
{
    std::vector<Particle> particles;
    float lastSpawnTime;
    size_t maxParticles;
    std::mt19937 rng;
    float animationTime;

    ParticleSystemState() : lastSpawnTime(0.0f), maxParticles(500000), rng(std::random_device{}()), animationTime(0.0f)
    {
    }
};

class ParticleRenderer
{
public:
    ParticleRenderer();
    ~ParticleRenderer();

    void initialize();
    void cleanup();
    void render(const std::vector<EmitterNode>& emitters, float deltaTime, int viewportWidth, int viewportHeight,
                int selectedEmitter = -1);
    void renderToTexture(const std::vector<EmitterNode>& emitters, float deltaTime, int width, int height,
                         int selectedEmitter = -1);

    void renderNodes(const std::vector<EmitterNode>& emitters, int selectedEmitter = -1);
    void renderGrid();
    void renderAxisGizmo(int viewportWidth, int viewportHeight);
    void renderGrabModeIndicator(int viewportWidth, int viewportHeight, GrabMode grabMode,
                                 const glm::vec3& emitterPosition);
    void renderScaleModeIndicator(int viewportWidth, int viewportHeight, ScaleMode scaleMode,
                                  const glm::vec3& emitterPosition, const glm::vec2& currentSize);
    void renderRotationModeIndicator(int viewportWidth, int viewportHeight, RotationMode rotationMode,
                                     const glm::vec3& emitterPosition);
    std::vector<glm::vec2> getAxisGizmoScreenPositions(int viewportWidth, int viewportHeight) const;

    void setCamera(const glm::mat4& view, const glm::mat4& projection);
    void setTextureDirectory(const std::string& directory);

    GLuint getFramebufferTexture() const { return colorTexture; }

    GLuint getFramebuffer() const { return framebuffer; }

    int getActiveParticleCount(int emitterIndex) const;
    int getTotalActiveParticleCount() const;

    int pickEmitter(const std::vector<EmitterNode>& emitters, float mouseX, float mouseY, int viewportWidth,
                    int viewportHeight) const;

    glm::vec3 screenToWorldDelta(float startMouseX, float startMouseY, float currentMouseX, float currentMouseY,
                                 int viewportWidth, int viewportHeight, const glm::vec3& referencePoint) const;

    glm::vec3 screenToWorldPlaneMovement(float startMouseX, float startMouseY, float currentMouseX, float currentMouseY,
                                         int viewportWidth, int viewportHeight, const glm::vec3& referencePoint) const;

    glm::vec3 mouseToProportionalPlaneMovement(float mouseDeltaX, float mouseDeltaY, GrabMode grabMode,
                                               float sensitivity = 0.01f) const;

    glm::vec3 mouseToCameraRelativeMovement(float mouseDeltaX, float mouseDeltaY, GrabMode grabMode,
                                            float sensitivity = 0.01f) const;

    glm::vec2 mouseToScale(float mouseDeltaX, float mouseDeltaY, const glm::vec2& startSize, ScaleMode scaleMode,
                           float sensitivity = 0.01f) const;

    glm::vec3 mouseToRotation(float mouseDeltaX, float mouseDeltaY, RotationMode rotationMode,
                              float sensitivity = 0.01f) const;

private:
    struct Ray
    {
        glm::vec3 origin;
        glm::vec3 direction;
    };

    Ray createRayFromMouse(float mouseX, float mouseY, int viewportWidth, int viewportHeight) const;
    bool rayIntersectsSphere(const Ray& ray, const glm::vec3& center, float radius, float& distance) const;
    bool rayIntersectsCone(const Ray& ray, const glm::vec3& apex, const glm::vec3& direction, float height, float angle,
                           float& distance) const;
    void updateParticles(const EmitterNode& emitter, ParticleSystemState& state, float deltaTime);
    void spawnParticle(const EmitterNode& emitter, ParticleSystemState& state, const glm::vec3& emitterPos);
    void renderParticles(const EmitterNode& emitter, const ParticleSystemState& state);

    GLuint shaderProgram;
    GLuint VAO, VBO;

    // Line rendering for nodes
    GLuint lineShaderProgram;
    GLuint lineVAO, lineVBO;

    // Framebuffer for rendering to texture
    GLuint framebuffer;
    GLuint colorTexture;
    GLuint depthBuffer;
    int fbWidth, fbHeight;

    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
    float globalAnimationTime;

    std::vector<ParticleSystemState> emitterStates;

    std::vector<GLuint> textures;
    std::unordered_map<std::string, GLuint> textureCache;
    std::string textureDirectory;

    void loadTexture(const std::string& textureName);
    GLuint getTexture(const std::string& textureName);

    const char* vertexShaderSource;
    const char* fragmentShaderSource;
    const char* lineVertexShaderSource;
    const char* lineFragmentShaderSource;

    void createShaders();
    void createLineShaders();
    void setupBuffers();
    void setupLineBuffers();
    void setupFramebuffer(int width, int height);
    void cleanupFramebuffer();

    void renderDummyNode(const glm::vec3& position);
    void renderEmitterNode(const EmitterNode& emitter, bool isSelected = false);
};

#endif // PARTICLE_SYSTEM_HPP
