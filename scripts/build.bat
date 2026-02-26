@echo off
::==============================================================================
:: QNote - Build Script for MSVC
:: Usage: build.bat [debug|release] [clean]
::==============================================================================

setlocal enabledelayedexpansion

:: Navigate to project root (parent of scripts folder)
cd /d "%~dp0.."

:: Configuration
set "BUILD_TYPE=Release"
set "CLEAN=0"

:: Parse arguments
:parse_args
if "%~1"=="" goto :done_args
if /i "%~1"=="debug" (
    set "BUILD_TYPE=Debug"
    shift
    goto :parse_args
)
if /i "%~1"=="release" (
    set "BUILD_TYPE=Release"
    shift
    goto :parse_args
)
if /i "%~1"=="clean" (
    set "CLEAN=1"
    shift
    goto :parse_args
)
shift
goto :parse_args
:done_args

echo.
echo ============================================================
echo  QNote Build Script
echo  Build Type: %BUILD_TYPE%
echo ============================================================
echo.

:: Find Visual Studio
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe not found. Please install Visual Studio 2019 or later.
    exit /b 1
)

:: Get VS installation path
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_PATH=%%i"
)

if not defined VS_PATH (
    echo ERROR: Visual Studio with C++ tools not found.
    exit /b 1
)

echo Found Visual Studio at: %VS_PATH%
echo.

:: Set up Visual Studio environment for x64
call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo ERROR: Failed to set up Visual Studio environment.
    exit /b 1
)

:: Clean old build folder and root exe
set "BUILD_DIR=build"
echo Cleaning old build...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
if exist "QNote.exe" del /q "QNote.exe"

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

:: Run CMake configure
echo Configuring with CMake...
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ..
if errorlevel 1 (
    echo ERROR: CMake configuration failed.
    cd ..
    exit /b 1
)

:: Build
echo.
echo Building QNote...
cmake --build . --config %BUILD_TYPE%
if errorlevel 1 (
    echo ERROR: Build failed.
    cd ..
    exit /b 1
)

cd ..

:: Compress with UPX for release builds (if available)
if /i "%BUILD_TYPE%"=="Release" (
    set "UPX_EXE="
    where upx >nul 2>&1
    if not errorlevel 1 (
        set "UPX_EXE=upx"
    ) else (
        :: Search common install locations
        for /f "delims=" %%P in ('dir /s /b "%LOCALAPPDATA%\Microsoft\WinGet\Packages\upx*.exe" 2^>nul') do set "UPX_EXE=%%P"
        if not defined UPX_EXE if exist "%ProgramFiles%\upx\upx.exe" set "UPX_EXE=%ProgramFiles%\upx\upx.exe"
    )
    if defined UPX_EXE (
        echo.
        echo Compressing with UPX...
        "!UPX_EXE!" --best --lzma "build\QNote.exe" >nul 2>&1
        if not errorlevel 1 (
            echo   UPX compression applied.
        ) else (
            echo   UPX compression failed, using uncompressed binary.
        )
    ) else (
        echo   UPX not found - install with: winget install upx.upx
    )
)

:: Copy exe to root folder (keep original in build directory)
echo Copying QNote.exe to root folder...
if exist "build\QNote.exe" (
    copy /y "build\QNote.exe" "QNote.exe" >nul
    echo Original executable remains in build\QNote.exe
) else (
    echo Warning: QNote.exe not found in build directory
)

echo.
echo ============================================================
echo  Build completed successfully!
echo  Output: QNote.exe (copy in root folder)
echo  Original: build\QNote.exe (kept in build directory)
echo ============================================================
echo.

:: Show file size
if exist "QNote.exe" (
    for %%A in ("QNote.exe") do (
        set "SIZE=%%~zA"
        set /a "SIZE_KB=SIZE/1024"
        echo Executable size: !SIZE_KB! KB
    )
)

exit /b 0
