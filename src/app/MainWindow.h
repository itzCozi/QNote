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
#include "NoteStore.h"
#include "CaptureWindow.h"
#include "NoteListWindow.h"
#include "FindBar.h"
#include "LineNumbersGutter.h"

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
    [[nodiscard]] bool Create(HINSTANCE hInstance, int nCmdShow, const std::wstring& initialFile = L"");
    
    // Run the message loop
    [[nodiscard]] int Run();
    
    // Get window handle
    [[nodiscard]] HWND GetHandle() const noexcept { return m_hwnd; }
    
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
    void OnViewLineNumbers();
    void OnViewZoomIn();
    void OnViewZoomOut();
    void OnViewZoomReset();
    
    // Encoding operations
    void OnEncodingChange(TextEncoding encoding);
    
    // Help operations
    void OnHelpAbout();
    
    // Notes operations
    void OnNotesNew();
    void OnNotesQuickCapture();
    void OnNotesAllNotes();
    void OnNotesPinned();
    void OnNotesTimeline();
    void OnNotesSearch();
    void OnNotesPinCurrent();
    void OnNotesSaveNow();
    void OnNotesDeleteCurrent();
    void OnHotkey(int hotkeyId);
    
    // Helper methods
    void UpdateTitle();
    void UpdateStatusBar();
    void UpdateMenuState();
    void UpdateRecentFilesMenu();
    bool PromptSaveChanges();
    bool LoadFile(const std::wstring& filePath);
    bool SaveFile(const std::wstring& filePath);
    void NewDocument();
    
    // Note store operations
    void InitializeNoteStore();
    void AutoSaveCurrentNote();
    void LoadNoteIntoEditor(const Note& note);
    void OpenNoteFromId(const std::wstring& noteId);
    
    // File auto-save operations
    void AutoSaveFileBackup();
    void DeleteAutoSaveBackup();
    
    // Line numbers gutter callback
    static void OnEditorScroll(void* userData);
    
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
    std::unique_ptr<FindBar> m_findBar;
    std::unique_ptr<LineNumbersGutter> m_lineNumbersGutter;
    
    // Note store and windows
    std::unique_ptr<NoteStore> m_noteStore;
    std::unique_ptr<CaptureWindow> m_captureWindow;
    std::unique_ptr<NoteListWindow> m_noteListWindow;
    std::unique_ptr<GlobalHotkeyManager> m_hotkeyManager;
    
    // Current note state
    std::wstring m_currentNoteId;
    bool m_isNoteMode = false;  // true = editing from note store, false = file mode
    
    // Current file state
    std::wstring m_currentFile;
    bool m_isNewFile = true;
    
    // Print settings
    PAGESETUPDLGW m_pageSetup = {};
    
    // Status bar parts widths
    static constexpr int STATUS_PARTS = 4;
    int m_statusPartWidths[STATUS_PARTS] = { 200, 100, 80, -1 };
    
    // Auto-save timer interval (ms)
    static constexpr UINT AUTOSAVE_INTERVAL = 3000;
    
    // File auto-save timer interval (ms) - every 30 seconds
    static constexpr UINT FILEAUTOSAVE_INTERVAL = 30000;
    
    // Window class name
    static constexpr wchar_t WINDOW_CLASS[] = L"QNoteMainWindow";
};

} // namespace QNote
