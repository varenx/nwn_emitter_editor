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

#include "emitter.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <cstdlib>
#include <glm/gtc/quaternion.hpp>

EmitterEditor::EmitterEditor() {
    // Initialize texture directory to home directory
    #ifdef _WIN32
        const char* userProfile = std::getenv("USERPROFILE");
        textureDirectory = userProfile ? std::string(userProfile) : "C:\\Users\\Public";
    #else
        const char* home = std::getenv("HOME");
        textureDirectory = home ? std::string(home) : "/";
    #endif
    
    // Create a default emitter
    addEmitter("default_emitter");
}

EmitterEditor::~EmitterEditor() = default;

EmitterNode EmitterEditor::createDefaultEmitter() {
    EmitterNode emitter;
    
    // Set reasonable defaults for a basic fire effect
    emitter.update = UpdateType::Fountain;
    emitter.render = RenderType::Normal;
    emitter.blend = BlendType::Lighten;
    emitter.texture = ""; // Start with no texture to show fallback rendering
    emitter.birthrate = 2.0f;
    emitter.lifeExp = 1.5f;
    emitter.velocity = 1.0f;
    emitter.spread = 45.0f;
    emitter.colorStart = {0.929f, 0.592f, 0.231f};
    emitter.colorEnd = {0.910f, 0.471f, 0.0f};
    emitter.sizeStart = 0.5f;
    emitter.sizeEnd = 0.0f;
    
    // Set emitter geometry to small rectangle
    emitter.xsize = 0.1f;
    emitter.ysize = 0.1f;
    
    // Set emitter flat on ground (no rotation needed in right-handed Z-up system)
    emitter.eulerAngles = glm::vec3(0.0f, 0.0f, 0.0f); // No rotation by default
    
    return emitter;
}

void EmitterEditor::addEmitter(const std::string& name) {
    EmitterNode emitter = createDefaultEmitter();
    emitter.name = name;
    emitters.push_back(emitter);
}

void EmitterEditor::removeEmitter(int index) {
    if (index >= 0 && index < static_cast<int>(emitters.size())) {
        emitters.erase(emitters.begin() + index);
    }
}

void EmitterEditor::duplicateEmitter(int index) {
    if (index < 0 || index >= static_cast<int>(emitters.size())) {
        return;
    }
    
    // Copy the existing emitter
    EmitterNode duplicate = emitters[index];
    
    // Parse existing name to check for numeric suffix
    std::string baseName = duplicate.name;
    int startingSuffix = 2;
    
    // Check if name already ends with _X pattern
    size_t lastUnderscore = baseName.find_last_of('_');
    if (lastUnderscore != std::string::npos && lastUnderscore < baseName.length() - 1) {
        std::string suffixStr = baseName.substr(lastUnderscore + 1);
        
        // Check if suffix is purely numeric
        bool isNumeric = true;
        for (char c : suffixStr) {
            if (!std::isdigit(c)) {
                isNumeric = false;
                break;
            }
        }
        
        if (isNumeric && !suffixStr.empty()) {
            // Extract base name without the _X suffix and start from next number
            baseName = baseName.substr(0, lastUnderscore);
            startingSuffix = std::stoi(suffixStr) + 1;
        }
    }
    
    // Generate unique name by incrementing suffix
    std::string newName;
    int suffix = startingSuffix;
    
    do {
        newName = baseName + "_" + std::to_string(suffix);
        suffix++;
        
        // Check if this name already exists
        bool nameExists = false;
        for (const auto& emitter : emitters) {
            if (emitter.name == newName) {
                nameExists = true;
                break;
            }
        }
        
        if (!nameExists) {
            break;
        }
    } while (suffix < 1000); // Safety limit
    
    duplicate.name = newName;
    emitters.push_back(duplicate);
}

void EmitterEditor::resetToNew() {
    emitters.clear();
    modelName = "emitter_model";
    addEmitter("default_emitter");
}

void EmitterEditor::setModelName(const std::string& name) {
    modelName = name;
}

std::string updateTypeToString(UpdateType type) {
    switch (type) {
        case UpdateType::Fountain: return "Fountain";
        case UpdateType::Single: return "Single";
        case UpdateType::Explosion: return "Explosion";
        case UpdateType::Lightning: return "Lightning";
        default: return "Fountain";
    }
}

std::string renderTypeToString(RenderType type) {
    switch (type) {
        case RenderType::Normal: return "Normal";
        case RenderType::Linked: return "Linked";
        case RenderType::Billboard_to_Local_Z: return "Billboard_to_Local_Z";
        case RenderType::Billboard_to_World_Z: return "Billboard_to_World_Z";
        case RenderType::Aligned_to_World_Z: return "Aligned_to_World_Z";
        case RenderType::Aligned_to_Particle_Direction: return "Aligned_to_Particle_Direction";
        case RenderType::Motion_Blur: return "Motion_Blur";
        default: return "Normal";
    }
}

std::string blendTypeToString(BlendType type) {
    switch (type) {
        case BlendType::Normal: return "Normal";
        case BlendType::Punch_Through: return "Punch-Through";
        case BlendType::Lighten: return "Lighten";
        default: return "Normal";
    }
}

std::string EmitterEditor::generateMDLText() const {
    std::stringstream ss;
    
    ss << "#MAXMODEL ASCII\n";
    ss << "# model: " << modelName << "\n";
    ss << "newmodel " << modelName << "\n";
    ss << "setsupermodel " << modelName << " NULL\n";
    ss << "classification effect\n";
    ss << "setanimationscale 1\n";
    ss << "#MAXGEOM ASCII\n";
    ss << "beginmodelgeom " << modelName << "\n";
    
    // Root dummy node
    ss << "node dummy " << modelName << "\n";
    ss << "  parent NULL\n";
    ss << "endnode\n";
    
    // Emitter nodes
    for (const auto& emitter : emitters) {
        ss << "node emitter " << emitter.name << "\n";
        ss << "  parent " << modelName << "\n";
        ss << "  p2p " << (emitter.p2p ? 1 : 0) << "\n";
        ss << "  p2p_sel " << emitter.p2p_sel << "\n";
        ss << "  affectedByWind " << (emitter.affectedByWind ? 1 : 0) << "\n";
        ss << "  m_isTinted " << (emitter.m_isTinted ? 1 : 0) << "\n";
        ss << "  bounce " << (emitter.bounce ? 1 : 0) << "\n";
        ss << "  random " << (emitter.random ? 1 : 0) << "\n";
        ss << "  inherit " << (emitter.inherit ? 1 : 0) << "\n";
        ss << "  inheritvel " << (emitter.inheritvel ? 1 : 0) << "\n";
        ss << "  inherit_local " << (emitter.inherit_local ? 1 : 0) << "\n";
        ss << "  splat " << (emitter.splat ? 1 : 0) << "\n";
        ss << "  inherit_part " << (emitter.inherit_part ? 1 : 0) << "\n";
        ss << "  renderorder " << emitter.renderorder << "\n";
        ss << "  spawntype " << static_cast<int>(emitter.spawntype) << "\n";
        ss << "  update " << updateTypeToString(emitter.update) << "\n";
        ss << "  render " << renderTypeToString(emitter.render) << "\n";
        ss << "  blend " << blendTypeToString(emitter.blend) << "\n";
        
        if (!emitter.texture.empty()) {
            ss << "  texture " << emitter.texture << "\n";
        }
        
        ss << "  xgrid " << emitter.xgrid << "\n";
        ss << "  ygrid " << emitter.ygrid << "\n";
        ss << "  loop " << (emitter.loop ? 1 : 0) << "\n";
        ss << "  deadspace " << emitter.deadspace << "\n";
        ss << "  twosidedtex " << (emitter.twosidedtex ? 1 : 0) << "\n";
        ss << "  blastRadius " << emitter.blastRadius << "\n";
        ss << "  blastLength " << emitter.blastLength << "\n";
        
        // Convert position from editor (Z-up) to MDL (Y-up) coordinate system
        glm::vec3 mdlPosition = EmitterNode::convertGameToMDL(emitter.position);
        ss << "  position " << mdlPosition.x << " " << mdlPosition.y << " " << mdlPosition.z << "\n";
        
        // Convert editor Euler angles to quaternion, then to MDL coordinate system
        glm::quat editorQuat = emitter.getOrientation();
        
        // Apply inverse coordinate system transformation: Z-up to Y-up
        // Try rotating around the Z-axis instead  
        glm::quat coordinateTransform = glm::angleAxis(glm::radians(-90.0f), glm::vec3(0, 0, 1));
        glm::quat mdlQuat = coordinateTransform * editorQuat;
        
        ss << "  orientation " << mdlQuat.w << " " << mdlQuat.x << " " << mdlQuat.y << " " << mdlQuat.z << "\n";
        
        if (emitter.xsize > 0 || emitter.ysize > 0) {
            ss << "  xsize " << emitter.xsize << "\n";
            ss << "  ysize " << emitter.ysize << "\n";
        }
        
        ss << "  colorStart " << emitter.colorStart.r << " " << emitter.colorStart.g << " " << emitter.colorStart.b << "\n";
        ss << "  colorEnd " << emitter.colorEnd.r << " " << emitter.colorEnd.g << " " << emitter.colorEnd.b << "\n";
        ss << "  alphaStart " << emitter.alphaStart << "\n";
        ss << "  alphaEnd " << emitter.alphaEnd << "\n";
        ss << "  sizeStart " << emitter.sizeStart << "\n";
        ss << "  sizeEnd " << emitter.sizeEnd << "\n";
        ss << "  sizeStart_y " << emitter.sizeStart_y << "\n";
        ss << "  sizeEnd_y " << emitter.sizeEnd_y << "\n";
        
        ss << "  birthrate " << emitter.birthrate << "\n";
        ss << "  lifeExp " << emitter.lifeExp << "\n";
        ss << "  mass " << emitter.mass << "\n";
        ss << "  spread " << emitter.spread << "\n";
        ss << "  particleRot " << emitter.particleRot << "\n";
        ss << "  velocity " << emitter.velocity << "\n";
        
        if (emitter.grav != 0.0f) ss << "  grav " << emitter.grav << "\n";
        if (emitter.drag != 0.0f) ss << "  drag " << emitter.drag << "\n";
        if (emitter.threshold != 0.0f) ss << "  threshold " << emitter.threshold << "\n";
        if (emitter.fps != 0.0f) ss << "  fps " << emitter.fps << "\n";
        if (emitter.frameStart != 0.0f) ss << "  frameStart " << emitter.frameStart << "\n";
        if (emitter.frameEnd != 0.0f) ss << "  frameEnd " << emitter.frameEnd << "\n";
        if (emitter.bounce_co != 0.0f) ss << "  bounce_co " << emitter.bounce_co << "\n";
        if (emitter.combinetime != 0.0f) ss << "  combinetime " << emitter.combinetime << "\n";
        if (emitter.blurlength != 0.0f) ss << "  blurlength " << emitter.blurlength << "\n";
        
        // Lightning properties
        if (emitter.lightningDelay != 0.0f) ss << "  lightningDelay " << emitter.lightningDelay << "\n";
        if (emitter.lightningRadius != 0.0f) ss << "  lightningRadius " << emitter.lightningRadius << "\n";
        if (emitter.lightningScale != 0.0f) ss << "  lightningScale " << emitter.lightningScale << "\n";
        if (emitter.lightningSubDiv != 0.0f) ss << "  lightningSubDiv " << emitter.lightningSubDiv << "\n";
        if (emitter.lightningZigZag != 0.0f) ss << "  lightningZigZag " << emitter.lightningZigZag << "\n";
        
        
        
        ss << "endnode\n";
    }
    
    ss << "endmodelgeom " << modelName << "\n";
    
    return ss.str();
}

void EmitterEditor::loadFromMDL(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }
    
    // Update texture directory to the directory containing the MDL file
    textureDirectory = std::filesystem::path(filename).parent_path().string();
    
    emitters.clear();
    
    std::string line;
    EmitterNode* currentEmitter = nullptr;
    
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string token;
        ss >> token;
        
        if (token == "node") {
            std::string nodeType;
            ss >> nodeType;
            if (nodeType == "emitter") {
                std::string name;
                ss >> name;
                
                // Check if this emitter already exists (animation keyframes)
                auto existingIt = std::find_if(emitters.begin(), emitters.end(), 
                    [&name](const EmitterNode& emitter) { return emitter.name == name; });
                
                if (existingIt != emitters.end()) {
                    currentEmitter = &(*existingIt); // Parse animation data for existing emitter
                } else {
                    // Create a fresh emitter with defaults, then we'll parse its properties
                    EmitterNode emitter = createDefaultEmitter();
                    emitter.name = name;
                    emitters.push_back(emitter);
                    currentEmitter = &emitters.back();
                }
            }
        } else if (currentEmitter && token == "endnode") {
            currentEmitter = nullptr;
        } else if (currentEmitter) {
            // Parse emitter properties
            if (token == "parent") {
                ss >> currentEmitter->parent;
            } else if (token == "p2p") {
                int val; ss >> val; currentEmitter->p2p = (val != 0);
            } else if (token == "p2p_sel") {
                ss >> currentEmitter->p2p_sel;
            } else if (token == "affectedByWind") {
                int val; ss >> val; currentEmitter->affectedByWind = (val != 0);
            } else if (token == "m_isTinted") {
                int val; ss >> val; currentEmitter->m_isTinted = (val != 0);
            } else if (token == "bounce") {
                int val; ss >> val; currentEmitter->bounce = (val != 0);
            } else if (token == "random") {
                int val; ss >> val; currentEmitter->random = (val != 0);
            } else if (token == "inherit") {
                int val; ss >> val; currentEmitter->inherit = (val != 0);
            } else if (token == "inheritvel") {
                int val; ss >> val; currentEmitter->inheritvel = (val != 0);
            } else if (token == "inherit_local") {
                int val; ss >> val; currentEmitter->inherit_local = (val != 0);
            } else if (token == "splat") {
                int val; ss >> val; currentEmitter->splat = (val != 0);
            } else if (token == "inherit_part") {
                int val; ss >> val; currentEmitter->inherit_part = (val != 0);
            } else if (token == "renderorder") {
                ss >> currentEmitter->renderorder;
            } else if (token == "spawntype") {
                int val; ss >> val; currentEmitter->spawntype = static_cast<SpawnType>(val);
            } else if (token == "update") {
                std::string updateStr; ss >> updateStr;
                if (updateStr == "Fountain") currentEmitter->update = UpdateType::Fountain;
                else if (updateStr == "Single") currentEmitter->update = UpdateType::Single;
                else if (updateStr == "Explosion") currentEmitter->update = UpdateType::Explosion;
                else if (updateStr == "Lightning") currentEmitter->update = UpdateType::Lightning;
            } else if (token == "render") {
                std::string renderStr; ss >> renderStr;
                if (renderStr == "Normal") currentEmitter->render = RenderType::Normal;
                else if (renderStr == "Linked") currentEmitter->render = RenderType::Linked;
                else if (renderStr == "Billboard_to_Local_Z") currentEmitter->render = RenderType::Billboard_to_Local_Z;
                else if (renderStr == "Billboard_to_World_Z") currentEmitter->render = RenderType::Billboard_to_World_Z;
                else if (renderStr == "Aligned_to_World_Z") currentEmitter->render = RenderType::Aligned_to_World_Z;
                else if (renderStr == "Aligned_to_Particle_Direction") currentEmitter->render = RenderType::Aligned_to_Particle_Direction;
                else if (renderStr == "Motion_Blur") currentEmitter->render = RenderType::Motion_Blur;
            } else if (token == "blend") {
                std::string blendStr; ss >> blendStr;
                if (blendStr == "Normal") currentEmitter->blend = BlendType::Normal;
                else if (blendStr == "Punch-Through") currentEmitter->blend = BlendType::Punch_Through;
                else if (blendStr == "Lighten") currentEmitter->blend = BlendType::Lighten;
            } else if (token == "texture") {
                ss >> currentEmitter->texture;
            } else if (token == "xgrid") {
                ss >> currentEmitter->xgrid;
            } else if (token == "ygrid") {
                ss >> currentEmitter->ygrid;
            } else if (token == "loop") {
                int val; ss >> val; currentEmitter->loop = (val != 0);
            } else if (token == "deadspace") {
                ss >> currentEmitter->deadspace;
            } else if (token == "twosidedtex") {
                int val; ss >> val; currentEmitter->twosidedtex = (val != 0);
            } else if (token == "blastRadius") {
                ss >> currentEmitter->blastRadius;
            } else if (token == "blastLength") {
                ss >> currentEmitter->blastLength;
            } else if (token == "position") {
                glm::vec3 mdlPos;
                ss >> mdlPos.x >> mdlPos.y >> mdlPos.z;
                currentEmitter->position = EmitterNode::convertMDLToGame(mdlPos);
            } else if (token == "orientation") {
                float w, x, y, z;
                ss >> w >> x >> y >> z;
                
                // MDL format uses (w, x, y, z) quaternion in Y-up coordinate system
                glm::quat mdlQuat(w, x, y, z);
                
                // Apply coordinate system transformation: Y-up to Z-up
                // Try rotating around the Z-axis instead
                glm::quat coordinateTransform = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 0, 1));
                glm::quat editorQuat = coordinateTransform * mdlQuat;
                
                // Convert quaternion to Euler angles in degrees
                currentEmitter->eulerAngles = glm::degrees(glm::eulerAngles(editorQuat));
            } else if (token == "xsize") {
                ss >> currentEmitter->xsize;
            } else if (token == "ysize") {
                ss >> currentEmitter->ysize;
            } else if (token == "colorStart") {
                ss >> currentEmitter->colorStart.r >> currentEmitter->colorStart.g >> currentEmitter->colorStart.b;
            } else if (token == "colorEnd") {
                ss >> currentEmitter->colorEnd.r >> currentEmitter->colorEnd.g >> currentEmitter->colorEnd.b;
            } else if (token == "alphaStart") {
                ss >> currentEmitter->alphaStart;
            } else if (token == "alphaEnd") {
                ss >> currentEmitter->alphaEnd;
            } else if (token == "sizeStart") {
                ss >> currentEmitter->sizeStart;
            } else if (token == "sizeEnd") {
                ss >> currentEmitter->sizeEnd;
            } else if (token == "sizeStart_y") {
                ss >> currentEmitter->sizeStart_y;
            } else if (token == "sizeEnd_y") {
                ss >> currentEmitter->sizeEnd_y;
            } else if (token == "birthrate") {
                ss >> currentEmitter->birthrate;
            } else if (token == "lifeExp") {
                ss >> currentEmitter->lifeExp;
            } else if (token == "mass") {
                ss >> currentEmitter->mass;
            } else if (token == "spread") {
                ss >> currentEmitter->spread;
            } else if (token == "particleRot") {
                ss >> currentEmitter->particleRot;
            } else if (token == "velocity") {
                ss >> currentEmitter->velocity;
            } else if (token == "grav") {
                ss >> currentEmitter->grav;
            } else if (token == "drag") {
                ss >> currentEmitter->drag;
            } else if (token == "threshold") {
                ss >> currentEmitter->threshold;
            } else if (token == "fps") {
                ss >> currentEmitter->fps;
            } else if (token == "frameStart") {
                ss >> currentEmitter->frameStart;
            } else if (token == "frameEnd") {
                ss >> currentEmitter->frameEnd;
            } else if (token == "bounce_co") {
                ss >> currentEmitter->bounce_co;
            } else if (token == "combinetime") {
                ss >> currentEmitter->combinetime;
            } else if (token == "blurlength") {
                ss >> currentEmitter->blurlength;
            } else if (token == "lightningDelay") {
                ss >> currentEmitter->lightningDelay;
            } else if (token == "lightningRadius") {
                ss >> currentEmitter->lightningRadius;
            } else if (token == "lightningScale") {
                ss >> currentEmitter->lightningScale;
            } else if (token == "lightningSubDiv") {
                ss >> currentEmitter->lightningSubDiv;
            } else if (token == "lightningZigZag") {
                ss >> currentEmitter->lightningZigZag;
            } else if (token == "positionkey") {
                // Parse position animation keyframes
                int numKeys;
                ss >> numKeys;
                currentEmitter->positionKeys.keyframes.clear();
                
                for (int i = 0; i < numKeys; ++i) {
                    if (std::getline(file, line)) {
                        std::stringstream keyss(line);
                        float time, x, y, z;
                        keyss >> time >> x >> y >> z;
                        glm::vec3 mdlPos(x, y, z);
                        glm::vec3 gamePos = EmitterNode::convertMDLToGame(mdlPos);
                        currentEmitter->positionKeys.keyframes.emplace_back(time, gamePos);
                    }
                }
            } else if (token == "orientationkey") {
                // Parse orientation animation keyframes
                int numKeys;
                ss >> numKeys;
                currentEmitter->orientationKeys.keyframes.clear();
                
                for (int i = 0; i < numKeys; ++i) {
                    if (std::getline(file, line)) {
                        std::stringstream keyss(line);
                        float time, x, y, z, w;
                        keyss >> time >> x >> y >> z >> w;
                        // Store as Euler angles or quaternion components - for now just xyz
                        currentEmitter->orientationKeys.keyframes.emplace_back(time, glm::vec3(x, y, z));
                    }
                }
            }
        }
    }
    
}

void EmitterEditor::saveToMDL(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to create file: " << filename << std::endl;
        return;
    }
    
    file << generateMDLText();
    file.close();
}