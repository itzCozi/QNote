//==============================================================================
// QNote - A Lightweight Notepad Clone
// MainWindow.cpp - Main window management implementation
//==============================================================================

#include "MainWindow.h"
#include "resource.h"
#include <CommCtrl.h>
#include <Commdlg.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <set>
#include <wincrypt.h>
#include <winhttp.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "Crypt32.lib")

// DWM dark title bar (Windows 10 1809+) - fallback for older SDKs
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

namespace QNote {

//------------------------------------------------------------------------------
// Helper: check if a string is empty or contains only whitespace
//------------------------------------------------------------------------------
static bool IsWhitespaceOnly(const std::wstring& text) {
    return text.find_first_not_of(L" \t\r\n") == std::wstring::npos;
}

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
MainWindow::MainWindow()
    : m_settingsManager(std::make_unique<SettingsManager>())
    , m_editor(std::make_unique<Editor>())
    , m_dialogManager(std::make_unique<DialogManager>())
    , m_findBar(std::make_unique<FindBar>())
    , m_lineNumbersGutter(std::make_unique<LineNumbersGutter>())
    , m_tabBar(std::make_unique<TabBar>())
    , m_documentManager(std::make_unique<DocumentManager>())
    , m_noteStore(std::make_unique<NoteStore>())
    , m_hotkeyManager(std::make_unique<GlobalHotkeyManager>())
{
}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
MainWindow::~MainWindow() {
}

//------------------------------------------------------------------------------
// Create the main window
//------------------------------------------------------------------------------
bool MainWindow::Create(HINSTANCE hInstance, int nCmdShow, const std::wstring& initialFile) {
    m_hInstance = hInstance;
    
    // Load settings
    (void)m_settingsManager->Load();
    
    // Initialize common controls
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);
    
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszMenuName = MAKEINTRESOURCEW(IDR_MAIN_MENU);
    wc.lpszClassName = WINDOW_CLASS;
    wc.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    
    if (!RegisterClassExW(&wc)) {
        MessageBoxW(nullptr, L"Failed to register window class.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    
    // Load accelerators
    m_hAccel = LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDR_ACCELERATORS));
    
    // Get settings
    const AppSettings& settings = m_settingsManager->GetSettings();
    
    // Create window
    int x = settings.windowX;
    int y = settings.windowY;
    int width = settings.windowWidth;
    int height = settings.windowHeight;
    
    // Validate position is on a visible monitor
    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    if (x != CW_USEDEFAULT && (x < workArea.left - width + 50 || x > workArea.right - 50)) {
        x = CW_USEDEFAULT;
    }
    if (y != CW_USEDEFAULT && (y < workArea.top - height + 50 || y > workArea.bottom - 50)) {
        y = CW_USEDEFAULT;
    }
    
    m_hwnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,  // Accept drag-drop
        WINDOW_CLASS,
        L"Untitled - QNote",
        WS_OVERLAPPEDWINDOW,
        x, y, width, height,
        nullptr,
        nullptr,
        hInstance,
        this  // Pass 'this' to WM_NCCREATE/WM_CREATE
    );
    
    if (!m_hwnd) {
        MessageBoxW(nullptr, L"Failed to create window.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    
    // Show window
    if (settings.windowMaximized) {
        ShowWindow(m_hwnd, SW_SHOWMAXIMIZED);
    } else {
        ShowWindow(m_hwnd, nCmdShow);
    }
    UpdateWindow(m_hwnd);
    
    // Load initial file if specified
    if (!initialFile.empty()) {
        LoadFile(initialFile);
    }
    
    return true;
}

//------------------------------------------------------------------------------
// Message loop
//------------------------------------------------------------------------------
int MainWindow::Run() {
    MSG msg = {};
    
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        // Check for FindBar messages first
        if (m_findBar && m_findBar->IsDialogMessage(&msg)) {
            continue;
        }
        
        // Check for dialog messages
        if (m_dialogManager->IsDialogMessage(&msg)) {
            continue;
        }
        
        // Translate accelerators
        if (m_hAccel && TranslateAcceleratorW(m_hwnd, m_hAccel, &msg)) {
            continue;
        }
        
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    
    return static_cast<int>(msg.wParam);
}

//------------------------------------------------------------------------------
// Window procedure (static)
//------------------------------------------------------------------------------
LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = nullptr;
    
    if (msg == WM_NCCREATE) {
        // Store 'this' pointer from create params
        auto* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<MainWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    
    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

//------------------------------------------------------------------------------
// Message handler
//------------------------------------------------------------------------------
LRESULT MainWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            OnCreate();
            return 0;
            
        case WM_DESTROY:
            OnDestroy();
            return 0;
            
        case WM_CLOSE:
            OnClose();
            return 0;
            
        case WM_SIZE:
            OnSize(LOWORD(lParam), HIWORD(lParam));
            return 0;
            
        case WM_SETFOCUS:
            OnSetFocus();
            return 0;
            
        case WM_COMMAND:
            OnCommand(LOWORD(wParam), HIWORD(wParam), reinterpret_cast<HWND>(lParam));
            return 0;
            
        case WM_DROPFILES:
            OnDropFiles(reinterpret_cast<HDROP>(wParam));
            return 0;
            
        case WM_INITMENUPOPUP:
            OnInitMenuPopup(reinterpret_cast<HMENU>(wParam), LOWORD(lParam), HIWORD(lParam));
            return 0;
            
        case WM_TIMER:
            OnTimer(static_cast<UINT_PTR>(wParam));
            return 0;
            
        case WM_CTLCOLOREDIT:
            return OnCtlColorEdit(reinterpret_cast<HDC>(wParam), reinterpret_cast<HWND>(lParam));
            
        case WM_APP_UPDATETITLE:
            UpdateTitle();
            return 0;
            
        case WM_APP_UPDATESTATUS:
            UpdateStatusBar();
            return 0;
            
        case WM_HOTKEY:
            OnHotkey(static_cast<int>(wParam));
            return 0;
            
        case WM_SYSCOMMAND:
            // Intercept minimize to send to tray
            if ((wParam & 0xFFF0) == SC_MINIMIZE) {
                MinimizeToTray();
                return 0;
            }
            break;
            
        case WM_APP_TRAYICON:
            // System tray icon messages
            if (lParam == WM_LBUTTONDBLCLK) {
                RestoreFromTray();
            } else if (lParam == WM_RBUTTONUP) {
                ShowTrayContextMenu();
            }
            return 0;

        case WM_APP_OPENNOTE:
            // Open a note by ID passed via lParam (as allocated string)
            if (lParam) {
                std::wstring* noteId = reinterpret_cast<std::wstring*>(lParam);
                OpenNoteFromId(*noteId);
                delete noteId;
            }
            return 0;

        case WM_DPICHANGED: {
            // Top-level window receives this when moved to a monitor with different DPI
            UINT newDpi = HIWORD(wParam);

            // Propagate DPI change to the tab bar
            if (m_tabBar) {
                m_tabBar->UpdateDPI(newDpi);
            }

            // Propagate DPI change to the line numbers gutter
            // Also re-set the editor font so the gutter gets updated font properties
            if (m_lineNumbersGutter) {
                m_lineNumbersGutter->UpdateDPI(newDpi);
                if (m_editor && m_lineNumbersGutter->IsVisible()) {
                    m_lineNumbersGutter->SetFont(m_editor->GetFont());
                }
            }

            // Resize the main window to the suggested rect
            const RECT* prcNewWindow = reinterpret_cast<const RECT*>(lParam);
            if (prcNewWindow) {
                SetWindowPos(m_hwnd, nullptr,
                    prcNewWindow->left, prcNewWindow->top,
                    prcNewWindow->right - prcNewWindow->left,
                    prcNewWindow->bottom - prcNewWindow->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
            }

            ResizeControls();
            return 0;
        }
    }
    
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

//------------------------------------------------------------------------------
// WM_CREATE handler
//------------------------------------------------------------------------------
void MainWindow::OnCreate() {
    // Apply dark title bar
    BOOL useDarkTitleBar = TRUE;
    DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkTitleBar, sizeof(useDarkTitleBar));
    
    // Create status bar
    CreateStatusBar();
    
    // Create editor
    const AppSettings& settings = m_settingsManager->GetSettings();
    if (!m_editor->Create(m_hwnd, m_hInstance, settings)) {
        MessageBoxW(m_hwnd, L"Failed to create editor control.", L"Error", MB_OK | MB_ICONERROR);
    }
    
    // Create line numbers gutter
    if (!m_lineNumbersGutter->Create(m_hwnd, m_hInstance, m_editor.get())) {
        MessageBoxW(m_hwnd, L"Failed to create line numbers gutter.", L"Error", MB_OK | MB_ICONERROR);
    }
    
    // Create tab bar
    if (!m_tabBar->Create(m_hwnd, m_hInstance)) {
        MessageBoxW(m_hwnd, L"Failed to create tab bar.", L"Error", MB_OK | MB_ICONERROR);
    }
    
    // Initialize document manager
    m_documentManager->Initialize(m_editor.get(), m_tabBar.get(), m_settingsManager.get());
    
    // Set up tab bar callbacks
    m_tabBar->SetCallback([this](TabNotification notification, int tabId) {
        switch (notification) {
            case TabNotification::TabSelected:
                OnTabSelected(tabId);
                break;
            case TabNotification::TabCloseRequested:
                OnTabCloseRequested(tabId);
                break;
            case TabNotification::NewTabRequested:
                OnTabNew();
                break;
            case TabNotification::TabRenamed:
                OnTabRenamed(tabId);
                break;
            case TabNotification::TabPinToggled:
                OnTabPinToggled(tabId);
                break;
            case TabNotification::CloseOthers:
                OnTabCloseOthers(tabId);
                break;
            case TabNotification::CloseAll:
                OnTabCloseAll();
                break;
            case TabNotification::CloseToRight:
                OnTabCloseToRight(tabId);
                break;
            case TabNotification::TabReordered:
                // Tab order changed via drag - just update document manager state
                if (m_documentManager) {
                    m_documentManager->SaveCurrentState();
                }
                break;
        }
    });
    
    // Create an initial tab
    m_documentManager->NewDocument();
    m_isNewFile = true;
    m_currentFile.clear();
    
    // Set up scroll callback for line numbers sync
    m_editor->SetScrollCallback(OnEditorScroll, this);
    
    // Apply line numbers setting
    if (settings.showLineNumbers) {
        m_lineNumbersGutter->SetFont(m_editor->GetFont());
        m_lineNumbersGutter->Show(true);
    }
    
    // Apply show whitespace setting
    m_editor->SetShowWhitespace(settings.showWhitespace);
    
    // Load custom keyboard shortcuts (overrides default accelerators)
    LoadKeyboardShortcuts();
    
    // Create find bar
    if (!m_findBar->Create(m_hwnd, m_hInstance, m_editor.get())) {
        MessageBoxW(m_hwnd, L"Failed to create find bar.", L"Error", MB_OK | MB_ICONERROR);
    }
    
    // Initialize dialog manager
    m_dialogManager->Initialize(m_hwnd, m_hInstance, m_editor.get(), 
                                 &m_settingsManager->GetSettings());
    
    // Update recent files menu
    UpdateRecentFilesMenu();
    
    // Initialize note store
    InitializeNoteStore();
    
    // Start status update timer
    SetTimer(m_hwnd, TIMER_STATUSUPDATE, 100, nullptr);
    
    // Start auto-save timer for notes
    SetTimer(m_hwnd, TIMER_AUTOSAVE, AUTOSAVE_INTERVAL, nullptr);
    
    // Start file auto-save timer
    SetTimer(m_hwnd, TIMER_FILEAUTOSAVE, FILEAUTOSAVE_INTERVAL, nullptr);
    
    // Start file change monitoring timer
    SetTimer(m_hwnd, TIMER_FILEWATCH, FILEWATCH_INTERVAL, nullptr);
    
    // Initialize system tray
    InitializeSystemTray();
    
    // Restore previous session (must be after tab bar + document manager init)
    LoadSession();
    
    // Initial resize
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    OnSize(rc.right, rc.bottom);
}

//------------------------------------------------------------------------------
// WM_DESTROY handler
//------------------------------------------------------------------------------
void MainWindow::OnDestroy() {
    // Kill timers
    KillTimer(m_hwnd, TIMER_STATUSUPDATE);
    KillTimer(m_hwnd, TIMER_AUTOSAVE);
    KillTimer(m_hwnd, TIMER_FILEAUTOSAVE);
    KillTimer(m_hwnd, TIMER_FILEWATCH);
    
    // Save session before destroying
    SaveSession();
    
    // Clean up system tray
    CleanupSystemTray();
    
    // Delete auto-save backup if file was saved successfully
    if (!m_isNoteMode && !m_isNewFile && !m_currentFile.empty() && !m_editor->IsModified()) {
        DeleteAutoSaveBackup();
    }
    
    // Auto-save current note if in note mode
    if (m_isNoteMode && m_editor->IsModified()) {
        AutoSaveCurrentNote();
    }
    
    // Unregister global hotkey
    if (m_hotkeyManager) {
        m_hotkeyManager->Unregister();
    }
    
    // Save window position
    WINDOWPLACEMENT wp = {};
    wp.length = sizeof(wp);
    if (GetWindowPlacement(m_hwnd, &wp)) {
        AppSettings& settings = m_settingsManager->GetSettings();
        settings.windowMaximized = (wp.showCmd == SW_SHOWMAXIMIZED);
        
        // Save normal (restored) position
        settings.windowX = wp.rcNormalPosition.left;
        settings.windowY = wp.rcNormalPosition.top;
        settings.windowWidth = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
        settings.windowHeight = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
        
        // Save zoom level
        settings.zoomLevel = m_editor->GetTextLength() > 0 ? m_settingsManager->GetSettings().zoomLevel : 100;
        
        // Save editor feature settings
        settings.showWhitespace = m_editor->IsShowWhitespace();
    }
    
    // Save settings
    (void)m_settingsManager->Save();
    
    // Clean up
    m_editor->Destroy();
    
    PostQuitMessage(0);
}

//------------------------------------------------------------------------------
// WM_CLOSE handler
//------------------------------------------------------------------------------
void MainWindow::OnClose() {
    // In note mode, auto-save and close without prompting
    if (m_isNoteMode) {
        if (m_editor->IsModified()) {
            AutoSaveCurrentNote();
        }
        DestroyWindow(m_hwnd);
        return;
    }
    
    // Save current editor state to document manager
    if (m_documentManager) {
        m_documentManager->SaveCurrentState();
    }
    
    // Check all tabs for unsaved changes
    if (m_documentManager) {
        auto ids = m_documentManager->GetAllTabIds();
        for (int id : ids) {
            auto* doc = m_documentManager->GetDocument(id);
            if (doc && doc->isModified && !(doc->isNewFile && IsWhitespaceOnly(doc->text))) {
                // Switch to the unsaved tab so user can see it
                OnTabSelected(id);
                
                std::wstring message = L"Do you want to save changes";
                if (!doc->filePath.empty()) {
                    message += L" to " + doc->GetDisplayTitle();
                }
                message += L"?";
                
                int result = MessageBoxW(m_hwnd, message.c_str(), L"QNote",
                                         MB_YESNOCANCEL | MB_ICONWARNING);
                
                switch (result) {
                    case IDYES:
                        if (!OnFileSave()) return;  // Save failed or cancelled
                        break;
                    case IDNO:
                        break;  // Continue without saving
                    case IDCANCEL:
                    default:
                        return;  // Cancel close
                }
            }
        }
    }
    
    DestroyWindow(m_hwnd);
}

//------------------------------------------------------------------------------
// WM_SIZE handler
//------------------------------------------------------------------------------
void MainWindow::OnSize(int width, int height) {
    ResizeControls();
}

//------------------------------------------------------------------------------
// WM_SETFOCUS handler
//------------------------------------------------------------------------------
void MainWindow::OnSetFocus() {
    if (m_editor) {
        m_editor->SetFocus();
    }
}

//------------------------------------------------------------------------------
// WM_COMMAND handler
//------------------------------------------------------------------------------
void MainWindow::OnCommand(WORD id, WORD code, HWND hwndCtl) {
    // Check for recent files
    if (id >= IDM_FILE_RECENT_BASE && id < IDM_FILE_RECENT_BASE + 10) {
        OnFileOpenRecent(id - IDM_FILE_RECENT_BASE);
        return;
    }
    
    switch (id) {
        // File menu
        case IDM_FILE_NEW:       OnFileNew(); break;
        case IDM_FILE_NEWWINDOW: OnFileNewWindow(); break;
        case IDM_FILE_OPEN:      OnFileOpen(); break;
        case IDM_FILE_SAVE:      OnFileSave(); break;
        case IDM_FILE_SAVEAS:    OnFileSaveAs(); break;
        case IDM_FILE_PAGESETUP: OnFilePageSetup(); break;
        case IDM_FILE_PRINT:     OnFilePrint(); break;
        case IDM_FILE_EXIT:      OnClose(); break;
        
        // Edit menu
        case IDM_EDIT_UNDO:      OnEditUndo(); break;
        case IDM_EDIT_REDO:      OnEditRedo(); break;
        case IDM_EDIT_CUT:       OnEditCut(); break;
        case IDM_EDIT_COPY:      OnEditCopy(); break;
        case IDM_EDIT_PASTE:     OnEditPaste(); break;
        case IDM_EDIT_DELETE:    OnEditDelete(); break;
        case IDM_EDIT_SELECTALL: OnEditSelectAll(); break;
        case IDM_EDIT_FIND:      OnEditFind(); break;
        case IDM_EDIT_FINDNEXT:  OnEditFindNext(); break;
        case IDM_EDIT_REPLACE:   OnEditReplace(); break;
        case IDM_EDIT_GOTO:      OnEditGoTo(); break;
        case IDM_EDIT_DATETIME:  OnEditDateTime(); break;
        
        // Format menu
        case IDM_FORMAT_WORDWRAP:   OnFormatWordWrap(); break;
        case IDM_FORMAT_FONT:       OnFormatFont(); break;
        case IDM_FORMAT_TABSIZE:    OnFormatTabSize(); break;
        case IDM_FORMAT_RTL:        OnFormatRTL(); break;
        case IDM_FORMAT_EOL_CRLF:   OnFormatLineEnding(LineEnding::CRLF); break;
        case IDM_FORMAT_EOL_LF:     OnFormatLineEnding(LineEnding::LF); break;
        case IDM_FORMAT_EOL_CR:     OnFormatLineEnding(LineEnding::CR); break;
        
        // View menu
        case IDM_VIEW_STATUSBAR:    OnViewStatusBar(); break;
        case IDM_VIEW_LINENUMBERS:  OnViewLineNumbers(); break;
        case IDM_VIEW_ZOOMIN:       OnViewZoomIn(); break;
        case IDM_VIEW_ZOOMOUT:      OnViewZoomOut(); break;
        case IDM_VIEW_ZOOMRESET:    OnViewZoomReset(); break;
        
        // Encoding menu
        case IDM_ENCODING_UTF8:     OnEncodingChange(TextEncoding::UTF8); break;
        case IDM_ENCODING_UTF8BOM:  OnEncodingChange(TextEncoding::UTF8_BOM); break;
        case IDM_ENCODING_UTF16LE:  OnEncodingChange(TextEncoding::UTF16_LE); break;
        case IDM_ENCODING_UTF16BE:  OnEncodingChange(TextEncoding::UTF16_BE); break;
        case IDM_ENCODING_ANSI:     OnEncodingChange(TextEncoding::ANSI); break;
        
        // Reopen with encoding
        case IDM_ENCODING_REOPEN_UTF8:    OnReopenWithEncoding(TextEncoding::UTF8); break;
        case IDM_ENCODING_REOPEN_UTF8BOM: OnReopenWithEncoding(TextEncoding::UTF8_BOM); break;
        case IDM_ENCODING_REOPEN_UTF16LE: OnReopenWithEncoding(TextEncoding::UTF16_LE); break;
        case IDM_ENCODING_REOPEN_UTF16BE: OnReopenWithEncoding(TextEncoding::UTF16_BE); break;
        case IDM_ENCODING_REOPEN_ANSI:    OnReopenWithEncoding(TextEncoding::ANSI); break;
        
        // Help menu
        case IDM_HELP_ABOUT: OnHelpAbout(); break;
        case IDM_HELP_CHECKUPDATE: OnHelpCheckUpdate(); break;
        
        // Notes menu
        case IDM_NOTES_NEW:          OnNotesNew(); break;
        case IDM_NOTES_QUICKCAPTURE: OnNotesQuickCapture(); break;
        case IDM_NOTES_ALLNOTES:     OnNotesAllNotes(); break;
        case IDM_NOTES_PINNED:       OnNotesPinned(); break;
        case IDM_NOTES_TIMELINE:     OnNotesTimeline(); break;
        case IDM_NOTES_SEARCH:       OnNotesSearch(); break;
        case IDM_NOTES_PINNOTE:      OnNotesPinCurrent(); break;
        case IDM_NOTES_SAVENOW:      OnNotesSaveNow(); break;
        case IDM_NOTES_DELETENOTE:   OnNotesDeleteCurrent(); break;
        
        // File operations (additional)
        case IDM_FILE_REVERT:        OnFileRevert(); break;
        case IDM_FILE_OPENCONTAINING: OnFileOpenContainingFolder(); break;
        
        // Text manipulation
        case IDM_EDIT_UPPERCASE:         OnEditUppercase(); break;
        case IDM_EDIT_LOWERCASE:         OnEditLowercase(); break;
        case IDM_EDIT_SORTLINES_ASC:     OnEditSortLines(true); break;
        case IDM_EDIT_SORTLINES_DESC:    OnEditSortLines(false); break;
        case IDM_EDIT_TRIMWHITESPACE:    OnEditTrimWhitespace(); break;
        case IDM_EDIT_REMOVEDUPLICATES:  OnEditRemoveDuplicateLines(); break;
        
        // Bookmarks
        case IDM_EDIT_TOGGLEBOOKMARK:    OnEditToggleBookmark(); break;
        case IDM_EDIT_NEXTBOOKMARK:      OnEditNextBookmark(); break;
        case IDM_EDIT_PREVBOOKMARK:      OnEditPrevBookmark(); break;
        case IDM_EDIT_CLEARBOOKMARKS:    OnEditClearBookmarks(); break;
        
        // View menu (additional)
        case IDM_VIEW_SHOWWHITESPACE:    OnViewShowWhitespace(); break;
        
        // Tools menu
        case IDM_TOOLS_EDITSHORTCUTS:    OnToolsEditShortcuts(); break;
        case IDM_TOOLS_COPYFILEPATH:     OnToolsCopyFilePath(); break;
        case IDM_TOOLS_AUTOSAVEOPTIONS:  OnToolsAutoSaveOptions(); break;
        case IDM_TOOLS_URLENCODE:        OnToolsUrlEncode(); break;
        case IDM_TOOLS_URLDECODE:        OnToolsUrlDecode(); break;
        case IDM_TOOLS_BASE64ENCODE:     OnToolsBase64Encode(); break;
        case IDM_TOOLS_BASE64DECODE:     OnToolsBase64Decode(); break;
        case IDM_TOOLS_INSERTLOREM:      OnToolsInsertLorem(); break;
        case IDM_TOOLS_SPLITLINES:       OnToolsSplitLines(); break;
        case IDM_TOOLS_TABSTOSPACES:     OnToolsTabsToSpaces(); break;
        case IDM_TOOLS_SPACESTOTABS:     OnToolsSpacesToTabs(); break;
        case IDM_TOOLS_WORDCOUNT:        OnToolsWordCount(); break;
        case IDM_TOOLS_REMOVEBLANKLINES: OnToolsRemoveBlankLines(); break;
        case IDM_TOOLS_JOINLINES:        OnToolsJoinLines(); break;
        case IDM_TOOLS_FORMATJSON:       OnToolsFormatJson(); break;
        case IDM_TOOLS_MINIFYJSON:       OnToolsMinifyJson(); break;
        case IDM_TOOLS_OPENTERMINAL:     OnToolsOpenTerminal(); break;
        
        // Notes menu (additional)
        case IDM_NOTES_EXPORT:           OnNotesExport(); break;
        case IDM_NOTES_IMPORT:           OnNotesImport(); break;
        
        // System tray
        case IDM_TRAY_SHOW:     RestoreFromTray(); break;
        case IDM_TRAY_CAPTURE:  OnNotesQuickCapture(); break;
        case IDM_TRAY_EXIT:     DestroyWindow(m_hwnd); break;
        
        // Tab menu
        case IDM_TAB_NEW:            OnTabNew(); break;
        case IDM_TAB_CLOSE:          OnTabClose(); break;
        case IDM_TAB_NEXT: {
            // Switch to next tab
            auto ids = m_tabBar->GetAllTabIds();
            if (ids.size() > 1) {
                int activeId = m_tabBar->GetActiveTabId();
                for (size_t i = 0; i < ids.size(); i++) {
                    if (ids[i] == activeId) {
                        int nextId = ids[(i + 1) % ids.size()];
                        OnTabSelected(nextId);
                        break;
                    }
                }
            }
            break;
        }
        case IDM_TAB_PREV: {
            // Switch to previous tab
            auto ids = m_tabBar->GetAllTabIds();
            if (ids.size() > 1) {
                int activeId = m_tabBar->GetActiveTabId();
                for (size_t i = 0; i < ids.size(); i++) {
                    if (ids[i] == activeId) {
                        int prevId = ids[(i + ids.size() - 1) % ids.size()];
                        OnTabSelected(prevId);
                        break;
                    }
                }
            }
            break;
        }
    }
    
    // Handle edit control notifications
    if (hwndCtl == m_editor->GetHandle() && code == EN_CHANGE) {
        // Sync modified state with DocumentManager and TabBar
        if (m_documentManager) {
            m_documentManager->SyncModifiedState();
            
            // Suppress modified indicator for new files with whitespace-only content
            auto* doc = m_documentManager->GetActiveDocument();
            if (doc && doc->isNewFile && doc->isModified && IsWhitespaceOnly(m_editor->GetText())) {
                m_editor->SetModified(false);
                doc->isModified = false;
                m_tabBar->SetTabModified(doc->tabId, false);
            }
        }
        UpdateTitle();
        UpdateStatusBar();
        if (m_lineNumbersGutter && m_lineNumbersGutter->IsVisible()) {
            m_lineNumbersGutter->Update();
        }
    }
}

//------------------------------------------------------------------------------
// WM_DROPFILES handler
//------------------------------------------------------------------------------
void MainWindow::OnDropFiles(HDROP hDrop) {
    wchar_t filePath[MAX_PATH] = {};
    
    // Get the number of dropped files
    UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
    
    // Open each dropped file in a new tab
    for (UINT i = 0; i < fileCount; i++) {
        if (DragQueryFileW(hDrop, i, filePath, MAX_PATH) > 0) {
            // Check if already open
            int existingTab = m_documentManager->FindDocumentByPath(filePath);
            if (existingTab >= 0) {
                if (i == fileCount - 1) {
                    OnTabSelected(existingTab);
                }
                continue;
            }
            
            // For the first file, reuse current tab if empty/untitled
            if (i == 0) {
                auto* activeDoc = m_documentManager->GetActiveDocument();
                if (activeDoc && activeDoc->isNewFile && !activeDoc->isModified &&
                    IsWhitespaceOnly(m_editor->GetText())) {
                    LoadFile(filePath);
                    continue;
                }
            }
            
            FileReadResult result = FileIO::ReadFile(filePath);
            if (result.success) {
                m_documentManager->OpenDocument(
                    filePath, result.content, result.detectedEncoding, result.detectedLineEnding);
                m_currentFile = filePath;
                m_isNewFile = false;
                m_isNoteMode = false;
                m_settingsManager->AddRecentFile(filePath);
                UpdateRecentFilesMenu();
            }
        }
    }
    
    DragFinish(hDrop);
    UpdateTitle();
    UpdateStatusBar();
    
    // Bring window to front
    SetForegroundWindow(m_hwnd);
}

//------------------------------------------------------------------------------
// WM_INITMENUPOPUP handler
//------------------------------------------------------------------------------
void MainWindow::OnInitMenuPopup(HMENU hMenu, UINT index, BOOL sysMenu) {
    if (sysMenu) return;
    
    UpdateMenuState();
}

//------------------------------------------------------------------------------
// WM_TIMER handler
//------------------------------------------------------------------------------
void MainWindow::OnTimer(UINT_PTR timerId) {
    if (timerId == TIMER_STATUSUPDATE) {
        UpdateStatusBar();
    } else if (timerId == TIMER_AUTOSAVE) {
        // Auto-save current note if in note mode and modified
        if (m_isNoteMode && m_editor->IsModified()) {
            AutoSaveCurrentNote();
        }
    } else if (timerId == TIMER_FILEAUTOSAVE) {
        // Auto-save file backup if in file mode and modified
        AutoSaveFileBackup();
    } else if (timerId == TIMER_FILEWATCH) {
        // Check if the current file has been modified externally
        CheckFileChanged();
    }
}

//------------------------------------------------------------------------------
// WM_CTLCOLOREDIT handler
//------------------------------------------------------------------------------
LRESULT MainWindow::OnCtlColorEdit(HDC hdc, HWND hwndEdit) {
    return DefWindowProcW(m_hwnd, WM_CTLCOLOREDIT, reinterpret_cast<WPARAM>(hdc), 
                          reinterpret_cast<LPARAM>(hwndEdit));
}

//------------------------------------------------------------------------------
// File -> New
//------------------------------------------------------------------------------
void MainWindow::OnFileNew() {
    // Create a new tab with an empty document
    OnTabNew();
}

//------------------------------------------------------------------------------
// File -> Open
//------------------------------------------------------------------------------
void MainWindow::OnFileOpen() {
    std::wstring filePath;
    if (FileIO::ShowOpenDialog(m_hwnd, filePath)) {
        // Check if already open in another tab
        int existingTab = m_documentManager->FindDocumentByPath(filePath);
        if (existingTab >= 0) {
            OnTabSelected(existingTab);
            return;
        }
        
        // If current tab is untitled, unmodified, and empty - reuse it
        auto* activeDoc = m_documentManager->GetActiveDocument();
        if (activeDoc && activeDoc->isNewFile && !activeDoc->isModified && 
            IsWhitespaceOnly(m_editor->GetText())) {
            // Reuse current tab
            LoadFile(filePath);
        } else {
            // Open in new tab
            FileReadResult result = FileIO::ReadFile(filePath);
            if (result.success) {
                int tabId = m_documentManager->OpenDocument(
                    filePath, result.content, result.detectedEncoding, result.detectedLineEnding);
                if (tabId >= 0) {
                    m_currentFile = filePath;
                    m_isNewFile = false;
                    m_isNoteMode = false;
                    m_currentNoteId.clear();
                    m_settingsManager->AddRecentFile(filePath);
                    UpdateRecentFilesMenu();
                    UpdateTitle();
                    UpdateStatusBar();
                }
            } else {
                MessageBoxW(m_hwnd, result.errorMessage.c_str(), L"Error Opening File",
                            MB_OK | MB_ICONERROR);
            }
        }
    }
}

//------------------------------------------------------------------------------
// File -> Save
//------------------------------------------------------------------------------
bool MainWindow::OnFileSave() {
    if (m_isNewFile || m_currentFile.empty()) {
        return OnFileSaveAs();
    }
    
    bool result = SaveFile(m_currentFile);
    if (result && m_documentManager) {
        m_documentManager->SetDocumentModified(m_documentManager->GetActiveTabId(), false);
    }
    return result;
}

//------------------------------------------------------------------------------
// File -> Save As
//------------------------------------------------------------------------------
bool MainWindow::OnFileSaveAs() {
    std::wstring filePath;
    TextEncoding encoding = m_editor->GetEncoding();
    
    if (FileIO::ShowSaveDialog(m_hwnd, filePath, encoding, m_currentFile)) {
        m_editor->SetEncoding(encoding);
        if (SaveFile(filePath)) {
            m_currentFile = filePath;
            m_isNewFile = false;
            m_settingsManager->AddRecentFile(filePath);
            UpdateRecentFilesMenu();
            
            // Update document manager with new file path
            if (m_documentManager) {
                int activeTab = m_documentManager->GetActiveTabId();
                m_documentManager->SetDocumentFilePath(activeTab, filePath);
                m_documentManager->SetDocumentModified(activeTab, false);
            }
            
            UpdateTitle();
            return true;
        }
    }
    
    return false;
}

//------------------------------------------------------------------------------
// File -> Open Recent
//------------------------------------------------------------------------------
void MainWindow::OnFileOpenRecent(int index) {
    const auto& recentFiles = m_settingsManager->GetSettings().recentFiles;
    
    if (index >= 0 && index < static_cast<int>(recentFiles.size())) {
        std::wstring filePath = recentFiles[index];
        
        // Check if already open
        int existingTab = m_documentManager->FindDocumentByPath(filePath);
        if (existingTab >= 0) {
            OnTabSelected(existingTab);
            return;
        }
        
        // Check if file still exists
        if (GetFileAttributesW(filePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            MessageBoxW(m_hwnd, L"The file no longer exists.", L"QNote", MB_OK | MB_ICONWARNING);
            m_settingsManager->RemoveRecentFile(filePath);
            UpdateRecentFilesMenu();
            return;
        }
        
        // Open in new tab if current tab has content, otherwise reuse
        auto* activeDoc = m_documentManager->GetActiveDocument();
        if (activeDoc && activeDoc->isNewFile && !activeDoc->isModified &&
            IsWhitespaceOnly(m_editor->GetText())) {
            LoadFile(filePath);
        } else {
            FileReadResult result = FileIO::ReadFile(filePath);
            if (result.success) {
                m_documentManager->OpenDocument(
                    filePath, result.content, result.detectedEncoding, result.detectedLineEnding);
                m_currentFile = filePath;
                m_isNewFile = false;
                m_isNoteMode = false;
                m_settingsManager->AddRecentFile(filePath);
                UpdateRecentFilesMenu();
                UpdateTitle();
                UpdateStatusBar();
            }
        }
    }
}

//------------------------------------------------------------------------------
// Edit menu handlers
//------------------------------------------------------------------------------
void MainWindow::OnEditUndo() {
    m_editor->Undo();
}

void MainWindow::OnEditRedo() {
    m_editor->Redo();
}

void MainWindow::OnEditCut() {
    m_editor->Cut();
}

void MainWindow::OnEditCopy() {
    m_editor->Copy();
}

void MainWindow::OnEditPaste() {
    m_editor->Paste();
}

void MainWindow::OnEditDelete() {
    m_editor->Delete();
}

void MainWindow::OnEditSelectAll() {
    m_editor->SelectAll();
}

void MainWindow::OnEditFind() {
    if (m_findBar) {
        m_findBar->Show(FindBarMode::Find);
    }
}

void MainWindow::OnEditFindNext() {
    if (m_findBar && m_findBar->IsVisible()) {
        m_findBar->FindNext();
    } else if (m_findBar) {
        // Show the find bar if not visible
        m_findBar->Show(FindBarMode::Find);
    }
}

void MainWindow::OnEditReplace() {
    if (m_findBar) {
        m_findBar->Show(FindBarMode::Replace);
    }
}

void MainWindow::OnEditGoTo() {
    int lineNumber = m_editor->GetCurrentLine() + 1;
    
    if (m_dialogManager->ShowGoToDialog(lineNumber)) {
        m_editor->GoToLine(lineNumber - 1);  // Convert to 0-based
    }
}

void MainWindow::OnEditDateTime() {
    m_editor->InsertDateTime();
}

//------------------------------------------------------------------------------
// Format menu handlers
//------------------------------------------------------------------------------
void MainWindow::OnFormatWordWrap() {
    bool newValue = !m_editor->IsWordWrapEnabled();
    m_editor->SetWordWrap(newValue);
    m_settingsManager->GetSettings().wordWrap = newValue;
}

void MainWindow::OnFormatFont() {
    AppSettings& settings = m_settingsManager->GetSettings();
    
    std::wstring fontName = settings.fontName;
    int fontSize = settings.fontSize;
    int fontWeight = settings.fontWeight;
    bool fontItalic = settings.fontItalic;
    
    if (m_dialogManager->ShowFontDialog(fontName, fontSize, fontWeight, fontItalic)) {
        settings.fontName = fontName;
        settings.fontSize = fontSize;
        settings.fontWeight = fontWeight;
        settings.fontItalic = fontItalic;
        
        m_editor->SetFont(fontName, fontSize, fontWeight, fontItalic);
    }
}

void MainWindow::OnFormatTabSize() {
    int tabSize = m_settingsManager->GetSettings().tabSize;
    
    if (m_dialogManager->ShowTabSizeDialog(tabSize)) {
        m_settingsManager->GetSettings().tabSize = tabSize;
        m_editor->SetTabSize(tabSize);
    }
}

void MainWindow::OnFormatLineEnding(LineEnding ending) {
    m_editor->SetLineEnding(ending);
    m_editor->SetModified(true);
    UpdateStatusBar();
    UpdateTitle();
}

//------------------------------------------------------------------------------
// Format -> Right-to-Left Reading Order
//------------------------------------------------------------------------------
void MainWindow::OnFormatRTL() {
    bool newValue = !m_editor->IsRTL();
    m_editor->SetRTL(newValue);
    m_settingsManager->GetSettings().rightToLeft = newValue;
}

//------------------------------------------------------------------------------
// File -> New Window
//------------------------------------------------------------------------------
void MainWindow::OnFileNewWindow() {
    // Get the path to our executable
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    
    // Launch a new instance
    ShellExecuteW(nullptr, L"open", exePath, nullptr, nullptr, SW_SHOWNORMAL);
}

//------------------------------------------------------------------------------
// File -> Page Setup
//------------------------------------------------------------------------------
void MainWindow::OnFilePageSetup() {
    // Initialize page setup dialog
    if (m_pageSetup.lStructSize == 0) {
        m_pageSetup.lStructSize = sizeof(m_pageSetup);
        m_pageSetup.hwndOwner = m_hwnd;
        m_pageSetup.Flags = PSD_DEFAULTMINMARGINS | PSD_MARGINS;
        
        // Default margins (in thousandths of inches)
        m_pageSetup.rtMargin.left = 1000;
        m_pageSetup.rtMargin.top = 1000;
        m_pageSetup.rtMargin.right = 1000;
        m_pageSetup.rtMargin.bottom = 1000;
    }
    
    PageSetupDlgW(&m_pageSetup);
}

//------------------------------------------------------------------------------
// File -> Print
//------------------------------------------------------------------------------
void MainWindow::OnFilePrint() {
    PRINTDLGW pd = {};
    pd.lStructSize = sizeof(pd);
    pd.hwndOwner = m_hwnd;
    pd.Flags = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE;
    pd.nCopies = 1;
    pd.nFromPage = 1;
    pd.nToPage = 1;
    pd.nMinPage = 1;
    pd.nMaxPage = 0xFFFF;
    
    if (!PrintDlgW(&pd)) {
        // User cancelled or error
        if (pd.hDC) {
            DeleteDC(pd.hDC);
        }
        return;
    }
    
    HDC hdc = pd.hDC;
    if (!hdc) {
        MessageBoxW(m_hwnd, L"Failed to create printer device context.", L"Print Error", 
                    MB_OK | MB_ICONERROR);
        return;
    }
    
    // Get text to print
    std::wstring text = m_editor->GetText();
    if (text.empty()) {
        DeleteDC(hdc);
        if (pd.hDevMode) GlobalFree(pd.hDevMode);
        if (pd.hDevNames) GlobalFree(pd.hDevNames);
        return;
    }
    
    // Set document info
    DOCINFOW di = {};
    di.cbSize = sizeof(di);
    std::wstring docName = m_isNewFile ? L"Untitled" : FileIO::GetFileName(m_currentFile);
    di.lpszDocName = docName.c_str();
    
    // Start the document
    if (StartDocW(hdc, &di) <= 0) {
        MessageBoxW(m_hwnd, L"Failed to start print job.", L"Print Error", 
                    MB_OK | MB_ICONERROR);
        DeleteDC(hdc);
        if (pd.hDevMode) GlobalFree(pd.hDevMode);
        if (pd.hDevNames) GlobalFree(pd.hDevNames);
        return;
    }
    
    // Get printer capabilities
    int pageWidth = GetDeviceCaps(hdc, HORZRES);
    int pageHeight = GetDeviceCaps(hdc, VERTRES);
    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
    int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
    
    // Apply margins from page setup (convert from thousandths of inches to pixels)
    int leftMargin = (m_pageSetup.rtMargin.left * dpiX) / 1000;
    int topMargin = (m_pageSetup.rtMargin.top * dpiY) / 1000;
    int rightMargin = (m_pageSetup.rtMargin.right * dpiX) / 1000;
    int bottomMargin = (m_pageSetup.rtMargin.bottom * dpiY) / 1000;
    
    int printWidth = pageWidth - leftMargin - rightMargin;
    int printHeight = pageHeight - topMargin - bottomMargin;
    
    // Create a font for printing (scale from screen)
    const AppSettings& settings = m_settingsManager->GetSettings();
    LOGFONTW lf = {};
    lf.lfHeight = -MulDiv(settings.fontSize, dpiY, 72);
    lf.lfWeight = settings.fontWeight;
    lf.lfItalic = settings.fontItalic ? TRUE : FALSE;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, settings.fontName.c_str());
    
    HFONT hFont = CreateFontIndirectW(&lf);
    HFONT hOldFont = static_cast<HFONT>(SelectObject(hdc, hFont));
    
    // Get text metrics
    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    int linesPerPage = printHeight / lineHeight;
    
    // Split text into lines
    std::vector<std::wstring> lines;
    std::wistringstream stream(text);
    std::wstring line;
    while (std::getline(stream, line)) {
        // Handle word wrap for printing
        while (!line.empty()) {
            SIZE textSize;
            GetTextExtentPoint32W(hdc, line.c_str(), static_cast<int>(line.length()), &textSize);
            
            if (textSize.cx <= printWidth) {
                lines.push_back(line);
                break;
            }
            
            // Find break point
            size_t breakPos = line.length();
            for (size_t i = line.length(); i > 0; --i) {
                std::wstring partial = line.substr(0, i);
                GetTextExtentPoint32W(hdc, partial.c_str(), static_cast<int>(i), &textSize);
                if (textSize.cx <= printWidth) {
                    // Try to break at word boundary
                    size_t lastSpace = partial.rfind(L' ');
                    if (lastSpace != std::wstring::npos && lastSpace > 0) {
                        breakPos = lastSpace + 1;
                    } else {
                        breakPos = i;
                    }
                    break;
                }
            }
            
            lines.push_back(line.substr(0, breakPos));
            if (breakPos < line.length()) {
                line = line.substr(breakPos);
                // Trim leading spaces
                size_t start = line.find_first_not_of(L' ');
                if (start != std::wstring::npos) {
                    line = line.substr(start);
                }
            } else {
                line.clear();
            }
        }
    }
    
    // Print pages
    int totalLines = static_cast<int>(lines.size());
    int currentLine = 0;
    
    while (currentLine < totalLines) {
        // Start a new page
        if (StartPage(hdc) <= 0) {
            break;
        }
        
        SelectObject(hdc, hFont);
        
        int y = topMargin;
        int linesOnPage = 0;
        
        while (currentLine < totalLines && linesOnPage < linesPerPage) {
            const std::wstring& lineText = lines[currentLine];
            TextOutW(hdc, leftMargin, y, lineText.c_str(), static_cast<int>(lineText.length()));
            
            y += lineHeight;
            ++currentLine;
            ++linesOnPage;
        }
        
        EndPage(hdc);
    }
    
    // Cleanup
    EndDoc(hdc);
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    DeleteDC(hdc);
    if (pd.hDevMode) GlobalFree(pd.hDevMode);
    if (pd.hDevNames) GlobalFree(pd.hDevNames);
}

//------------------------------------------------------------------------------
// View menu handlers
//------------------------------------------------------------------------------
void MainWindow::OnViewStatusBar() {
    AppSettings& settings = m_settingsManager->GetSettings();
    settings.showStatusBar = !settings.showStatusBar;
    
    ShowWindow(m_hwndStatus, settings.showStatusBar ? SW_SHOW : SW_HIDE);
    ResizeControls();
}

void MainWindow::OnViewLineNumbers() {
    AppSettings& settings = m_settingsManager->GetSettings();
    settings.showLineNumbers = !settings.showLineNumbers;
    
    if (m_lineNumbersGutter) {
        m_lineNumbersGutter->SetFont(m_editor->GetFont());
        m_lineNumbersGutter->Show(settings.showLineNumbers);
    }
    ResizeControls();
}

void MainWindow::OnViewZoomIn() {
    AppSettings& settings = m_settingsManager->GetSettings();
    settings.zoomLevel = (std::min)(500, settings.zoomLevel + 10);
    m_editor->ApplyZoom(settings.zoomLevel);
    UpdateStatusBar();
}

void MainWindow::OnViewZoomOut() {
    AppSettings& settings = m_settingsManager->GetSettings();
    settings.zoomLevel = (std::max)(25, settings.zoomLevel - 10);
    m_editor->ApplyZoom(settings.zoomLevel);
    UpdateStatusBar();
}

void MainWindow::OnViewZoomReset() {
    AppSettings& settings = m_settingsManager->GetSettings();
    settings.zoomLevel = 100;
    m_editor->ApplyZoom(100);
    UpdateStatusBar();
}

//------------------------------------------------------------------------------\n// View -> Show Whitespace
//------------------------------------------------------------------------------
void MainWindow::OnViewShowWhitespace() {
    bool newState = !m_editor->IsShowWhitespace();
    m_editor->SetShowWhitespace(newState);
    m_settingsManager->GetSettings().showWhitespace = newState;
}

//------------------------------------------------------------------------------
// Bookmark operations
//------------------------------------------------------------------------------
void MainWindow::OnEditToggleBookmark() {
    m_editor->ToggleBookmark();
    if (m_lineNumbersGutter && m_lineNumbersGutter->IsVisible()) {
        m_lineNumbersGutter->Update();
    }
}

void MainWindow::OnEditNextBookmark() {
    m_editor->NextBookmark();
}

void MainWindow::OnEditPrevBookmark() {
    m_editor->PrevBookmark();
}

void MainWindow::OnEditClearBookmarks() {
    m_editor->ClearBookmarks();
    if (m_lineNumbersGutter && m_lineNumbersGutter->IsVisible()) {
        m_lineNumbersGutter->Update();
    }
}

//------------------------------------------------------------------------------
// Tools -> Edit Keyboard Shortcuts
//------------------------------------------------------------------------------
void MainWindow::OnToolsEditShortcuts() {
    // Get shortcuts file path
    std::wstring settingsPath = m_settingsManager->GetSettingsPath();
    size_t pos = settingsPath.rfind(L"config.ini");
    if (pos == std::wstring::npos) return;
    
    std::wstring shortcutsPath = settingsPath.substr(0, pos) + L"shortcuts.ini";
    
    // Create default file if it doesn't exist
    if (GetFileAttributesW(shortcutsPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        CreateDefaultShortcutsFile(shortcutsPath);
    }
    
    // Open the file in a new tab
    FileReadResult result = FileIO::ReadFile(shortcutsPath);
    if (result.success) {
        // Check if already open
        int existingTab = m_documentManager->FindDocumentByPath(shortcutsPath);
        if (existingTab >= 0) {
            OnTabSelected(existingTab);
        } else {
            auto* activeDoc = m_documentManager->GetActiveDocument();
            if (activeDoc && activeDoc->isNewFile && !activeDoc->isModified && 
                IsWhitespaceOnly(m_editor->GetText())) {
                LoadFile(shortcutsPath);
            } else {
                m_documentManager->OpenDocument(
                    shortcutsPath, result.content, result.detectedEncoding, result.detectedLineEnding);
                m_currentFile = shortcutsPath;
                m_isNewFile = false;
                UpdateTitle();
                UpdateStatusBar();
            }
        }
    }
}

//------------------------------------------------------------------------------
// Tools -> Copy File Path to Clipboard
//------------------------------------------------------------------------------
void MainWindow::OnToolsCopyFilePath() {
    if (m_currentFile.empty() || m_isNewFile) return;
    
    if (!OpenClipboard(m_hwnd)) return;
    EmptyClipboard();
    
    size_t len = (m_currentFile.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    if (hMem) {
        wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
        if (pMem) {
            wcscpy_s(pMem, m_currentFile.size() + 1, m_currentFile.c_str());
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
    }
    CloseClipboard();
    
    // Brief status bar notification
    SendMessageW(m_hwndStatus, SB_SETTEXTW, SB_PART_COUNTS,
                 reinterpret_cast<LPARAM>(L"Path copied to clipboard"));
}

//------------------------------------------------------------------------------
// Tools -> Auto-Save Options dialog
//------------------------------------------------------------------------------
struct AutoSaveDialogData {
    bool enabled;
    int intervalSeconds;
};

INT_PTR CALLBACK MainWindow::AutoSaveDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            SetWindowLongPtrW(hDlg, DWLP_USER, lParam);
            auto* data = reinterpret_cast<AutoSaveDialogData*>(lParam);
            CheckDlgButton(hDlg, IDC_AUTOSAVE_ENABLE, data->enabled ? BST_CHECKED : BST_UNCHECKED);
            SetDlgItemInt(hDlg, IDC_AUTOSAVE_INTERVAL, data->intervalSeconds, FALSE);
            // Enable/disable interval field based on checkbox
            EnableWindow(GetDlgItem(hDlg, IDC_AUTOSAVE_INTERVAL), data->enabled);
            return TRUE;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_AUTOSAVE_ENABLE: {
                    bool checked = IsDlgButtonChecked(hDlg, IDC_AUTOSAVE_ENABLE) == BST_CHECKED;
                    EnableWindow(GetDlgItem(hDlg, IDC_AUTOSAVE_INTERVAL), checked);
                    return TRUE;
                }
                case IDOK: {
                    auto* data = reinterpret_cast<AutoSaveDialogData*>(GetWindowLongPtrW(hDlg, DWLP_USER));
                    data->enabled = IsDlgButtonChecked(hDlg, IDC_AUTOSAVE_ENABLE) == BST_CHECKED;
                    BOOL success = FALSE;
                    int val = GetDlgItemInt(hDlg, IDC_AUTOSAVE_INTERVAL, &success, FALSE);
                    if (success && val >= 5 && val <= 3600) {
                        data->intervalSeconds = val;
                    } else {
                        MessageBoxW(hDlg, L"Please enter an interval between 5 and 3600 seconds.",
                                    L"QNote", MB_OK | MB_ICONWARNING);
                        return TRUE;
                    }
                    EndDialog(hDlg, IDOK);
                    return TRUE;
                }
                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

void MainWindow::OnToolsAutoSaveOptions() {
    AppSettings& settings = m_settingsManager->GetSettings();
    AutoSaveDialogData data;
    data.enabled = settings.fileAutoSave;
    data.intervalSeconds = static_cast<int>(FILEAUTOSAVE_INTERVAL / 1000);
    
    INT_PTR result = DialogBoxParamW(m_hInstance, MAKEINTRESOURCEW(IDD_AUTOSAVE),
                                      m_hwnd, AutoSaveDlgProc, reinterpret_cast<LPARAM>(&data));
    if (result == IDOK) {
        settings.fileAutoSave = data.enabled;
        (void)m_settingsManager->Save();
        
        // Restart or stop the file auto-save timer
        KillTimer(m_hwnd, TIMER_FILEAUTOSAVE);
        if (data.enabled) {
            SetTimer(m_hwnd, TIMER_FILEAUTOSAVE, static_cast<UINT>(data.intervalSeconds * 1000), nullptr);
        }
    }
}

//------------------------------------------------------------------------------
// Tools -> URL Encode Selection
//------------------------------------------------------------------------------
void MainWindow::OnToolsUrlEncode() {
    std::wstring sel = m_editor->GetSelectedText();
    if (sel.empty()) return;
    
    // Convert to UTF-8 for encoding
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, sel.c_str(), static_cast<int>(sel.size()), nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 0) return;
    std::string utf8(utf8Len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, sel.c_str(), static_cast<int>(sel.size()), &utf8[0], utf8Len, nullptr, nullptr);
    
    std::string encoded;
    encoded.reserve(utf8.size() * 3);
    for (unsigned char c : utf8) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += static_cast<char>(c);
        } else {
            char hex[4];
            sprintf_s(hex, "%%%02X", c);
            encoded += hex;
        }
    }
    
    // Convert back to wide string
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, encoded.c_str(), static_cast<int>(encoded.size()), nullptr, 0);
    if (wideLen <= 0) return;
    std::wstring result(wideLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, encoded.c_str(), static_cast<int>(encoded.size()), &result[0], wideLen);
    
    m_editor->ReplaceSelection(result);
}

//------------------------------------------------------------------------------
// Tools -> URL Decode Selection
//------------------------------------------------------------------------------
void MainWindow::OnToolsUrlDecode() {
    std::wstring sel = m_editor->GetSelectedText();
    if (sel.empty()) return;
    
    // Convert to UTF-8 for decoding
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, sel.c_str(), static_cast<int>(sel.size()), nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 0) return;
    std::string utf8(utf8Len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, sel.c_str(), static_cast<int>(sel.size()), &utf8[0], utf8Len, nullptr, nullptr);
    
    std::string decoded;
    decoded.reserve(utf8.size());
    for (size_t i = 0; i < utf8.size(); ++i) {
        if (utf8[i] == '%' && i + 2 < utf8.size()) {
            char hex[3] = { utf8[i + 1], utf8[i + 2], '\0' };
            char* end = nullptr;
            long val = strtol(hex, &end, 16);
            if (end == hex + 2) {
                decoded += static_cast<char>(val);
                i += 2;
                continue;
            }
        }
        if (utf8[i] == '+') {
            decoded += ' ';
        } else {
            decoded += utf8[i];
        }
    }
    
    // Convert back to wide string
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, decoded.c_str(), static_cast<int>(decoded.size()), nullptr, 0);
    if (wideLen <= 0) return;
    std::wstring result(wideLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, decoded.c_str(), static_cast<int>(decoded.size()), &result[0], wideLen);
    
    m_editor->ReplaceSelection(result);
}

//------------------------------------------------------------------------------
// Tools -> Base64 Encode Selection
//------------------------------------------------------------------------------
void MainWindow::OnToolsBase64Encode() {
    std::wstring sel = m_editor->GetSelectedText();
    if (sel.empty()) return;
    
    // Convert to UTF-8
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, sel.c_str(), static_cast<int>(sel.size()), nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 0) return;
    std::string utf8(utf8Len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, sel.c_str(), static_cast<int>(sel.size()), &utf8[0], utf8Len, nullptr, nullptr);
    
    // Use CryptBinaryToStringA for Base64
    DWORD b64Len = 0;
    if (!CryptBinaryToStringA(reinterpret_cast<const BYTE*>(utf8.data()), static_cast<DWORD>(utf8.size()),
                               CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &b64Len)) {
        return;
    }
    std::string b64(b64Len, '\0');
    if (!CryptBinaryToStringA(reinterpret_cast<const BYTE*>(utf8.data()), static_cast<DWORD>(utf8.size()),
                               CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &b64[0], &b64Len)) {
        return;
    }
    b64.resize(b64Len);
    
    // Convert to wide string
    std::wstring result(b64.begin(), b64.end());
    m_editor->ReplaceSelection(result);
}

//------------------------------------------------------------------------------
// Tools -> Base64 Decode Selection
//------------------------------------------------------------------------------
void MainWindow::OnToolsBase64Decode() {
    std::wstring sel = m_editor->GetSelectedText();
    if (sel.empty()) return;
    
    // Convert selection to narrow string (Base64 is ASCII)
    std::string b64;
    b64.reserve(sel.size());
    for (wchar_t wc : sel) {
        b64 += static_cast<char>(wc & 0x7F);
    }
    
    // Decode
    DWORD binLen = 0;
    if (!CryptStringToBinaryA(b64.c_str(), static_cast<DWORD>(b64.size()),
                               CRYPT_STRING_BASE64, nullptr, &binLen, nullptr, nullptr)) {
        MessageBoxW(m_hwnd, L"Invalid Base64 data.", L"QNote", MB_OK | MB_ICONWARNING);
        return;
    }
    std::vector<BYTE> bin(binLen);
    if (!CryptStringToBinaryA(b64.c_str(), static_cast<DWORD>(b64.size()),
                               CRYPT_STRING_BASE64, bin.data(), &binLen, nullptr, nullptr)) {
        MessageBoxW(m_hwnd, L"Failed to decode Base64 data.", L"QNote", MB_OK | MB_ICONWARNING);
        return;
    }
    
    // Convert from UTF-8 to wide string
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(bin.data()),
                                       static_cast<int>(binLen), nullptr, 0);
    if (wideLen <= 0) return;
    std::wstring result(wideLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(bin.data()),
                         static_cast<int>(binLen), &result[0], wideLen);
    
    m_editor->ReplaceSelection(result);
}

//------------------------------------------------------------------------------
// Tools -> Insert Lorem Ipsum
//------------------------------------------------------------------------------
void MainWindow::OnToolsInsertLorem() {
    m_editor->ReplaceSelection(L"Lorem ipsum dolor sit amet, consectetur adipiscing elit.");
}

//------------------------------------------------------------------------------
// Tools -> Split Lines at Column
//------------------------------------------------------------------------------
INT_PTR CALLBACK MainWindow::SplitLinesDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG:
            SetWindowLongPtrW(hDlg, DWLP_USER, lParam);
            SetDlgItemInt(hDlg, IDC_SPLITLINES_WIDTH, 80, FALSE);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK: {
                    auto* pWidth = reinterpret_cast<int*>(GetWindowLongPtrW(hDlg, DWLP_USER));
                    BOOL success = FALSE;
                    int val = GetDlgItemInt(hDlg, IDC_SPLITLINES_WIDTH, &success, FALSE);
                    if (success && val >= 1 && val <= 999) {
                        *pWidth = val;
                        EndDialog(hDlg, IDOK);
                    } else {
                        MessageBoxW(hDlg, L"Please enter a width between 1 and 999.",
                                    L"QNote", MB_OK | MB_ICONWARNING);
                    }
                    return TRUE;
                }
                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

void MainWindow::OnToolsSplitLines() {
    int maxWidth = 80;
    INT_PTR result = DialogBoxParamW(m_hInstance, MAKEINTRESOURCEW(IDD_SPLITLINES),
                                      m_hwnd, SplitLinesDlgProc, reinterpret_cast<LPARAM>(&maxWidth));
    if (result != IDOK) return;
    
    // Operate on selection if present, otherwise on whole document
    bool hasSelection = false;
    DWORD selStart, selEnd;
    m_editor->GetSelection(selStart, selEnd);
    hasSelection = (selStart != selEnd);
    
    std::wstring text = hasSelection ? m_editor->GetSelectedText() : m_editor->GetText();
    if (text.empty()) return;
    
    std::vector<std::wstring> lines;
    std::wistringstream stream(text);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        
        // Split this line if it exceeds maxWidth
        while (static_cast<int>(line.size()) > maxWidth) {
            // Try to break at a space
            int breakPos = maxWidth;
            for (int i = maxWidth - 1; i > 0; --i) {
                if (line[i] == L' ' || line[i] == L'\t') {
                    breakPos = i + 1;
                    break;
                }
            }
            lines.push_back(line.substr(0, breakPos));
            line = line.substr(breakPos);
            // Trim leading spaces from the continuation
            size_t firstNonSpace = line.find_first_not_of(L' ');
            if (firstNonSpace != std::wstring::npos && firstNonSpace > 0) {
                line = line.substr(firstNonSpace);
            }
        }
        lines.push_back(line);
    }
    
    std::wstring resultText;
    for (size_t i = 0; i < lines.size(); ++i) {
        resultText += lines[i];
        if (i < lines.size() - 1) resultText += L"\r\n";
    }
    
    if (hasSelection) {
        m_editor->ReplaceSelection(resultText);
    } else {
        m_editor->SelectAll();
        m_editor->ReplaceSelection(resultText);
    }
}

//------------------------------------------------------------------------------
// Tools -> Tabs to Spaces
//------------------------------------------------------------------------------
void MainWindow::OnToolsTabsToSpaces() {
    std::wstring text = m_editor->GetText();
    if (text.empty()) return;
    
    int tabSize = m_settingsManager->GetSettings().tabSize;
    std::wstring spaces(tabSize, L' ');
    
    std::wstring result;
    result.reserve(text.size());
    for (wchar_t ch : text) {
        if (ch == L'\t') {
            result += spaces;
        } else {
            result += ch;
        }
    }
    
    if (result != text) {
        m_editor->SelectAll();
        m_editor->ReplaceSelection(result);
    }
}

//------------------------------------------------------------------------------
// Tools -> Spaces to Tabs
//------------------------------------------------------------------------------
void MainWindow::OnToolsSpacesToTabs() {
    std::wstring text = m_editor->GetText();
    if (text.empty()) return;
    
    int tabSize = m_settingsManager->GetSettings().tabSize;
    std::wstring spaces(tabSize, L' ');
    
    // Replace leading spaces with tabs on each line
    std::vector<std::wstring> lines;
    std::wistringstream stream(text);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        
        std::wstring newLine;
        size_t i = 0;
        // Convert leading spaces to tabs
        while (i + tabSize <= line.size()) {
            bool allSpaces = true;
            for (int j = 0; j < tabSize; ++j) {
                if (line[i + j] != L' ') { allSpaces = false; break; }
            }
            if (allSpaces) {
                newLine += L'\t';
                i += tabSize;
            } else {
                break;
            }
        }
        newLine += line.substr(i);
        lines.push_back(newLine);
    }
    
    std::wstring result;
    for (size_t i = 0; i < lines.size(); ++i) {
        result += lines[i];
        if (i < lines.size() - 1) result += L"\r\n";
    }
    
    if (result != text) {
        m_editor->SelectAll();
        m_editor->ReplaceSelection(result);
    }
}

//------------------------------------------------------------------------------
// Tools -> Word Count
//------------------------------------------------------------------------------
void MainWindow::OnToolsWordCount() {
    std::wstring text = m_editor->GetText();
    
    int charCount = static_cast<int>(text.size());
    int charNoSpaces = 0;
    int wordCount = 0;
    int lineCount = 1;
    int paragraphCount = 0;
    bool inWord = false;
    bool lastWasNewline = false;
    
    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t ch = text[i];
        
        if (ch != L' ' && ch != L'\t' && ch != L'\r' && ch != L'\n') {
            charNoSpaces++;
        }
        
        if (ch == L'\n') {
            lineCount++;
            if (lastWasNewline) {
                paragraphCount++;
            }
            lastWasNewline = true;
        } else if (ch != L'\r') {
            lastWasNewline = false;
        }
        
        if (ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n') {
            if (inWord) {
                wordCount++;
                inWord = false;
            }
        } else {
            inWord = true;
        }
    }
    if (inWord) wordCount++;
    if (charCount > 0) paragraphCount++; // count last paragraph
    
    // Also count selection stats
    std::wstring sel = m_editor->GetSelectedText();
    std::wstring selInfo;
    if (!sel.empty()) {
        int selChars = static_cast<int>(sel.size());
        int selWords = 0;
        bool selInWord = false;
        for (wchar_t ch : sel) {
            if (ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n') {
                if (selInWord) { selWords++; selInWord = false; }
            } else {
                selInWord = true;
            }
        }
        if (selInWord) selWords++;
        
        wchar_t selBuf[128];
        swprintf_s(selBuf, L"\n\nSelection:\n  Characters: %d\n  Words: %d", selChars, selWords);
        selInfo = selBuf;
    }
    
    wchar_t msg[512];
    swprintf_s(msg, 
        L"Document Statistics:\n"
        L"  Characters (with spaces): %d\n"
        L"  Characters (no spaces): %d\n"
        L"  Words: %d\n"
        L"  Lines: %d\n"
        L"  Paragraphs: %d%s",
        charCount, charNoSpaces, wordCount, lineCount, paragraphCount, selInfo.c_str());
    
    MessageBoxW(m_hwnd, msg, L"Word Count - QNote", MB_OK | MB_ICONINFORMATION);
}

//------------------------------------------------------------------------------
// Tools -> Remove Blank Lines
//------------------------------------------------------------------------------
void MainWindow::OnToolsRemoveBlankLines() {
    std::wstring text = m_editor->GetText();
    if (text.empty()) return;
    
    std::vector<std::wstring> lines;
    std::wistringstream stream(text);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        // Keep the line only if it's not blank
        if (line.find_first_not_of(L" \t") != std::wstring::npos) {
            lines.push_back(line);
        }
    }
    
    std::wstring result;
    for (size_t i = 0; i < lines.size(); ++i) {
        result += lines[i];
        if (i < lines.size() - 1) result += L"\r\n";
    }
    
    m_editor->SelectAll();
    m_editor->ReplaceSelection(result);
}

//------------------------------------------------------------------------------
// Tools -> Join Lines
//------------------------------------------------------------------------------
void MainWindow::OnToolsJoinLines() {
    std::wstring sel = m_editor->GetSelectedText();
    if (sel.empty()) return;
    
    std::wstring result;
    result.reserve(sel.size());
    bool lastWasNewline = false;
    for (size_t i = 0; i < sel.size(); ++i) {
        wchar_t ch = sel[i];
        if (ch == L'\r') continue; // skip CR
        if (ch == L'\n') {
            if (!lastWasNewline) {
                result += L' '; // replace first newline with space
            }
            lastWasNewline = true;
        } else {
            lastWasNewline = false;
            result += ch;
        }
    }
    
    m_editor->ReplaceSelection(result);
}

//------------------------------------------------------------------------------
// Tools -> Format JSON (pretty-print)
//------------------------------------------------------------------------------
void MainWindow::OnToolsFormatJson() {
    // Operate on selection or whole document
    bool hasSelection = false;
    DWORD selStart, selEnd;
    m_editor->GetSelection(selStart, selEnd);
    hasSelection = (selStart != selEnd);
    
    std::wstring text = hasSelection ? m_editor->GetSelectedText() : m_editor->GetText();
    if (text.empty()) return;
    
    // Simple JSON pretty-printer
    std::wstring result;
    result.reserve(text.size() * 2);
    int indent = 0;
    bool inString = false;
    bool escaped = false;
    
    auto addNewline = [&]() {
        result += L"\r\n";
        for (int i = 0; i < indent; ++i) result += L"    ";
    };
    
    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t ch = text[i];
        
        if (escaped) {
            result += ch;
            escaped = false;
            continue;
        }
        
        if (ch == L'\\' && inString) {
            result += ch;
            escaped = true;
            continue;
        }
        
        if (ch == L'"') {
            inString = !inString;
            result += ch;
            continue;
        }
        
        if (inString) {
            result += ch;
            continue;
        }
        
        // Skip existing whitespace outside strings
        if (ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n') {
            continue;
        }
        
        if (ch == L'{' || ch == L'[') {
            result += ch;
            // Check if empty object/array
            size_t next = i + 1;
            while (next < text.size() && (text[next] == L' ' || text[next] == L'\t' || 
                   text[next] == L'\r' || text[next] == L'\n')) next++;
            if (next < text.size() && ((ch == L'{' && text[next] == L'}') || 
                                        (ch == L'[' && text[next] == L']'))) {
                result += text[next];
                i = next;
            } else {
                indent++;
                addNewline();
            }
        } else if (ch == L'}' || ch == L']') {
            indent--;
            if (indent < 0) indent = 0;
            addNewline();
            result += ch;
        } else if (ch == L',') {
            result += ch;
            addNewline();
        } else if (ch == L':') {
            result += L": ";
        } else {
            result += ch;
        }
    }
    
    if (hasSelection) {
        m_editor->ReplaceSelection(result);
    } else {
        m_editor->SelectAll();
        m_editor->ReplaceSelection(result);
    }
}

//------------------------------------------------------------------------------
// Tools -> Minify JSON
//------------------------------------------------------------------------------
void MainWindow::OnToolsMinifyJson() {
    bool hasSelection = false;
    DWORD selStart, selEnd;
    m_editor->GetSelection(selStart, selEnd);
    hasSelection = (selStart != selEnd);
    
    std::wstring text = hasSelection ? m_editor->GetSelectedText() : m_editor->GetText();
    if (text.empty()) return;
    
    std::wstring result;
    result.reserve(text.size());
    bool inString = false;
    bool escaped = false;
    
    for (wchar_t ch : text) {
        if (escaped) {
            result += ch;
            escaped = false;
            continue;
        }
        if (ch == L'\\' && inString) {
            result += ch;
            escaped = true;
            continue;
        }
        if (ch == L'"') {
            inString = !inString;
            result += ch;
            continue;
        }
        if (inString) {
            result += ch;
            continue;
        }
        // Skip whitespace outside strings
        if (ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n') {
            continue;
        }
        result += ch;
    }
    
    if (hasSelection) {
        m_editor->ReplaceSelection(result);
    } else {
        m_editor->SelectAll();
        m_editor->ReplaceSelection(result);
    }
}

//------------------------------------------------------------------------------
// Tools -> Open Terminal Here
//------------------------------------------------------------------------------
void MainWindow::OnToolsOpenTerminal() {
    std::wstring dir;
    if (!m_currentFile.empty() && !m_isNewFile) {
        size_t pos = m_currentFile.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
            dir = m_currentFile.substr(0, pos);
        }
    }
    
    if (dir.empty()) {
        wchar_t cwd[MAX_PATH] = {};
        GetCurrentDirectoryW(MAX_PATH, cwd);
        dir = cwd;
    }
    
    // Open PowerShell in the directory
    ShellExecuteW(nullptr, L"open", L"powershell.exe", nullptr, dir.c_str(), SW_SHOWNORMAL);
}

//------------------------------------------------------------------------------
// Notes -> Export Notes
//------------------------------------------------------------------------------
void MainWindow::OnNotesExport() {
    if (!m_noteStore || m_noteStore->GetNoteCount() == 0) {
        MessageBoxW(m_hwnd, L"There are no notes to export.", L"QNote", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    OPENFILENAMEW ofn = {};
    wchar_t filePath[MAX_PATH] = L"notes_export.json";
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Export Notes";
    ofn.lpstrDefExt = L"json";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    
    if (GetSaveFileNameW(&ofn)) {
        if (m_noteStore->ExportNotes(filePath)) {
            std::wstring msg = L"Successfully exported " + std::to_wstring(m_noteStore->GetNoteCount()) + L" notes.";
            MessageBoxW(m_hwnd, msg.c_str(), L"Export Complete", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxW(m_hwnd, L"Failed to export notes.", L"Export Error", MB_OK | MB_ICONERROR);
        }
    }
}

//------------------------------------------------------------------------------
// Notes -> Import Notes
//------------------------------------------------------------------------------
void MainWindow::OnNotesImport() {
    OPENFILENAMEW ofn = {};
    wchar_t filePath[MAX_PATH] = {};
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Import Notes";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    
    if (GetOpenFileNameW(&ofn)) {
        if (!m_noteStore) {
            InitializeNoteStore();
        }
        
        size_t countBefore = m_noteStore->GetNoteCount();
        if (m_noteStore->ImportNotes(filePath)) {
            size_t added = m_noteStore->GetNoteCount() - countBefore;
            std::wstring msg = L"Successfully imported " + std::to_wstring(added) + L" new notes.";
            MessageBoxW(m_hwnd, msg.c_str(), L"Import Complete", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxW(m_hwnd, L"Failed to import notes. The file may be invalid.", L"Import Error", MB_OK | MB_ICONERROR);
        }
    }
}

//------------------------------------------------------------------------------
// Keyboard shortcuts - Load custom shortcuts from INI file
//------------------------------------------------------------------------------
void MainWindow::LoadKeyboardShortcuts() {
    std::wstring settingsPath = m_settingsManager->GetSettingsPath();
    size_t pos = settingsPath.rfind(L"config.ini");
    if (pos == std::wstring::npos) return;
    
    std::wstring shortcutsPath = settingsPath.substr(0, pos) + L"shortcuts.ini";
    
    // If no custom shortcuts file exists, keep the default accelerator table
    if (GetFileAttributesW(shortcutsPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return;
    }
    
    // Define command name to ID mapping
    struct CmdMapping {
        const wchar_t* name;
        WORD cmdId;
    };
    
    static const CmdMapping cmdMap[] = {
        // File
        { L"FileNew",          IDM_FILE_NEW },
        { L"FileNewWindow",    IDM_FILE_NEWWINDOW },
        { L"FileOpen",         IDM_FILE_OPEN },
        { L"FileSave",         IDM_FILE_SAVE },
        { L"FileSaveAs",       IDM_FILE_SAVEAS },
        { L"FilePrint",        IDM_FILE_PRINT },
        // Edit
        { L"EditUndo",         IDM_EDIT_UNDO },
        { L"EditRedo",         IDM_EDIT_REDO },
        { L"EditCut",          IDM_EDIT_CUT },
        { L"EditCopy",         IDM_EDIT_COPY },
        { L"EditPaste",        IDM_EDIT_PASTE },
        { L"EditDelete",       IDM_EDIT_DELETE },
        { L"EditSelectAll",    IDM_EDIT_SELECTALL },
        { L"EditFind",         IDM_EDIT_FIND },
        { L"EditFindNext",     IDM_EDIT_FINDNEXT },
        { L"EditReplace",      IDM_EDIT_REPLACE },
        { L"EditGoTo",         IDM_EDIT_GOTO },
        { L"EditDateTime",     IDM_EDIT_DATETIME },
        // Text Operations
        { L"EditUppercase",    IDM_EDIT_UPPERCASE },
        { L"EditLowercase",    IDM_EDIT_LOWERCASE },
        // View
        { L"ViewZoomIn",       IDM_VIEW_ZOOMIN },
        { L"ViewZoomOut",      IDM_VIEW_ZOOMOUT },
        { L"ViewZoomReset",    IDM_VIEW_ZOOMRESET },
        // Tabs
        { L"TabNew",           IDM_TAB_NEW },
        { L"TabClose",         IDM_TAB_CLOSE },
        { L"TabNext",          IDM_TAB_NEXT },
        { L"TabPrev",          IDM_TAB_PREV },
        // Notes
        { L"NotesCapture",     IDM_NOTES_QUICKCAPTURE },
        { L"NotesAllNotes",    IDM_NOTES_ALLNOTES },
        { L"NotesSearch",      IDM_NOTES_SEARCH },
        // Bookmarks
        { L"ToggleBookmark",   IDM_EDIT_TOGGLEBOOKMARK },
        { L"NextBookmark",     IDM_EDIT_NEXTBOOKMARK },
        { L"PrevBookmark",     IDM_EDIT_PREVBOOKMARK },
        { L"ClearBookmarks",   IDM_EDIT_CLEARBOOKMARKS },
    };
    
    std::vector<ACCEL> accels;
    
    for (const auto& cmd : cmdMap) {
        wchar_t buf[256] = {};
        GetPrivateProfileStringW(L"Shortcuts", cmd.name, L"", buf, 256, shortcutsPath.c_str());
        
        std::wstring shortcut = buf;
        if (shortcut.empty()) continue;
        
        // Parse "Ctrl+Shift+K" style string
        BYTE fVirt = FVIRTKEY;
        WORD key = 0;
        
        std::wstring remaining = shortcut;
        // Parse modifiers
        while (true) {
            size_t plusPos = remaining.find(L'+');
            if (plusPos == std::wstring::npos) break;
            
            std::wstring modifier = remaining.substr(0, plusPos);
            remaining = remaining.substr(plusPos + 1);
            
            if (_wcsicmp(modifier.c_str(), L"Ctrl") == 0) fVirt |= FCONTROL;
            else if (_wcsicmp(modifier.c_str(), L"Shift") == 0) fVirt |= FSHIFT;
            else if (_wcsicmp(modifier.c_str(), L"Alt") == 0) fVirt |= FALT;
        }
        
        // remaining is the key name
        key = ParseVirtualKey(remaining);
        if (key == 0) continue;
        
        ACCEL accel = {};
        accel.fVirt = fVirt;
        accel.key = key;
        accel.cmd = cmd.cmdId;
        accels.push_back(accel);
    }
    
    if (!accels.empty()) {
        HACCEL hNewAccel = CreateAcceleratorTableW(accels.data(), static_cast<int>(accels.size()));
        if (hNewAccel) {
            if (m_hAccel) {
                DestroyAcceleratorTable(m_hAccel);
            }
            m_hAccel = hNewAccel;
        }
    }
}

//------------------------------------------------------------------------------
// Create default shortcuts INI file
//------------------------------------------------------------------------------
void MainWindow::CreateDefaultShortcutsFile(const std::wstring& path) {
    std::wstring content;
    content += L"; =============================================================================\r\n";
    content += L"; QNote - Keyboard Shortcuts Configuration\r\n";
    content += L"; =============================================================================\r\n";
    content += L";\r\n";
    content += L"; Format:    CommandName=Modifier+Key\r\n";
    content += L"; Modifiers: Ctrl, Shift, Alt (combine with +)\r\n";
    content += L"; Keys:      A-Z, 0-9, F1-F12, Tab, Delete, Escape, Enter, Space,\r\n";
    content += L";            Plus, Minus, Home, End, PageUp, PageDown, Insert, Backspace\r\n";
    content += L";\r\n";
    content += L"; To customize a shortcut, change the value after the = sign.\r\n";
    content += L"; To disable a shortcut, delete the value (e.g., FileNew=).\r\n";
    content += L"; Save this file and restart QNote to apply changes.\r\n";
    content += L";\r\n";
    content += L"\r\n[Shortcuts]\r\n";
    content += L"\r\n";
    content += L"; --- File ---\r\n";
    content += L"FileNew=Ctrl+N\r\n";
    content += L"FileNewWindow=Ctrl+Shift+N\r\n";
    content += L"FileOpen=Ctrl+O\r\n";
    content += L"FileSave=Ctrl+S\r\n";
    content += L"FileSaveAs=Ctrl+Shift+S\r\n";
    content += L"FilePrint=Ctrl+P\r\n";
    content += L"\r\n";
    content += L"; --- Edit ---\r\n";
    content += L"EditUndo=Ctrl+Z\r\n";
    content += L"EditRedo=Ctrl+Y\r\n";
    content += L"EditCut=Ctrl+X\r\n";
    content += L"EditCopy=Ctrl+C\r\n";
    content += L"EditPaste=Ctrl+V\r\n";
    content += L"EditDelete=Delete\r\n";
    content += L"EditSelectAll=Ctrl+A\r\n";
    content += L"EditFind=Ctrl+F\r\n";
    content += L"EditFindNext=F3\r\n";
    content += L"EditReplace=Ctrl+H\r\n";
    content += L"EditGoTo=Ctrl+G\r\n";
    content += L"EditDateTime=F5\r\n";
    content += L"\r\n";
    content += L"; --- Text Operations ---\r\n";
    content += L"EditUppercase=Ctrl+Shift+U\r\n";
    content += L"EditLowercase=Ctrl+U\r\n";
    content += L"\r\n";
    content += L"; --- View ---\r\n";
    content += L"ViewZoomIn=Ctrl+Plus\r\n";
    content += L"ViewZoomOut=Ctrl+Minus\r\n";
    content += L"ViewZoomReset=Ctrl+0\r\n";
    content += L"\r\n";
    content += L"; --- Tabs ---\r\n";
    content += L"TabNew=Ctrl+T\r\n";
    content += L"TabClose=Ctrl+W\r\n";
    content += L"TabNext=Ctrl+Tab\r\n";
    content += L"TabPrev=Ctrl+Shift+Tab\r\n";
    content += L"\r\n";
    content += L"; --- Notes ---\r\n";
    content += L"NotesCapture=Ctrl+Shift+Q\r\n";
    content += L"NotesAllNotes=Ctrl+Shift+A\r\n";
    content += L"NotesSearch=Ctrl+Shift+F\r\n";
    content += L"\r\n";
    content += L"; --- Bookmarks ---\r\n";
    content += L"ToggleBookmark=F2\r\n";
    content += L"NextBookmark=Ctrl+F2\r\n";
    content += L"PrevBookmark=Shift+F2\r\n";
    content += L"ClearBookmarks=Ctrl+Shift+F2\r\n";
    
    (void)FileIO::WriteFile(path, content, TextEncoding::UTF8, LineEnding::CRLF);
}

//------------------------------------------------------------------------------
// Parse key name to virtual key code
//------------------------------------------------------------------------------
WORD MainWindow::ParseVirtualKey(const std::wstring& keyName) {
    if (keyName.length() == 1) {
        wchar_t c = towupper(keyName[0]);
        if (c >= L'A' && c <= L'Z') return static_cast<WORD>(c);
        if (c >= L'0' && c <= L'9') return static_cast<WORD>(c);
    }
    
    if (_wcsicmp(keyName.c_str(), L"F1") == 0) return VK_F1;
    if (_wcsicmp(keyName.c_str(), L"F2") == 0) return VK_F2;
    if (_wcsicmp(keyName.c_str(), L"F3") == 0) return VK_F3;
    if (_wcsicmp(keyName.c_str(), L"F4") == 0) return VK_F4;
    if (_wcsicmp(keyName.c_str(), L"F5") == 0) return VK_F5;
    if (_wcsicmp(keyName.c_str(), L"F6") == 0) return VK_F6;
    if (_wcsicmp(keyName.c_str(), L"F7") == 0) return VK_F7;
    if (_wcsicmp(keyName.c_str(), L"F8") == 0) return VK_F8;
    if (_wcsicmp(keyName.c_str(), L"F9") == 0) return VK_F9;
    if (_wcsicmp(keyName.c_str(), L"F10") == 0) return VK_F10;
    if (_wcsicmp(keyName.c_str(), L"F11") == 0) return VK_F11;
    if (_wcsicmp(keyName.c_str(), L"F12") == 0) return VK_F12;
    if (_wcsicmp(keyName.c_str(), L"Tab") == 0) return VK_TAB;
    if (_wcsicmp(keyName.c_str(), L"Delete") == 0) return VK_DELETE;
    if (_wcsicmp(keyName.c_str(), L"Escape") == 0) return VK_ESCAPE;
    if (_wcsicmp(keyName.c_str(), L"Enter") == 0) return VK_RETURN;
    if (_wcsicmp(keyName.c_str(), L"Space") == 0) return VK_SPACE;
    if (_wcsicmp(keyName.c_str(), L"Plus") == 0) return VK_OEM_PLUS;
    if (_wcsicmp(keyName.c_str(), L"Minus") == 0) return VK_OEM_MINUS;
    if (_wcsicmp(keyName.c_str(), L"Home") == 0) return VK_HOME;
    if (_wcsicmp(keyName.c_str(), L"End") == 0) return VK_END;
    if (_wcsicmp(keyName.c_str(), L"PageUp") == 0) return VK_PRIOR;
    if (_wcsicmp(keyName.c_str(), L"PageDown") == 0) return VK_NEXT;
    if (_wcsicmp(keyName.c_str(), L"Insert") == 0) return VK_INSERT;
    if (_wcsicmp(keyName.c_str(), L"Backspace") == 0) return VK_BACK;
    
    return 0;
}

//------------------------------------------------------------------------------
// Format accelerator key to display string
//------------------------------------------------------------------------------
std::wstring MainWindow::FormatAccelKey(BYTE fVirt, WORD key) {
    std::wstring result;
    if (fVirt & FCONTROL) result += L"Ctrl+";
    if (fVirt & FSHIFT) result += L"Shift+";
    if (fVirt & FALT) result += L"Alt+";
    
    if (key >= 'A' && key <= 'Z') result += static_cast<wchar_t>(key);
    else if (key >= '0' && key <= '9') result += static_cast<wchar_t>(key);
    else if (key == VK_F1) result += L"F1";
    else if (key == VK_F2) result += L"F2";
    else if (key == VK_F3) result += L"F3";
    else if (key == VK_F4) result += L"F4";
    else if (key == VK_F5) result += L"F5";
    else if (key == VK_F6) result += L"F6";
    else if (key == VK_F7) result += L"F7";
    else if (key == VK_F8) result += L"F8";
    else if (key == VK_F9) result += L"F9";
    else if (key == VK_F10) result += L"F10";
    else if (key == VK_F11) result += L"F11";
    else if (key == VK_F12) result += L"F12";
    else if (key == VK_TAB) result += L"Tab";
    else if (key == VK_DELETE) result += L"Delete";
    else if (key == VK_ESCAPE) result += L"Escape";
    else if (key == VK_RETURN) result += L"Enter";
    else if (key == VK_SPACE) result += L"Space";
    else if (key == VK_OEM_PLUS) result += L"Plus";
    else if (key == VK_OEM_MINUS) result += L"Minus";
    else if (key == VK_HOME) result += L"Home";
    else if (key == VK_END) result += L"End";
    else if (key == VK_PRIOR) result += L"PageUp";
    else if (key == VK_NEXT) result += L"PageDown";
    else if (key == VK_INSERT) result += L"Insert";
    else if (key == VK_BACK) result += L"Backspace";
    else result += L"?";
    
    return result;
}

//------------------------------------------------------------------------------
// Encoding change handler
//------------------------------------------------------------------------------
void MainWindow::OnEncodingChange(TextEncoding encoding) {
    m_editor->SetEncoding(encoding);
    m_editor->SetModified(true);
    UpdateStatusBar();
    UpdateTitle();
}

//------------------------------------------------------------------------------
// Reopen current file with a specific encoding
//------------------------------------------------------------------------------
void MainWindow::OnReopenWithEncoding(TextEncoding encoding) {
    if (m_currentFile.empty() || m_isNewFile) {
        MessageBoxW(m_hwnd, L"No file is currently open to reopen.", L"QNote", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    if (m_editor->IsModified()) {
        int result = MessageBoxW(m_hwnd, 
            L"The file has unsaved changes. Reopening with a different encoding will discard them.\nContinue?",
            L"QNote", MB_YESNO | MB_ICONWARNING);
        if (result != IDYES) return;
    }
    
    m_ignoreNextFileChange = true;
    FileReadResult result = FileIO::ReadFileWithEncoding(m_currentFile, encoding);
    if (result.success) {
        m_editor->SetText(result.content);
        m_editor->SetEncoding(encoding);
        m_editor->SetLineEnding(result.detectedLineEnding);
        m_editor->SetModified(false);
        m_editor->SetSelection(0, 0);
        
        // Update document manager
        if (m_documentManager) {
            auto* doc = m_documentManager->GetActiveDocument();
            if (doc) {
                doc->encoding = encoding;
                doc->lineEnding = result.detectedLineEnding;
                doc->text = result.content;
            }
            m_documentManager->SetDocumentModified(m_documentManager->GetActiveTabId(), false);
        }
        
        StartFileMonitoring();
        UpdateTitle();
        UpdateStatusBar();
    } else {
        MessageBoxW(m_hwnd, result.errorMessage.c_str(), L"Error Reopening File", MB_OK | MB_ICONERROR);
    }
}

//------------------------------------------------------------------------------
// Help -> About
//------------------------------------------------------------------------------
void MainWindow::OnHelpAbout() {
    m_dialogManager->ShowAboutDialog();
}

//------------------------------------------------------------------------------
// Help -> Check for Updates
// Queries GitHub Releases API and compares version to current
//------------------------------------------------------------------------------
void MainWindow::OnHelpCheckUpdate() {
    // Current version from resource.h
    const int currentMajor = VER_MAJOR;
    const int currentMinor = VER_MINOR;
    const int currentPatch = VER_PATCH;
    
    std::wstring latestVersion;
    std::wstring downloadUrl;
    bool updateAvailable = false;
    bool checkFailed = false;
    
    // Initialize WinHTTP
    HINTERNET hSession = WinHttpOpen(
        L"QNote Update Checker/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    
    if (!hSession) {
        checkFailed = true;
    }
    
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;
    
    if (!checkFailed) {
        hConnect = WinHttpConnect(hSession, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) {
            checkFailed = true;
        }
    }
    
    if (!checkFailed) {
        hRequest = WinHttpOpenRequest(
            hConnect,
            L"GET",
            L"/repos/itzcozi/qnote/releases/latest",
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);
        if (!hRequest) {
            checkFailed = true;
        }
    }
    
    if (!checkFailed) {
        // Add User-Agent header (required by GitHub API)
        WinHttpAddRequestHeaders(hRequest, L"User-Agent: QNote", (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
        
        if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            checkFailed = true;
        }
    }
    
    if (!checkFailed) {
        if (!WinHttpReceiveResponse(hRequest, nullptr)) {
            checkFailed = true;
        }
    }
    
    std::string responseBody;
    if (!checkFailed) {
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                break;
            }
            
            if (dwSize == 0) break;
            
            std::vector<char> buffer(dwSize + 1, 0);
            if (!WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                break;
            }
            
            responseBody.append(buffer.data(), dwDownloaded);
        } while (dwSize > 0);
    }
    
    // Cleanup
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);
    
    // Parse response (simple JSON parsing for "tag_name" field)
    if (!checkFailed && !responseBody.empty()) {
        // Look for "tag_name": "X.Y.Z" or "tag_name": "vX.Y.Z"
        std::string tagKey = "\"tag_name\"";
        size_t tagPos = responseBody.find(tagKey);
        if (tagPos != std::string::npos) {
            size_t colonPos = responseBody.find(':', tagPos);
            size_t quoteStart = responseBody.find('"', colonPos + 1);
            size_t quoteEnd = responseBody.find('"', quoteStart + 1);
            
            if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                std::string tag = responseBody.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                
                // Remove leading 'v' if present
                if (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V')) {
                    tag = tag.substr(1);
                }
                
                // Parse version X.Y.Z
                int remoteMajor = 0, remoteMinor = 0, remotePatch = 0;
                if (sscanf_s(tag.c_str(), "%d.%d.%d", &remoteMajor, &remoteMinor, &remotePatch) >= 2) {
                    latestVersion = std::wstring(tag.begin(), tag.end());
                    
                    // Compare versions
                    if (remoteMajor > currentMajor ||
                        (remoteMajor == currentMajor && remoteMinor > currentMinor) ||
                        (remoteMajor == currentMajor && remoteMinor == currentMinor && remotePatch > currentPatch)) {
                        updateAvailable = true;
                        downloadUrl = L"https://github.com/itzcozi/qnote/releases/latest";
                    }
                }
            }
        }
        
        // Look for html_url for the release page
        std::string urlKey = "\"html_url\"";
        size_t urlPos = responseBody.find(urlKey);
        if (urlPos != std::string::npos) {
            size_t colonPos = responseBody.find(':', urlPos);
            size_t quoteStart = responseBody.find('"', colonPos + 1);
            size_t quoteEnd = responseBody.find('"', quoteStart + 1);
            
            if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                std::string url = responseBody.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                downloadUrl = std::wstring(url.begin(), url.end());
            }
        }
    } else if (responseBody.empty()) {
        checkFailed = true;
    }
    
    // Show result to user
    if (checkFailed) {
        MessageBoxW(m_hwnd,
            L"Unable to check for updates.\n\n"
            L"Please check your internet connection or visit:\n"
            L"https://github.com/itzcozi/qnote/releases",
            L"Update Check Failed",
            MB_OK | MB_ICONWARNING);
    } else if (updateAvailable) {
        std::wstring message = L"A new version of QNote is available!\n\n"
            L"Current version: " + std::to_wstring(currentMajor) + L"." + 
            std::to_wstring(currentMinor) + L"." + std::to_wstring(currentPatch) + L"\n"
            L"Latest version: " + latestVersion + L"\n\n"
            L"Would you like to open the download page?";
        
        int result = MessageBoxW(m_hwnd, message.c_str(), L"Update Available", MB_YESNO | MB_ICONINFORMATION);
        if (result == IDYES) {
            ShellExecuteW(nullptr, L"open", downloadUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    } else {
        std::wstring message = L"You are running the latest version of QNote.\n\n"
            L"Version: " + std::to_wstring(currentMajor) + L"." + 
            std::to_wstring(currentMinor) + L"." + std::to_wstring(currentPatch);
        
        MessageBoxW(m_hwnd, message.c_str(), L"No Updates Available", MB_OK | MB_ICONINFORMATION);
    }
}

//------------------------------------------------------------------------------
// Update window title
//------------------------------------------------------------------------------
void MainWindow::UpdateTitle() {
    std::wstring title;
    
    // Add modification indicator
    if (m_editor->IsModified()) {
        title = L"*";
    }
    
    // Check if document manager has an active document with a custom title
    if (m_documentManager) {
        auto* doc = m_documentManager->GetActiveDocument();
        if (doc && !doc->customTitle.empty()) {
            title += doc->customTitle;
            title += L" - QNote";
            SetWindowTextW(m_hwnd, title.c_str());
            return;
        }
    }
    
    // Handle note mode vs file mode
    if (m_isNoteMode) {
        // Note mode - show note title
        if (m_currentNoteId.empty()) {
            title += L"New Note";
        } else if (m_noteStore) {
            auto note = m_noteStore->GetNote(m_currentNoteId);
            if (note) {
                std::wstring noteTitle = note->GetDisplayTitle();
                if (note->isPinned) {
                    title += L"\u2605 ";  // Star for pinned
                }
                title += noteTitle;
            } else {
                title += L"Note";
            }
        } else {
            title += L"Note";
        }
        title += L" - QNote Notes";
    } else {
        // File mode - show filename
        if (m_isNewFile || m_currentFile.empty()) {
            title += L"Untitled";
        } else {
            title += FileIO::GetFileName(m_currentFile);
        }
        title += L" - QNote";
    }
    
    SetWindowTextW(m_hwnd, title.c_str());
}

//------------------------------------------------------------------------------
// Update status bar
//------------------------------------------------------------------------------
void MainWindow::UpdateStatusBar() {
    if (!m_hwndStatus || !IsWindowVisible(m_hwndStatus)) {
        return;
    }
    
    // Line and column
    int line = m_editor->GetCurrentLine() + 1;
    int column = m_editor->GetCurrentColumn() + 1;
    
    // Check for selection
    DWORD selStart, selEnd;
    m_editor->GetSelection(selStart, selEnd);
    
    wchar_t posText[128];
    if (selStart != selEnd) {
        DWORD selLen = (selEnd > selStart) ? (selEnd - selStart) : (selStart - selEnd);
        swprintf_s(posText, L"Ln %d, Col %d  |  %lu sel", line, column, static_cast<unsigned long>(selLen));
    } else {
        swprintf_s(posText, L"Ln %d, Col %d", line, column);
    }
    SendMessageW(m_hwndStatus, SB_SETTEXTW, SB_PART_POSITION, reinterpret_cast<LPARAM>(posText));
    
    // Encoding
    const wchar_t* encodingText = EncodingToString(m_editor->GetEncoding());
    SendMessageW(m_hwndStatus, SB_SETTEXTW, SB_PART_ENCODING, reinterpret_cast<LPARAM>(encodingText));
    
    // Line ending
    const wchar_t* eolText = LineEndingToString(m_editor->GetLineEnding());
    SendMessageW(m_hwndStatus, SB_SETTEXTW, SB_PART_EOL, reinterpret_cast<LPARAM>(eolText));
    
    // Zoom
    wchar_t zoomText[32];
    swprintf_s(zoomText, L"%d%%", m_settingsManager->GetSettings().zoomLevel);
    SendMessageW(m_hwndStatus, SB_SETTEXTW, SB_PART_ZOOM, reinterpret_cast<LPARAM>(zoomText));
    
    // Word and character count
    int textLen = m_editor->GetTextLength();
    // Simple word count: count transitions from whitespace to non-whitespace
    int wordCount = 0;
    if (textLen > 0) {
        std::wstring text = m_editor->GetText();
        bool inWord = false;
        for (wchar_t c : text) {
            if (c == L' ' || c == L'\t' || c == L'\r' || c == L'\n') {
                inWord = false;
            } else if (!inWord) {
                inWord = true;
                wordCount++;
            }
        }
    }
    wchar_t countText[64];
    swprintf_s(countText, L"%d words, %d chars", wordCount, textLen);
    SendMessageW(m_hwndStatus, SB_SETTEXTW, SB_PART_COUNTS, reinterpret_cast<LPARAM>(countText));
}

//------------------------------------------------------------------------------
// Update menu item states
//------------------------------------------------------------------------------
void MainWindow::UpdateMenuState() {
    HMENU hMenu = GetMenu(m_hwnd);
    if (!hMenu) return;
    
    // Edit menu state
    bool canUndo = m_editor->CanUndo();
    bool canRedo = m_editor->CanRedo();
    bool hasSelection = false;
    DWORD start, end;
    m_editor->GetSelection(start, end);
    hasSelection = (start != end);
    
    // Check clipboard for paste
    bool canPaste = IsClipboardFormatAvailable(CF_UNICODETEXT) || 
                    IsClipboardFormatAvailable(CF_TEXT);
    
    EnableMenuItem(hMenu, IDM_EDIT_UNDO, MF_BYCOMMAND | (canUndo ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_EDIT_REDO, MF_BYCOMMAND | (canRedo ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_EDIT_CUT, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_EDIT_COPY, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_EDIT_PASTE, MF_BYCOMMAND | (canPaste ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_EDIT_DELETE, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
    
    // Text operation menu states
    EnableMenuItem(hMenu, IDM_EDIT_UPPERCASE, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_EDIT_LOWERCASE, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
    
    // File operation states
    bool hasFile = !m_isNewFile && !m_currentFile.empty();
    EnableMenuItem(hMenu, IDM_FILE_REVERT, MF_BYCOMMAND | (hasFile ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_FILE_OPENCONTAINING, MF_BYCOMMAND | (hasFile ? MF_ENABLED : MF_GRAYED));
    
    // Reopen with encoding requires a file
    EnableMenuItem(hMenu, IDM_ENCODING_REOPEN_UTF8, MF_BYCOMMAND | (hasFile ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_ENCODING_REOPEN_UTF8BOM, MF_BYCOMMAND | (hasFile ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_ENCODING_REOPEN_UTF16LE, MF_BYCOMMAND | (hasFile ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_ENCODING_REOPEN_UTF16BE, MF_BYCOMMAND | (hasFile ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_ENCODING_REOPEN_ANSI, MF_BYCOMMAND | (hasFile ? MF_ENABLED : MF_GRAYED));
    
    // Format menu state
    CheckMenuItem(hMenu, IDM_FORMAT_WORDWRAP, 
                  MF_BYCOMMAND | (m_editor->IsWordWrapEnabled() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_FORMAT_RTL,
                  MF_BYCOMMAND | (m_editor->IsRTL() ? MF_CHECKED : MF_UNCHECKED));
    
    // Line ending checkmarks
    LineEnding eol = m_editor->GetLineEnding();
    CheckMenuItem(hMenu, IDM_FORMAT_EOL_CRLF, 
                  MF_BYCOMMAND | (eol == LineEnding::CRLF ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_FORMAT_EOL_LF, 
                  MF_BYCOMMAND | (eol == LineEnding::LF ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_FORMAT_EOL_CR, 
                  MF_BYCOMMAND | (eol == LineEnding::CR ? MF_CHECKED : MF_UNCHECKED));
    
    // View menu state
    CheckMenuItem(hMenu, IDM_VIEW_STATUSBAR,
                  MF_BYCOMMAND | (m_settingsManager->GetSettings().showStatusBar ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_VIEW_LINENUMBERS,
                  MF_BYCOMMAND | (m_settingsManager->GetSettings().showLineNumbers ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_VIEW_SHOWWHITESPACE,
                  MF_BYCOMMAND | (m_editor->IsShowWhitespace() ? MF_CHECKED : MF_UNCHECKED));
    
    // Encoding checkmarks
    TextEncoding enc = m_editor->GetEncoding();
    CheckMenuItem(hMenu, IDM_ENCODING_UTF8, 
                  MF_BYCOMMAND | (enc == TextEncoding::UTF8 ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_ENCODING_UTF8BOM, 
                  MF_BYCOMMAND | (enc == TextEncoding::UTF8_BOM ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_ENCODING_UTF16LE, 
                  MF_BYCOMMAND | (enc == TextEncoding::UTF16_LE ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_ENCODING_UTF16BE, 
                  MF_BYCOMMAND | (enc == TextEncoding::UTF16_BE ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_ENCODING_ANSI, 
                  MF_BYCOMMAND | (enc == TextEncoding::ANSI ? MF_CHECKED : MF_UNCHECKED));
    
    // Tools menu state
    EnableMenuItem(hMenu, IDM_TOOLS_COPYFILEPATH, MF_BYCOMMAND | (hasFile ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_TOOLS_OPENTERMINAL, MF_BYCOMMAND | (hasFile ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_TOOLS_URLENCODE, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_TOOLS_URLDECODE, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_TOOLS_BASE64ENCODE, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_TOOLS_BASE64DECODE, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_TOOLS_JOINLINES, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
}

//------------------------------------------------------------------------------
// Update recent files menu
//------------------------------------------------------------------------------
void MainWindow::UpdateRecentFilesMenu() {
    HMENU hMainMenu = GetMenu(m_hwnd);
    if (!hMainMenu) return;
    
    // Get File menu
    HMENU hFileMenu = GetSubMenu(hMainMenu, 0);
    if (!hFileMenu) return;
    
    // Find Recent Files submenu
    HMENU hRecentMenu = nullptr;
    int itemCount = GetMenuItemCount(hFileMenu);
    for (int i = 0; i < itemCount; i++) {
        HMENU hSub = GetSubMenu(hFileMenu, i);
        if (hSub) {
            wchar_t text[64] = {};
            GetMenuStringW(hFileMenu, i, text, 64, MF_BYPOSITION);
            if (wcsstr(text, L"Recent") != nullptr) {
                hRecentMenu = hSub;
                break;
            }
        }
    }
    
    if (!hRecentMenu) return;
    
    // Clear existing items
    while (GetMenuItemCount(hRecentMenu) > 0) {
        DeleteMenu(hRecentMenu, 0, MF_BYPOSITION);
    }
    
    // Add recent files
    const auto& recentFiles = m_settingsManager->GetSettings().recentFiles;
    
    if (recentFiles.empty()) {
        AppendMenuW(hRecentMenu, MF_STRING | MF_GRAYED, 0, L"(Empty)");
    } else {
        for (size_t i = 0; i < recentFiles.size(); i++) {
            std::wstring menuText;
            if (i < 9) {
                menuText = L"&" + std::to_wstring(i + 1) + L" ";
            } else {
                menuText = std::to_wstring(i + 1) + L" ";
            }
            menuText += recentFiles[i];
            
            AppendMenuW(hRecentMenu, MF_STRING, IDM_FILE_RECENT_BASE + static_cast<UINT>(i), 
                        menuText.c_str());
        }
    }
}

//------------------------------------------------------------------------------
// Prompt to save changes
//------------------------------------------------------------------------------
bool MainWindow::PromptSaveChanges() {
    if (!m_editor->IsModified()) {
        return true;
    }
    
    // An untitled file with whitespace-only content has nothing to save
    if (m_isNewFile && IsWhitespaceOnly(m_editor->GetText())) {
        return true;
    }
    
    std::wstring message = L"Do you want to save changes";
    if (!m_isNewFile && !m_currentFile.empty()) {
        message += L" to " + FileIO::GetFileName(m_currentFile);
    }
    message += L"?";
    
    int result = MessageBoxW(m_hwnd, message.c_str(), L"QNote", 
                             MB_YESNOCANCEL | MB_ICONWARNING);
    
    switch (result) {
        case IDYES:
            return OnFileSave();
        case IDNO:
            return true;
        case IDCANCEL:
        default:
            return false;
    }
}

//------------------------------------------------------------------------------
// Load file
//------------------------------------------------------------------------------
bool MainWindow::LoadFile(const std::wstring& filePath) {
    FileReadResult result = FileIO::ReadFile(filePath);
    
    if (!result.success) {
        MessageBoxW(m_hwnd, result.errorMessage.c_str(), L"Error Opening File", 
                    MB_OK | MB_ICONERROR);
        return false;
    }
    
    m_editor->SetText(result.content);
    m_editor->SetEncoding(result.detectedEncoding);
    m_editor->SetLineEnding(result.detectedLineEnding);
    m_editor->SetModified(false);
    
    m_currentFile = filePath;
    m_isNewFile = false;
    
    // Sync with document manager
    if (m_documentManager) {
        int activeTab = m_documentManager->GetActiveTabId();
        m_documentManager->SetDocumentFilePath(activeTab, filePath);
        m_documentManager->SetDocumentModified(activeTab, false);
        
        auto* doc = m_documentManager->GetActiveDocument();
        if (doc) {
            doc->encoding = result.detectedEncoding;
            doc->lineEnding = result.detectedLineEnding;
            doc->text = result.content;
            doc->isNewFile = false;
            doc->isNoteMode = false;
            doc->noteId.clear();
        }
    }
    
    // Add to recent files
    m_settingsManager->AddRecentFile(filePath);
    UpdateRecentFilesMenu();
    
    UpdateTitle();
    UpdateStatusBar();
    
    // Start monitoring the file for external changes
    StartFileMonitoring();
    
    // Move cursor to start
    m_editor->SetSelection(0, 0);
    m_editor->SetFocus();
    
    return true;
}

//------------------------------------------------------------------------------
// Save file
//------------------------------------------------------------------------------
bool MainWindow::SaveFile(const std::wstring& filePath) {
    std::wstring text = m_editor->GetText();
    
    FileWriteResult result = FileIO::WriteFile(
        filePath,
        text,
        m_editor->GetEncoding(),
        m_editor->GetLineEnding()
    );
    
    if (!result.success) {
        MessageBoxW(m_hwnd, result.errorMessage.c_str(), L"Error Saving File",
                    MB_OK | MB_ICONERROR);
        return false;
    }
    
    m_editor->SetModified(false);
    
    // Delete auto-save backup on successful save
    DeleteAutoSaveBackup();
    
    // Refresh file write time to avoid false change detection
    m_ignoreNextFileChange = true;
    StartFileMonitoring();
    
    // Add to recent files
    m_settingsManager->AddRecentFile(filePath);
    UpdateRecentFilesMenu();
    
    UpdateTitle();
    
    return true;
}

//------------------------------------------------------------------------------
// Create new document
//------------------------------------------------------------------------------
void MainWindow::NewDocument() {
    // Reset the document state in the DocumentManager (text, path, title, etc.)
    if (m_documentManager) {
        m_documentManager->ResetActiveDocument();
    }
    
    m_currentFile.clear();
    m_isNewFile = true;
    m_isNoteMode = false;
    m_currentNoteId.clear();
    
    StartFileMonitoring();
    UpdateTitle();
    UpdateStatusBar();
    
    // Update line numbers gutter
    if (m_lineNumbersGutter && m_lineNumbersGutter->IsVisible()) {
        m_lineNumbersGutter->Update();
    }
    
    m_editor->SetFocus();
}

//------------------------------------------------------------------------------
// Create status bar
//------------------------------------------------------------------------------
void MainWindow::CreateStatusBar() {
    m_hwndStatus = CreateWindowExW(
        0,
        STATUSCLASSNAMEW,
        nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        m_hwnd,
        nullptr,
        m_hInstance,
        nullptr
    );
    
    if (m_hwndStatus) {
        // Set up status bar parts
        int widths[STATUS_PARTS] = { 150, 250, 330, 400, -1 };
        SendMessageW(m_hwndStatus, SB_SETPARTS, STATUS_PARTS, reinterpret_cast<LPARAM>(widths));
    }
    
    // Show/hide based on settings
    if (!m_settingsManager->GetSettings().showStatusBar) {
        ShowWindow(m_hwndStatus, SW_HIDE);
    }
}

//------------------------------------------------------------------------------
// Resize child controls
//------------------------------------------------------------------------------
void MainWindow::ResizeControls() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    
    int statusHeight = 0;
    int findBarHeight = 0;
    int gutterWidth = 0;
    int tabBarHeight = 0;
    int topOffset = 0;  // No gap below menu
    
    // Position status bar
    if (m_hwndStatus && IsWindowVisible(m_hwndStatus)) {
        SendMessageW(m_hwndStatus, WM_SIZE, 0, 0);
        
        RECT statusRect;
        GetWindowRect(m_hwndStatus, &statusRect);
        statusHeight = statusRect.bottom - statusRect.top;
    }
    
    // Position tab bar below menu
    if (m_tabBar && m_tabBar->GetHandle()) {
        tabBarHeight = m_tabBar->GetHeight();
        m_tabBar->Resize(0, topOffset, rc.right);
    }
    
    // Position find bar below tab bar
    if (m_findBar && m_findBar->IsVisible()) {
        findBarHeight = m_findBar->GetHeight();
        m_findBar->Resize(0, topOffset + tabBarHeight, rc.right);
    }
    
    // Calculate content area top position
    int contentTop = topOffset + tabBarHeight + findBarHeight;
    int contentHeight = rc.bottom - statusHeight - contentTop;
    
    // Position line numbers gutter
    if (m_lineNumbersGutter && m_lineNumbersGutter->IsVisible()) {
        gutterWidth = m_lineNumbersGutter->GetWidth();
        m_lineNumbersGutter->Resize(0, contentTop, contentHeight);
    }
    
    // Position editor next to gutter
    if (m_editor) {
        m_editor->Resize(gutterWidth, contentTop, rc.right - gutterWidth, contentHeight);
    }
    
    // Update gutter display after editor resize
    if (m_lineNumbersGutter && m_lineNumbersGutter->IsVisible()) {
        m_lineNumbersGutter->Update();
    }
}

//------------------------------------------------------------------------------
// Initialize note store and related components
//------------------------------------------------------------------------------
void MainWindow::InitializeNoteStore() {
    // Initialize the note store
    if (m_noteStore) {
        (void)m_noteStore->Initialize();
    }
    
    // Create capture window
    m_captureWindow = std::make_unique<CaptureWindow>();
    (void)m_captureWindow->Create(m_hInstance, m_noteStore.get());
    // Set callback for when user wants to edit in main window
    m_captureWindow->SetEditCallback([this](const std::wstring& noteId) {
        OpenNoteFromId(noteId);
    });
    
    // Create note list window
    m_noteListWindow = std::make_unique<NoteListWindow>();
    (void)m_noteListWindow->Create(m_hInstance, m_hwnd, m_noteStore.get());
    // Set callback for opening notes
    m_noteListWindow->SetOpenCallback([this](const Note& note) {
        LoadNoteIntoEditor(note);
    });
    
    // Register global hotkey (Ctrl+Shift+Q)
    if (m_hotkeyManager) {
        (void)m_hotkeyManager->Register(m_hwnd, HOTKEY_QUICKCAPTURE);
    }
}

//------------------------------------------------------------------------------
// Auto-save current note
//------------------------------------------------------------------------------
void MainWindow::AutoSaveCurrentNote() {
    if (!m_isNoteMode || !m_noteStore || !m_editor) return;
    
    std::wstring content = m_editor->GetText();
    
    if (m_currentNoteId.empty()) {
        // Create a new note
        Note newNote = m_noteStore->CreateNote(content);
        m_currentNoteId = newNote.id;
    } else {
        // Update existing note
        auto existingNote = m_noteStore->GetNote(m_currentNoteId);
        if (existingNote) {
            Note updated = *existingNote;
            updated.content = content;
            (void)m_noteStore->UpdateNote(updated);
        }
    }
    
    m_editor->SetModified(false);
    UpdateTitle();
}

//------------------------------------------------------------------------------
// Load a note into the editor
//------------------------------------------------------------------------------
void MainWindow::LoadNoteIntoEditor(const Note& note) {
    // Save current note if in note mode
    if (m_isNoteMode && m_editor->IsModified()) {
        AutoSaveCurrentNote();
    }
    
    // Switch to note mode
    m_isNoteMode = true;
    m_currentNoteId = note.id;
    m_currentFile.clear();
    m_isNewFile = true;
    
    // Load note content
    m_editor->SetText(note.content);
    m_editor->SetModified(false);
    m_editor->SetSelection(0, 0);
    m_editor->SetFocus();
    
    UpdateTitle();
    UpdateStatusBar();
    
    // Bring window to front
    SetForegroundWindow(m_hwnd);
}

//------------------------------------------------------------------------------
// Open a note by ID
//------------------------------------------------------------------------------
void MainWindow::OpenNoteFromId(const std::wstring& noteId) {
    if (!m_noteStore) return;
    
    auto note = m_noteStore->GetNote(noteId);
    if (note) {
        LoadNoteIntoEditor(*note);
    }
}

//------------------------------------------------------------------------------
// Handle global hotkey
//------------------------------------------------------------------------------
void MainWindow::OnHotkey(int hotkeyId) {
    if (hotkeyId == HOTKEY_QUICKCAPTURE) {
        OnNotesQuickCapture();
    }
}

//------------------------------------------------------------------------------
// Notes -> New Note
//------------------------------------------------------------------------------
void MainWindow::OnNotesNew() {
    // Save current note if in note mode
    if (m_isNoteMode && m_editor->IsModified()) {
        AutoSaveCurrentNote();
    }
    
    // Switch to note mode with a new note
    m_isNoteMode = true;
    m_currentNoteId.clear();
    m_currentFile.clear();
    m_isNewFile = true;
    
    m_editor->Clear();
    m_editor->SetModified(false);
    m_editor->SetFocus();
    
    UpdateTitle();
    UpdateStatusBar();
}

//------------------------------------------------------------------------------
// Notes -> Quick Capture
//------------------------------------------------------------------------------
void MainWindow::OnNotesQuickCapture() {
    if (m_captureWindow) {
        m_captureWindow->Toggle();
    }
}

//------------------------------------------------------------------------------
// Notes -> All Notes
//------------------------------------------------------------------------------
void MainWindow::OnNotesAllNotes() {
    if (m_noteListWindow) {
        m_noteListWindow->SetViewMode(NoteListViewMode::AllNotes);
        m_noteListWindow->Show();
    }
}

//------------------------------------------------------------------------------
// Notes -> Pinned Notes
//------------------------------------------------------------------------------
void MainWindow::OnNotesPinned() {
    if (m_noteListWindow) {
        m_noteListWindow->SetViewMode(NoteListViewMode::Pinned);
        m_noteListWindow->Show();
    }
}

//------------------------------------------------------------------------------
// Notes -> Timeline
//------------------------------------------------------------------------------
void MainWindow::OnNotesTimeline() {
    if (m_noteListWindow) {
        m_noteListWindow->SetViewMode(NoteListViewMode::Timeline);
        m_noteListWindow->Show();
    }
}

//------------------------------------------------------------------------------
// Notes -> Search
//------------------------------------------------------------------------------
void MainWindow::OnNotesSearch() {
    if (m_noteListWindow) {
        m_noteListWindow->SetViewMode(NoteListViewMode::AllNotes);
        m_noteListWindow->Show();
        // Focus the search box (will need to add this method to NoteListWindow)
    }
}

//------------------------------------------------------------------------------
// Notes -> Pin/Unpin Current Note
//------------------------------------------------------------------------------
void MainWindow::OnNotesPinCurrent() {
    if (!m_isNoteMode || m_currentNoteId.empty() || !m_noteStore) {
        MessageBoxW(m_hwnd, L"No note is currently being edited.", L"QNote", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    (void)m_noteStore->TogglePin(m_currentNoteId);
    
    auto note = m_noteStore->GetNote(m_currentNoteId);
    if (note) {
        std::wstring msg = note->isPinned ? L"Note pinned." : L"Note unpinned.";
        // Could show in status bar instead
        UpdateTitle();
    }
}

//------------------------------------------------------------------------------
// Notes -> Save Note Now
//------------------------------------------------------------------------------
void MainWindow::OnNotesSaveNow() {
    if (m_isNoteMode) {
        AutoSaveCurrentNote();
        // Brief visual feedback could be added here
    } else {
        // In file mode, use regular save
        OnFileSave();
    }
}

//------------------------------------------------------------------------------
// Notes -> Delete Current Note
//------------------------------------------------------------------------------
void MainWindow::OnNotesDeleteCurrent() {
    if (!m_isNoteMode || m_currentNoteId.empty() || !m_noteStore) {
        MessageBoxW(m_hwnd, L"No note is currently being edited.", L"QNote", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    auto note = m_noteStore->GetNote(m_currentNoteId);
    std::wstring title = note ? note->GetDisplayTitle() : L"this note";
    
    std::wstring msg = L"Delete \"" + title + L"\"? This cannot be undone.";
    if (MessageBoxW(m_hwnd, msg.c_str(), L"Delete Note", MB_YESNO | MB_ICONWARNING) == IDYES) {
        (void)m_noteStore->DeleteNote(m_currentNoteId);
        
        // Start a new note
        OnNotesNew();
        
        // Refresh note list if visible
        if (m_noteListWindow && m_noteListWindow->IsVisible()) {
            m_noteListWindow->RefreshList();
        }
    }
}

//------------------------------------------------------------------------------
// Auto-save file backup (.autosave file)
//------------------------------------------------------------------------------
void MainWindow::AutoSaveFileBackup() {
    // Only auto-save in file mode with a real file
    if (m_isNoteMode || m_isNewFile || m_currentFile.empty()) {
        return;
    }
    
    // Check if auto-save is enabled and file is modified
    if (!m_settingsManager->GetSettings().fileAutoSave || !m_editor->IsModified()) {
        return;
    }
    
    // Create auto-save path
    std::wstring autoSavePath = m_currentFile + L".autosave";
    
    // Write the backup file
    std::wstring text = m_editor->GetText();
    FileWriteResult result = FileIO::WriteFile(
        autoSavePath,
        text,
        m_editor->GetEncoding(),
        m_editor->GetLineEnding()
    );
    
    // Silent failure - don't bother user with auto-save errors
    (void)result;
}

//------------------------------------------------------------------------------
// Delete auto-save backup file
//------------------------------------------------------------------------------
void MainWindow::DeleteAutoSaveBackup() {
    if (m_currentFile.empty()) {
        return;
    }
    
    std::wstring autoSavePath = m_currentFile + L".autosave";
    
    // Delete the backup file if it exists
    if (GetFileAttributesW(autoSavePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        DeleteFileW(autoSavePath.c_str());
    }
}

//------------------------------------------------------------------------------
// Editor scroll callback for line numbers sync
//------------------------------------------------------------------------------
void MainWindow::OnEditorScroll(void* userData) {
    MainWindow* self = static_cast<MainWindow*>(userData);
    if (self && self->m_lineNumbersGutter && self->m_lineNumbersGutter->IsVisible()) {
        self->m_lineNumbersGutter->OnEditorScroll();
    }
}

//------------------------------------------------------------------------------
// File -> Revert to Saved
//------------------------------------------------------------------------------
void MainWindow::OnFileRevert() {
    if (m_currentFile.empty() || m_isNewFile) {
        MessageBoxW(m_hwnd, L"No file to revert to.", L"QNote", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    if (m_editor->IsModified()) {
        int result = MessageBoxW(m_hwnd,
            L"Discard all changes and reload the file from disk?",
            L"Revert to Saved", MB_YESNO | MB_ICONWARNING);
        if (result != IDYES) return;
    }
    
    m_ignoreNextFileChange = true;
    LoadFile(m_currentFile);
}

//------------------------------------------------------------------------------
// File -> Open Containing Folder
//------------------------------------------------------------------------------
void MainWindow::OnFileOpenContainingFolder() {
    if (m_currentFile.empty() || m_isNewFile) {
        MessageBoxW(m_hwnd, L"No file is currently open.", L"QNote", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    // Use explorer.exe /select to open the folder and highlight the file
    std::wstring params = L"/select,\"" + m_currentFile + L"\"";
    ShellExecuteW(nullptr, L"open", L"explorer.exe", params.c_str(), nullptr, SW_SHOWNORMAL);
}

//------------------------------------------------------------------------------
// Edit -> Uppercase Selection
//------------------------------------------------------------------------------
void MainWindow::OnEditUppercase() {
    std::wstring sel = m_editor->GetSelectedText();
    if (sel.empty()) return;
    
    std::transform(sel.begin(), sel.end(), sel.begin(), ::towupper);
    m_editor->ReplaceSelection(sel);
}

//------------------------------------------------------------------------------
// Edit -> Lowercase Selection
//------------------------------------------------------------------------------
void MainWindow::OnEditLowercase() {
    std::wstring sel = m_editor->GetSelectedText();
    if (sel.empty()) return;
    
    std::transform(sel.begin(), sel.end(), sel.begin(), ::towlower);
    m_editor->ReplaceSelection(sel);
}

//------------------------------------------------------------------------------
// Edit -> Sort Lines (ascending or descending)
//------------------------------------------------------------------------------
void MainWindow::OnEditSortLines(bool ascending) {
    std::wstring text = m_editor->GetText();
    if (text.empty()) return;
    
    // Split into lines
    std::vector<std::wstring> lines;
    std::wistringstream stream(text);
    std::wstring line;
    while (std::getline(stream, line)) {
        // Remove trailing \r if present
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    
    // Sort
    if (ascending) {
        std::sort(lines.begin(), lines.end(), [](const std::wstring& a, const std::wstring& b) {
            return _wcsicmp(a.c_str(), b.c_str()) < 0;
        });
    } else {
        std::sort(lines.begin(), lines.end(), [](const std::wstring& a, const std::wstring& b) {
            return _wcsicmp(a.c_str(), b.c_str()) > 0;
        });
    }
    
    // Rejoin with \r\n (RichEdit uses \r internally, but SetText handles it)
    std::wstring result;
    for (size_t i = 0; i < lines.size(); ++i) {
        result += lines[i];
        if (i < lines.size() - 1) {
            result += L"\r\n";
        }
    }
    
    m_editor->SelectAll();
    m_editor->ReplaceSelection(result);
}

//------------------------------------------------------------------------------
// Edit -> Trim Trailing Whitespace
//------------------------------------------------------------------------------
void MainWindow::OnEditTrimWhitespace() {
    std::wstring text = m_editor->GetText();
    if (text.empty()) return;
    
    std::vector<std::wstring> lines;
    std::wistringstream stream(text);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        // Trim trailing whitespace
        size_t end = line.find_last_not_of(L" \t");
        if (end != std::wstring::npos) {
            line = line.substr(0, end + 1);
        } else if (!line.empty()) {
            line.clear();  // Line was all whitespace
        }
        lines.push_back(line);
    }
    
    std::wstring result;
    for (size_t i = 0; i < lines.size(); ++i) {
        result += lines[i];
        if (i < lines.size() - 1) {
            result += L"\r\n";
        }
    }
    
    m_editor->SelectAll();
    m_editor->ReplaceSelection(result);
}

//------------------------------------------------------------------------------
// Edit -> Remove Duplicate Lines
//------------------------------------------------------------------------------
void MainWindow::OnEditRemoveDuplicateLines() {
    std::wstring text = m_editor->GetText();
    if (text.empty()) return;
    
    std::vector<std::wstring> lines;
    std::set<std::wstring> seen;
    std::wistringstream stream(text);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        if (seen.insert(line).second) {
            lines.push_back(line);
        }
    }
    
    std::wstring result;
    for (size_t i = 0; i < lines.size(); ++i) {
        result += lines[i];
        if (i < lines.size() - 1) {
            result += L"\r\n";
        }
    }
    
    m_editor->SelectAll();
    m_editor->ReplaceSelection(result);
}

//------------------------------------------------------------------------------
// File change monitoring
//------------------------------------------------------------------------------
void MainWindow::StartFileMonitoring() {
    if (!m_currentFile.empty() && !m_isNewFile) {
        m_lastWriteTime = GetFileLastWriteTime(m_currentFile);
    } else {
        m_lastWriteTime = {};
    }
}

void MainWindow::CheckFileChanged() {
    if (m_currentFile.empty() || m_isNewFile || m_isNoteMode) return;
    if (m_ignoreNextFileChange) {
        m_ignoreNextFileChange = false;
        return;
    }
    
    // Check if file still exists
    if (GetFileAttributesW(m_currentFile.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    
    FILETIME currentTime = GetFileLastWriteTime(m_currentFile);
    if (currentTime.dwHighDateTime == 0 && currentTime.dwLowDateTime == 0) return;
    
    if (CompareFileTime(&currentTime, &m_lastWriteTime) != 0) {
        m_lastWriteTime = currentTime;
        
        int result = MessageBoxW(m_hwnd,
            L"The file has been modified by another program.\nDo you want to reload it?",
            L"File Changed", MB_YESNO | MB_ICONQUESTION);
        
        if (result == IDYES) {
            LoadFile(m_currentFile);
        }
    }
}

FILETIME MainWindow::GetFileLastWriteTime(const std::wstring& path) {
    FILETIME ft = {};
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        GetFileTime(hFile, nullptr, nullptr, &ft);
        CloseHandle(hFile);
    }
    return ft;
}

//------------------------------------------------------------------------------
// Session save/restore
//------------------------------------------------------------------------------
void MainWindow::SaveSession() {
    if (!m_documentManager || !m_settingsManager) return;
    
    // Build session file path
    std::wstring sessionPath = m_settingsManager->GetSettingsPath();
    size_t pos = sessionPath.rfind(L"config.ini");
    if (pos == std::wstring::npos) return;
    std::wstring sessionDir = sessionPath.substr(0, pos);
    sessionPath = sessionDir + L"session.ini";
    
    // Save current document state
    m_documentManager->SaveCurrentState();
    
    auto ids = m_documentManager->GetAllTabIds();
    int activeTabId = m_documentManager->GetActiveTabId();
    
    // Delete old session file
    DeleteFileW(sessionPath.c_str());
    
    // Don't save session if there's only one empty untitled tab
    if (ids.size() == 1) {
        auto* doc = m_documentManager->GetDocument(ids[0]);
        if (doc && doc->isNewFile && !doc->isModified && IsWhitespaceOnly(doc->text)) {
            return;
        }
    }
    
    // Write tab count and active index
    WritePrivateProfileStringW(L"Session", L"TabCount",
        std::to_wstring(ids.size()).c_str(), sessionPath.c_str());
    
    int activeIndex = 0;
    for (int i = 0; i < static_cast<int>(ids.size()); i++) {
        if (ids[i] == activeTabId) { activeIndex = i; break; }
    }
    WritePrivateProfileStringW(L"Session", L"ActiveIndex",
        std::to_wstring(activeIndex).c_str(), sessionPath.c_str());
    
    for (int i = 0; i < static_cast<int>(ids.size()); i++) {
        auto* doc = m_documentManager->GetDocument(ids[i]);
        if (!doc) continue;
        
        std::wstring section = L"Tab" + std::to_wstring(i);
        
        auto writeStr = [&](const wchar_t* key, const std::wstring& val) {
            WritePrivateProfileStringW(section.c_str(), key, val.c_str(), sessionPath.c_str());
        };
        auto writeInt = [&](const wchar_t* key, int val) {
            writeStr(key, std::to_wstring(val));
        };
        
        writeStr(L"FilePath", doc->filePath);
        writeStr(L"CustomTitle", doc->customTitle);
        writeInt(L"IsNewFile", doc->isNewFile ? 1 : 0);
        writeInt(L"IsPinned", doc->isPinned ? 1 : 0);
        writeInt(L"Encoding", static_cast<int>(doc->encoding));
        writeInt(L"LineEnding", static_cast<int>(doc->lineEnding));
        writeInt(L"CursorStart", static_cast<int>(doc->cursorStart));
        writeInt(L"CursorEnd", static_cast<int>(doc->cursorEnd));
        writeInt(L"FirstVisibleLine", doc->firstVisibleLine);
        writeInt(L"IsNoteMode", doc->isNoteMode ? 1 : 0);
        writeStr(L"NoteId", doc->noteId);
        
        // Save bookmarks as comma-separated line numbers
        std::wstring bmStr;
        for (int bm : doc->bookmarks) {
            if (!bmStr.empty()) bmStr += L",";
            bmStr += std::to_wstring(bm);
        }
        writeStr(L"Bookmarks", bmStr);
        
        writeInt(L"IsModified", doc->isModified ? 1 : 0);
        
        // For untitled or modified tabs, save content to sidecar file
        if (doc->isNewFile || doc->isModified) {
            std::wstring contentPath = sessionDir + L"session_tab" + std::to_wstring(i) + L".txt";
            (void)FileIO::WriteFile(contentPath, doc->text, TextEncoding::UTF8, LineEnding::LF);
            writeInt(L"HasSavedContent", 1);
        }
    }
}

void MainWindow::LoadSession() {
    if (!m_documentManager || !m_settingsManager) return;
    
    std::wstring sessionPath = m_settingsManager->GetSettingsPath();
    size_t pos = sessionPath.rfind(L"config.ini");
    if (pos == std::wstring::npos) return;
    std::wstring sessionDir = sessionPath.substr(0, pos);
    sessionPath = sessionDir + L"session.ini";
    
    if (GetFileAttributesW(sessionPath.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    
    int tabCount = GetPrivateProfileIntW(L"Session", L"TabCount", 0, sessionPath.c_str());
    int activeIndex = GetPrivateProfileIntW(L"Session", L"ActiveIndex", 0, sessionPath.c_str());
    
    if (tabCount <= 0) {
        DeleteFileW(sessionPath.c_str());
        return;
    }
    
    for (int i = 0; i < tabCount; i++) {
        std::wstring section = L"Tab" + std::to_wstring(i);
        wchar_t buf[4096] = {};
        
        GetPrivateProfileStringW(section.c_str(), L"FilePath", L"", buf, 4096, sessionPath.c_str());
        std::wstring filePath = buf;
        
        GetPrivateProfileStringW(section.c_str(), L"CustomTitle", L"", buf, 4096, sessionPath.c_str());
        std::wstring customTitle = buf;
        
        bool isNewFile = GetPrivateProfileIntW(section.c_str(), L"IsNewFile", 1, sessionPath.c_str()) != 0;
        bool isPinned = GetPrivateProfileIntW(section.c_str(), L"IsPinned", 0, sessionPath.c_str()) != 0;
        auto encoding = static_cast<TextEncoding>(GetPrivateProfileIntW(section.c_str(), L"Encoding", 1, sessionPath.c_str()));
        auto lineEnding = static_cast<LineEnding>(GetPrivateProfileIntW(section.c_str(), L"LineEnding", 0, sessionPath.c_str()));
        DWORD cursorStart = static_cast<DWORD>(GetPrivateProfileIntW(section.c_str(), L"CursorStart", 0, sessionPath.c_str()));
        DWORD cursorEnd = static_cast<DWORD>(GetPrivateProfileIntW(section.c_str(), L"CursorEnd", 0, sessionPath.c_str()));
        int firstVisibleLine = GetPrivateProfileIntW(section.c_str(), L"FirstVisibleLine", 0, sessionPath.c_str());
        bool isNoteMode = GetPrivateProfileIntW(section.c_str(), L"IsNoteMode", 0, sessionPath.c_str()) != 0;
        
        GetPrivateProfileStringW(section.c_str(), L"NoteId", L"", buf, 4096, sessionPath.c_str());
        std::wstring noteId = buf;
        
        // Parse bookmarks
        GetPrivateProfileStringW(section.c_str(), L"Bookmarks", L"", buf, 4096, sessionPath.c_str());
        std::set<int> bookmarks;
        {
            std::wstring bmStr = buf;
            std::wistringstream bmStream(bmStr);
            std::wstring token;
            while (std::getline(bmStream, token, L',')) {
                if (!token.empty()) {
                    try { bookmarks.insert(std::stoi(token)); } catch (...) {}
                }
            }
        }
        
        bool hasSavedContent = GetPrivateProfileIntW(section.c_str(), L"HasSavedContent", 0, sessionPath.c_str()) != 0;
        
        std::wstring content;
        
        if (!isNewFile && !filePath.empty()) {
            // File-based tab: re-read from disk
            if (GetFileAttributesW(filePath.c_str()) == INVALID_FILE_ATTRIBUTES) continue;
            FileReadResult readResult = FileIO::ReadFile(filePath);
            if (!readResult.success) continue;
            content = readResult.content;
            encoding = readResult.detectedEncoding;
            lineEnding = readResult.detectedLineEnding;
        } else if (hasSavedContent) {
            // Untitled/modified: read from sidecar
            std::wstring contentPath = sessionDir + L"session_tab" + std::to_wstring(i) + L".txt";
            FileReadResult readResult = FileIO::ReadFile(contentPath);
            if (readResult.success) content = readResult.content;
        }
        
        if (i == 0) {
            // Reuse the initial empty tab created in OnCreate
            int tabId = m_documentManager->GetActiveTabId();
            auto* doc = m_documentManager->GetActiveDocument();
            if (doc) {
                doc->filePath = filePath;
                doc->customTitle = customTitle;
                doc->isNewFile = isNewFile;
                doc->isPinned = isPinned;
                doc->encoding = encoding;
                doc->lineEnding = lineEnding;
                doc->cursorStart = cursorStart;
                doc->cursorEnd = cursorEnd;
                doc->firstVisibleLine = firstVisibleLine;
                doc->isNoteMode = isNoteMode;
                doc->noteId = noteId;
                doc->text = content;
                doc->isModified = GetPrivateProfileIntW(section.c_str(), L"IsModified", 0, sessionPath.c_str()) != 0;
                doc->bookmarks = bookmarks;
                
                m_tabBar->SetTabTitle(tabId, doc->GetDisplayTitle());
                if (!filePath.empty()) m_tabBar->SetTabFilePath(tabId, filePath);
                if (isPinned) m_tabBar->SetTabPinned(tabId, true);
                if (doc->isModified) m_tabBar->SetTabModified(tabId, true);
                
                m_editor->SetText(content);
                m_editor->SetEncoding(encoding);
                m_editor->SetLineEnding(lineEnding);
                m_editor->SetModified(doc->isModified);
                m_editor->SetBookmarks(bookmarks);
                
                m_currentFile = filePath;
                m_isNewFile = isNewFile;
                m_isNoteMode = isNoteMode;
                m_currentNoteId = noteId;
            }
        } else {
            int tabId = m_documentManager->OpenDocument(filePath, content, encoding, lineEnding);
            auto* doc = m_documentManager->GetDocument(tabId);
            if (doc) {
                doc->customTitle = customTitle;
                doc->isNewFile = isNewFile;
                doc->isPinned = isPinned;
                doc->cursorStart = cursorStart;
                doc->cursorEnd = cursorEnd;
                doc->firstVisibleLine = firstVisibleLine;
                doc->isNoteMode = isNoteMode;
                doc->noteId = noteId;
                doc->bookmarks = bookmarks;
                
                doc->isModified = GetPrivateProfileIntW(section.c_str(), L"IsModified", 0, sessionPath.c_str()) != 0;
                if (doc->isModified) {
                    m_tabBar->SetTabModified(tabId, true);
                }
                if (!customTitle.empty()) m_tabBar->SetTabTitle(tabId, customTitle);
                if (isPinned) m_tabBar->SetTabPinned(tabId, true);
            }
        }
    }
    
    // Switch to the previously active tab
    auto ids = m_documentManager->GetAllTabIds();
    if (activeIndex >= 0 && activeIndex < static_cast<int>(ids.size())) {
        OnTabSelected(ids[activeIndex]);
    }
    
    // Start monitoring the restored file
    StartFileMonitoring();
    
    // Clean up sidecar files
    for (int i = 0; i < tabCount; i++) {
        std::wstring contentPath = sessionDir + L"session_tab" + std::to_wstring(i) + L".txt";
        DeleteFileW(contentPath.c_str());
    }
    
    // Delete session file
    DeleteFileW(sessionPath.c_str());
    
    UpdateTitle();
    UpdateStatusBar();
}

//------------------------------------------------------------------------------
// System tray
//------------------------------------------------------------------------------
void MainWindow::InitializeSystemTray() {
    m_trayIcon = {};
    m_trayIcon.cbSize = sizeof(m_trayIcon);
    m_trayIcon.hWnd = m_hwnd;
    m_trayIcon.uID = 1;
    m_trayIcon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_trayIcon.uCallbackMessage = WM_APP_TRAYICON;
    m_trayIcon.hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wcscpy_s(m_trayIcon.szTip, L"QNote");
    
    Shell_NotifyIconW(NIM_ADD, &m_trayIcon);
    m_trayIconCreated = true;
}

void MainWindow::CleanupSystemTray() {
    if (m_trayIconCreated) {
        Shell_NotifyIconW(NIM_DELETE, &m_trayIcon);
        m_trayIconCreated = false;
    }
}

void MainWindow::ShowTrayContextMenu() {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_SHOW, L"&Show QNote");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_CAPTURE, L"Quick &Capture\tCtrl+Shift+Q");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_EXIT, L"E&xit");
    
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(m_hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);
}

void MainWindow::MinimizeToTray() {
    ShowWindow(m_hwnd, SW_HIDE);
    m_minimizedToTray = true;
}

void MainWindow::RestoreFromTray() {
    ShowWindow(m_hwnd, SW_SHOW);
    ShowWindow(m_hwnd, SW_RESTORE);
    SetForegroundWindow(m_hwnd);
    m_minimizedToTray = false;
}

//------------------------------------------------------------------------------
// Tab -> New Tab (Ctrl+T)
//------------------------------------------------------------------------------
void MainWindow::OnTabNew() {
    if (m_documentManager) {
        m_documentManager->NewDocument();
        m_currentFile.clear();
        m_isNewFile = true;
        m_isNoteMode = false;
        m_currentNoteId.clear();
        StartFileMonitoring();
        UpdateTitle();
        UpdateStatusBar();
        
        if (m_lineNumbersGutter && m_lineNumbersGutter->IsVisible()) {
            m_lineNumbersGutter->Update();
        }
    }
}

//------------------------------------------------------------------------------
// Tab -> Close Tab (Ctrl+W) 
//------------------------------------------------------------------------------
void MainWindow::OnTabClose() {
    if (!m_documentManager || !m_tabBar) return;
    
    int activeTab = m_documentManager->GetActiveTabId();
    OnTabCloseRequested(activeTab);
}

//------------------------------------------------------------------------------
// Tab selected (user clicked a different tab)
//------------------------------------------------------------------------------
void MainWindow::OnTabSelected(int tabId) {
    if (!m_documentManager) return;
    
    m_documentManager->SwitchToDocument(tabId);
    
    // Update MainWindow state from the new active document
    auto* doc = m_documentManager->GetActiveDocument();
    if (doc) {
        m_currentFile = doc->filePath;
        m_isNewFile = doc->isNewFile;
        m_isNoteMode = doc->isNoteMode;
        m_currentNoteId = doc->noteId;
    }
    
    // Update file monitoring for the newly active file
    StartFileMonitoring();
    
    UpdateTitle();
    UpdateStatusBar();
    
    // Update line numbers gutter
    if (m_lineNumbersGutter && m_lineNumbersGutter->IsVisible()) {
        m_lineNumbersGutter->Update();
    }
}

//------------------------------------------------------------------------------
// Tab close requested (user clicked X on a tab)
//------------------------------------------------------------------------------
void MainWindow::OnTabCloseRequested(int tabId) {
    if (!m_documentManager || !m_tabBar) return;
    
    // Don't close the last tab - instead clear it
    if (m_documentManager->GetDocumentCount() <= 1) {
        auto* doc = m_documentManager->GetActiveDocument();
        if (doc && doc->isModified) {
            if (!PromptSaveTab(tabId)) return;
        }
        // Reset the current tab instead of closing
        NewDocument();
        return;
    }
    
    // Check for unsaved changes
    if (!PromptSaveTab(tabId)) return;
    
    m_documentManager->CloseDocument(tabId);
    
    // Update MainWindow state from new active document
    auto* doc = m_documentManager->GetActiveDocument();
    if (doc) {
        m_currentFile = doc->filePath;
        m_isNewFile = doc->isNewFile;
        m_isNoteMode = doc->isNoteMode;
        m_currentNoteId = doc->noteId;
    }
    
    UpdateTitle();
    UpdateStatusBar();
}

//------------------------------------------------------------------------------
// Tab renamed via double-click
//------------------------------------------------------------------------------
void MainWindow::OnTabRenamed(int tabId) {
    if (!m_documentManager || !m_tabBar) return;
    
    auto* tabItem = m_tabBar->GetTab(tabId);
    if (tabItem) {
        m_documentManager->SetDocumentTitle(tabId, tabItem->title);
        UpdateTitle();
    }
}

//------------------------------------------------------------------------------
// Tab pin toggled via context menu
//------------------------------------------------------------------------------
void MainWindow::OnTabPinToggled(int tabId) {
    if (!m_documentManager) return;
    
    auto* doc = m_documentManager->GetDocument(tabId);
    if (doc) {
        bool newPinState = !doc->isPinned;
        m_documentManager->SetDocumentPinned(tabId, newPinState);
    }
}

//------------------------------------------------------------------------------
// Close other tabs
//------------------------------------------------------------------------------
void MainWindow::OnTabCloseOthers(int tabId) {
    if (!m_documentManager) return;
    
    auto ids = m_documentManager->GetAllTabIds();
    for (int id : ids) {
        if (id == tabId) continue;
        auto* doc = m_documentManager->GetDocument(id);
        if (doc && doc->isModified) {
            if (!PromptSaveTab(id)) return;
        }
    }
    
    m_documentManager->CloseOtherDocuments(tabId);
    OnTabSelected(tabId);
}

//------------------------------------------------------------------------------
// Close all tabs
//------------------------------------------------------------------------------
void MainWindow::OnTabCloseAll() {
    if (!m_documentManager) return;
    
    auto ids = m_documentManager->GetAllTabIds();
    for (int id : ids) {
        auto* doc = m_documentManager->GetDocument(id);
        if (doc && doc->isModified) {
            if (!PromptSaveTab(id)) return;
        }
    }
    
    m_documentManager->CloseAllDocuments();
    
    // Create a fresh tab
    OnTabNew();
}

//------------------------------------------------------------------------------
// Close tabs to the right
//------------------------------------------------------------------------------
void MainWindow::OnTabCloseToRight(int tabId) {
    if (!m_documentManager) return;
    
    // Check for unsaved changes in tabs to the right
    auto ids = m_documentManager->GetAllTabIds();
    bool found = false;
    for (int id : ids) {
        if (id == tabId) { found = true; continue; }
        if (!found) continue;
        auto* doc = m_documentManager->GetDocument(id);
        if (doc && doc->isModified) {
            if (!PromptSaveTab(id)) return;
        }
    }
    
    m_documentManager->CloseDocumentsToRight(tabId);
}

//------------------------------------------------------------------------------
// Prompt to save a specific tab's document
//------------------------------------------------------------------------------
bool MainWindow::PromptSaveTab(int tabId) {
    if (!m_documentManager) return true;
    
    // Sync editor content to document state if this is the active tab
    if (tabId == m_documentManager->GetActiveTabId()) {
        m_documentManager->SaveCurrentState();
    }
    
    auto* doc = m_documentManager->GetDocument(tabId);
    if (!doc || !doc->isModified) return true;
    
    // An untitled file with whitespace-only content has nothing to save
    if (doc->isNewFile && IsWhitespaceOnly(doc->text)) return true;
    
    // Switch to the tab so user can see it
    if (tabId != m_documentManager->GetActiveTabId()) {
        OnTabSelected(tabId);
    }
    
    std::wstring message = L"Do you want to save changes";
    if (!doc->filePath.empty()) {
        message += L" to " + doc->GetDisplayTitle();
    }
    message += L"?";
    
    int result = MessageBoxW(m_hwnd, message.c_str(), L"QNote",
                             MB_YESNOCANCEL | MB_ICONWARNING);
    
    switch (result) {
        case IDYES:
            return OnFileSave();
        case IDNO:
            return true;
        case IDCANCEL:
        default:
            return false;
    }
}

} // namespace QNote
