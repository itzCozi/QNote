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
#include <ctime>
#include <set>
#include <wincrypt.h>
#include <winhttp.h>
#include <objbase.h>
#include <bcrypt.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "bcrypt.lib")

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
bool MainWindow::Create(HINSTANCE hInstance, int nCmdShow, const std::wstring& initialFile,
                        int posX, int posY) {
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
    
    // If a specific position was requested (e.g. tear-off), use it
    if (posX != CW_USEDEFAULT && posY != CW_USEDEFAULT) {
        x = posX;
        y = posY;
    }
    
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
        
        // Translate accelerators only when the main window or its children have focus.
        // Sub-windows (NoteListWindow, CaptureWindow, etc.) are independent top-level
        // windows and need to receive their own keyboard messages (DEL, Ctrl+A, etc.)
        if (m_hAccel && (msg.hwnd == m_hwnd || IsChild(m_hwnd, msg.hwnd))) {
            if (TranslateAcceleratorW(m_hwnd, m_hAccel, &msg)) {
                continue;
            }
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
            if (wParam != SIZE_MINIMIZED) {
                OnSize(LOWORD(lParam), HIWORD(lParam));
            }
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
            // Intercept minimize
            if ((wParam & 0xFFF0) == SC_MINIMIZE) {
                if (m_settingsManager->GetSettings().minimizeMode == 1) {
                    MinimizeToTray();
                    return 0;
                }
                // else fall through to default (minimize to taskbar)
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
    
    const AppSettings& settings = m_settingsManager->GetSettings();
    
    // Create tab bar
    if (!m_tabBar->Create(m_hwnd, m_hInstance)) {
        MessageBoxW(m_hwnd, L"Failed to create tab bar.", L"Error", MB_OK | MB_ICONERROR);
    }
    
    // Initialize document manager (creates per-tab editors internally)
    m_documentManager->Initialize(m_hwnd, m_hInstance, m_tabBar.get(), m_settingsManager.get());
    
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
            case TabNotification::TabDetached:
                OnTabDetached(tabId);
                break;
        }
    });
    
    // Create an initial tab (this creates the first per-tab editor)
    m_documentManager->NewDocument();
    m_isNewFile = true;
    m_currentFile.clear();
    
    // Get the active editor from the document manager
    m_editor = m_documentManager->GetActiveEditor();
    
    // Create line numbers gutter (uses the active editor)
    if (!m_lineNumbersGutter->Create(m_hwnd, m_hInstance, m_editor)) {
        MessageBoxW(m_hwnd, L"Failed to create line numbers gutter.", L"Error", MB_OK | MB_ICONERROR);
    }
    
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
    if (!m_findBar->Create(m_hwnd, m_hInstance, m_editor)) {
        MessageBoxW(m_hwnd, L"Failed to create find bar.", L"Error", MB_OK | MB_ICONERROR);
    }
    
    // Initialize dialog manager
    m_dialogManager->Initialize(m_hwnd, m_hInstance, m_editor, 
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
    
    // Start real auto-save timer if save style is AutoSave
    if (m_settingsManager->GetSettings().saveStyle == SaveStyle::AutoSave) {
        SetTimer(m_hwnd, TIMER_REALSAVE, m_settingsManager->GetSettings().autoSaveDelayMs, nullptr);
    }
    
    // Initialize system tray
    InitializeSystemTray();
    
    // Restore previous session (must be after tab bar + document manager init)
    LoadSession();
    
    // Apply always-on-top if saved
    if (settings.alwaysOnTop) {
        SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
    
    // Apply menu bar visibility
    if (!settings.menuBarVisible) {
        SetMenu(m_hwnd, nullptr);
    }
    
    // Auto-update check on startup if enabled
    if (settings.autoUpdate) {
        // Trigger update check in background (non-blocking)
        // For simplicity, we'll just do a quick check next time - not blocking startup
    }
    
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
    KillTimer(m_hwnd, TIMER_REALSAVE);
    
    // Save session before destroying
    SaveSession();
    
    // Clean up system tray
    CleanupSystemTray();
    
    // Delete auto-save backup if file was saved successfully
    if (m_editor && !m_isNoteMode && !m_isNewFile && !m_currentFile.empty() && !m_editor->IsModified()) {
        DeleteAutoSaveBackup();
    }
    
    // Auto-save current note if in note mode
    if (m_editor && m_isNoteMode && m_editor->IsModified()) {
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
        settings.zoomLevel = (m_editor && m_editor->GetTextLength() > 0) ? m_settingsManager->GetSettings().zoomLevel : 100;
        
        // Save editor feature settings
        if (m_editor) settings.showWhitespace = m_editor->IsShowWhitespace();
    }
    
    // Save settings
    (void)m_settingsManager->Save();
    
    // Clean up - DocumentManager owns all editors, will clean up on destruction
    m_editor = nullptr;
    
    PostQuitMessage(0);
}

//------------------------------------------------------------------------------
// WM_CLOSE handler
//------------------------------------------------------------------------------
void MainWindow::OnClose() {
    // If close mode is "minimize to tray" and not force-quitting, hide to tray instead
    const auto& settings = m_settingsManager->GetSettings();
    if (settings.closeMode == 1 && !m_forceQuit) {
        MinimizeToTray();
        return;
    }
    m_forceQuit = false;  // Reset for next time
    
    // In note mode, auto-save and close without prompting
    if (m_isNoteMode) {
        if (m_editor && m_editor->IsModified()) {
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
                if (!settings.promptSaveOnClose) {
                    // Skip the dialog - just discard changes
                    continue;
                }
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
        
        // File menu (additional 2)
        case IDM_FILE_SAVEALL:         OnFileSaveAll(); break;
        case IDM_FILE_CLOSEALL:        OnFileCloseAll(); break;
        case IDM_FILE_OPENFROMCLIPBOARD: OnFileOpenFromClipboard(); break;
        
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
        case IDM_FORMAT_SCROLLLINES: OnFormatScrollLines(); break;
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
        case IDM_HELP_WEBSITE: ShellExecuteW(m_hwnd, L"open", L"https://qnote.ar0.eu/", nullptr, nullptr, SW_SHOWNORMAL); break;
        case IDM_HELP_BUGREPORT: ShellExecuteW(m_hwnd, L"open", L"https://github.com/itzCozi/QNote/issues/new?template=bug_report.md", nullptr, nullptr, SW_SHOWNORMAL); break;
        
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
        
        // Edit menu (additional 2)
        case IDM_EDIT_TITLECASE:         OnEditTitleCase(); break;
        case IDM_EDIT_REVERSELINES:      OnEditReverseLines(); break;
        case IDM_EDIT_NUMBERLINES:       OnEditNumberLines(); break;
        case IDM_EDIT_TOGGLECOMMENT:     OnEditToggleComment(); break;
        
        // Bookmarks
        case IDM_EDIT_TOGGLEBOOKMARK:    OnEditToggleBookmark(); break;
        case IDM_EDIT_NEXTBOOKMARK:      OnEditNextBookmark(); break;
        case IDM_EDIT_PREVBOOKMARK:      OnEditPrevBookmark(); break;
        case IDM_EDIT_CLEARBOOKMARKS:    OnEditClearBookmarks(); break;
        
        // View menu (additional)
        case IDM_VIEW_SHOWWHITESPACE:    OnViewShowWhitespace(); break;
        case IDM_VIEW_ALWAYSONTOP:       OnViewAlwaysOnTop(); break;
        case IDM_VIEW_FULLSCREEN:        OnViewFullScreen(); break;
        case IDM_VIEW_TOGGLEMENUBAR:     OnViewToggleMenuBar(); break;
        
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
        case IDM_TOOLS_SETTINGS:         OnToolsSettings(); break;
        case IDM_TOOLS_CALCULATE:        OnToolsCalculate(); break;
        case IDM_TOOLS_INSERTGUID:       OnToolsInsertGuid(); break;
        case IDM_TOOLS_INSERTFILEPATH:   OnToolsInsertFilePath(); break;
        case IDM_TOOLS_CONVERTEOL_SEL:   OnToolsConvertEolSelection(); break;
        case IDM_TOOLS_CHECKSUM:         OnToolsChecksum(); break;
        case IDM_TOOLS_RUNSELECTION:     OnToolsRunSelection(); break;
        
        // Notes menu (additional)
        case IDM_NOTES_EXPORT:           OnNotesExport(); break;
        case IDM_NOTES_IMPORT:           OnNotesImport(); break;
        case IDM_NOTES_FAVORITES:        OnNotesFavorites(); break;
        case IDM_NOTES_DUPLICATE:        OnNotesDuplicate(); break;
        
        // System tray
        case IDM_TRAY_SHOW:     RestoreFromTray(); break;
        case IDM_TRAY_CAPTURE:  OnNotesQuickCapture(); break;
        case IDM_TRAY_EXIT:     m_forceQuit = true; SendMessageW(m_hwnd, WM_CLOSE, 0, 0); break;
        
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
    if (m_editor && hwndCtl == m_editor->GetHandle() && code == EN_CHANGE) {
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
        if (m_isNoteMode && m_editor && m_editor->IsModified()) {
            AutoSaveCurrentNote();
        }
    } else if (timerId == TIMER_FILEAUTOSAVE) {
        // Auto-save file backup if in file mode and modified
        AutoSaveFileBackup();
    } else if (timerId == TIMER_FILEWATCH) {
        // Check if the current file has been modified externally
        CheckFileChanged();
    } else if (timerId == TIMER_REALSAVE) {
        // Auto-save the actual file (not a backup) when save style is AutoSave
        if (!m_isNoteMode && m_editor && m_editor->IsModified() &&
            !m_isNewFile && !m_currentFile.empty()) {
            m_ignoreNextFileChange = true;
            SaveFile(m_currentFile);
        }
    }
}

//------------------------------------------------------------------------------
// WM_CTLCOLOREDIT handler
//------------------------------------------------------------------------------
LRESULT MainWindow::OnCtlColorEdit(HDC hdc, HWND hwndEdit) {
    return DefWindowProcW(m_hwnd, WM_CTLCOLOREDIT, reinterpret_cast<WPARAM>(hdc), 
                          reinterpret_cast<LPARAM>(hwndEdit));
}

} // namespace QNote

