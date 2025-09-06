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

#ifndef GRAB_MODE_HPP
#define GRAB_MODE_HPP

enum class GrabMode
{
    None,
    Free,
    X_Axis,
    Y_Axis,
    Z_Axis,
    YZ_Plane,
    XZ_Plane,
    XY_Plane
};

enum class ScaleMode
{
    None,
    Uniform
};

enum class RotationMode
{
    None,
    Free,
    X_Axis,
    Y_Axis,
    Z_Axis
};

#endif // GRAB_MODE_HPP
