# QNote - A Better Notepad for Windows

**MAKE DEMO AND PUT HERE**

[Website](https://qnote.ar0.eu) | [Download](https://qnote.ar0.eu/download)

## Features

- **Tabbed Interface** — Work with multiple files simultaneously (Ctrl+T to open new tab, Ctrl+W to close)
- **Notes System** — Quick capture notes with global hotkey, pin important notes, timeline view, and full-text search
- **Multi-level Undo/Redo** — Powered by RichEdit control with unlimited history
- **Find & Replace** — Full regex support, match case, wrap around, direction control
- **Bookmarks** — Toggle (F2), navigate (Ctrl+F2 / Shift+F2), and clear all bookmarks
- **Text Operations** — Uppercase/lowercase, sort lines, trim whitespace, remove duplicates
- **Tools** — URL encode/decode, Base64 encode/decode, JSON format/minify, word count, Lorem ipsum generator
- **Line Numbers** — Optional gutter with line numbers
- **Encodings** — UTF-8, UTF-8 with BOM, UTF-16 LE/BE, ANSI (save and reopen with any encoding)
- **Line Endings** — Windows (CRLF), Unix (LF), Mac (CR)
- **Zoom** — Ctrl+Plus/Minus/0 to zoom in, out, or reset
- **Word Wrap** — Toggle from Format menu
- **Auto-Save** — Configurable interval for automatic saving
- **Dark Mode** — Windows 10/11 dark title bar support
- **Customizable Shortcuts** — Edit keyboard shortcuts from Tools menu
- **Drag & Drop** — Open files by dropping them onto the window
- **Print Support** — Page setup and print dialogs

## Keyboard Shortcuts

| Key | Action | Key | Action |
| --- | --- | --- | --- |
| **Ctrl+N** | New file | **Ctrl+T** | New tab |
| **Ctrl+O** | Open file | **Ctrl+W** | Close tab |
| **Ctrl+S** | Save | **Ctrl+Tab** | Next tab |
| **Ctrl+Shift+S** | Save As | **Ctrl+Shift+Tab** | Previous tab |
| **Ctrl+Shift+N** | New window | **Ctrl+P** | Print |
| **Ctrl+Z** | Undo | **Ctrl+Y** | Redo |
| **Ctrl+X/C/V** | Cut / Copy / Paste | **Ctrl+A** | Select All |
| **Ctrl+F** | Find | **Ctrl+H** | Replace |
| **Ctrl+G** | Go to line | **F3** | Find next |
| **F5** | Insert date/time | **F2** | Toggle bookmark |
| **Ctrl+F2** | Next bookmark | **Shift+F2** | Previous bookmark |
| **Ctrl+Shift+F2** | Clear all bookmarks | | |
| **Ctrl++** | Zoom in | **Ctrl+-** | Zoom out |
| **Ctrl+0** | Reset zoom | | |
| **Ctrl+Shift+Q** | Quick capture note | **Ctrl+Shift+A** | All notes |
| **Ctrl+Shift+F** | Search notes | | |

## Building

### Prerequisites

- **Visual Studio 2019+** with C++ Desktop Development workload, or **MinGW-w64**
- **CMake 3.16+**
- **Inno Setup 6** (optional, for creating installer)

### Quick Build (MSVC)

```batch
cd scripts
build.bat release
```

The executable will be placed in the `build` folder.

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

This creates `QNote-1.0.0-Portable.zip` and `QNote-1.0.0-Setup.exe` (if Inno Setup is installed).

## License

[MIT](LICENSE)
