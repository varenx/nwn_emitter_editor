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

#include "toast_manager.hpp"
#include <algorithm>
#include <ctime>
#include <imgui.h>
#include <iomanip>
#include <sstream>

ToastManager::ToastManager() {}

ToastManager::~ToastManager() {}

void ToastManager::addToast(const std::string& title, const std::string& message, const std::string& icon,
                            bool showTimestamp)
{
    // Remove oldest toasts if we're at max capacity
    while (toasts.size() >= static_cast<size_t>(maxToasts))
    {
        toasts.erase(toasts.begin());
    }

    Toast toast(title, message, icon, showTimestamp);
    toast.duration = defaultDuration;
    toasts.push_back(toast);
}

void ToastManager::update(float deltaTime)
{
    // Update all toasts
    for (auto& toast : toasts)
    {
        toast.timeAlive += deltaTime;
        toast.alpha = toast.getAlpha();
    }

    // Remove expired toasts
    removeExpiredToasts();
}

void ToastManager::render()
{
    if (toasts.empty())
        return;

    // Get the main window viewport for positioning relative to the window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 windowPos = ImVec2(viewport->Pos.x + viewport->Size.x - 10,
                              viewport->Pos.y + viewport->Size.y - 10); // Bottom right corner of main window

    // Render toasts from bottom to top (newest at bottom)
    float totalHeight = 0;

    // Calculate total height first
    for (int i = static_cast<int>(toasts.size()) - 1; i >= 0; i--)
    {
        const Toast& toast = toasts[i];
        if (toast.alpha <= 0.0f)
            continue;

        // Calculate toast size
        ImVec2 textSize = ImGui::CalcTextSize(toast.title.c_str());
        ImVec2 messageSize = ImGui::CalcTextSize(toast.message.c_str());

        float toastWidth = std::max(textSize.x, messageSize.x) + 20; // Padding
        float toastHeight = textSize.y + messageSize.y + 20; // Title + message + padding

        if (toast.showTimestamp)
        {
            std::string timestampStr = formatTimestamp(toast.timestamp);
            ImVec2 timestampSize = ImGui::CalcTextSize(timestampStr.c_str());
            toastWidth = std::max(toastWidth, timestampSize.x + 20);
            toastHeight += timestampSize.y + 5; // Add timestamp height + spacing
        }

        totalHeight += toastHeight + 10; // Toast height + spacing between toasts
    }

    // Now render each toast
    float currentY = windowPos.y - totalHeight;

    for (int i = static_cast<int>(toasts.size()) - 1; i >= 0; i--)
    {
        const Toast& toast = toasts[i];
        if (toast.alpha <= 0.0f)
            continue;

        // Calculate toast dimensions
        ImVec2 textSize = ImGui::CalcTextSize(toast.title.c_str());
        ImVec2 messageSize = ImGui::CalcTextSize(toast.message.c_str());

        float toastWidth = std::max(textSize.x, messageSize.x) + 20;
        float toastHeight = textSize.y + messageSize.y + 20;

        if (toast.showTimestamp)
        {
            std::string timestampStr = formatTimestamp(toast.timestamp);
            ImVec2 timestampSize = ImGui::CalcTextSize(timestampStr.c_str());
            toastWidth = std::max(toastWidth, timestampSize.x + 20);
            toastHeight += timestampSize.y + 5;
        }

        // Clamp minimum width
        toastWidth = std::max(toastWidth, 200.0f);

        // Position this toast
        ImVec2 toastPos = ImVec2(windowPos.x - toastWidth, currentY);

        // Set up the toast window
        ImGui::SetNextWindowPos(toastPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(toastWidth, toastHeight), ImGuiCond_Always);

        // Apply alpha for fade in/out
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, toast.alpha);

        // Toast window styling
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 0.8f));

        std::string windowName = "Toast##" + std::to_string(i);

        if (ImGui::Begin(windowName.c_str(), nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing))
        {

            // Render icon if provided
            if (!toast.icon.empty())
            {
                ImGui::Text("%s", toast.icon.c_str());
                ImGui::SameLine();
            }

            // Render title
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImGui::TextWrapped("%s", toast.title.c_str());
            ImGui::PopStyleColor();

            // Render message
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
            ImGui::TextWrapped("%s", toast.message.c_str());
            ImGui::PopStyleColor();

            // Render timestamp if enabled
            if (toast.showTimestamp)
            {
                std::string timestampStr = formatTimestamp(toast.timestamp);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::Text("%s", timestampStr.c_str());
                ImGui::PopStyleColor();
            }
        }
        ImGui::End();

        // Pop styling
        ImGui::PopStyleColor(2); // WindowBg, Border
        ImGui::PopStyleVar(3); // Alpha, WindowRounding, WindowBorderSize

        currentY += toastHeight + 10; // Move to next toast position
    }
}

std::string ToastManager::formatTimestamp(const std::chrono::system_clock::time_point& timestamp) const
{
    // Convert to time_t for formatting
    std::time_t time = std::chrono::system_clock::to_time_t(timestamp);

    // Try to use locale-specific formatting if possible
    std::stringstream ss;
    try
    {
        std::tm* tm = std::localtime(&time);
        if (tm)
        {
            // Try locale-specific format first
            ss << std::put_time(tm, "%c");
            std::string result = ss.str();
            if (!result.empty())
            {
                return result;
            }
        }
    }
    catch (...)
    {
        // Fall back to MySQL format if locale formatting fails
    }

    // Fallback to MySQL format: Y-m-d H:i (YYYY-MM-DD HH:MM)
    std::tm* tm = std::localtime(&time);
    if (tm)
    {
        ss.str("");
        ss << std::put_time(tm, "%Y-%m-%d %H:%M");
        return ss.str();
    }

    return "Invalid time";
}

void ToastManager::removeExpiredToasts()
{
    toasts.erase(std::remove_if(toasts.begin(), toasts.end(), [](const Toast& toast) { return toast.shouldRemove(); }),
                 toasts.end());
}
