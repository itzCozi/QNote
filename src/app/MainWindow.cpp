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

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

namespace QNote {

//------------------------------------------------------------------------------
// DWM dark title bar (Windows 10 1809+)
//------------------------------------------------------------------------------
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
MainWindow::MainWindow()
    : m_settingsManager(std::make_unique<SettingsManager>())
    , m_editor(std::make_unique<Editor>())
    , m_dialogManager(std::make_unique<DialogManager>())
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
        // Check for dialog messages first
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
            
        case WM_APP_OPENNOTE:
            // Open a note by ID passed via lParam (as allocated string)
            if (lParam) {
                std::wstring* noteId = reinterpret_cast<std::wstring*>(lParam);
                OpenNoteFromId(*noteId);
                delete noteId;
            }
            return 0;
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
    
    // In file mode, prompt for save as before
    if (PromptSaveChanges()) {
        DestroyWindow(m_hwnd);
    }
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
        case IDM_VIEW_STATUSBAR: OnViewStatusBar(); break;
        case IDM_VIEW_ZOOMIN:    OnViewZoomIn(); break;
        case IDM_VIEW_ZOOMOUT:   OnViewZoomOut(); break;
        case IDM_VIEW_ZOOMRESET: OnViewZoomReset(); break;
        
        // Encoding menu
        case IDM_ENCODING_UTF8:     OnEncodingChange(TextEncoding::UTF8); break;
        case IDM_ENCODING_UTF8BOM:  OnEncodingChange(TextEncoding::UTF8_BOM); break;
        case IDM_ENCODING_UTF16LE:  OnEncodingChange(TextEncoding::UTF16_LE); break;
        case IDM_ENCODING_UTF16BE:  OnEncodingChange(TextEncoding::UTF16_BE); break;
        case IDM_ENCODING_ANSI:     OnEncodingChange(TextEncoding::ANSI); break;
        
        // Help menu
        case IDM_HELP_ABOUT: OnHelpAbout(); break;
        
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
    }
    
    // Handle edit control notifications
    if (hwndCtl == m_editor->GetHandle() && code == EN_CHANGE) {
        UpdateTitle();
    }
}

//------------------------------------------------------------------------------
// WM_DROPFILES handler
//------------------------------------------------------------------------------
void MainWindow::OnDropFiles(HDROP hDrop) {
    wchar_t filePath[MAX_PATH] = {};
    
    // Get the first dropped file
    if (DragQueryFileW(hDrop, 0, filePath, MAX_PATH) > 0) {
        // Check for unsaved changes
        if (PromptSaveChanges()) {
            LoadFile(filePath);
        }
    }
    
    DragFinish(hDrop);
    
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
    if (!PromptSaveChanges()) {
        return;
    }
    
    NewDocument();
}

//------------------------------------------------------------------------------
// File -> Open
//------------------------------------------------------------------------------
void MainWindow::OnFileOpen() {
    if (!PromptSaveChanges()) {
        return;
    }
    
    std::wstring filePath;
    if (FileIO::ShowOpenDialog(m_hwnd, filePath)) {
        LoadFile(filePath);
    }
}

//------------------------------------------------------------------------------
// File -> Save
//------------------------------------------------------------------------------
bool MainWindow::OnFileSave() {
    if (m_isNewFile || m_currentFile.empty()) {
        return OnFileSaveAs();
    }
    
    return SaveFile(m_currentFile);
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
        if (!PromptSaveChanges()) {
            return;
        }
        
        std::wstring filePath = recentFiles[index];
        
        // Check if file still exists
        if (GetFileAttributesW(filePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            MessageBoxW(m_hwnd, L"The file no longer exists.", L"QNote", MB_OK | MB_ICONWARNING);
            m_settingsManager->RemoveRecentFile(filePath);
            UpdateRecentFilesMenu();
            return;
        }
        
        LoadFile(filePath);
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
    m_dialogManager->ShowFindDialog();
}

void MainWindow::OnEditFindNext() {
    m_dialogManager->FindNext();
}

void MainWindow::OnEditReplace() {
    m_dialogManager->ShowReplaceDialog();
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
// Help -> About
//------------------------------------------------------------------------------
void MainWindow::OnHelpAbout() {
    m_dialogManager->ShowAboutDialog();
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
    
    wchar_t posText[64];
    swprintf_s(posText, L"Ln %d, Col %d", line, column);
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
    
    // Add to recent files
    m_settingsManager->AddRecentFile(filePath);
    UpdateRecentFilesMenu();
    
    UpdateTitle();
    UpdateStatusBar();
    
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
    m_editor->Clear();
    m_editor->SetModified(false);
    m_editor->SetEncoding(m_settingsManager->GetSettings().defaultEncoding);
    m_editor->SetLineEnding(m_settingsManager->GetSettings().defaultLineEnding);
    
    m_currentFile.clear();
    m_isNewFile = true;
    
    UpdateTitle();
    UpdateStatusBar();
    
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
        int widths[STATUS_PARTS] = { 150, 250, 330, -1 };
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
    
    // Position status bar
    if (m_hwndStatus && IsWindowVisible(m_hwndStatus)) {
        SendMessageW(m_hwndStatus, WM_SIZE, 0, 0);
        
        RECT statusRect;
        GetWindowRect(m_hwndStatus, &statusRect);
        statusHeight = statusRect.bottom - statusRect.top;
    }
    
    // Position editor
    if (m_editor) {
        m_editor->Resize(0, 0, rc.right, rc.bottom - statusHeight);
    }
}

//------------------------------------------------------------------------------
// Initialize note store and related components
//------------------------------------------------------------------------------
void MainWindow::InitializeNoteStore() {
    // Initialize the note store
    if (m_noteStore) {
        m_noteStore->Initialize();
    }
    
    // Create capture window
    m_captureWindow = std::make_unique<CaptureWindow>();
    if (m_captureWindow->Create(m_hInstance, m_noteStore.get())) {
        // Set callback for when user wants to edit in main window
        m_captureWindow->SetEditCallback([this](const std::wstring& noteId) {
            OpenNoteFromId(noteId);
        });
    }
    
    // Create note list window
    m_noteListWindow = std::make_unique<NoteListWindow>();
    if (m_noteListWindow->Create(m_hInstance, m_hwnd, m_noteStore.get())) {
        // Set callback for opening notes
        m_noteListWindow->SetOpenCallback([this](const Note& note) {
            LoadNoteIntoEditor(note);
        });
    }
    
    // Register global hotkey (Ctrl+Shift+Q)
    if (m_hotkeyManager) {
        m_hotkeyManager->Register(m_hwnd, HOTKEY_QUICKCAPTURE);
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
            m_noteStore->UpdateNote(updated);
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
    
    m_noteStore->TogglePin(m_currentNoteId);
    
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
        m_noteStore->DeleteNote(m_currentNoteId);
        
        // Start a new note
        OnNotesNew();
        
        // Refresh note list if visible
        if (m_noteListWindow && m_noteListWindow->IsVisible()) {
            m_noteListWindow->RefreshList();
        }
    }
}

} // namespace QNote
