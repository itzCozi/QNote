# QNote — A Better Notepad for Windows

<p align="center">
  <img src=".github/demo.gif" alt="QNote demo" width="720">
</p>

<p align="center">
  <a href="https://qnote.ar0.eu">Website</a> · <a href="https://qnote.ar0.eu/download">Download</a> · <a href="LICENSE">MIT License</a>
</p>

QNote is a free, open-source notepad replacement for Windows. Small footprint, native UI, zero telemetry — with the editing features you actually want.

## Features

- Tabbed editing with multi-level undo/redo
- VS Code-style line editing — cut/copy line, move/duplicate lines, smart home, block indent
- Find & replace with regex support
- Built-in notes system with quick capture, pinning, and full-text search
- Bookmarks, line numbers, show whitespace, zoom
- Text tools — sort, trim, join, split, case conversion, URL/Base64 encode, JSON format
- UTF-8, UTF-16, ANSI encodings · CRLF/LF/CR line endings
- Auto-save, drag-and-drop, print, dark title bar, customisable shortcuts
- Advanced printing with headers/footers, page numbers, print preview, and PDF export
- Portable mode available

## Building

### Prerequisites

- **Visual Studio 2019+** with C++ Desktop Development workload
- **CMake 3.16+**
- **Inno Setup 6** *(optional — for creating the installer)*

### Quick Build

```batch
cd scripts
build.bat release
```

The executable is placed in the project root and `build/` folder.

### Manual Build

```batch
mkdir build && cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

### Creating a Release

```batch
cd scripts
release.bat 1.0.0
```

Produces `QNote-1.0.0-Portable.zip` and `QNote-1.0.0-Setup.exe` (if Inno Setup is installed).

## Author

Cooper Ransom

## License

[MIT](LICENSE)


*Tiny. Quick. Simple. Efficient.*  
<sub>Never anything more. Never anything less.</sub>