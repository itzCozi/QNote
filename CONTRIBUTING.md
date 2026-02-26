# Contributing to QNote

Thank you for your interest in contributing to QNote! This document provides guidelines and information about contributing.

## Getting Started

### Prerequisites

- **Windows 10/11** (development is Windows-only)
- **MinGW-w64** with GCC 13+ (MSYS2 recommended)
- **CMake** 3.16 or higher
- **Git**

### Setting Up the Development Environment

1. Clone the repository:
   ```bash
   git clone https://github.com/itzcozi/qnote.git
   cd qnote
   ```

2. Install dependencies (using MSYS2):
   ```bash
   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake
   ```

3. Build the project:
   ```bash
   cd scripts
   ./build.bat
   ```

## How to Contribute

### Reporting Bugs

1. Check if the bug has already been reported in [Issues](https://github.com/itzcozi/qnote/issues)
2. If not, create a new issue using the **Bug Report** template
3. Include as much detail as possible: steps to reproduce, expected vs actual behavior, screenshots

### Suggesting Features

1. Check existing issues and discussions for similar suggestions
2. Create a new issue using the **Feature Request** template
3. Explain the use case and how it benefits users

### Submitting Code

1. **Fork** the repository
2. Create a **feature branch** from `main`:
   ```bash
   git checkout -b feature/your-feature-name
   ```
3. Make your changes
4. **Test** your changes locally
5. **Commit** with clear, descriptive messages:
   ```bash
   git commit -m "Add: description of what you added"
   git commit -m "Fix: description of what you fixed"
   ```
6. **Push** to your fork:
   ```bash
   git push origin feature/your-feature-name
   ```
7. Open a **Pull Request** against `main`

## Code Style

### C++ Guidelines

- Use **C++17** features where appropriate
- Follow existing code formatting (4-space indentation)
- Use descriptive variable and function names
- Add comments for complex logic
- Keep functions focused and reasonably sized
- Use `const` and `noexcept` where applicable

### Commit Messages

- Start with a verb: Add, Fix, Update, Remove, Refactor
- Keep the first line under 72 characters
- Reference issues when applicable: `Fix: crash on startup (#123)`

### File Organization

```
src/
├── app/        # Main application (entry point, main window)
├── core/       # Core functionality (file I/O, settings, note storage)
├── ui/         # UI components (editor, tabs, dialogs)
└── resources/  # Windows resources (icons, menus, manifest)
```

## Pull Request Guidelines

- Keep PRs focused on a single feature or fix
- Update documentation if your change affects user-facing features
- Ensure the project builds without warnings
- Add screenshots for UI changes
- Be responsive to review feedback

## Testing

Before submitting:
1. Build in both Debug and Release modes
2. Test on Windows 10 and 11 if possible
3. Verify your changes don't break existing functionality
4. Test with different file encodings if your change involves file handling

## Questions?

- Open a [Discussion](https://github.com/itzcozi/qnote/discussions) for general questions
- Check existing documentation in the [README](README.md)

## License

By contributing to QNote, you agree that your contributions will be licensed under the [MIT License](LICENSE).
