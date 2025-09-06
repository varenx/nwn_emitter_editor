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

#ifndef TOAST_MANAGER_HPP
#define TOAST_MANAGER_HPP

#include <chrono>
#include <imgui.h>
#include <string>
#include <vector>

struct Toast
{
    std::string title;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    std::string icon; // Optional icon (empty string for no icon)
    bool showTimestamp = true;
    float duration = 5.0f; // Duration to show toast in seconds
    float fadeOutDuration = 0.5f; // How long the fade out animation takes

    // Internal state for animation
    float timeAlive = 0.0f;
    float alpha = 0.0f;
    bool isVisible = true;

    Toast(const std::string& title, const std::string& message, const std::string& icon = "",
          bool showTimestamp = true) :
        title(title), message(message), timestamp(std::chrono::system_clock::now()), icon(icon),
        showTimestamp(showTimestamp)
    {
    }

    bool shouldRemove() const { return timeAlive > (duration + fadeOutDuration); }

    float getAlpha() const
    {
        if (timeAlive < 0.2f)
        {
            return timeAlive / 0.2f;
        }
        if (timeAlive > duration)
        {
            float fadeTime = timeAlive - duration;
            return 1.0f - (fadeTime / fadeOutDuration);
        }
        return 1.0f;
    }
};

class ToastManager
{
public:
    ToastManager();
    ~ToastManager();

    void addToast(const std::string& title, const std::string& message, const std::string& icon = "",
                  bool showTimestamp = true);
    void update(float deltaTime);
    void render();

    void setMaxToasts(int max) { maxToasts = max; }
    void setDefaultDuration(float duration) { defaultDuration = duration; }

private:
    std::vector<Toast> toasts;
    int maxToasts = 5;
    float defaultDuration = 5.0f;

    std::string formatTimestamp(const std::chrono::system_clock::time_point& timestamp) const;
    void removeExpiredToasts();
};

#endif // TOAST_MANAGER_HPP
