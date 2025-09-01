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

#include "file_dialog.hpp"
#include <algorithm>
#include <cstring>
#include <imgui.h>
#include <iostream>

// Static member initialization
std::string FileDialog::currentPath = "";
std::string FileDialog::saveFilename = "";
std::string FileDialog::lastSavedFilename = "";
std::string FileDialog::searchFilter = "";
std::vector<std::filesystem::path> FileDialog::cachedDirectoryContents;
bool FileDialog::filesLoaded = false;

// Initialize default path based on platform
static std::string getDefaultPath()
{
#ifdef _WIN32
    // Windows - use Desktop
    const char *userProfile = std::getenv("USERPROFILE");
    if (userProfile) {
        return std::string(userProfile) + "\\Desktop";
    }
    return "C:\\Users\\Public\\Desktop";
#else
    // Mac/Linux - use home directory
    const char *home = std::getenv("HOME");
    if (home) {
        return std::string(home);
    }
    return "/";
#endif
}

bool FileDialog::renderLoadDialog(const char *label, std::string &selectedFile)
{
    // Initialize path on first use
    if (currentPath.empty()) {
        currentPath = getDefaultPath();
    }

    bool fileSelected = false;

    if (ImGui::BeginPopupModal(label, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Load MDL File");
        ImGui::Text("Current Path: %s", currentPath.c_str());
        ImGui::Separator();

        // Navigation buttons
        if (ImGui::Button("..") && currentPath != "/") {
            std::filesystem::path parent = std::filesystem::path(currentPath).parent_path();
            navigateToPath(parent.string());
        }
        ImGui::SameLine();
        if (ImGui::Button("Home")) {
            navigateToPath(getDefaultPath());
        }

        ImGui::Separator();

        // Search input
        char searchBuf[256];
        strncpy(searchBuf, searchFilter.c_str(), sizeof(searchBuf) - 1);
        searchBuf[sizeof(searchBuf) - 1] = '\0';

        ImGui::Text("Search:");
        if (ImGui::InputText("##search", searchBuf, sizeof(searchBuf))) {
            searchFilter = std::string(searchBuf);
        }

        // Show current filter for debugging
        if (!searchFilter.empty()) {
            ImGui::Text("Filtering by: '%s'", searchFilter.c_str());
        }

        ImGui::Separator();

        // List directory contents (filtered)
        auto contents = getFilteredDirectoryContents();

        // Create a child window for scrollable file list
        if (ImGui::BeginChild("FileList", ImVec2(400, 300), true)) {
            for (const auto &entry: contents) {
                std::string filename = entry.filename().string();

                if (std::filesystem::is_directory(entry)) {
                    // Directory
                    if (ImGui::Selectable(("[DIR] " + filename).c_str())) {
                        navigateToPath(entry.string());
                    }
                } else if (filename.ends_with(".mdl")) {
                    // MDL file
                    if (ImGui::Selectable(filename.c_str())) {
                        selectedFile = entry.string();
                        fileSelected = true;
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return fileSelected;
}

bool FileDialog::renderSaveDialog(const char *label, std::string &filename)
{
    // Initialize path on first use
    if (currentPath.empty()) {
        currentPath = getDefaultPath();
    }

    bool fileSaved = false;

    if (ImGui::BeginPopupModal(label, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Save MDL File");
        ImGui::Text("Current Path: %s", currentPath.c_str());
        ImGui::Separator();

        // Navigation (same as load dialog)
        if (ImGui::Button("..") && currentPath != "/") {
            std::filesystem::path parent = std::filesystem::path(currentPath).parent_path();
            navigateToPath(parent.string());
        }
        ImGui::SameLine();
        if (ImGui::Button("Home")) {
            navigateToPath(getDefaultPath());
        }

        ImGui::Separator();

        // Search input
        char searchBuf[256];
        strncpy(searchBuf, searchFilter.c_str(), sizeof(searchBuf) - 1);
        searchBuf[sizeof(searchBuf) - 1] = '\0';

        ImGui::Text("Search:");
        if (ImGui::InputText("##search", searchBuf, sizeof(searchBuf))) {
            searchFilter = std::string(searchBuf);
        }

        // Show directory browser for navigation
        ImGui::Text("Browse directories:");
        auto filteredContents = getFilteredDirectoryContents();
        if (ImGui::BeginChild("DirectoryBrowser", ImVec2(400, 200), true)) {
            for (const auto &entry: filteredContents) {
                std::string filename = entry.filename().string();

                if (std::filesystem::is_directory(entry)) {
                    // Directory - can navigate into it
                    if (ImGui::Selectable(("[DIR] " + filename).c_str())) {
                        navigateToPath(entry.string());
                    }
                } else if (filename.ends_with(".mdl")) {
                    // Show existing MDL files for reference
                    ImGui::Text("%s", filename.c_str());
                }
            }
        }
        ImGui::EndChild();

        ImGui::Separator();

        // Filename input
        char filenameBuf[32] = ""; // 16 chars + .mdl + null terminator

        // Set default filename: use last saved filename if exists, otherwise "default_emitter"
        std::string defaultName = lastSavedFilename.empty() ? "default_emitter" : lastSavedFilename;

        if (saveFilename.empty()) {
            // First time opening dialog, use default
            saveFilename = defaultName;
        }

        strncpy(filenameBuf, saveFilename.c_str(), sizeof(filenameBuf) - 1);
        filenameBuf[sizeof(filenameBuf) - 1] = '\0';

        ImGui::Text("Filename (max 16 chars):");
        if (ImGui::InputText("##filename", filenameBuf, 17)) { // Limit to 16 chars
            saveFilename = std::string(filenameBuf);
        }

        std::string inputName = std::string(filenameBuf);
        bool isValid = isValidMDLFilename(inputName);
        bool isEmpty = inputName.empty();

        if (isEmpty) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Filename cannot be empty!");
        } else if (!isValid) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1),
                               "Invalid filename! Max 16 characters, alphanumeric + underscore only");
        }

        // Show full path preview
        std::string fullPath = currentPath + "/" + inputName + ".mdl";
        ImGui::Text("Full path: %s", fullPath.c_str());

        ImGui::Separator();

        if (ImGui::Button("Save") && isValid && !isEmpty) {
            saveFilename = inputName;
            lastSavedFilename = inputName; // Remember this filename for next time
            filename = fullPath;
            fileSaved = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return fileSaved;
}

bool FileDialog::renderSaveAsDialog(const char *label, std::string &filename, const std::string &currentFilePath)
{
    // Set up the dialog with the current file's path and name if available
    if (!currentFilePath.empty()) {
        std::filesystem::path filePath(currentFilePath);
        currentPath = filePath.parent_path().string();
        saveFilename = filePath.stem().string(); // Filename without extension
        // Also update the last saved filename
        lastSavedFilename = saveFilename;
    } else if (currentPath.empty()) {
        currentPath = getDefaultPath();
    }

    bool fileSaved = false;

    if (ImGui::BeginPopupModal(label, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Save As MDL File");
        ImGui::Text("Current Path: %s", currentPath.c_str());
        ImGui::Separator();

        // Navigation (same as save dialog)
        if (ImGui::Button("..") && currentPath != "/") {
            std::filesystem::path parent = std::filesystem::path(currentPath).parent_path();
            navigateToPath(parent.string());
        }
        ImGui::SameLine();
        if (ImGui::Button("Home")) {
            navigateToPath(getDefaultPath());
        }

        ImGui::Separator();

        // Search input
        char searchBuf[256];
        strncpy(searchBuf, searchFilter.c_str(), sizeof(searchBuf) - 1);
        searchBuf[sizeof(searchBuf) - 1] = '\0';

        ImGui::Text("Search:");
        if (ImGui::InputText("##search", searchBuf, sizeof(searchBuf))) {
            searchFilter = std::string(searchBuf);
        }

        // Show directory browser for navigation
        ImGui::Text("Browse directories:");
        auto filteredContents = getFilteredDirectoryContents();
        if (ImGui::BeginChild("DirectoryBrowser", ImVec2(400, 200), true)) {
            for (const auto &entry: filteredContents) {
                std::string filename = entry.filename().string();

                if (std::filesystem::is_directory(entry)) {
                    // Directory - can navigate into it
                    if (ImGui::Selectable(("[DIR] " + filename).c_str())) {
                        navigateToPath(entry.string());
                    }
                } else if (filename.ends_with(".mdl")) {
                    // Show existing MDL files for reference and allow clicking to fill name
                    if (ImGui::Selectable(filename.c_str())) {
                        // Fill in the filename without extension when user clicks on existing file
                        saveFilename = std::filesystem::path(filename).stem().string();
                    }
                }
            }
        }
        ImGui::EndChild();

        ImGui::Separator();

        // Filename input
        char filenameBuf[32] = ""; // 16 chars + .mdl + null terminator

        strncpy(filenameBuf, saveFilename.c_str(), sizeof(filenameBuf) - 1);
        filenameBuf[sizeof(filenameBuf) - 1] = '\0';

        ImGui::Text("Filename (max 16 chars):");
        if (ImGui::InputText("##filename", filenameBuf, 17)) { // Limit to 16 chars
            saveFilename = std::string(filenameBuf);
        }

        std::string inputName = std::string(filenameBuf);
        bool isValid = isValidMDLFilename(inputName);
        bool isEmpty = inputName.empty();

        if (isEmpty) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Filename cannot be empty!");
        } else if (!isValid) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1),
                               "Invalid filename! Max 16 characters, alphanumeric + underscore only");
        }

        // Show full path preview
        std::string fullPath = currentPath + "/" + inputName + ".mdl";
        ImGui::Text("Full path: %s", fullPath.c_str());

        ImGui::Separator();

        if (ImGui::Button("Save") && isValid && !isEmpty) {
            saveFilename = inputName;
            lastSavedFilename = inputName; // Remember this filename for next time
            filename = fullPath;
            fileSaved = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return fileSaved;
}

bool FileDialog::renderTextureDialog(const char *label, std::string &selectedTexture)
{
    // Initialize path on first use
    if (currentPath.empty()) {
        currentPath = getDefaultPath();
    }

    bool textureSelected = false;

    if (ImGui::BeginPopupModal(label, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Select Texture File");
        ImGui::Text("Current Path: %s", currentPath.c_str());
        ImGui::Separator();

        // Navigation buttons
        if (ImGui::Button("..") && currentPath != "/") {
            std::filesystem::path parent = std::filesystem::path(currentPath).parent_path();
            navigateToPath(parent.string());
        }
        ImGui::SameLine();
        if (ImGui::Button("Home")) {
            navigateToPath(getDefaultPath());
        }

        ImGui::Separator();

        // Search input
        char searchBuf[256];
        strncpy(searchBuf, searchFilter.c_str(), sizeof(searchBuf) - 1);
        searchBuf[sizeof(searchBuf) - 1] = '\0';

        ImGui::Text("Search:");
        if (ImGui::InputText("##search", searchBuf, sizeof(searchBuf))) {
            searchFilter = std::string(searchBuf);
        }

        // Show current filter for debugging
        if (!searchFilter.empty()) {
            ImGui::Text("Filtering by: '%s'", searchFilter.c_str());
        }

        ImGui::Separator();

        // List directory contents (filtered)
        auto contents = getFilteredDirectoryContents();

        // Create a child window for scrollable file list
        if (ImGui::BeginChild("TextureFileList", ImVec2(400, 300), true)) {
            for (const auto &entry: contents) {
                std::string filename = entry.filename().string();

                if (std::filesystem::is_directory(entry)) {
                    // Directory
                    if (ImGui::Selectable(("[DIR] " + filename).c_str())) {
                        navigateToPath(entry.string());
                    }
                } else if (isValidTextureFile(filename)) {
                    // Texture file - show filename without extension
                    std::string displayName = std::filesystem::path(filename).stem().string();
                    if (ImGui::Selectable(displayName.c_str())) {
                        selectedTexture = entry.string();
                        textureSelected = true;
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return textureSelected;
}

void FileDialog::navigateToPath(const std::string &path)
{
    try {
        if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
            currentPath = std::filesystem::canonical(path).string();
            // Clear search filter when navigating to a new directory
            searchFilter.clear();
            // Mark files as not loaded so they get refreshed for new directory
            filesLoaded = false;
        }
    } catch (const std::filesystem::filesystem_error &) {
        // Keep current path if navigation fails
    }
}

std::vector<std::filesystem::path> FileDialog::getDirectoryContents()
{
    // Only load if not already loaded
    if (!filesLoaded) {
        cachedDirectoryContents.clear();

        try {
            for (const auto &entry: std::filesystem::directory_iterator(currentPath)) {
                cachedDirectoryContents.push_back(entry.path());
            }
        } catch (const std::filesystem::filesystem_error &) {
            // Return empty if can't read directory
        }

        // Sort: directories first, then files, both alphabetically
        std::sort(cachedDirectoryContents.begin(), cachedDirectoryContents.end(), [](const auto &a, const auto &b) {
            bool aIsDir = std::filesystem::is_directory(a);
            bool bIsDir = std::filesystem::is_directory(b);

            if (aIsDir != bIsDir) {
                return aIsDir > bIsDir; // Directories first
            }
            return a.filename() < b.filename(); // Alphabetical within type
        });

        filesLoaded = true;
    }

    return cachedDirectoryContents;
}

bool FileDialog::isValidMDLFilename(const std::string &filename)
{
    if (filename.empty() || filename.length() > 16) {
        return false;
    }

    // Check for valid characters (alphanumeric + underscore)
    for (char c: filename) {
        if (!std::isalnum(c) && c != '_') {
            return false;
        }
    }

    return true;
}

bool FileDialog::isValidTextureFile(const std::string &filename)
{
    return filename.ends_with(".dds") || filename.ends_with(".tga") || filename.ends_with(".png") ||
           filename.ends_with(".jpg");
}

std::string FileDialog::extractModelName(const std::string &filename)
{
    std::filesystem::path path(filename);
    return path.stem().string(); // Gets filename without extension
}

void FileDialog::setLastSavedFilename(const std::string &filename) { lastSavedFilename = filename; }

void FileDialog::clearFileCache() { filesLoaded = false; }

bool FileDialog::matchesFilter(const std::string &filename)
{
    if (searchFilter.empty()) {
        return true; // No filter, show all
    }

    // Convert both to lowercase for case-insensitive comparison
    std::string lowerFilename = filename;
    std::string lowerFilter = searchFilter;

    std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    return lowerFilename.find(lowerFilter) != std::string::npos;
}

std::vector<std::filesystem::path> FileDialog::getFilteredDirectoryContents()
{
    std::vector<std::filesystem::path> contents = getDirectoryContents();

    if (searchFilter.empty()) {
        return contents; // No filter, return all
    }

    std::vector<std::filesystem::path> filtered;

    for (const auto &entry: contents) {
        std::string filename = entry.filename().string();

        // Filter both directories and files by search term
        if (matchesFilter(filename)) {
            filtered.push_back(entry);
        }
    }

    return filtered;
}
