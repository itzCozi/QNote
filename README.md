# QNote

A lightweight, open-source Notepad replacement for Windows 10/11.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-blue.svg)
![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)

## Features

- **Lightweight**: Pure Win32 API, no frameworks, small binary size (~150KB)
- **Fast**: Near-instant startup, efficient text handling
- **Familiar**: Classic Notepad UI that feels right at home
- **Encoding Support**: Auto-detect and convert between UTF-8, UTF-8 BOM, UTF-16 LE/BE, and ANSI
- **Find & Replace**: With regular expression support via `std::regex`
- **Modern Features**: Dark mode support, drag-and-drop, recent files, zoom
- **Portable**: Single executable, no installer required
- **Zero Dependencies**: Uses only Windows system libraries

## Requirements

- Windows 10 version 1809 or later (Windows 11 supported)
- No runtime dependencies (CRT statically linked)

## Installation

### Option 1: Download Release

Download the latest `QNote.exe` from the [Releases](../../releases) page.

### Option 2: Build from Source

See [Building](#building) section below.

## Usage

### Basic Usage

```
QNote.exe [filename]
```

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+N | New file |
| Ctrl+O | Open file |
| Ctrl+S | Save |
| Ctrl+Shift+S | Save As |
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
| Ctrl+X | Cut |
| Ctrl+C | Copy |
| Ctrl+V | Paste |
| Ctrl+A | Select All |
| Ctrl+F | Find |
| F3 | Find Next |
| Ctrl+H | Replace |
| Ctrl+G | Go To Line |
| F5 | Insert Date/Time |
| Ctrl++ | Zoom In |
| Ctrl+- | Zoom Out |
| Ctrl+0 | Reset Zoom |

### Settings

Settings are stored in `%APPDATA%\QNote\config.ini` and include:

- Window position and size
- Font settings
- Word wrap preference
- Zoom level
- Recent files list
- Search options

## Building

### Prerequisites

- **Visual Studio 2019/2022** with C++ Desktop Development workload, or
- **MinGW-w64** with GCC 8.0 or later
- **CMake** 3.16 or later

### Building with Visual Studio (Recommended)

#### Using Visual Studio IDE

1. Open the folder in Visual Studio 2019/2022
2. CMake will configure automatically
3. Select Release or Debug configuration
4. Build → Build All

#### Using Developer Command Prompt

```batch
# Quick build
build.bat release

# Or with CMake directly
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```

### Building with MinGW-w64

```bash
mkdir build
cd build
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

### Build Output

The compiled executable will be at:
- `build/QNote.exe` (MSVC NMake)
- `build/Release/QNote.exe` (MSVC Visual Studio)
- `build/QNote.exe` (MinGW)

### Static CRT Linking

The CMake configuration automatically links the C runtime statically for release builds:
- **MSVC**: Uses `/MT` flag
- **MinGW**: Uses `-static -static-libgcc -static-libstdc++`

This ensures the executable runs on any Windows 10/11 system without requiring Visual C++ Redistributables.

## Project Structure

```
QNote/
├── src/
│   ├── main.cpp          # WinMain entry point
│   ├── MainWindow.cpp/h  # Main window, menus, commands
│   ├── Editor.cpp/h      # Edit control wrapper
│   ├── Dialogs.cpp/h     # Find/Replace, GoTo dialogs
│   ├── Settings.cpp/h    # INI-based settings persistence
│   ├── FileIO.cpp/h      # File I/O with encoding support
│   ├── resource.h        # Resource identifiers
│   ├── resource.rc       # Menus, accelerators, dialogs
│   └── QNote.exe.manifest  # DPI awareness, visual styles
├── CMakeLists.txt        # CMake build configuration
├── build.bat             # Quick build script for MSVC
├── LICENSE               # MIT License
└── README.md             # This file
```

## Technical Details

### Encoding Detection

QNote automatically detects file encoding using:
1. BOM (Byte Order Mark) detection for UTF-8, UTF-16 LE/BE
2. UTF-8 validity checking for BOM-less UTF-8
3. Heuristic null-byte pattern detection for BOM-less UTF-16
4. Fallback to ANSI (Windows codepage)

### Line Endings

Supports all three line ending formats:
- **CRLF** (Windows): `\r\n`
- **LF** (Unix/Linux/macOS): `\n`
- **CR** (Classic Mac): `\r`

Auto-detects on open and can convert between formats via the Format menu.

### Dark Mode

Automatically follows Windows system theme using:
- Registry key: `HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize\AppsUseLightTheme`
- DWM API: `DwmSetWindowAttribute` with `DWMWA_USE_IMMERSIVE_DARK_MODE`

### Limitations

- Maximum file size: 256 MB (practical limit of standard Edit control)
- No syntax highlighting (intentional - keeping it simple like Notepad)
- Single-level undo only (Windows Edit control limitation)

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Ensure it compiles with both MSVC and MinGW
5. Submit a pull request

### Code Style

- Use C++17 features where appropriate
- Follow existing naming conventions (PascalCase for types, camelCase for variables)
- Comment Win32 API quirks and non-obvious behavior
- RAII for all resources (handles, GDI objects)
- Unicode throughout (wchar_t, std::wstring)

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Acknowledgments

- Inspired by the simplicity of Windows Notepad
- Built with pure Win32 API for maximum compatibility and small size
- Uses `std::regex` for find/replace functionality
