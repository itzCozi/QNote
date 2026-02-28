//==============================================================================
// QNote - A Lightweight Notepad Clone
// CaptureWindow.cpp - Quick capture popup window implementation
//==============================================================================

#include "CaptureWindow.h"
#include <CommCtrl.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")

// DWM dark title bar (Windows 10 1809+) - fallback for older SDKs
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

namespace QNote {

//------------------------------------------------------------------------------
// CaptureWindow Implementation
//------------------------------------------------------------------------------

CaptureWindow::CaptureWindow() = default;

CaptureWindow::~CaptureWindow() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    if (m_hFont) {
        DeleteObject(m_hFont);
    }
    if (m_hSmallFont) {
        DeleteObject(m_hSmallFont);
    }
}

bool CaptureWindow::Create(HINSTANCE hInstance, NoteStore* noteStore) {
    m_hInstance = hInstance;
    m_noteStore = noteStore;
    
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = 0;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = WINDOW_CLASS;
    
    if (!RegisterClassExW(&wc)) {
        // Class may already be registered
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }
    
    // Create window (initially hidden)
    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,  // Always on top, no taskbar
        WINDOW_CLASS,
        L"Quick Capture - QNote",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        nullptr,
        nullptr,
        hInstance,
        this
    );
    
    if (!m_hwnd) {
        return false;
    }
    
    // Round corners on Windows 11
    DWM_WINDOW_CORNER_PREFERENCE cornerPref = DWMWCP_ROUND;
    DwmSetWindowAttribute(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));
    
    // Dark title bar
    BOOL useDarkTitleBar = TRUE;
    DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkTitleBar, sizeof(useDarkTitleBar));
    
    return true;
}

void CaptureWindow::Show() {
    if (!m_hwnd) return;
    
    PositionWindow();
    ClearAndFocus();
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    
    // Focus the edit control
    if (m_hwndEdit) {
        SetFocus(m_hwndEdit);
    }
}

void CaptureWindow::Hide() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

void CaptureWindow::Toggle() {
    if (IsVisible()) {
        Hide();
    } else {
        Show();
    }
}

bool CaptureWindow::IsVisible() const {
    return m_hwnd && IsWindowVisible(m_hwnd);
}

LRESULT CALLBACK CaptureWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    CaptureWindow* pThis = nullptr;
    
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = static_cast<CaptureWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<CaptureWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    
    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CaptureWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            OnCreate();
            return 0;
            
        case WM_DESTROY:
            OnDestroy();
            return 0;
            
        case WM_SIZE:
            OnSize(LOWORD(lParam), HIWORD(lParam));
            return 0;
            
        case WM_COMMAND:
            OnCommand(LOWORD(wParam), HIWORD(wParam), reinterpret_cast<HWND>(lParam));
            return 0;
            
        case WM_KEYDOWN:
            OnKeyDown(wParam);
            return 0;
            
        case WM_CTLCOLOREDIT:
            return OnCtlColorEdit(reinterpret_cast<HDC>(wParam), reinterpret_cast<HWND>(lParam));
            
        case WM_CTLCOLORSTATIC:
            return OnCtlColorStatic(reinterpret_cast<HDC>(wParam), reinterpret_cast<HWND>(lParam));
            
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) {
                // Auto-save when losing focus (if there's content)
                if (m_hwndEdit && GetWindowTextLengthW(m_hwndEdit) > 0) {
                    SaveNote();
                }
            }
            break;
            
        case WM_CLOSE:
            // Hide first (triggers WM_ACTIVATE which handles saving)
            Hide();
            return 0;
    }
    
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

void CaptureWindow::OnCreate() {
    // Create fonts
    m_hFont = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );
    
    m_hSmallFont = CreateFontW(
        -11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );
    
    CreateControls();
}

void CaptureWindow::OnDestroy() {
    // Cleanup handled in destructor
}

void CaptureWindow::OnSize(int width, int height) {
    if (!m_hwndEdit) return;
    
    int editHeight = height - PADDING * 3 - BUTTON_HEIGHT - 16;  // Space for hint
    
    // Position edit control
    SetWindowPos(m_hwndEdit, nullptr, 
        PADDING, PADDING, 
        width - PADDING * 2, editHeight,
        SWP_NOZORDER);
    
    // Position hint label
    if (m_hwndHintLabel) {
        SetWindowPos(m_hwndHintLabel, nullptr,
            PADDING, PADDING + editHeight + 2,
            width - PADDING * 2, 14,
            SWP_NOZORDER);
    }
    
    // Position buttons at bottom
    int buttonY = height - PADDING - BUTTON_HEIGHT;
    int buttonX = width - PADDING;
    
    // Edit button (rightmost)
    if (m_hwndEditBtn) {
        buttonX -= BUTTON_WIDTH;
        SetWindowPos(m_hwndEditBtn, nullptr, buttonX, buttonY, 
            BUTTON_WIDTH, BUTTON_HEIGHT, SWP_NOZORDER);
        buttonX -= 5;  // Gap
    }
    
    // Pin button
    if (m_hwndPinBtn) {
        buttonX -= BUTTON_WIDTH;
        SetWindowPos(m_hwndPinBtn, nullptr, buttonX, buttonY, 
            BUTTON_WIDTH, BUTTON_HEIGHT, SWP_NOZORDER);
        buttonX -= 5;
    }
    
    // Save button
    if (m_hwndSaveBtn) {
        buttonX -= BUTTON_WIDTH;
        SetWindowPos(m_hwndSaveBtn, nullptr, buttonX, buttonY, 
            BUTTON_WIDTH, BUTTON_HEIGHT, SWP_NOZORDER);
    }
}

void CaptureWindow::OnCommand(WORD id, WORD code, HWND hwndCtl) {
    switch (id) {
        case IDC_SAVE_BTN:
            SaveNote();
            ClearAndFocus();
            break;
            
        case IDC_PIN_BTN:
            TogglePinNote();
            break;
            
        case IDC_EDIT_BTN:
            // Save first, then open in main editor
            SaveNote();
            if (m_editCallback && !m_currentNoteId.empty()) {
                m_editCallback(m_currentNoteId);
            }
            Hide();
            break;
            
        case IDC_CAPTURE_EDIT:
            if (code == EN_CHANGE) {
                // Content changed - mark as needing auto-save
                // The auto-save happens on WM_ACTIVATE or explicit save
            }
            break;
    }
}

void CaptureWindow::OnKeyDown(WPARAM key) {
    switch (key) {
        case VK_ESCAPE:
            // Save and hide
            if (m_hwndEdit && GetWindowTextLengthW(m_hwndEdit) > 0) {
                SaveNote();
            }
            Hide();
            break;
            
        case VK_RETURN:
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                // Ctrl+Enter = Save and clear
                SaveNote();
                ClearAndFocus();
            }
            break;
    }
}

LRESULT CaptureWindow::OnCtlColorEdit(HDC hdc, HWND hwndEdit) {
    return DefWindowProcW(m_hwnd, WM_CTLCOLOREDIT, reinterpret_cast<WPARAM>(hdc), 
                          reinterpret_cast<LPARAM>(hwndEdit));
}

LRESULT CaptureWindow::OnCtlColorStatic(HDC hdc, HWND hwndStatic) {
    return DefWindowProcW(m_hwnd, WM_CTLCOLORSTATIC, reinterpret_cast<WPARAM>(hdc), 
                          reinterpret_cast<LPARAM>(hwndStatic));
}

void CaptureWindow::SaveNote() {
    if (!m_hwndEdit || !m_noteStore) return;
    
    int textLen = GetWindowTextLengthW(m_hwndEdit);
    if (textLen == 0) return;
    
    std::wstring text(textLen + 1, L'\0');
    GetWindowTextW(m_hwndEdit, &text[0], textLen + 1);
    text.resize(textLen);
    
    if (m_currentNoteId.empty()) {
        // Create new note
        Note newNote = m_noteStore->CreateNote(text);
        m_currentNoteId = newNote.id;
        if (m_isPinned) {
            (void)m_noteStore->TogglePin(m_currentNoteId);
        }
    } else {
        // Update existing note
        auto existingNote = m_noteStore->GetNote(m_currentNoteId);
        if (existingNote) {
            Note updated = *existingNote;
            updated.content = text;
            updated.isPinned = m_isPinned;
            (void)m_noteStore->UpdateNote(updated);
        }
    }
    
    if (m_captureCallback) {
        m_captureCallback(text);
    }
}

void CaptureWindow::ClearAndFocus() {
    m_currentNoteId.clear();
    m_isPinned = false;
    
    if (m_hwndEdit) {
        SetWindowTextW(m_hwndEdit, L"");
        SetFocus(m_hwndEdit);
    }
    
    // Update pin button text
    if (m_hwndPinBtn) {
        SetWindowTextW(m_hwndPinBtn, L"Pin");
    }
}

void CaptureWindow::TogglePinNote() {
    m_isPinned = !m_isPinned;
    
    // Update button text
    if (m_hwndPinBtn) {
        SetWindowTextW(m_hwndPinBtn, m_isPinned ? L"Unpin" : L"Pin");
    }
    
    // If note already exists, toggle its pin status
    if (!m_currentNoteId.empty() && m_noteStore) {
        (void)m_noteStore->TogglePin(m_currentNoteId);
    }
}

void CaptureWindow::PositionWindow() {
    if (!m_hwnd) return;
    
    // Get cursor position
    POINT cursorPos;
    GetCursorPos(&cursorPos);
    
    // Get work area of the monitor containing the cursor
    HMONITOR hMonitor = MonitorFromPoint(cursorPos, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(hMonitor, &mi);
    
    RECT workArea = mi.rcWork;
    
    // Position near cursor but ensure fully visible
    int x = cursorPos.x - WINDOW_WIDTH / 2;
    int y = cursorPos.y - WINDOW_HEIGHT / 2;
    
    // Clamp to work area
    if (x < workArea.left) x = workArea.left;
    if (y < workArea.top) y = workArea.top;
    if (x + WINDOW_WIDTH > workArea.right) x = workArea.right - WINDOW_WIDTH;
    if (y + WINDOW_HEIGHT > workArea.bottom) y = workArea.bottom - WINDOW_HEIGHT;
    
    SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, WINDOW_WIDTH, WINDOW_HEIGHT, 
                 SWP_NOACTIVATE);
}

void CaptureWindow::CreateControls() {
    // Edit control (multiline)
    m_hwndEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
        PADDING, PADDING, 
        WINDOW_WIDTH - PADDING * 2, 100,
        m_hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_CAPTURE_EDIT)),
        m_hInstance,
        nullptr
    );
    
    if (m_hwndEdit && m_hFont) {
        SendMessageW(m_hwndEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
    }
    
    // Subclass edit control to intercept keyboard shortcuts
    if (m_hwndEdit && !m_editSubclassed) {
        if (SetWindowSubclass(m_hwndEdit, EditSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this))) {
            m_editSubclassed = true;
        }
    }
    
    // Hint label
    m_hwndHintLabel = CreateWindowExW(
        0,
        L"STATIC",
        L"Ctrl+Enter to save and clear | Esc to close",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        PADDING, 110, 
        WINDOW_WIDTH - PADDING * 2, 14,
        m_hwnd,
        nullptr,
        m_hInstance,
        nullptr
    );
    
    if (m_hwndHintLabel && m_hSmallFont) {
        SendMessageW(m_hwndHintLabel, WM_SETFONT, reinterpret_cast<WPARAM>(m_hSmallFont), TRUE);
    }
    
    // Save button
    m_hwndSaveBtn = CreateWindowExW(
        0,
        L"BUTTON",
        L"Save",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, BUTTON_WIDTH, BUTTON_HEIGHT,
        m_hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SAVE_BTN)),
        m_hInstance,
        nullptr
    );
    
    // Pin button
    m_hwndPinBtn = CreateWindowExW(
        0,
        L"BUTTON",
        L"Pin",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, BUTTON_WIDTH, BUTTON_HEIGHT,
        m_hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PIN_BTN)),
        m_hInstance,
        nullptr
    );
    
    // Edit in main window button
    m_hwndEditBtn = CreateWindowExW(
        0,
        L"BUTTON",
        L"Edit...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, BUTTON_WIDTH, BUTTON_HEIGHT,
        m_hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_EDIT_BTN)),
        m_hInstance,
        nullptr
    );
    
    // Set font for buttons
    if (m_hFont) {
        if (m_hwndSaveBtn) SendMessageW(m_hwndSaveBtn, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
        if (m_hwndPinBtn) SendMessageW(m_hwndPinBtn, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
        if (m_hwndEditBtn) SendMessageW(m_hwndEditBtn, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
    }
}

LRESULT CALLBACK CaptureWindow::EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                                   UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    CaptureWindow* pThis = reinterpret_cast<CaptureWindow*>(dwRefData);
    
    if (msg == WM_KEYDOWN) {
        switch (wParam) {
            case VK_ESCAPE:
                // Save and hide
                if (pThis) {
                    if (GetWindowTextLengthW(hwnd) > 0) {
                        pThis->SaveNote();
                    }
                    pThis->Hide();
                    return 0;
                }
                break;
                
            case VK_RETURN:
                if (GetKeyState(VK_CONTROL) & 0x8000) {
                    // Ctrl+Enter = Save and clear
                    if (pThis) {
                        pThis->SaveNote();
                        pThis->ClearAndFocus();
                        return 0;
                    }
                }
                break;
        }
    } else if (msg == WM_NCDESTROY) {
        // Clean up subclass
        RemoveWindowSubclass(hwnd, EditSubclassProc, uIdSubclass);
        if (pThis) {
            pThis->m_editSubclassed = false;
        }
    }
    
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

//------------------------------------------------------------------------------
// GlobalHotkeyManager Implementation
//------------------------------------------------------------------------------

GlobalHotkeyManager::GlobalHotkeyManager() = default;

GlobalHotkeyManager::~GlobalHotkeyManager() {
    Unregister();
}

bool GlobalHotkeyManager::Register(HWND targetWindow, int hotkeyId) {
    if (m_registered) {
        Unregister();
    }
    
    m_targetWindow = targetWindow;
    m_hotkeyId = hotkeyId;
    
    // Register Ctrl+Shift+Q
    if (RegisterHotKey(targetWindow, hotkeyId, m_modifiers | MOD_NOREPEAT, m_virtualKey)) {
        m_registered = true;
        return true;
    }
    
    // If Ctrl+Shift+Q fails, try Ctrl+Alt+Q (Win+Q conflicts with Windows 11 Widgets)
    m_modifiers = MOD_CONTROL | MOD_ALT;
    m_virtualKey = 'Q';
    if (RegisterHotKey(targetWindow, hotkeyId, m_modifiers | MOD_NOREPEAT, m_virtualKey)) {
        m_registered = true;
        return true;
    }
    
    return false;
}

void GlobalHotkeyManager::Unregister() {
    if (m_registered && m_targetWindow) {
        UnregisterHotKey(m_targetWindow, m_hotkeyId);
        m_registered = false;
    }
}

std::wstring GlobalHotkeyManager::GetHotkeyDescription() const {
    std::wstring desc;
    
    if (m_modifiers & MOD_WIN) desc += L"Win+";
    if (m_modifiers & MOD_CONTROL) desc += L"Ctrl+";
    if (m_modifiers & MOD_SHIFT) desc += L"Shift+";
    if (m_modifiers & MOD_ALT) desc += L"Alt+";
    
    desc += static_cast<wchar_t>(m_virtualKey);
    
    return desc;
}

} // namespace QNote
