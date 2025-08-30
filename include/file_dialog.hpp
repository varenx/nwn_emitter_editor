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

#ifndef FILE_DIALOG_HPP
#define FILE_DIALOG_HPP

#include <filesystem>
#include <string>
#include <vector>

class FileDialog
{
public:
    static bool renderLoadDialog(const char* label, std::string& selectedFile);
    static bool renderSaveDialog(const char* label, std::string& filename);
    static bool renderSaveAsDialog(const char* label, std::string& filename,
                                   const std::string& currentPath);
    static bool renderTextureDialog(const char* label, std::string& selectedTexture);
    static std::string extractModelName(const std::string& filename);
    static void setLastSavedFilename(const std::string& filename);

private:
    static std::string currentPath;
    static std::string saveFilename;
    static std::string lastSavedFilename;
    static std::string searchFilter;

    static void navigateToPath(const std::string& path);
    static std::vector<std::filesystem::path> getDirectoryContents();
    static std::vector<std::filesystem::path> getFilteredDirectoryContents();
    static bool isValidMDLFilename(const std::string& filename);
    static bool isValidTextureFile(const std::string& filename);
    static bool matchesFilter(const std::string& filename);
};

#endif// FILE_DIALOG_HPP