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
#include "Settings.h"
#include "Editor.h"
#include "FileIO.h"
#include "Dialogs.h"
#include "NoteStore.h"
#include "CaptureWindow.h"
#include "NoteListWindow.h"
#include "FindBar.h"
#include "LineNumbersGutter.h"
#include "TabBar.h"
#include "DocumentManager.h"
#include "SettingsWindow.h"
#include "PrintPreviewWindow.h"

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
    // posX/posY: optional window position override (CW_USEDEFAULT to ignore)
    [[nodiscard]] bool Create(HINSTANCE hInstance, int nCmdShow, const std::wstring& initialFile = L"",
                              int posX = CW_USEDEFAULT, int posY = CW_USEDEFAULT);
    
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
    
    // Tab operations
    void OnTabNew();
    void OnTabClose();
    void OnTabSelected(int tabId);
    void OnTabCloseRequested(int tabId);
    void OnTabRenamed(int tabId);
    void OnTabPinToggled(int tabId);
    void OnTabCloseOthers(int tabId);
    void OnTabCloseAll();
    void OnTabCloseToRight(int tabId);
    void OnTabDetached(int tabId);
    bool PromptSaveTab(int tabId);

    // File operations
    void OnFileNew();
    void OnFileNewWindow();
    void OnFileOpen();
    bool OnFileSave();
    bool OnFileSaveAs();
    void OnFileOpenRecent(int index);
    void OnFilePageSetup();
    void OnFilePrint();
    void OnFileSaveAll();
    void OnFileCloseAll();
    void OnFileOpenFromClipboard();
    
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
    void OnFormatScrollLines();
    void OnFormatLineEnding(LineEnding ending);
    void OnFormatRTL();
    
    // View operations
    void OnViewStatusBar();
    void OnViewLineNumbers();
    void OnViewZoomIn();
    void OnViewZoomOut();
    void OnViewZoomReset();
    void OnViewShowWhitespace();
    void OnViewAlwaysOnTop();
    void OnViewFullScreen();
    void OnViewToggleMenuBar();
    void OnViewSpellCheck();
    
    // Encoding operations
    void OnEncodingChange(TextEncoding encoding);
    void OnReopenWithEncoding(TextEncoding encoding);
    
    // Help operations
    void OnHelpAbout();
    void OnHelpCheckUpdate();
    
    // File operations (additional)
    void OnFileRevert();
    void OnFileOpenContainingFolder();
    
    // Text manipulation operations
    void OnEditUppercase();
    void OnEditLowercase();
    void OnEditTitleCase();
    void OnEditSortLines(bool ascending);
    void OnEditTrimWhitespace();
    void OnEditRemoveDuplicateLines();
    void OnEditReverseLines();
    void OnEditNumberLines();
    void OnEditToggleComment();
    
    // Bookmark operations
    void OnEditToggleBookmark();
    void OnEditNextBookmark();
    void OnEditPrevBookmark();
    void OnEditClearBookmarks();
    
    // Tools operations
    void OnToolsEditShortcuts();
    void OnToolsCopyFilePath();
    void OnToolsAutoSaveOptions();
    void OnToolsUrlEncode();
    void OnToolsUrlDecode();
    void OnToolsBase64Encode();
    void OnToolsBase64Decode();
    void OnToolsInsertLorem();
    void OnToolsSplitLines();
    void OnToolsTabsToSpaces();
    void OnToolsSpacesToTabs();
    void OnToolsWordCount();
    void OnToolsRemoveBlankLines();
    void OnToolsJoinLines();
    void OnToolsFormatJson();
    void OnToolsMinifyJson();
    void OnToolsOpenTerminal();
    void OnToolsSettings();
    void OnToolsCalculate();
    void OnToolsInsertGuid();
    void OnToolsInsertFilePath();
    void OnToolsConvertEolSelection();
    void OnToolsChecksum();
    void OnToolsRunSelection();
    
    // Tools dialog procs
    static INT_PTR CALLBACK AutoSaveDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK SplitLinesDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK ConvertEolDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK RunOutputDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
    
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
    void OnNotesExport();
    void OnNotesImport();
    void OnNotesFavorites();
    void OnNotesDuplicate();
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
    void LoadNoteIntoEditor(const NoteSummary& summary);
    void OpenNoteFromId(const std::wstring& noteId);
    
    // File auto-save operations
    void AutoSaveFileBackup();
    void DeleteAutoSaveBackup();
    
    // File change monitoring
    void StartFileMonitoring();
    void CheckFileChanged();
    FILETIME GetFileLastWriteTime(const std::wstring& path);
    
    // Session save/restore
    void SaveSession();
    void LoadSession();
    
    // Keyboard shortcuts
    void LoadKeyboardShortcuts();
    void CreateDefaultShortcutsFile(const std::wstring& path);
    WORD ParseVirtualKey(const std::wstring& keyName);
    std::wstring FormatAccelKey(BYTE fVirt, WORD key);
    
    // System tray
    void InitializeSystemTray();
    void CleanupSystemTray();
    void ShowTrayContextMenu();
    void MinimizeToTray();
    void RestoreFromTray();
    
    // Line numbers gutter callback
    static void OnEditorScroll(void* userData);
    
    // Update m_editor pointer and sub-component references on tab switch
    void UpdateActiveEditor();
    
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
    Editor* m_editor = nullptr;  // Points to active tab's editor (owned by DocumentManager)
    std::unique_ptr<DialogManager> m_dialogManager;
    std::unique_ptr<FindBar> m_findBar;
    std::unique_ptr<LineNumbersGutter> m_lineNumbersGutter;
    std::unique_ptr<TabBar> m_tabBar;
    std::unique_ptr<DocumentManager> m_documentManager;
    
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
    
    // System tray
    NOTIFYICONDATAW m_trayIcon = {};
    bool m_forceQuit = false;  // When true, OnClose always quits (bypasses minimize-to-tray)
    bool m_trayIconCreated = false;
    bool m_minimizedToTray = false;
    
    // Full screen state
    bool m_isFullScreen = false;
    LONG m_preFullScreenStyle = 0;
    RECT m_preFullScreenRect = {};
    HMENU m_savedMenu = nullptr;
    
    // File change monitoring
    FILETIME m_lastWriteTime = {};
    bool m_ignoreNextFileChange = false;
    
    // Status bar parts widths
    static constexpr int STATUS_PARTS = 5;
    int m_statusPartWidths[STATUS_PARTS] = { 200, 100, 80, 60, -1 };
    
    // Auto-save timer interval (ms)
    static constexpr UINT AUTOSAVE_INTERVAL = 3000;
    
    // File auto-save timer interval (ms) - every 30 seconds
    static constexpr UINT FILEAUTOSAVE_INTERVAL = 30000;
    
    // File change monitoring interval (ms) - every 2 seconds
    static constexpr UINT FILEWATCH_INTERVAL = 2000;
    
    // Window class name
    static constexpr wchar_t WINDOW_CLASS[] = L"QNoteMainWindow";
};

} // namespace QNote
