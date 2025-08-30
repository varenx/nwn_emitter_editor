# NWN Emitter Editor

A modern particle effect editor for Neverwinter Nights (NWN) emitter files. This application allows you to
create, edit, and preview particle emitters used in NWN models with real-time 3D visualization.

![Screenshot](/docs/images/screenshot1.png?raw=true)

## Hotkeys

### File Operations
- **Ctrl+N** - New MDL file
- **Ctrl+O** - Open MDL file
- **Ctrl+S** - Save MDL file (quick save)
- **Ctrl+Shift+S** - Save As MDL file
- **Ctrl+Q** - Exit application

### Emitter Operations
- **Shift+A** - Add new default emitter
- **Shift+D** - Duplicate selected emitter
- **X** - Delete selected emitter

### Camera Controls
- **Middle Mouse** - Rotate camera around target
- **Shift+Middle Mouse** - Pan camera
- **Mouse Wheel** - Zoom in/out

## License

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option)
any later version.

## Acknowledgments

- **Neverwinter Vault Discord Community** - Support and feedback and for keeping this game alive
- **Sean Barrett** - STB Image library (https://github.com/nothings)
- **Omar Cornut** - Dear ImGui (https://github.com/ocornut)
- **G-Truc Creation** - GLM Mathematics Library (https://github.com/g-truc/glm)
- **Marcus Geelnard & Camilla Löwy and all contributors** - GLFW (https://www.glfw.org)

## Building

This project uses CMake and requires OpenGL, GLFW, and GLM. Build with:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
