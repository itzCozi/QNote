@echo off
::==============================================================================
:: QNote - Release Script
:: Builds, packages portable ZIP, and optionally creates installer
:: Usage: release.bat [version]
:: Example: release.bat 1.0.0
::==============================================================================

setlocal enabledelayedexpansion

:: Navigate to project root (parent of scripts folder)
cd /d "%~dp0.."

set "VERSION=1.0.0"
if not "%~1"=="" set "VERSION=%~1"

echo.
echo ============================================================
echo  QNote Release Script v%VERSION%
echo ============================================================
echo.

:: ----------------------------------------------------------------
:: Step 1: Build Release
:: ----------------------------------------------------------------
echo [1/3] Building QNote...
call scripts\build.bat release
if errorlevel 1 (
    echo ERROR: Build failed.
    exit /b 1
)

:: Verify build succeeded
if not exist "build\QNote.exe" (
    echo ERROR: build\QNote.exe not found after build.
    exit /b 1
)

echo   Build successful.

:: ----------------------------------------------------------------
:: Step 2: Create Portable ZIP
:: ----------------------------------------------------------------
echo.
echo [2/3] Creating portable ZIP...

:: Create dist directory
if not exist "dist" mkdir "dist"

set "PORTABLE_DIR=dist\QNote-Portable"
if exist "%PORTABLE_DIR%" rmdir /s /q "%PORTABLE_DIR%"
mkdir "%PORTABLE_DIR%"

copy /y "build\QNote.exe" "%PORTABLE_DIR%\" >nul
copy /y "LICENSE" "%PORTABLE_DIR%\" >nul
copy /y "README.md" "%PORTABLE_DIR%\" >nul

:: Use PowerShell to create ZIP (available on Windows 10+)
set "ZIP_FILE=dist\QNote-%VERSION%-Portable.zip"
if exist "%ZIP_FILE%" del /q "%ZIP_FILE%"

powershell -NoProfile -Command "Compress-Archive -Path '%PORTABLE_DIR%\*' -DestinationPath '%ZIP_FILE%' -Force"
if errorlevel 1 (
    echo WARNING: Failed to create ZIP. PowerShell may not be available.
) else (
    echo   Created: %ZIP_FILE%
    for %%A in ("%ZIP_FILE%") do (
        set "SIZE=%%~zA"
        set /a "SIZE_KB=SIZE/1024"
        echo   Size: !SIZE_KB! KB
    )
)

:: Clean up temp portable dir
rmdir /s /q "%PORTABLE_DIR%"

:: ----------------------------------------------------------------
:: Step 3: Create Installer (optional)
:: ----------------------------------------------------------------
echo.
echo [3/3] Creating installer...

:: Find Inno Setup compiler
set "ISCC="
if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" (
    set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
)
if not defined ISCC if exist "%ProgramFiles%\Inno Setup 6\ISCC.exe" (
    set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
)

if not defined ISCC (
    echo WARNING: Inno Setup 6 not found. Skipping installer creation.
    echo   Download from: https://jrsoftware.org/isdl.php
    echo   Then re-run this script.
    goto :done
)

echo   Found Inno Setup: %ISCC%
"%ISCC%" /Q "installer\QNote.iss"
if errorlevel 1 (
    echo ERROR: Inno Setup compilation failed.
    exit /b 1
)

if exist "dist\QNote-%VERSION%-Setup.exe" (
    echo   Created: dist\QNote-%VERSION%-Setup.exe
    for %%A in ("dist\QNote-%VERSION%-Setup.exe") do (
        set "SIZE=%%~zA"
        set /a "SIZE_KB=SIZE/1024"
        echo   Size: !SIZE_KB! KB
    )
)

:done
echo.
echo ============================================================
echo  Release v%VERSION% ready!
echo ============================================================
echo.
echo Files to upload to GitHub Releases:
echo.
if exist "build\QNote.exe" (
    echo   build\QNote.exe
    for %%A in ("build\QNote.exe") do (
        set "SIZE=%%~zA"
        set /a "SIZE_KB=SIZE/1024"
        echo     Size: !SIZE_KB! KB
    )
)
if exist "dist\QNote-%VERSION%-Portable.zip" (
    echo   dist\QNote-%VERSION%-Portable.zip
)
if exist "dist\QNote-%VERSION%-Setup.exe" (
    echo   dist\QNote-%VERSION%-Setup.exe
)
echo.
echo SHA256 checksums (for release notes):
echo.
if exist "build\QNote.exe" (
    for /f "skip=1" %%H in ('certutil -hashfile "build\QNote.exe" SHA256') do (
        if not "%%H"=="CertUtil:" echo   QNote.exe: %%H
        goto :hash2
    )
)
:hash2
if exist "dist\QNote-%VERSION%-Portable.zip" (
    for /f "skip=1" %%H in ('certutil -hashfile "dist\QNote-%VERSION%-Portable.zip" SHA256') do (
        if not "%%H"=="CertUtil:" echo   Portable.zip: %%H
        goto :hash3
    )
)
:hash3
if exist "dist\QNote-%VERSION%-Setup.exe" (
    for /f "skip=1" %%H in ('certutil -hashfile "dist\QNote-%VERSION%-Setup.exe" SHA256') do (
        if not "%%H"=="CertUtil:" echo   Setup.exe: %%H
        goto :hashend
    )
)
:hashend
echo.
echo Next steps:
echo   1. Go to https://github.com/itzcozi/qnote/releases/new
echo   2. Tag: v%VERSION%
echo   3. Upload the files listed above
echo   4. Publish!
echo.

exit /b 0
