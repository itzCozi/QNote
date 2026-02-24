# QNote Release Build Guide

## Prerequisites

1. **Visual Studio 2019+** with C++ Desktop Development workload
2. **CMake 3.16+** (usually included with VS)
3. **Inno Setup 6** (optional, for installer) — [Download](https://jrsoftware.org/isdl.php)

## Building a Release

```batch
release.bat 1.0.0
```

Replace `1.0.0` with your version number.

### What It Does

| Step | Action | Output |
|------|--------|--------|
| 1 | Builds Release binary | QNote.exe |
| 2 | Creates portable ZIP | QNote-1.0.0-Portable.zip |
| 3 | Creates installer | QNote-1.0.0-Setup.exe |
| 4 | Generates SHA256 checksums | Displayed in console |

## Publishing to GitHub Releases

1. Go to **https://github.com/itzcozi/qnote/releases/new**
2. Click **"Choose a tag"** → type `v1.0.0` → **"Create new tag"**
3. Title: `QNote v1.0.0`
4. Drag and drop these files:
   - QNote-1.0.0-Portable.zip
   - QNote-1.0.0-Setup.exe
5. Paste the SHA256 checksums from the console into the release notes
6. Click **"Publish release"**

## Updating Version Number

Before a new release, update the version in these files:

| File | Line | Value |
|------|------|-------|
| CMakeLists.txt | Line 9 | `VERSION 1.0.0` |
| resource.h | Line 10 | `#define APP_VERSION L"1.0.0"` |
| QNote.iss | Line 12 | `#define MyAppVersion "1.0.0"` |

Then run `release.bat 1.0.0` with your new version.
