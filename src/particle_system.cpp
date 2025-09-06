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

#include "particle_system.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <unordered_map>
#include "stb_dds.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Vertex attribute layout constants
constexpr int VERTEX_STRIDE = 14; // position(3) + texcoord(2) + color(4) + size(1) + velocity(3) + age(1)

const char* vertexShaderCode = R"(
#version 410 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;
layout(location = 3) in float aSize;
layout(location = 4) in vec3 aVelocity;
layout(location = 5) in float aAge;

uniform mat4 view;
uniform mat4 projection;
uniform int renderMode; // 0=Normal, 1=Linked, 2=Billboard_Local_Z, 3=Billboard_World_Z, 4=Aligned_World_Z, 5=Aligned_Particle_Dir, 6=Motion_Blur
uniform int xGrid;
uniform int yGrid;
uniform float fps;
uniform float frameStart;
uniform float frameEnd;

out vec2 TexCoord;
out vec4 Color;

void main() {
    vec4 worldPos = vec4(aPos, 1.0);

    vec3 right, up;

    if (renderMode == 0) { // Normal - face camera
        // Standard billboarding - extract right and up from inverse view matrix
        right = normalize(vec3(view[0][0], view[1][0], view[2][0]));
        up = normalize(vec3(view[0][1], view[1][1], view[2][1]));

        // Transform to world space first, then apply view
        vec3 billboardPos = aPos + (right * (aTexCoord.x - 0.5) + up * (aTexCoord.y - 0.5)) * aSize;
        gl_Position = projection * view * vec4(billboardPos, 1.0);
    }
    else if (renderMode == 2) { // Billboard to Local Z - face emission direction
        right = vec3(1.0, 0.0, 0.0);
        up = vec3(0.0, 1.0, 0.0);
        vec3 billboardPos = aPos + (right * (aTexCoord.x - 0.5) + up * (aTexCoord.y - 0.5)) * aSize;
        gl_Position = projection * view * vec4(billboardPos, 1.0);
    }
    else if (renderMode == 3) { // Billboard to World Z - face up from ground
        right = vec3(1.0, 0.0, 0.0);
        up = vec3(0.0, 1.0, 0.0);
        vec3 billboardPos = aPos + (right * (aTexCoord.x - 0.5) + up * (aTexCoord.y - 0.5)) * aSize;
        gl_Position = projection * view * vec4(billboardPos, 1.0);
    }
    else if (renderMode == 4) { // Aligned to World Z - perpendicular to ground
        right = vec3(1.0, 0.0, 0.0);
        up = vec3(0.0, 0.0, 1.0);
        vec3 billboardPos = aPos + (right * (aTexCoord.x - 0.5) + up * (aTexCoord.y - 0.5)) * aSize;
        gl_Position = projection * view * vec4(billboardPos, 1.0);
    }
    else if (renderMode == 5) { // Aligned to Particle Direction
        vec3 dir = normalize(aVelocity);
        right = normalize(cross(dir, vec3(0.0, 0.0, 1.0)));
        up = cross(right, dir);
        vec3 billboardPos = aPos + (right * (aTexCoord.x - 0.5) + up * (aTexCoord.y - 0.5)) * aSize;
        gl_Position = projection * view * vec4(billboardPos, 1.0);
    }
    else if (renderMode == 6) { // Motion Blur - stretch along velocity
        float speed = length(aVelocity);
        vec3 dir = speed > 0.01 ? normalize(aVelocity) : vec3(0.0, 0.0, 1.0);
        float stretch = min(speed * 0.1, 2.0); // Limit stretching

        right = normalize(cross(dir, vec3(0.0, 0.0, 1.0)));
        up = dir;

        vec3 billboardPos = aPos + (right * (aTexCoord.x - 0.5) * aSize + up * (aTexCoord.y - 0.5) * aSize * (1.0 + stretch));
        gl_Position = projection * view * vec4(billboardPos, 1.0);
    }
    else if (renderMode == 1) { // Linked - similar to normal but particles will be connected
        vec4 viewPos = view * worldPos;
        right = vec3(view[0][0], view[1][0], view[2][0]);
        up = vec3(view[0][1], view[1][1], view[2][1]);
        vec3 billboardPos = viewPos.xyz + (right * (aTexCoord.x - 0.5) + up * (aTexCoord.y - 0.5)) * aSize;
        gl_Position = projection * vec4(billboardPos, 1.0);
    }
    else { // Default to Normal behavior
        vec4 viewPos = view * worldPos;
        right = vec3(view[0][0], view[1][0], view[2][0]);
        up = vec3(view[0][1], view[1][1], view[2][1]);
        vec3 billboardPos = viewPos.xyz + (right * (aTexCoord.x - 0.5) + up * (aTexCoord.y - 0.5)) * aSize;
        gl_Position = projection * vec4(billboardPos, 1.0);
    }

    // Calculate texture atlas coordinates
    vec2 finalTexCoord = aTexCoord;
    if (xGrid > 1 || yGrid > 1) {
        // Calculate current frame based on particle age and animation settings
        float totalFrames = frameEnd - frameStart + 1.0;
        float animationTime = aAge * fps;
        float currentFrame = frameStart + mod(animationTime, totalFrames);
        int frameIndex = int(currentFrame);

        // Calculate atlas coordinates
        int frameX = frameIndex % xGrid;
        int frameY = frameIndex / xGrid;

        vec2 frameSize = vec2(1.0 / float(xGrid), 1.0 / float(yGrid));
        vec2 frameOffset = vec2(float(frameX), float(frameY)) * frameSize;

        finalTexCoord = frameOffset + aTexCoord * frameSize;
    }

    TexCoord = finalTexCoord;
    Color = aColor;
}
)";

const char* fragmentShaderCode = R"(
#version 410 core

in vec2 TexCoord;
in vec4 Color;

out vec4 FragColor;

uniform sampler2D particleTexture;
uniform bool hasTexture;

void main() {
    vec4 texColor = vec4(1.0);
    if (hasTexture) {
        texColor = texture(particleTexture, TexCoord);
    } else {
        // Create a simple circular gradient for untextured particles
        vec2 center = vec2(0.5, 0.5);
        float dist = distance(TexCoord, center);
        float alpha = 1.0 - smoothstep(0.3, 0.5, dist);
        texColor = vec4(1.0, 1.0, 1.0, alpha);
    }

    FragColor = Color * texColor;

    // Alpha test for punch-through blend
    if (FragColor.a < 0.01) {
        discard;
    }
}
)";

const char* lineVertexShaderCode = R"(
#version 410 core

layout(location = 0) in vec3 aPos;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 model;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

const char* lineFragmentShaderCode = R"(
#version 410 core

out vec4 FragColor;
uniform vec3 lineColor;

void main() {
    FragColor = vec4(lineColor, 1.0);
}
)";

ParticleRenderer::ParticleRenderer() :
    shaderProgram(0), VAO(0), VBO(0), lineShaderProgram(0), lineVAO(0), lineVBO(0), framebuffer(0), colorTexture(0),
    depthBuffer(0), fbWidth(0), fbHeight(0), viewMatrix(1.0f), projectionMatrix(1.0f), globalAnimationTime(0.0f),
    vertexShaderSource(vertexShaderCode), fragmentShaderSource(fragmentShaderCode),
    lineVertexShaderSource(lineVertexShaderCode), lineFragmentShaderSource(lineFragmentShaderCode)
{
}

ParticleRenderer::~ParticleRenderer() { cleanup(); }

void ParticleRenderer::initialize()
{
    createShaders();
    createLineShaders();
    setupBuffers();
    setupLineBuffers();

    // Enable blending for particles
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);

    // Enable line width control
    glLineWidth(1.0f);
}

void ParticleRenderer::cleanup()
{
    cleanupFramebuffer();

    if (shaderProgram)
    {
        glDeleteProgram(shaderProgram);
        shaderProgram = 0;
    }
    if (lineShaderProgram)
    {
        glDeleteProgram(lineShaderProgram);
        lineShaderProgram = 0;
    }
    if (VAO)
    {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
    if (VBO)
    {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
    if (lineVAO)
    {
        glDeleteVertexArrays(1, &lineVAO);
        lineVAO = 0;
    }
    if (lineVBO)
    {
        glDeleteBuffers(1, &lineVBO);
        lineVBO = 0;
    }

    for (GLuint texture : textures)
    {
        glDeleteTextures(1, &texture);
    }
    textures.clear();
    textureCache.clear();
}

void ParticleRenderer::createShaders()
{
    // Compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "Vertex shader compilation failed: " << infoLog << std::endl;
    }

    // Compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Fragment shader compilation failed: " << infoLog << std::endl;
    }

    // Link shaders
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed: " << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void ParticleRenderer::createLineShaders()
{
    // Compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &lineVertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "Line vertex shader compilation failed: " << infoLog << std::endl;
    }

    // Compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &lineFragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Line fragment shader compilation failed: " << infoLog << std::endl;
    }

    // Link shaders
    lineShaderProgram = glCreateProgram();
    glAttachShader(lineShaderProgram, vertexShader);
    glAttachShader(lineShaderProgram, fragmentShader);
    glLinkProgram(lineShaderProgram);

    glGetProgramiv(lineShaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(lineShaderProgram, 512, nullptr, infoLog);
        std::cerr << "Line shader program linking failed: " << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void ParticleRenderer::setupBuffers()
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // Each particle vertex: position(3) + texcoord(2) + color(4) + size(1) + velocity(3) + age(1) = 14 floats
    // 6 vertices per particle (2 triangles), VERTEX_STRIDE floats per vertex
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * VERTEX_STRIDE * 6 * 100000, nullptr, GL_DYNAMIC_DRAW);

    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, VERTEX_STRIDE * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Texture coordinates
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, VERTEX_STRIDE * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Color
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, VERTEX_STRIDE * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // Size
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, VERTEX_STRIDE * sizeof(float), (void*)(9 * sizeof(float)));
    glEnableVertexAttribArray(3);

    // Velocity
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, VERTEX_STRIDE * sizeof(float), (void*)(10 * sizeof(float)));
    glEnableVertexAttribArray(4);

    // Age
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, VERTEX_STRIDE * sizeof(float), (void*)(13 * sizeof(float)));
    glEnableVertexAttribArray(5);

    glBindVertexArray(0);
}

void ParticleRenderer::setupLineBuffers()
{
    glGenVertexArrays(1, &lineVAO);
    glGenBuffers(1, &lineVBO);

    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);

    // Line vertices: position(3) only
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * 100, nullptr, GL_DYNAMIC_DRAW);

    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void ParticleRenderer::setCamera(const glm::mat4& view, const glm::mat4& projection)
{
    viewMatrix = view;
    projectionMatrix = projection;
}

void ParticleRenderer::render(const std::vector<EmitterNode>& emitters, float deltaTime, int viewportWidth,
                              int viewportHeight, int selectedEmitter)
{
    // Resize state vector if needed
    while (emitterStates.size() < emitters.size())
    {
        emitterStates.emplace_back();
    }
    while (emitterStates.size() > emitters.size())
    {
        emitterStates.pop_back();
    }

    // Don't override viewport here - let main control it

    // Render grid first
    renderGrid();

    // Render dummy node (root)
    renderDummyNode(glm::vec3(0.0f));

    // Update and render each emitter
    for (size_t i = 0; i < emitters.size(); ++i)
    {
        updateParticles(emitters[i], emitterStates[i], deltaTime);
        renderParticles(emitters[i], emitterStates[i]);
    }

    // Render emitter nodes
    renderNodes(emitters, selectedEmitter);
}

void ParticleRenderer::updateParticles(const EmitterNode& emitter, ParticleSystemState& state, float deltaTime)
{
    // Update animation time
    state.animationTime += deltaTime;
    // Update existing particles
    for (auto& particle : state.particles)
    {
        if (particle.active)
        {
            particle.life -= deltaTime;
            if (particle.life <= 0.0f)
            {
                particle.active = false;
                continue;
            }

            // Update position
            particle.position += particle.velocity * deltaTime;

            // Apply gravity (in Z-up coordinate system, gravity points down in -Z)
            particle.velocity.z -= emitter.grav * deltaTime;

            // Apply drag
            particle.velocity *= (1.0f - emitter.drag * deltaTime);

            // Update color and size based on life percentage
            float lifePercent = particle.life / particle.maxLife;
            particle.color = glm::vec4(glm::mix(emitter.colorEnd, emitter.colorStart, lifePercent),
                                       glm::mix(emitter.alphaEnd, emitter.alphaStart, lifePercent));
            particle.size = glm::mix(emitter.sizeEnd, emitter.sizeStart, lifePercent);

            // Apply rotation
            particle.rotation += emitter.particleRot * deltaTime;
        }
    }

    // Spawn new particles for fountain emitters
    if (emitter.update == UpdateType::Fountain && emitter.birthrate > 0.0f)
    {
        float spawnInterval = 1.0f / emitter.birthrate;
        state.lastSpawnTime += deltaTime;

        while (state.lastSpawnTime >= spawnInterval && state.particles.size() < state.maxParticles)
        {
            // Use animated position if available
            glm::vec3 emitterPos = emitter.getAnimatedPosition(state.animationTime);
            spawnParticle(emitter, state, emitterPos);
            state.lastSpawnTime -= spawnInterval;
        }
    }
}

void ParticleRenderer::spawnParticle(const EmitterNode& emitter, ParticleSystemState& state,
                                     const glm::vec3& emitterPos)
{
    // Find inactive particle or add new one
    Particle* particle = nullptr;
    for (auto& p : state.particles)
    {
        if (!p.active)
        {
            particle = &p;
            break;
        }
    }

    if (!particle && state.particles.size() < state.maxParticles)
    {
        state.particles.emplace_back();
        particle = &state.particles.back();
    }

    if (!particle)
        return; // No available particle slots

    // Initialize particle
    particle->active = true;
    particle->life = emitter.lifeExp;
    particle->maxLife = emitter.lifeExp;
    particle->mass = emitter.mass;

    // Random position within emitter bounds (in local space)
    std::uniform_real_distribution<float> xDist(-emitter.xsize / 2.0f, emitter.xsize / 2.0f);
    std::uniform_real_distribution<float> yDist(-emitter.ysize / 2.0f, emitter.ysize / 2.0f);

    glm::vec3 localPos = glm::vec3(xDist(state.rng), yDist(state.rng), 0.0f);

    // Transform local position by emitter orientation
    glm::mat3 rotMatrix = glm::mat3_cast(emitter.getOrientation());
    particle->position = emitterPos + rotMatrix * localPos;

    // Random velocity direction within 3D cone spread (in local space)
    std::uniform_real_distribution<float> spreadDist(0.0f, emitter.spread / 2.0f);
    std::uniform_real_distribution<float> azimuthDist(0.0f, 360.0f);
    std::uniform_real_distribution<float> velocityVar(0.8f, 1.2f);

    float spreadAngle = glm::radians(spreadDist(state.rng)); // Cone angle from center
    float azimuth = glm::radians(azimuthDist(state.rng)); // Random rotation around cone axis
    float speed = emitter.velocity * velocityVar(state.rng);

    // Generate velocity in 3D cone around Z-axis
    float x = sin(spreadAngle) * cos(azimuth) * speed;
    float y = sin(spreadAngle) * sin(azimuth) * speed;
    float z = cos(spreadAngle) * speed;

    glm::vec3 localVelocity = glm::vec3(x, y, z);

    // Transform velocity by emitter orientation
    particle->velocity = rotMatrix * localVelocity;

    // Initial color and size
    particle->color = glm::vec4(emitter.colorStart, emitter.alphaStart);
    particle->size = emitter.sizeStart;
    particle->rotation = 0.0f;
}

void ParticleRenderer::renderParticles(const EmitterNode& emitter, const ParticleSystemState& state)
{
    if (state.particles.empty())
        return;

    // Save current blend state
    GLint srcBlend, dstBlend;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcBlend);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &dstBlend);

    glUseProgram(shaderProgram);

    // Set uniforms
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLint projLoc = glGetUniformLocation(shaderProgram, "projection");
    GLint hasTextureLoc = glGetUniformLocation(shaderProgram, "hasTexture");
    GLint renderModeLoc = glGetUniformLocation(shaderProgram, "renderMode");

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &viewMatrix[0][0]);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projectionMatrix[0][0]);
    glUniform1i(renderModeLoc, static_cast<int>(emitter.render));

    // Set texture atlas uniforms
    glUniform1i(glGetUniformLocation(shaderProgram, "xGrid"), emitter.xgrid);
    glUniform1i(glGetUniformLocation(shaderProgram, "yGrid"), emitter.ygrid);
    glUniform1f(glGetUniformLocation(shaderProgram, "fps"), emitter.fps > 0 ? emitter.fps : 1.0f);
    glUniform1f(glGetUniformLocation(shaderProgram, "frameStart"), emitter.frameStart);
    glUniform1f(glGetUniformLocation(shaderProgram, "frameEnd"),
                emitter.frameEnd > 0 ? emitter.frameEnd : emitter.xgrid * emitter.ygrid - 1);

    // Bind texture if available
    GLuint texture = getTexture(emitter.texturePath.empty() ? emitter.texture : emitter.texturePath);
    bool hasTexture = (texture != 0 && (!emitter.texturePath.empty() || !emitter.texture.empty()));
    glUniform1i(hasTextureLoc, hasTexture ? 1 : 0);

    if (hasTexture)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(glGetUniformLocation(shaderProgram, "particleTexture"), 0);
    }

    // Set blend mode specific to particles
    switch (emitter.blend)
    {
    case BlendType::Normal:
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
    case BlendType::Lighten:
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        break;
    case BlendType::Punch_Through:
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
    }

    // Prepare vertex data
    std::vector<float> vertexData;

    for (const auto& particle : state.particles)
    {
        if (!particle.active)
            continue;

        // Create quad vertices for each particle - positions are relative to particle center
        // Vertex layout: position(3) + texcoord(2) + color(4) + size(1) + velocity(3) + age(1)
        float age = particle.getAge();

        // Triangle 1
        vertexData.insert(vertexData.end(),
                          {
                              particle.position.x, particle.position.y, particle.position.z, // center pos
                              0.0f, 0.0f, // texcoord
                              particle.color.r, particle.color.g, particle.color.b, particle.color.a, // color
                              particle.size, // size
                              particle.velocity.x, particle.velocity.y, particle.velocity.z, // velocity
                              age // particle age
                          });
        vertexData.insert(vertexData.end(),
                          {particle.position.x, particle.position.y, particle.position.z, 1.0f, 0.0f, particle.color.r,
                           particle.color.g, particle.color.b, particle.color.a, particle.size, particle.velocity.x,
                           particle.velocity.y, particle.velocity.z, age});
        vertexData.insert(vertexData.end(),
                          {particle.position.x, particle.position.y, particle.position.z, 1.0f, 1.0f, particle.color.r,
                           particle.color.g, particle.color.b, particle.color.a, particle.size, particle.velocity.x,
                           particle.velocity.y, particle.velocity.z, age});

        // Triangle 2
        vertexData.insert(vertexData.end(),
                          {particle.position.x, particle.position.y, particle.position.z, 0.0f, 0.0f, particle.color.r,
                           particle.color.g, particle.color.b, particle.color.a, particle.size, particle.velocity.x,
                           particle.velocity.y, particle.velocity.z, age});
        vertexData.insert(vertexData.end(),
                          {particle.position.x, particle.position.y, particle.position.z, 1.0f, 1.0f, particle.color.r,
                           particle.color.g, particle.color.b, particle.color.a, particle.size, particle.velocity.x,
                           particle.velocity.y, particle.velocity.z, age});
        vertexData.insert(vertexData.end(),
                          {particle.position.x, particle.position.y, particle.position.z, 0.0f, 1.0f, particle.color.r,
                           particle.color.g, particle.color.b, particle.color.a, particle.size, particle.velocity.x,
                           particle.velocity.y, particle.velocity.z, age});
    }

    if (vertexData.empty())
        return;

    // Upload and draw
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertexData.size() * sizeof(float), vertexData.data());

    glDepthMask(GL_FALSE); // Disable depth writing for particles
    glDrawArrays(GL_TRIANGLES, 0, vertexData.size() / VERTEX_STRIDE);
    glDepthMask(GL_TRUE); // Re-enable for other objects

    glBindVertexArray(0);
    glUseProgram(0);

    // Restore original blend state for editor elements
    glBlendFunc(srcBlend, dstBlend);
}

void ParticleRenderer::loadTexture(const std::string& textureNameOrPath)
{
    if (textureNameOrPath.empty())
        return;
    if (textureCache.find(textureNameOrPath) != textureCache.end())
        return;

    unsigned char* data = nullptr;
    int width, height, channels;
    std::string texturePath;

    stbi_set_flip_vertically_on_load(true);

    // Check if it's a full path or just a name
    if (textureNameOrPath.find('/') != std::string::npos || textureNameOrPath.find('\\') != std::string::npos)
    {
        // It's a full path, try loading directly
        texturePath = textureNameOrPath;

        if (texturePath.ends_with(".dds"))
        {
            data = stbi_load_dds(texturePath.c_str(), &width, &height, &channels, 0);
        }
        else
        {
            data = stbi_load(texturePath.c_str(), &width, &height, &channels, 0);
        }
    }
    else
    {
        // It's just a name, try multiple extensions in texture directory
        std::vector<std::string> extensions = {".dds", ".tga", ".png", ".jpg"};

        for (const auto& ext : extensions)
        {
            texturePath = textureDirectory + "/" + textureNameOrPath + ext;

            if (ext == ".dds")
            {
                data = stbi_load_dds(texturePath.c_str(), &width, &height, &channels, 0);
            }
            else
            {
                data = stbi_load(texturePath.c_str(), &width, &height, &channels, 0);
            }

            if (data)
            {
                break; // Successfully loaded
            }
        }
    }

    GLuint texture = 0;
    if (data)
    {
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        GLenum format = GL_RGB;
        if (channels == 4)
            format = GL_RGBA;
        else if (channels == 1)
            format = GL_RED;

        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Free memory - both stb_image and stb_dds use malloc
        stbi_image_free(data);

        textures.push_back(texture);
        textureCache[textureNameOrPath] = texture;

        std::cout << "Loaded texture: " << texturePath << " (" << width << "x" << height << ")" << std::endl;
    }
    else
    {
        std::cerr << "Failed to load texture: " << textureNameOrPath << " (tried .dds, .tga, .png, .jpg extensions)"
                  << std::endl;
        textureCache[textureNameOrPath] = 0;
    }
}

void ParticleRenderer::setTextureDirectory(const std::string& directory) { textureDirectory = directory; }

GLuint ParticleRenderer::getTexture(const std::string& textureNameOrPath)
{
    if (textureNameOrPath.empty())
        return 0;

    auto it = textureCache.find(textureNameOrPath);
    if (it == textureCache.end())
    {
        loadTexture(textureNameOrPath);
        it = textureCache.find(textureNameOrPath);
    }

    return (it != textureCache.end()) ? it->second : 0;
}

void ParticleRenderer::renderNodes(const std::vector<EmitterNode>& emitters, int selectedEmitter)
{
    // Enable depth writing for wireframes
    glDepthMask(GL_TRUE);

    for (int i = 0; i < static_cast<int>(emitters.size()); ++i)
    {
        bool isSelected = (i == selectedEmitter);
        renderEmitterNode(emitters[i], isSelected);
    }
}

void ParticleRenderer::renderDummyNode(const glm::vec3& position)
{
    // Ensure standard blend state for editor elements
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(lineShaderProgram);

    // Set uniforms
    GLint viewLoc = glGetUniformLocation(lineShaderProgram, "view");
    GLint projLoc = glGetUniformLocation(lineShaderProgram, "projection");
    GLint modelLoc = glGetUniformLocation(lineShaderProgram, "model");
    GLint colorLoc = glGetUniformLocation(lineShaderProgram, "lineColor");

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &viewMatrix[0][0]);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projectionMatrix[0][0]);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);

    // Yellow color for dummy node
    glUniform3f(colorLoc, 1.0f, 1.0f, 0.0f);

    // Cross shape vertices (3D cross)
    float crossSize = 0.5f;
    std::vector<float> crossVertices = {// X axis
                                        -crossSize, 0.0f, 0.0f, crossSize, 0.0f, 0.0f,
                                        // Y axis
                                        0.0f, -crossSize, 0.0f, 0.0f, crossSize, 0.0f,
                                        // Z axis
                                        0.0f, 0.0f, -crossSize, 0.0f, 0.0f, crossSize};

    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, crossVertices.size() * sizeof(float), crossVertices.data());

    glDrawArrays(GL_LINES, 0, 6);

    glBindVertexArray(0);
    glUseProgram(0);
}

void ParticleRenderer::renderEmitterNode(const EmitterNode& emitter, bool isSelected)
{
    // Ensure standard blend state for editor elements
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(lineShaderProgram);

    // Set uniforms
    GLint viewLoc = glGetUniformLocation(lineShaderProgram, "view");
    GLint projLoc = glGetUniformLocation(lineShaderProgram, "projection");
    GLint modelLoc = glGetUniformLocation(lineShaderProgram, "model");
    GLint colorLoc = glGetUniformLocation(lineShaderProgram, "lineColor");

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &viewMatrix[0][0]);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projectionMatrix[0][0]);

    // Apply both animated translation and rotation
    glm::vec3 animatedPos = emitter.getAnimatedPosition(globalAnimationTime);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), animatedPos);
    model = model * glm::mat4_cast(emitter.getOrientation());
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);

    // Color based on selection state
    if (isSelected)
    {
        // Bright cyan for selected emitter
        glUniform3f(colorLoc, 0.0f, 1.0f, 1.0f);
    }
    else
    {
        // Dimmed cyan for non-selected emitters
        glUniform3f(colorLoc, 0.0f, 0.4f, 0.4f);
    }

    std::vector<float> emitterVertices;

    // Draw emitter bounds as a rectangle if xsize/ysize are set
    if (emitter.xsize > 0.0f || emitter.ysize > 0.0f)
    {
        float halfX = emitter.xsize * 0.5f;
        float halfY = emitter.ysize * 0.5f;

        // Rectangle outline
        emitterVertices = {
            -halfX, -halfY, 0.0f, halfX,  -halfY, 0.0f, // bottom
            halfX,  -halfY, 0.0f, halfX,  halfY,  0.0f, // right
            halfX,  halfY,  0.0f, -halfX, halfY,  0.0f, // top
            -halfX, halfY,  0.0f, -halfX, -halfY, 0.0f // left
        };
    }
    else
    {
        // Default small cross for point emitters
        float size = 0.3f;
        emitterVertices = {-size, 0.0f, 0.0f, size, 0.0f, 0.0f, 0.0f, -size, 0.0f, 0.0f, size, 0.0f};
    }

    // Add emission direction indicator (based on spread) - perpendicular to surface
    if (emitter.velocity > 0.0f)
    {
        float arrowLength = 1.0f;
        float spreadRad = glm::radians(emitter.spread * 0.5f);

        // Center line showing main direction (along local Z-axis)
        emitterVertices.insert(emitterVertices.end(), {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, arrowLength});

        // Spread indicators (cone around Z-axis)
        if (emitter.spread > 0.0f)
        {
            float spreadX = sin(spreadRad) * arrowLength;
            float spreadZ = cos(spreadRad) * arrowLength;

            // Four spread lines forming a cone
            emitterVertices.insert(emitterVertices.end(),
                                   {0.0f,     0.0f,    0.0f, -spreadX, 0.0f, spreadZ, 0.0f,    0.0f,
                                    0.0f,     spreadX, 0.0f, spreadZ,  0.0f, 0.0f,    0.0f,    0.0f,
                                    -spreadX, spreadZ, 0.0f, 0.0f,     0.0f, 0.0f,    spreadX, spreadZ});
        }
    }

    if (!emitterVertices.empty())
    {
        glBindVertexArray(lineVAO);
        glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, emitterVertices.size() * sizeof(float), emitterVertices.data());

        glDrawArrays(GL_LINES, 0, emitterVertices.size() / 3);

        glBindVertexArray(0);
    }

    glUseProgram(0);
}

void ParticleRenderer::renderGrid()
{
    // Ensure standard blend state for editor elements
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(lineShaderProgram);

    // Set uniforms
    GLint viewLoc = glGetUniformLocation(lineShaderProgram, "view");
    GLint projLoc = glGetUniformLocation(lineShaderProgram, "projection");
    GLint modelLoc = glGetUniformLocation(lineShaderProgram, "model");
    GLint colorLoc = glGetUniformLocation(lineShaderProgram, "lineColor");

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &viewMatrix[0][0]);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projectionMatrix[0][0]);

    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);

    // Light gray color for grid (visible against dark background)
    glUniform3f(colorLoc, 0.5f, 0.5f, 0.5f);

    std::vector<float> gridVertices;

    // Create grid lines
    float gridSize = 10.0f;
    int gridLines = 21; // -10 to +10
    float step = gridSize / (gridLines - 1) * 2.0f;

    for (int i = 0; i < gridLines; ++i)
    {
        float pos = -gridSize + i * step;

        // Lines parallel to X axis (in XY plane, Z=0)
        gridVertices.insert(gridVertices.end(), {-gridSize, pos, 0.0f, gridSize, pos, 0.0f});

        // Lines parallel to Y axis (in XY plane, Z=0)
        gridVertices.insert(gridVertices.end(), {pos, -gridSize, 0.0f, pos, gridSize, 0.0f});
    }

    // Highlight main axes (in XY plane)
    std::vector<float> axisVertices = {// X axis (red line would be nice but we'll use brighter gray)
                                       -gridSize, 0.0f, 0.0f, gridSize, 0.0f, 0.0f,
                                       // Y axis
                                       0.0f, -gridSize, 0.0f, 0.0f, gridSize, 0.0f};

    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);

    // Render grid
    glUniform3f(colorLoc, 0.4f, 0.4f, 0.4f);
    glBufferSubData(GL_ARRAY_BUFFER, 0, gridVertices.size() * sizeof(float), gridVertices.data());
    glDrawArrays(GL_LINES, 0, gridVertices.size() / 3);

    // Render main axes
    glUniform3f(colorLoc, 0.7f, 0.7f, 0.7f);
    glBufferSubData(GL_ARRAY_BUFFER, 0, axisVertices.size() * sizeof(float), axisVertices.data());
    glDrawArrays(GL_LINES, 0, axisVertices.size() / 3);

    glBindVertexArray(0);
    glUseProgram(0);
}

void ParticleRenderer::renderAxisGizmo(int viewportWidth, int viewportHeight)
{
    // Position gizmo in top-right corner using screen coordinates
    glm::vec2 screenGizmoCenter(viewportWidth - 60.0f, 60.0f);
    float gizmoSize = 40.0f;

    // Ensure standard blend state for editor elements
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(lineShaderProgram);

    GLint viewLoc = glGetUniformLocation(lineShaderProgram, "view");
    GLint projLoc = glGetUniformLocation(lineShaderProgram, "projection");
    GLint modelLoc = glGetUniformLocation(lineShaderProgram, "model");
    GLint colorLoc = glGetUniformLocation(lineShaderProgram, "lineColor");

    // Create orthographic projection for screen-space gizmo
    glm::mat4 gizmoProjection = glm::ortho(0.0f, (float)viewportWidth, 0.0f, (float)viewportHeight, -1.0f, 1.0f);

    // Use identity matrices - we'll do the transformation manually
    glm::mat4 identity = glm::mat4(1.0f);
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &identity[0][0]);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &gizmoProjection[0][0]);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &identity[0][0]);

    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);

    // Disable depth test for gizmo so it's always on top
    glDisable(GL_DEPTH_TEST);

    // Define world coordinate axis directions (Z-up system)
    std::vector<glm::vec3> worldAxisDirections = {
        glm::vec3(1.0f, 0.0f, 0.0f), // +X (right)
        glm::vec3(-1.0f, 0.0f, 0.0f), // -X (left)
        glm::vec3(0.0f, 1.0f, 0.0f), // +Y (away from viewer)
        glm::vec3(0.0f, -1.0f, 0.0f), // -Y (toward viewer)
        glm::vec3(0.0f, 0.0f, 1.0f), // +Z (up)
        glm::vec3(0.0f, 0.0f, -1.0f) // -Z (down)
    };

    std::vector<glm::vec3> axisColors = {
        glm::vec3(1.0f, 0.4f, 0.4f), // +X bright red
        glm::vec3(0.7f, 0.3f, 0.3f), // -X dark red
        glm::vec3(0.4f, 1.0f, 0.4f), // +Y bright green
        glm::vec3(0.3f, 0.7f, 0.3f), // -Y dark green
        glm::vec3(0.4f, 0.4f, 1.0f), // +Z bright blue
        glm::vec3(0.3f, 0.3f, 0.7f) // -Z dark blue
    };

    for (size_t i = 0; i < worldAxisDirections.size(); ++i)
    {
        // Transform world direction to camera space
        glm::vec4 cameraSpaceDir = viewMatrix * glm::vec4(worldAxisDirections[i], 0.0f);

        // Convert to screen coordinates
        glm::vec2 screenEnd = screenGizmoCenter + glm::vec2(cameraSpaceDir.x, cameraSpaceDir.y) * gizmoSize;

        // Create line from center to endpoint
        std::vector<float> axisVertices = {screenGizmoCenter.x, screenGizmoCenter.y, 0.0f,
                                           screenEnd.x,         screenEnd.y,         0.0f};

        glUniform3f(colorLoc, axisColors[i].r, axisColors[i].g, axisColors[i].b);
        glBufferSubData(GL_ARRAY_BUFFER, 0, axisVertices.size() * sizeof(float), axisVertices.data());
        glDrawArrays(GL_LINES, 0, 2);
    }

    // Re-enable depth test
    glEnable(GL_DEPTH_TEST);

    glBindVertexArray(0);
    glUseProgram(0);
}

std::vector<glm::vec2> ParticleRenderer::getAxisGizmoScreenPositions(int viewportWidth, int viewportHeight) const
{
    // Gizmo screen position in top-right corner
    glm::vec2 screenGizmoCenter(viewportWidth - 60.0f, 60.0f);
    float gizmoSize = 40.0f;

    // Define world coordinate axis directions (Z-up system)
    std::vector<glm::vec3> worldAxisDirections = {
        glm::vec3(1.0f, 0.0f, 0.0f), // +X (right)
        glm::vec3(-1.0f, 0.0f, 0.0f), // -X (left)
        glm::vec3(0.0f, 1.0f, 0.0f), // +Y (away from viewer)
        glm::vec3(0.0f, -1.0f, 0.0f), // -Y (toward viewer)
        glm::vec3(0.0f, 0.0f, 1.0f), // +Z (up)
        glm::vec3(0.0f, 0.0f, -1.0f) // -Z (down)
    };

    std::vector<glm::vec2> screenPositions;

    for (const auto& worldDir : worldAxisDirections)
    {
        // Transform world direction to camera space (this shows how the world axes appear to camera)
        glm::vec4 cameraSpaceDir = viewMatrix * glm::vec4(worldDir, 0.0f);

        // Project to screen space around gizmo center
        // Use X and Y components for screen positioning, Z determines depth (for future occlusion)
        glm::vec2 screenPos = screenGizmoCenter + glm::vec2(cameraSpaceDir.x, cameraSpaceDir.y) * gizmoSize;
        screenPositions.push_back(screenPos);
    }

    return screenPositions;
}

void ParticleRenderer::renderGrabModeIndicator(int viewportWidth, int viewportHeight, GrabMode grabMode,
                                               const glm::vec3& emitterPosition)
{
    if (grabMode == GrabMode::None)
    {
        return;
    }

    // Ensure standard blend state for editor elements
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(lineShaderProgram);

    GLint viewLoc = glGetUniformLocation(lineShaderProgram, "view");
    GLint projLoc = glGetUniformLocation(lineShaderProgram, "projection");
    GLint modelLoc = glGetUniformLocation(lineShaderProgram, "model");
    GLint colorLoc = glGetUniformLocation(lineShaderProgram, "lineColor");

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &viewMatrix[0][0]);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projectionMatrix[0][0]);

    // Position indicator at emitter location
    glm::mat4 model = glm::translate(glm::mat4(1.0f), emitterPosition);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);

    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);

    // Disable depth test so indicator is always visible
    glDisable(GL_DEPTH_TEST);

    std::vector<float> indicatorVertices;
    float axisLength = 2.0f;

    switch (grabMode)
    {
    case GrabMode::Free:
        // Show all three axes in bright colors
        glUniform3f(colorLoc, 1.0f, 0.2f, 0.2f); // Red X
        indicatorVertices = {0.0f, 0.0f, 0.0f, axisLength, 0.0f, 0.0f};
        glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
        glDrawArrays(GL_LINES, 0, 2);

        glUniform3f(colorLoc, 0.2f, 1.0f, 0.2f); // Green Y
        indicatorVertices = {0.0f, 0.0f, 0.0f, 0.0f, axisLength, 0.0f};
        glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
        glDrawArrays(GL_LINES, 0, 2);

        glUniform3f(colorLoc, 0.2f, 0.2f, 1.0f); // Blue Z
        indicatorVertices = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, axisLength};
        glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
        glDrawArrays(GL_LINES, 0, 2);
        break;

    case GrabMode::X_Axis:
        // Bright red X axis
        glUniform3f(colorLoc, 1.0f, 0.0f, 0.0f); // Red for X axis
        indicatorVertices = {-axisLength, 0.0f, 0.0f, axisLength, 0.0f, 0.0f};
        glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
        glDrawArrays(GL_LINES, 0, 2);
        break;

    case GrabMode::Y_Axis:
        // Bright green Y axis
        glUniform3f(colorLoc, 0.0f, 1.0f, 0.0f); // Green for Y axis
        indicatorVertices = {0.0f, -axisLength, 0.0f, 0.0f, axisLength, 0.0f};
        glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
        glDrawArrays(GL_LINES, 0, 2);
        break;

    case GrabMode::Z_Axis:
        // Bright blue Z axis
        glUniform3f(colorLoc, 0.0f, 0.0f, 1.0f); // Blue for Z axis
        indicatorVertices = {0.0f, 0.0f, -axisLength, 0.0f, 0.0f, axisLength};
        glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
        glDrawArrays(GL_LINES, 0, 2);
        break;

    case GrabMode::YZ_Plane:
        { // Shift+X: Y-Z plane
            // Show Y and Z axes in yellow, dim X
            glUniform3f(colorLoc, 1.0f, 1.0f, 0.0f); // Yellow for active plane
            indicatorVertices = {0.0f, -axisLength, 0.0f, 0.0f, axisLength, 0.0f}; // Y axis
            glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
            glDrawArrays(GL_LINES, 0, 2);

            indicatorVertices = {0.0f, 0.0f, -axisLength, 0.0f, 0.0f, axisLength}; // Z axis
            glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
            glDrawArrays(GL_LINES, 0, 2);

            // Draw plane outline
            glUniform3f(colorLoc, 1.0f, 1.0f, 0.0f); // Yellow plane
            float planeSize = axisLength * 0.7f;
            indicatorVertices = {
                0.0f, -planeSize, -planeSize, 0.0f, planeSize,  -planeSize, // Bottom edge
                0.0f, planeSize,  -planeSize, 0.0f, planeSize,  planeSize, // Right edge
                0.0f, planeSize,  planeSize,  0.0f, -planeSize, planeSize, // Top edge
                0.0f, -planeSize, planeSize,  0.0f, -planeSize, -planeSize // Left edge
            };
            glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
            glDrawArrays(GL_LINES, 0, 8);
            break;
        }

    case GrabMode::XZ_Plane:
        { // Shift+Y: X-Z plane
            // Show X and Z axes in yellow
            glUniform3f(colorLoc, 1.0f, 1.0f, 0.0f); // Yellow for active plane
            indicatorVertices = {-axisLength, 0.0f, 0.0f, axisLength, 0.0f, 0.0f}; // X axis
            glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
            glDrawArrays(GL_LINES, 0, 2);

            indicatorVertices = {0.0f, 0.0f, -axisLength, 0.0f, 0.0f, axisLength}; // Z axis
            glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
            glDrawArrays(GL_LINES, 0, 2);

            // Draw plane outline
            float planeSize2 = axisLength * 0.7f;
            indicatorVertices = {
                -planeSize2, 0.0f, -planeSize2, planeSize2,  0.0f, -planeSize2, // Front edge
                planeSize2,  0.0f, -planeSize2, planeSize2,  0.0f, planeSize2, // Right edge
                planeSize2,  0.0f, planeSize2,  -planeSize2, 0.0f, planeSize2, // Back edge
                -planeSize2, 0.0f, planeSize2,  -planeSize2, 0.0f, -planeSize2 // Left edge
            };
            glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
            glDrawArrays(GL_LINES, 0, 8);
            break;
        }

    case GrabMode::XY_Plane:
        { // Shift+Z: X-Y plane
            // Show X and Y axes in yellow
            glUniform3f(colorLoc, 1.0f, 1.0f, 0.0f); // Yellow for active plane
            indicatorVertices = {-axisLength, 0.0f, 0.0f, axisLength, 0.0f, 0.0f}; // X axis
            glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
            glDrawArrays(GL_LINES, 0, 2);

            indicatorVertices = {0.0f, -axisLength, 0.0f, 0.0f, axisLength, 0.0f}; // Y axis
            glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
            glDrawArrays(GL_LINES, 0, 2);

            // Draw plane outline
            float planeSize3 = axisLength * 0.7f;
            indicatorVertices = {
                -planeSize3, -planeSize3, 0.0f, planeSize3,  -planeSize3, 0.0f, // Bottom edge
                planeSize3,  -planeSize3, 0.0f, planeSize3,  planeSize3,  0.0f, // Right edge
                planeSize3,  planeSize3,  0.0f, -planeSize3, planeSize3,  0.0f, // Top edge
                -planeSize3, planeSize3,  0.0f, -planeSize3, -planeSize3, 0.0f // Left edge
            };
            glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
            glDrawArrays(GL_LINES, 0, 8);
            break;
        }

    default:
        break;
    }

    // Re-enable depth test
    glEnable(GL_DEPTH_TEST);

    glBindVertexArray(0);
    glUseProgram(0);
}

void ParticleRenderer::setupFramebuffer(int width, int height)
{
    if (framebuffer != 0 && fbWidth == width && fbHeight == height)
    {
        return; // Already setup with correct size
    }

    cleanupFramebuffer();

    fbWidth = width;
    fbHeight = height;

    // Generate framebuffer
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    // Create color texture
    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);

    // Create depth buffer
    glGenRenderbuffers(1, &depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);

    // Check framebuffer completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "Framebuffer not complete!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ParticleRenderer::cleanupFramebuffer()
{
    if (framebuffer)
    {
        glDeleteFramebuffers(1, &framebuffer);
        framebuffer = 0;
    }
    if (colorTexture)
    {
        glDeleteTextures(1, &colorTexture);
        colorTexture = 0;
    }
    if (depthBuffer)
    {
        glDeleteRenderbuffers(1, &depthBuffer);
        depthBuffer = 0;
    }
    fbWidth = fbHeight = 0;
}

void ParticleRenderer::renderToTexture(const std::vector<EmitterNode>& emitters, float deltaTime, int width, int height,
                                       int selectedEmitter)
{
    // Update global animation time
    globalAnimationTime += deltaTime;
    setupFramebuffer(width, height);

    // Bind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, width, height);

    // Clear with dark grey background
    glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render scene
    render(emitters, deltaTime, width, height, selectedEmitter);

    // Render axis gizmo in top-right corner
    renderAxisGizmo(width, height);

    // Unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

int ParticleRenderer::getActiveParticleCount(int emitterIndex) const
{
    if (emitterIndex < 0 || emitterIndex >= static_cast<int>(emitterStates.size()))
        return 0;

    int activeCount = 0;
    for (const auto& particle : emitterStates[emitterIndex].particles)
    {
        if (particle.active)
            activeCount++;
    }
    return activeCount;
}

int ParticleRenderer::getTotalActiveParticleCount() const
{
    int totalCount = 0;
    for (const auto& state : emitterStates)
    {
        for (const auto& particle : state.particles)
        {
            if (particle.active)
                totalCount++;
        }
    }
    return totalCount;
}

ParticleRenderer::Ray ParticleRenderer::createRayFromMouse(float mouseX, float mouseY, int viewportWidth,
                                                           int viewportHeight) const
{
    // Convert mouse coordinates to normalized device coordinates
    float x = (2.0f * mouseX) / viewportWidth - 1.0f;
    float y = 1.0f - (2.0f * mouseY) / viewportHeight;

    // Create points in clip space
    glm::vec4 rayClip = glm::vec4(x, y, -1.0f, 1.0f);

    // Transform to eye space
    glm::mat4 invProjection = glm::inverse(projectionMatrix);
    glm::vec4 rayEye = invProjection * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

    // Transform to world space
    glm::mat4 invView = glm::inverse(viewMatrix);
    glm::vec4 rayWorld = invView * rayEye;
    glm::vec3 rayDir = glm::normalize(glm::vec3(rayWorld));

    // Get camera position (origin of ray)
    glm::vec3 rayOrigin = glm::vec3(invView[3]);

    Ray ray;
    ray.origin = rayOrigin;
    ray.direction = rayDir;
    return ray;
}

bool ParticleRenderer::rayIntersectsSphere(const Ray& ray, const glm::vec3& center, float radius, float& distance) const
{
    glm::vec3 oc = ray.origin - center;
    float a = glm::dot(ray.direction, ray.direction);
    float b = 2.0f * glm::dot(oc, ray.direction);
    float c = glm::dot(oc, oc) - radius * radius;

    float discriminant = b * b - 4 * a * c;
    if (discriminant < 0)
    {
        return false;
    }

    float sqrtDiscriminant = sqrt(discriminant);
    float t1 = (-b - sqrtDiscriminant) / (2.0f * a);
    float t2 = (-b + sqrtDiscriminant) / (2.0f * a);

    // We want the closest positive intersection
    if (t1 > 0)
    {
        distance = t1;
        return true;
    }
    else if (t2 > 0)
    {
        distance = t2;
        return true;
    }

    return false;
}

bool ParticleRenderer::rayIntersectsCone(const Ray& ray, const glm::vec3& apex, const glm::vec3& direction,
                                         float height, float angle, float& distance) const
{
    // Simplified cone intersection - treat as expanding sphere along direction
    // This is a reasonable approximation for picking purposes

    float cosAngle = cos(angle);
    float sinAngle = sin(angle);

    // Check multiple points along the cone axis for intersection
    const int samples = 10;
    for (int i = 1; i <= samples; ++i)
    {
        float t = (height * i) / samples;
        glm::vec3 point = apex + direction * t;
        float radius = t * sinAngle;

        if (radius > 0.1f)
        { // Minimum radius for picking
            float dist;
            if (rayIntersectsSphere(ray, point, radius, dist))
            {
                distance = dist;
                return true;
            }
        }
    }

    return false;
}

int ParticleRenderer::pickEmitter(const std::vector<EmitterNode>& emitters, float mouseX, float mouseY,
                                  int viewportWidth, int viewportHeight) const
{
    Ray ray = createRayFromMouse(mouseX, mouseY, viewportWidth, viewportHeight);

    int closestEmitter = -1;
    float closestDistance = std::numeric_limits<float>::max();

    for (int i = 0; i < static_cast<int>(emitters.size()); ++i)
    {
        const EmitterNode& emitter = emitters[i];
        glm::vec3 emitterPos = emitter.getAnimatedPosition(globalAnimationTime);
        float distance;

        // Check intersection with emitter node (as a sphere)
        float nodeRadius = std::max(0.5f, std::max(emitter.xsize, emitter.ysize) * 0.5f + 0.2f);
        if (rayIntersectsSphere(ray, emitterPos, nodeRadius, distance))
        {
            if (distance < closestDistance)
            {
                closestDistance = distance;
                closestEmitter = i;
            }
        }

        // Check intersection with spread cone (if velocity > 0 and spread > 0)
        if (emitter.velocity > 0.0f && emitter.spread > 0.0f)
        {
            // Transform cone direction by emitter orientation
            glm::mat3 rotMatrix = glm::mat3_cast(emitter.getOrientation());
            glm::vec3 coneDirection = rotMatrix * glm::vec3(0.0f, 0.0f, 1.0f); // Local Z-axis

            float coneHeight = 2.0f; // Visible length of cone
            float coneAngle = glm::radians(emitter.spread * 0.5f);

            if (rayIntersectsCone(ray, emitterPos, coneDirection, coneHeight, coneAngle, distance))
            {
                if (distance < closestDistance)
                {
                    closestDistance = distance;
                    closestEmitter = i;
                }
            }
        }
    }

    return closestEmitter;
}

glm::vec3 ParticleRenderer::screenToWorldDelta(float startMouseX, float startMouseY, float currentMouseX,
                                               float currentMouseY, int viewportWidth, int viewportHeight,
                                               const glm::vec3& referencePoint) const
{
    // Create rays from start and current mouse positions
    Ray startRay = createRayFromMouse(startMouseX, startMouseY, viewportWidth, viewportHeight);
    Ray currentRay = createRayFromMouse(currentMouseX, currentMouseY, viewportWidth, viewportHeight);

    // Project reference point to view space to get depth
    glm::vec4 refViewSpace = viewMatrix * glm::vec4(referencePoint, 1.0f);
    float depth = -refViewSpace.z; // Distance from camera

    // Calculate world positions at the reference depth
    glm::vec3 startWorldPos = startRay.origin + startRay.direction * depth;
    glm::vec3 currentWorldPos = currentRay.origin + currentRay.direction * depth;

    return currentWorldPos - startWorldPos;
}

glm::vec3 ParticleRenderer::screenToWorldPlaneMovement(float startMouseX, float startMouseY, float currentMouseX,
                                                       float currentMouseY, int viewportWidth, int viewportHeight,
                                                       const glm::vec3& referencePoint) const
{
    // Calculate mouse delta in screen space (pixels)
    float mouseDeltaX = currentMouseX - startMouseX;
    float mouseDeltaY = currentMouseY - startMouseY;

    // Convert to normalized device coordinates (-1 to 1)
    float ndcDeltaX = (2.0f * mouseDeltaX) / viewportWidth;
    float ndcDeltaY = -(2.0f * mouseDeltaY) / viewportHeight; // Flip Y for OpenGL

    // Project reference point to screen space to get its depth and screen position
    glm::vec4 refClipSpace = projectionMatrix * viewMatrix * glm::vec4(referencePoint, 1.0f);
    glm::vec3 refNDC = glm::vec3(refClipSpace) / refClipSpace.w;

    // Create two points in NDC space: current position and position + mouse delta
    glm::vec3 startNDC = refNDC;
    glm::vec3 endNDC = refNDC + glm::vec3(ndcDeltaX, ndcDeltaY, 0.0f); // Keep same depth

    // Convert back to world space
    glm::mat4 invVP = glm::inverse(projectionMatrix * viewMatrix);

    glm::vec4 startWorld = invVP * glm::vec4(startNDC, 1.0f);
    startWorld /= startWorld.w;

    glm::vec4 endWorld = invVP * glm::vec4(endNDC, 1.0f);
    endWorld /= endWorld.w;

    return glm::vec3(endWorld - startWorld);
}

glm::vec3 ParticleRenderer::mouseToProportionalPlaneMovement(float mouseDeltaX, float mouseDeltaY, GrabMode grabMode,
                                                             float sensitivity) const
{
    glm::vec3 movement(0.0f);

    switch (grabMode)
    {
    case GrabMode::YZ_Plane: // Shift+X: Move on Y-Z plane
        movement.y = mouseDeltaX * sensitivity; // Mouse X → Y axis
        movement.z = -mouseDeltaY * sensitivity; // Mouse Y → Z axis (inverted for intuitive up/down)
        break;

    case GrabMode::XZ_Plane: // Shift+Y: Move on X-Z plane
        movement.x = mouseDeltaX * sensitivity; // Mouse X → X axis
        movement.z = -mouseDeltaY * sensitivity; // Mouse Y → Z axis (inverted for intuitive up/down)
        break;

    case GrabMode::XY_Plane: // Shift+Z: Move on X-Y plane
        movement.x = mouseDeltaX * sensitivity; // Mouse X → X axis
        movement.y = mouseDeltaY * sensitivity; // Mouse Y → Y axis
        break;

    case GrabMode::X_Axis: // X only
        movement.x = mouseDeltaX * sensitivity; // Mouse X → X axis
        break;

    case GrabMode::Y_Axis: // Y only
        movement.y = mouseDeltaX * sensitivity; // Mouse X → Y axis
        break;

    case GrabMode::Z_Axis: // Z only
        movement.z = -mouseDeltaY * sensitivity; // Mouse Y → Z axis (inverted for intuitive up/down)
        break;

    case GrabMode::Free:
    case GrabMode::None:
    default:
        // For free movement, use the screen-space projection method
        return glm::vec3(0.0f); // Will be handled by the calling code
    }

    return movement;
}

glm::vec3 ParticleRenderer::mouseToCameraRelativeMovement(float mouseDeltaX, float mouseDeltaY, GrabMode grabMode,
                                                          float sensitivity) const
{
    // Extract camera orientation vectors from view matrix
    glm::mat4 invView = glm::inverse(viewMatrix);
    glm::vec3 cameraRight = glm::normalize(glm::vec3(invView[0])); // Camera's right vector
    glm::vec3 cameraUp = glm::normalize(glm::vec3(invView[1])); // Camera's up vector
    glm::vec3 cameraForward = -glm::normalize(glm::vec3(invView[2])); // Camera's forward vector (negative Z)

    glm::vec3 movement(0.0f);

    switch (grabMode)
    {
    case GrabMode::Free:
        // Free movement: Mouse X = camera right/left, Mouse Y = camera up/down
        movement = cameraRight * mouseDeltaX * sensitivity + cameraUp * (-mouseDeltaY * sensitivity);
        break;

    case GrabMode::X_Axis:
        {
            // X axis constraint: project camera right onto world X-axis
            glm::vec3 worldX(1.0f, 0.0f, 0.0f);
            float rightDotX = glm::dot(cameraRight, worldX);
            float upDotX = glm::dot(cameraUp, worldX);
            // Use mouse movement that best aligns with world X
            float xMovement =
                (std::abs(rightDotX) > std::abs(upDotX)) ? mouseDeltaX * rightDotX : -mouseDeltaY * upDotX;
            movement = worldX * xMovement * sensitivity;
            break;
        }

    case GrabMode::Y_Axis:
        {
            // Y axis constraint: project camera vectors onto world Y-axis
            glm::vec3 worldY(0.0f, 1.0f, 0.0f);
            float rightDotY = glm::dot(cameraRight, worldY);
            float upDotY = glm::dot(cameraUp, worldY);
            float yMovement =
                (std::abs(rightDotY) > std::abs(upDotY)) ? mouseDeltaX * rightDotY : -mouseDeltaY * upDotY;
            movement = worldY * yMovement * sensitivity;
            break;
        }

    case GrabMode::Z_Axis:
        {
            // Z axis constraint: project camera vectors onto world Z-axis
            glm::vec3 worldZ(0.0f, 0.0f, 1.0f);
            float rightDotZ = glm::dot(cameraRight, worldZ);
            float upDotZ = glm::dot(cameraUp, worldZ);
            float zMovement =
                (std::abs(rightDotZ) > std::abs(upDotZ)) ? mouseDeltaX * rightDotZ : -mouseDeltaY * upDotZ;
            movement = worldZ * zMovement * sensitivity;
            break;
        }

    case GrabMode::YZ_Plane:
        {
            // Y-Z Plane (Shift+X): Movement in camera space, projected onto Y-Z plane
            glm::vec3 cameraMovement =
                cameraRight * mouseDeltaX * sensitivity + cameraUp * (-mouseDeltaY * sensitivity);
            // Remove X component to constrain to Y-Z plane
            movement = glm::vec3(0.0f, cameraMovement.y, cameraMovement.z);
            break;
        }

    case GrabMode::XZ_Plane:
        {
            // X-Z Plane (Shift+Y): Movement in camera space, projected onto X-Z plane
            glm::vec3 cameraMovement =
                cameraRight * mouseDeltaX * sensitivity + cameraUp * (-mouseDeltaY * sensitivity);
            // Remove Y component to constrain to X-Z plane
            movement = glm::vec3(cameraMovement.x, 0.0f, cameraMovement.z);
            break;
        }

    case GrabMode::XY_Plane:
        {
            // X-Y Plane (Shift+Z): Movement in camera space, projected onto X-Y plane
            glm::vec3 cameraMovement =
                cameraRight * mouseDeltaX * sensitivity + cameraUp * (-mouseDeltaY * sensitivity);
            // Remove Z component to constrain to X-Y plane
            movement = glm::vec3(cameraMovement.x, cameraMovement.y, 0.0f);
            break;
        }

    case GrabMode::None:
    default:
        break;
    }

    return movement;
}

glm::vec2 ParticleRenderer::mouseToScale(float mouseDeltaX, float mouseDeltaY, const glm::vec2& startSize,
                                         ScaleMode scaleMode, float sensitivity) const
{
    glm::vec2 scaledSize = startSize;

    switch (scaleMode)
    {
    case ScaleMode::Uniform:
        {
            // Use Y mouse movement for uniform scaling (up = bigger, down = smaller)
            float scaleFactor = 1.0f + (-mouseDeltaY * sensitivity);
            scaledSize = startSize * scaleFactor;

            // Clamp to min/max bounds
            scaledSize.x = glm::clamp(scaledSize.x, 0.0f, 500.0f);
            scaledSize.y = glm::clamp(scaledSize.y, 0.0f, 500.0f);
            break;
        }
    case ScaleMode::None:
    default:
        break;
    }

    return scaledSize;
}

void ParticleRenderer::renderScaleModeIndicator(int viewportWidth, int viewportHeight, ScaleMode scaleMode,
                                                const glm::vec3& emitterPosition, const glm::vec2& currentSize)
{
    if (scaleMode == ScaleMode::None)
    {
        return;
    }

    // Ensure standard blend state for editor elements
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(lineShaderProgram);

    GLint viewLoc = glGetUniformLocation(lineShaderProgram, "view");
    GLint projLoc = glGetUniformLocation(lineShaderProgram, "projection");
    GLint modelLoc = glGetUniformLocation(lineShaderProgram, "model");
    GLint colorLoc = glGetUniformLocation(lineShaderProgram, "lineColor");

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &viewMatrix[0][0]);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projectionMatrix[0][0]);

    // Position indicator at emitter location
    glm::mat4 model = glm::translate(glm::mat4(1.0f), emitterPosition);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);

    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);

    // Disable depth test so indicator is always visible
    glDisable(GL_DEPTH_TEST);

    std::vector<float> indicatorVertices;

    switch (scaleMode)
    {
    case ScaleMode::Uniform:
        {
            // Draw a square outline showing the current scale
            float halfX = currentSize.x * 0.5f;
            float halfY = currentSize.y * 0.5f;

            // Use cyan color for scale mode
            glUniform3f(colorLoc, 0.0f, 1.0f, 1.0f); // Cyan

            indicatorVertices = {
                -halfX, -halfY, 0.0f, halfX,  -halfY, 0.0f, // Bottom edge
                halfX,  -halfY, 0.0f, halfX,  halfY,  0.0f, // Right edge
                halfX,  halfY,  0.0f, -halfX, halfY,  0.0f, // Top edge
                -halfX, halfY,  0.0f, -halfX, -halfY, 0.0f // Left edge
            };
            glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
            glDrawArrays(GL_LINES, 0, 8);

            // Draw diagonal cross lines to show it's a scale indicator
            indicatorVertices = {
                -halfX * 0.7f, -halfY * 0.7f, 0.0f, halfX * 0.7f,  halfY * 0.7f, 0.0f, // Diagonal 1
                halfX * 0.7f,  -halfY * 0.7f, 0.0f, -halfX * 0.7f, halfY * 0.7f, 0.0f // Diagonal 2
            };
            glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
            glDrawArrays(GL_LINES, 0, 4);
            break;
        }
    case ScaleMode::None:
    default:
        break;
    }

    // Re-enable depth test
    glEnable(GL_DEPTH_TEST);

    glBindVertexArray(0);
    glUseProgram(0);
}

void ParticleRenderer::renderRotationModeIndicator(int viewportWidth, int viewportHeight, RotationMode rotationMode,
                                                   const glm::vec3& emitterPosition)
{
    if (rotationMode == RotationMode::None)
    {
        return;
    }

    // Ensure standard blend state for editor elements
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(lineShaderProgram);
    GLint viewLoc = glGetUniformLocation(lineShaderProgram, "view");
    GLint projLoc = glGetUniformLocation(lineShaderProgram, "projection");
    GLint modelLoc = glGetUniformLocation(lineShaderProgram, "model");
    GLint colorLoc = glGetUniformLocation(lineShaderProgram, "lineColor");

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &viewMatrix[0][0]);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projectionMatrix[0][0]);

    // Position indicator at emitter location
    glm::mat4 model = glm::translate(glm::mat4(1.0f), emitterPosition);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);

    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);

    // Disable depth test so indicator is always visible
    glDisable(GL_DEPTH_TEST);

    std::vector<float> indicatorVertices;
    float circleRadius = 1.0f;
    int numSegments = 32;

    switch (rotationMode)
    {
    case RotationMode::Free:
        // Show all three rotation circles (XY, XZ, YZ planes) in bright colors
        // XY plane rotation circle (around Z axis) - Blue
        glUniform3f(colorLoc, 0.3f, 0.3f, 1.0f);
        indicatorVertices.clear();
        for (int i = 0; i < numSegments; ++i)
        {
            float angle1 = 2.0f * M_PI * i / numSegments;
            float angle2 = 2.0f * M_PI * (i + 1) / numSegments;
            indicatorVertices.insert(indicatorVertices.end(),
                                     {circleRadius * cos(angle1), circleRadius * sin(angle1), 0.0f,
                                      circleRadius * cos(angle2), circleRadius * sin(angle2), 0.0f});
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
        glDrawArrays(GL_LINES, 0, numSegments * 2);

        // XZ plane rotation circle (around Y axis) - Green
        glUniform3f(colorLoc, 0.3f, 1.0f, 0.3f);
        indicatorVertices.clear();
        for (int i = 0; i < numSegments; ++i)
        {
            float angle1 = 2.0f * M_PI * i / numSegments;
            float angle2 = 2.0f * M_PI * (i + 1) / numSegments;
            indicatorVertices.insert(indicatorVertices.end(),
                                     {circleRadius * cos(angle1), 0.0f, circleRadius * sin(angle1),
                                      circleRadius * cos(angle2), 0.0f, circleRadius * sin(angle2)});
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
        glDrawArrays(GL_LINES, 0, numSegments * 2);

        // YZ plane rotation circle (around X axis) - Red
        glUniform3f(colorLoc, 1.0f, 0.3f, 0.3f);
        indicatorVertices.clear();
        for (int i = 0; i < numSegments; ++i)
        {
            float angle1 = 2.0f * M_PI * i / numSegments;
            float angle2 = 2.0f * M_PI * (i + 1) / numSegments;
            indicatorVertices.insert(indicatorVertices.end(),
                                     {0.0f, circleRadius * cos(angle1), circleRadius * sin(angle1), 0.0f,
                                      circleRadius * cos(angle2), circleRadius * sin(angle2)});
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
        glDrawArrays(GL_LINES, 0, numSegments * 2);
        break;

    case RotationMode::X_Axis:
        // Show only YZ plane rotation circle (around X axis) - Bright Red
        glUniform3f(colorLoc, 1.0f, 0.2f, 0.2f);
        indicatorVertices.clear();
        for (int i = 0; i < numSegments; ++i)
        {
            float angle1 = 2.0f * M_PI * i / numSegments;
            float angle2 = 2.0f * M_PI * (i + 1) / numSegments;
            indicatorVertices.insert(indicatorVertices.end(),
                                     {0.0f, circleRadius * cos(angle1), circleRadius * sin(angle1), 0.0f,
                                      circleRadius * cos(angle2), circleRadius * sin(angle2)});
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
        glDrawArrays(GL_LINES, 0, numSegments * 2);
        break;

    case RotationMode::Y_Axis:
        // Show only XZ plane rotation circle (around Y axis) - Bright Green
        glUniform3f(colorLoc, 0.2f, 1.0f, 0.2f);
        indicatorVertices.clear();
        for (int i = 0; i < numSegments; ++i)
        {
            float angle1 = 2.0f * M_PI * i / numSegments;
            float angle2 = 2.0f * M_PI * (i + 1) / numSegments;
            indicatorVertices.insert(indicatorVertices.end(),
                                     {circleRadius * cos(angle1), 0.0f, circleRadius * sin(angle1),
                                      circleRadius * cos(angle2), 0.0f, circleRadius * sin(angle2)});
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
        glDrawArrays(GL_LINES, 0, numSegments * 2);
        break;

    case RotationMode::Z_Axis:
        // Show only XY plane rotation circle (around Z axis) - Bright Blue
        glUniform3f(colorLoc, 0.2f, 0.2f, 1.0f);
        indicatorVertices.clear();
        for (int i = 0; i < numSegments; ++i)
        {
            float angle1 = 2.0f * M_PI * i / numSegments;
            float angle2 = 2.0f * M_PI * (i + 1) / numSegments;
            indicatorVertices.insert(indicatorVertices.end(),
                                     {circleRadius * cos(angle1), circleRadius * sin(angle1), 0.0f,
                                      circleRadius * cos(angle2), circleRadius * sin(angle2), 0.0f});
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, indicatorVertices.size() * sizeof(float), indicatorVertices.data());
        glDrawArrays(GL_LINES, 0, numSegments * 2);
        break;

    case RotationMode::None:
    default:
        break;
    }

    // Re-enable depth test
    glEnable(GL_DEPTH_TEST);

    glBindVertexArray(0);
    glUseProgram(0);
}

glm::vec3 ParticleRenderer::mouseToRotation(float mouseDeltaX, float mouseDeltaY, RotationMode rotationMode,
                                            float sensitivity) const
{
    glm::vec3 rotationDelta(0.0f);

    switch (rotationMode)
    {
    case RotationMode::Free:
        // Free rotation: X mouse movement affects Z rotation, Y mouse movement affects X rotation
        rotationDelta.x = -mouseDeltaY * sensitivity * 180.0f; // Pitch (alpha)
        rotationDelta.y = 0.0f; // Roll (beta) - not easily controlled with 2D mouse
        rotationDelta.z = mouseDeltaX * sensitivity * 180.0f; // Yaw (gamma)
        break;

    case RotationMode::X_Axis:
        // Rotation around X axis only (pitch/alpha)
        rotationDelta.x = -mouseDeltaY * sensitivity * 180.0f;
        rotationDelta.y = 0.0f;
        rotationDelta.z = 0.0f;
        break;

    case RotationMode::Y_Axis:
        // Rotation around Y axis only (roll/beta)
        rotationDelta.x = 0.0f;
        rotationDelta.y = mouseDeltaX * sensitivity * 180.0f;
        rotationDelta.z = 0.0f;
        break;

    case RotationMode::Z_Axis:
        // Rotation around Z axis only (yaw/gamma)
        rotationDelta.x = 0.0f;
        rotationDelta.y = 0.0f;
        rotationDelta.z = mouseDeltaX * sensitivity * 180.0f;
        break;

    case RotationMode::None:
    default:
        break;
    }

    return rotationDelta;
}
