//==============================================================================
// QNote - A Lightweight Notepad Clone
// MainWindow.h - Main window management
//==============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Commdlg.h>
#include <shellapi.h>
#include <string>
#include <memory>
#include <algorithm>
#include "Settings.h"
#include "Editor.h"
#include "FileIO.h"
#include "Dialogs.h"

namespace QNote {

//------------------------------------------------------------------------------
// Main window class
//------------------------------------------------------------------------------
class MainWindow {
public:
    MainWindow();
    ~MainWindow();
    
    // Prevent copying
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;
    
    // Initialize and create the main window
    bool Create(HINSTANCE hInstance, int nCmdShow, const std::wstring& initialFile = L"");
    
    // Run the message loop
    int Run();
    
    // Get window handle
    HWND GetHandle() const { return m_hwnd; }
    
private:
    // Window procedure
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Message handlers
    void OnCreate();
    void OnDestroy();
    void OnClose();
    void OnSize(int width, int height);
    void OnSetFocus();
    void OnCommand(WORD id, WORD code, HWND hwndCtl);
    void OnDropFiles(HDROP hDrop);
    void OnInitMenuPopup(HMENU hMenu, UINT index, BOOL sysMenu);
    void OnTimer(UINT_PTR timerId);
    LRESULT OnCtlColorEdit(HDC hdc, HWND hwndEdit);
    
    // File operations
    void OnFileNew();
    void OnFileNewWindow();
    void OnFileOpen();
    bool OnFileSave();
    bool OnFileSaveAs();
    void OnFileOpenRecent(int index);
    void OnFilePageSetup();
    void OnFilePrint();
    
    // Edit operations
    void OnEditUndo();
    void OnEditRedo();
    void OnEditCut();
    void OnEditCopy();
    void OnEditPaste();
    void OnEditDelete();
    void OnEditSelectAll();
    void OnEditFind();
    void OnEditFindNext();
    void OnEditReplace();
    void OnEditGoTo();
    void OnEditDateTime();
    
    // Format operations
    void OnFormatWordWrap();
    void OnFormatFont();
    void OnFormatTabSize();
    void OnFormatLineEnding(LineEnding ending);
    void OnFormatRTL();
    
    // View operations
    void OnViewStatusBar();
    void OnViewZoomIn();
    void OnViewZoomOut();
    void OnViewZoomReset();
    void OnViewTheme(ThemeMode mode);
    
    // Encoding operations
    void OnEncodingChange(TextEncoding encoding);
    
    // Help operations
    void OnHelpAbout();
    
    // Helper methods
    void UpdateTitle();
    void UpdateStatusBar();
    void UpdateMenuState();
    void UpdateRecentFilesMenu();
    bool PromptSaveChanges();
    bool LoadFile(const std::wstring& filePath);
    bool SaveFile(const std::wstring& filePath);
    void NewDocument();
    
    // Dark mode support
    bool IsSystemDarkMode();
    void ApplyDarkMode(bool darkMode);
    bool ShouldUseDarkMode();
    
    // Create child controls
    void CreateStatusBar();
    void ResizeControls();
    
private:
    HWND m_hwnd = nullptr;
    HWND m_hwndStatus = nullptr;
    HINSTANCE m_hInstance = nullptr;
    HACCEL m_hAccel = nullptr;
    
    // Core components
    std::unique_ptr<SettingsManager> m_settingsManager;
    std::unique_ptr<Editor> m_editor;
    std::unique_ptr<DialogManager> m_dialogManager;
    
    // Current file state
    std::wstring m_currentFile;
    bool m_isNewFile = true;
    
    // Print settings
    PAGESETUPDLGW m_pageSetup = {};
    
    // Dark mode
    bool m_darkMode = false;
    HBRUSH m_darkBgBrush = nullptr;
    
    // Status bar parts widths
    static constexpr int STATUS_PARTS = 4;
    int m_statusPartWidths[STATUS_PARTS] = { 200, 100, 80, -1 };
    
    // Window class name
    static constexpr wchar_t WINDOW_CLASS[] = L"QNoteMainWindow";
};

} // namespace QNote
