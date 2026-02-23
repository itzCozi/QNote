//==============================================================================
// QNote - A Lightweight Notepad Clone
// main.cpp - Application entry point
//==============================================================================
//
// QNote is a lightweight, open-source text editor for Windows 10/11.
// It implements core Notepad functionality using pure Win32 API and C++17.
//
// Features:
// - File operations with encoding detection (UTF-8, UTF-16, ANSI)
// - Find and Replace with regex support
// - Word wrap, zoom, and font customization
// - Dark mode support
// - Recent files list
// - Persistent settings
//
// Copyright (c) 2026 - MIT License
//
//==============================================================================

#include <Windows.h>
#include <shellapi.h>
#include <objbase.h>
#include "MainWindow.h"
#include "Editor.h"

// Enable visual styles
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Link required libraries
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "advapi32.lib")

//------------------------------------------------------------------------------
// Check if this is the application's first instance and handle single-instance
// behavior. Returns false if another instance is already running.
//------------------------------------------------------------------------------
static bool CheckSingleInstance() {
    // Create a named mutex for single-instance check
    // Note: Allowing multiple instances for now (like Notepad)
    return true;
}

//------------------------------------------------------------------------------
// Parse command line to get the file to open
//------------------------------------------------------------------------------
static std::wstring ParseCommandLine() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    
    std::wstring filePath;
    
    if (argv && argc > 1) {
        // Get the first argument as the file path
        filePath = argv[1];
        
        // Handle quoted paths
        if (!filePath.empty() && filePath.front() == L'"') {
            filePath = filePath.substr(1);
        }
        if (!filePath.empty() && filePath.back() == L'"') {
            filePath.pop_back();
        }
        
        // If it's a relative path, make it absolute
        if (!filePath.empty() && filePath[0] != L'\\' && 
            (filePath.length() < 2 || filePath[1] != L':')) {
            wchar_t currentDir[MAX_PATH];
            if (GetCurrentDirectoryW(MAX_PATH, currentDir)) {
                std::wstring fullPath = currentDir;
                fullPath += L"\\";
                fullPath += filePath;
                
                wchar_t absolutePath[MAX_PATH];
                if (GetFullPathNameW(fullPath.c_str(), MAX_PATH, absolutePath, nullptr)) {
                    filePath = absolutePath;
                }
            }
        }
    }
    
    if (argv) {
        LocalFree(argv);
    }
    
    return filePath;
}

//------------------------------------------------------------------------------
// Enable high DPI awareness
//------------------------------------------------------------------------------
static void EnableHighDPI() {
    // Try SetProcessDpiAwarenessContext (Windows 10 1703+)
    using SetProcessDpiAwarenessContextFunc = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    
    if (hUser32) {
        auto pSetProcessDpiAwarenessContext = reinterpret_cast<SetProcessDpiAwarenessContextFunc>(
            GetProcAddress(hUser32, "SetProcessDpiAwarenessContext"));
        
        if (pSetProcessDpiAwarenessContext) {
            pSetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            return;
        }
    }
    
    // Fallback: Try SetProcessDpiAwareness (Windows 8.1+)
    HMODULE hShcore = LoadLibraryW(L"shcore.dll");
    if (hShcore) {
        using SetProcessDpiAwarenessFunc = HRESULT(WINAPI*)(int);
        auto pSetProcessDpiAwareness = reinterpret_cast<SetProcessDpiAwarenessFunc>(
            GetProcAddress(hShcore, "SetProcessDpiAwareness"));
        
        if (pSetProcessDpiAwareness) {
            pSetProcessDpiAwareness(2);  // PROCESS_PER_MONITOR_DPI_AWARE
        }
        
        FreeLibrary(hShcore);
    }
}

//------------------------------------------------------------------------------
// Application entry point
//------------------------------------------------------------------------------
int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    
    // Check for single instance (optional - currently allowing multiple instances)
    if (!CheckSingleInstance()) {
        return 0;
    }
    
    // Enable high DPI
    EnableHighDPI();
    
    // Initialize COM (needed for file dialogs on some Windows versions)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) {
        // COM init failed, but we can continue without it
    }
    
    // Initialize RichEdit library for multi-level undo/redo
    if (!QNote::Editor::InitializeRichEdit()) {
        MessageBoxW(nullptr, L"Failed to initialize RichEdit library.", L"Error", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }
    
    // Parse command line for initial file
    std::wstring initialFile = ParseCommandLine();
    
    // Create and run main window
    QNote::MainWindow mainWindow;
    
    if (!mainWindow.Create(hInstance, nCmdShow, initialFile)) {
        CoUninitialize();
        return 1;
    }
    
    int result = mainWindow.Run();
    
    // Cleanup
    QNote::Editor::UninitializeRichEdit();
    CoUninitialize();
    
    return result;
}

//------------------------------------------------------------------------------
// Alternative entry point for console builds (debugging)
//------------------------------------------------------------------------------
#ifdef _DEBUG
int wmain(int argc, wchar_t* argv[]) {
    return wWinMain(GetModuleHandleW(nullptr), nullptr, GetCommandLineW(), SW_SHOWDEFAULT);
}
#endif
