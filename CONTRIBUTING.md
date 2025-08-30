# Contributing to NWN Emitter Editor

Thank you for your interest in contributing to the NWN Emitter Editor! We welcome contributions that help improve this
tool for the Neverwinter Nights community.

## Code Formatting

**Always use the included `.clang-format` file for formatting code.** This ensures consistent code style across the
project. Before submitting any changes, make sure to format your code:

```bash
clang-format -i src/*.cpp include/*.hpp
```

## License Requirements

All modifications must be made under GPL3 as the project itself is GPL3. By contributing to this project, you agree that
your contributions will be licensed under the same terms.

## Feature Pull Requests

Feature PRs are especially welcome for the following planned features that will help turn this editor into a
fully-fledged VFX toolkit:

### Pull Request Guidelines

- Provide a clear description of what your changes do
- Always test your build before submitting a PR

### Potential Features the author had in mind for the future

- Timeline System - Animation timeline for keyframe editing and preview
- Nicer File Dialogs
- Geometry Rendering
- Particle system presets and templates
- Advanced texture management
- Performance optimizations
- Additional emitter types and effects
- Undo/redo system
- Improved camera controls
- Better visualization tools

## Getting Started

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/your-feature-name`
3. Make your changes following the code style guidelines
4. Test your changes thoroughly
5. Format your code with clang-format
6. Commit your changes with a clear commit message
7. Push to your fork and submit a pull request
