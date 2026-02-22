# QNote

A lightweight Win32 Notepad replacement for Windows 10/11.

[Download Latest Release](https://www.google.com/search?q=../../releases)

## Features

* **Performance:** ~150KB binary, instant startup, zero dependencies.
* **Engine:** Pure Win32 API (no frameworks), statically linked CRT.
* **Modern:** Native Dark Mode, Regex search, Drag-and-Drop, Zoom.
* **Encoding:** Auto-detects/converts UTF-8 (BOM/no BOM), UTF-16 LE/BE, and ANSI.
* **Portable:** Single executable; settings saved in `%APPDATA%\QNote\config.ini`.

## Keyboard Shortcuts

| Key | Action | Key | Action |
| --- | --- | --- | --- |
| **Ctrl+N/O/S** | New / Open / Save | **Ctrl+F/H/G** | Find / Replace / Go To |
| **Ctrl+Z/Y** | Undo / Redo | **F3 / F5** | Find Next / Insert Date |
| **Ctrl+ +/-/0** | Zoom In / Out / Reset | **Ctrl+Shift+S** | Save As |

## Building

**Prerequisites:** Visual Studio 2019+ or MinGW-w64; CMake 3.16+.

### Quick Build (MSVC)

```batch
mkdir build && cd build
cmake .. -A x64
cmake --build . --config Release

```

### Quick Build (MinGW)

```bash
mkdir build && cd build
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
cmake --build .

```

*Note: Release builds automatically use static CRT linking (`/MT` or `-static`).*

## Technical Limits

* **Max Size:** 256 MB (Standard Win32 Edit control limit).
* **Undo:** Single-level undo.
* **Highlighting:** None (intentional simplicity).

## License

[MIT](https://www.google.com/search?q=LICENSE)
