//==============================================================================
// QNote - A Lightweight Notepad Clone
// Dialogs.cpp - Find/Replace, GoTo, and other dialog boxes implementation
//==============================================================================

#include "Dialogs.h"
#include "Editor.h"
#include "Settings.h"
#include "resource.h"
#include <commdlg.h>

namespace QNote {

// Static instance pointer for dialog callbacks
DialogManager* DialogManager::s_instance = nullptr;

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
DialogManager::DialogManager() {
    s_instance = this;
}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
DialogManager::~DialogManager() {
    CloseFindDialog();
    CloseReplaceDialog();
    s_instance = nullptr;
}

//------------------------------------------------------------------------------
// Initialize
//------------------------------------------------------------------------------
void DialogManager::Initialize(HWND parent, HINSTANCE hInstance, Editor* editor, AppSettings* settings) noexcept {
    m_hwndParent = parent;
    m_hInstance = hInstance;
    m_editor = editor;
    m_settings = settings;
    
    // Load search settings
    if (settings) {
        m_findParams.matchCase = settings->searchMatchCase;
        m_findParams.wrapAround = settings->searchWrapAround;
        m_findParams.useRegex = settings->searchUseRegex;
        m_replaceParams.matchCase = settings->searchMatchCase;
        m_replaceParams.wrapAround = settings->searchWrapAround;
        m_replaceParams.useRegex = settings->searchUseRegex;
    }
}

//------------------------------------------------------------------------------
// Show Find dialog
//------------------------------------------------------------------------------
void DialogManager::ShowFindDialog() {
    // Close replace dialog if open
    CloseReplaceDialog();
    
    if (m_hwndFind) {
        // Already open, just activate it
        SetForegroundWindow(m_hwndFind);
        return;
    }
    
    m_hwndFind = CreateDialogParamW(
        m_hInstance,
        MAKEINTRESOURCEW(IDD_FIND),
        m_hwndParent,
        FindDlgProc,
        reinterpret_cast<LPARAM>(this)
    );
    
    if (m_hwndFind) {
        ShowWindow(m_hwndFind, SW_SHOW);
    }
}

//------------------------------------------------------------------------------
// Show Replace dialog
//------------------------------------------------------------------------------
void DialogManager::ShowReplaceDialog() {
    // Close find dialog if open
    CloseFindDialog();
    
    if (m_hwndReplace) {
        // Already open, just activate it
        SetForegroundWindow(m_hwndReplace);
        return;
    }
    
    m_hwndReplace = CreateDialogParamW(
        m_hInstance,
        MAKEINTRESOURCEW(IDD_REPLACE),
        m_hwndParent,
        ReplaceDlgProc,
        reinterpret_cast<LPARAM>(this)
    );
    
    if (m_hwndReplace) {
        ShowWindow(m_hwndReplace, SW_SHOW);
    }
}

//------------------------------------------------------------------------------
// Show Go To Line dialog
//------------------------------------------------------------------------------
bool DialogManager::ShowGoToDialog(int& lineNumber) {
    INT_PTR result = DialogBoxParamW(
        m_hInstance,
        MAKEINTRESOURCEW(IDD_GOTO),
        m_hwndParent,
        GoToDlgProc,
        reinterpret_cast<LPARAM>(&lineNumber)
    );
    
    return result == IDOK;
}

//------------------------------------------------------------------------------
// Show Tab Size dialog
//------------------------------------------------------------------------------
bool DialogManager::ShowTabSizeDialog(int& tabSize) {
    INT_PTR result = DialogBoxParamW(
        m_hInstance,
        MAKEINTRESOURCEW(IDD_TABSIZE),
        m_hwndParent,
        TabSizeDlgProc,
        reinterpret_cast<LPARAM>(&tabSize)
    );
    
    return result == IDOK;
}

//------------------------------------------------------------------------------
// Show Scroll Lines dialog
//------------------------------------------------------------------------------
bool DialogManager::ShowScrollLinesDialog(int& scrollLines) {
    INT_PTR result = DialogBoxParamW(
        m_hInstance,
        MAKEINTRESOURCEW(IDD_SCROLLLINES),
        m_hwndParent,
        ScrollLinesDlgProc,
        reinterpret_cast<LPARAM>(&scrollLines)
    );
    
    return result == IDOK;
}

//------------------------------------------------------------------------------
// Show Font dialog
//------------------------------------------------------------------------------
bool DialogManager::ShowFontDialog(std::wstring& fontName, int& fontSize, int& fontWeight, bool& italic) {
    LOGFONTW lf = {};
    lf.lfWeight = fontWeight;
    lf.lfItalic = italic ? TRUE : FALSE;
    lf.lfCharSet = DEFAULT_CHARSET;
    wcsncpy_s(lf.lfFaceName, fontName.c_str(), LF_FACESIZE - 1);
    
    // Calculate height from point size
    HDC hdc = GetDC(m_hwndParent);
    lf.lfHeight = -MulDiv(fontSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(m_hwndParent, hdc);
    
    CHOOSEFONTW cf = {};
    cf.lStructSize = sizeof(cf);
    cf.hwndOwner = m_hwndParent;
    cf.lpLogFont = &lf;
    cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_EFFECTS;
    cf.nFontType = SCREEN_FONTTYPE;
    
    if (ChooseFontW(&cf)) {
        fontName = lf.lfFaceName;
        fontSize = cf.iPointSize / 10;
        fontWeight = lf.lfWeight;
        italic = lf.lfItalic != FALSE;
        return true;
    }
    
    return false;
}

//------------------------------------------------------------------------------
// Show About dialog
//------------------------------------------------------------------------------
void DialogManager::ShowAboutDialog() {
    DialogBoxParamW(
        m_hInstance,
        MAKEINTRESOURCEW(IDD_ABOUT),
        m_hwndParent,
        AboutDlgProc,
        0
    );
}

//------------------------------------------------------------------------------
// Close Find dialog
//------------------------------------------------------------------------------
void DialogManager::CloseFindDialog() noexcept {
    if (m_hwndFind) {
        DestroyWindow(m_hwndFind);
        m_hwndFind = nullptr;
    }
}

//------------------------------------------------------------------------------
// Close Replace dialog
//------------------------------------------------------------------------------
void DialogManager::CloseReplaceDialog() noexcept {
    if (m_hwndReplace) {
        DestroyWindow(m_hwndReplace);
        m_hwndReplace = nullptr;
    }
}

//------------------------------------------------------------------------------
// Find Next (F3)
//------------------------------------------------------------------------------
void DialogManager::FindNext() {
    if (m_findParams.searchText.empty()) {
        // Show find dialog if no search text
        ShowFindDialog();
        return;
    }
    
    if (m_editor) {
        bool found = m_editor->SearchText(
            m_findParams.searchText,
            m_findParams.matchCase,
            m_findParams.wrapAround,
            m_findParams.searchUp,
            m_findParams.useRegex
        );
        
        if (!found) {
            MessageBoxW(m_hwndParent, L"Cannot find the specified text.", 
                        L"QNote", MB_OK | MB_ICONINFORMATION);
        }
    }
}

//------------------------------------------------------------------------------
// Check if message is for a dialog
//------------------------------------------------------------------------------
bool DialogManager::IsDialogMessage(MSG* pMsg) noexcept {
    if (m_hwndFind && ::IsDialogMessage(m_hwndFind, pMsg)) {
        return true;
    }
    if (m_hwndReplace && ::IsDialogMessage(m_hwndReplace, pMsg)) {
        return true;
    }
    return false;
}

//------------------------------------------------------------------------------
// Find dialog procedure
//------------------------------------------------------------------------------
INT_PTR CALLBACK DialogManager::FindDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            if (s_instance) {
                s_instance->OnFindInit(hwnd);
            }
            return TRUE;
        }
        
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDC_FIND_NEXT:
                    if (s_instance) {
                        s_instance->OnFindNext(hwnd);
                    }
                    return TRUE;
                    
                case IDCANCEL:
                    if (s_instance) {
                        s_instance->OnFindClose(hwnd);
                    }
                    return TRUE;
                    
                case IDC_MATCH_CASE:
                case IDC_WRAP_AROUND:
                case IDC_USE_REGEX:
                case IDC_DIRECTION_UP:
                case IDC_DIRECTION_DOWN:
                    // Update find params from controls
                    if (s_instance) {
                        s_instance->m_findParams.matchCase = 
                            IsDlgButtonChecked(hwnd, IDC_MATCH_CASE) == BST_CHECKED;
                        s_instance->m_findParams.wrapAround = 
                            IsDlgButtonChecked(hwnd, IDC_WRAP_AROUND) == BST_CHECKED;
                        s_instance->m_findParams.useRegex = 
                            IsDlgButtonChecked(hwnd, IDC_USE_REGEX) == BST_CHECKED;
                        s_instance->m_findParams.searchUp = 
                            IsDlgButtonChecked(hwnd, IDC_DIRECTION_UP) == BST_CHECKED;
                    }
                    return TRUE;
            }
            break;
        }
        
        case WM_CLOSE:
            if (s_instance) {
                s_instance->OnFindClose(hwnd);
            }
            return TRUE;
    }
    
    return FALSE;
}

//------------------------------------------------------------------------------
// Replace dialog procedure
//------------------------------------------------------------------------------
INT_PTR CALLBACK DialogManager::ReplaceDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            if (s_instance) {
                s_instance->OnReplaceInit(hwnd);
            }
            return TRUE;
        }
        
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDC_FIND_NEXT:
                    if (s_instance) {
                        // Get current search text
                        wchar_t buffer[1024] = {};
                        GetDlgItemTextW(hwnd, IDC_FIND_EDIT, buffer, 1024);
                        s_instance->m_replaceParams.searchText = buffer;
                        s_instance->m_findParams.searchText = buffer;
                        
                        // Find next
                        if (s_instance->m_editor) {
                            bool found = s_instance->m_editor->SearchText(
                                s_instance->m_replaceParams.searchText,
                                s_instance->m_replaceParams.matchCase,
                                s_instance->m_replaceParams.wrapAround,
                                false,  // searchUp
                                s_instance->m_replaceParams.useRegex
                            );
                            if (!found) {
                                MessageBoxW(hwnd, L"Cannot find the specified text.",
                                            L"QNote", MB_OK | MB_ICONINFORMATION);
                            }
                        }
                    }
                    return TRUE;
                    
                case IDC_REPLACE_BTN:
                    if (s_instance) {
                        s_instance->OnReplace(hwnd);
                    }
                    return TRUE;
                    
                case IDC_REPLACE_ALL:
                    if (s_instance) {
                        s_instance->OnReplaceAll(hwnd);
                    }
                    return TRUE;
                    
                case IDCANCEL:
                    if (s_instance) {
                        s_instance->OnReplaceClose(hwnd);
                    }
                    return TRUE;
                    
                case IDC_MATCH_CASE:
                case IDC_WRAP_AROUND:
                case IDC_USE_REGEX:
                    // Update replace params from controls
                    if (s_instance) {
                        s_instance->m_replaceParams.matchCase = 
                            IsDlgButtonChecked(hwnd, IDC_MATCH_CASE) == BST_CHECKED;
                        s_instance->m_replaceParams.wrapAround = 
                            IsDlgButtonChecked(hwnd, IDC_WRAP_AROUND) == BST_CHECKED;
                        s_instance->m_replaceParams.useRegex = 
                            IsDlgButtonChecked(hwnd, IDC_USE_REGEX) == BST_CHECKED;
                    }
                    return TRUE;
            }
            break;
        }
        
        case WM_CLOSE:
            if (s_instance) {
                s_instance->OnReplaceClose(hwnd);
            }
            return TRUE;
    }
    
    return FALSE;
}

//------------------------------------------------------------------------------
// Go To dialog procedure
//------------------------------------------------------------------------------
INT_PTR CALLBACK DialogManager::GoToDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static int* pLineNumber = nullptr;
    
    switch (msg) {
        case WM_INITDIALOG: {
            pLineNumber = reinterpret_cast<int*>(lParam);
            
            // Set current line in edit box
            if (pLineNumber && s_instance && s_instance->m_editor) {
                int currentLine = s_instance->m_editor->GetCurrentLine() + 1;  // 1-based
                SetDlgItemInt(hwnd, IDC_GOTO_LINE, currentLine, FALSE);
            }
            
            // Select all text in edit box
            SendDlgItemMessageW(hwnd, IDC_GOTO_LINE, EM_SETSEL, 0, -1);
            
            // Center dialog on parent
            RECT parentRect, dlgRect;
            GetWindowRect(GetParent(hwnd), &parentRect);
            GetWindowRect(hwnd, &dlgRect);
            int x = parentRect.left + (parentRect.right - parentRect.left - (dlgRect.right - dlgRect.left)) / 2;
            int y = parentRect.top + (parentRect.bottom - parentRect.top - (dlgRect.bottom - dlgRect.top)) / 2;
            SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            
            return TRUE;
        }
        
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDOK: {
                    if (pLineNumber) {
                        BOOL translated = FALSE;
                        int line = GetDlgItemInt(hwnd, IDC_GOTO_LINE, &translated, FALSE);
                        if (translated && line > 0) {
                            *pLineNumber = line;
                            EndDialog(hwnd, IDOK);
                        } else {
                            MessageBoxW(hwnd, L"Please enter a valid line number.",
                                        L"Go To Line", MB_OK | MB_ICONWARNING);
                        }
                    }
                    return TRUE;
                }
                
                case IDCANCEL:
                    EndDialog(hwnd, IDCANCEL);
                    return TRUE;
            }
            break;
        }
    }
    
    return FALSE;
}

//------------------------------------------------------------------------------
// Tab Size dialog procedure
//------------------------------------------------------------------------------
INT_PTR CALLBACK DialogManager::TabSizeDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static int* pTabSize = nullptr;
    
    switch (msg) {
        case WM_INITDIALOG: {
            pTabSize = reinterpret_cast<int*>(lParam);
            
            if (pTabSize) {
                SetDlgItemInt(hwnd, IDC_TABSIZE_EDIT, *pTabSize, FALSE);
            }
            
            SendDlgItemMessageW(hwnd, IDC_TABSIZE_EDIT, EM_SETSEL, 0, -1);
            
            // Center dialog
            RECT parentRect, dlgRect;
            GetWindowRect(GetParent(hwnd), &parentRect);
            GetWindowRect(hwnd, &dlgRect);
            int x = parentRect.left + (parentRect.right - parentRect.left - (dlgRect.right - dlgRect.left)) / 2;
            int y = parentRect.top + (parentRect.bottom - parentRect.top - (dlgRect.bottom - dlgRect.top)) / 2;
            SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            
            return TRUE;
        }
        
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDOK: {
                    if (pTabSize) {
                        BOOL translated = FALSE;
                        int size = GetDlgItemInt(hwnd, IDC_TABSIZE_EDIT, &translated, FALSE);
                        if (translated && size >= 1 && size <= 16) {
                            *pTabSize = size;
                            EndDialog(hwnd, IDOK);
                        } else {
                            MessageBoxW(hwnd, L"Tab size must be between 1 and 16.",
                                        L"Tab Size", MB_OK | MB_ICONWARNING);
                        }
                    }
                    return TRUE;
                }
                
                case IDCANCEL:
                    EndDialog(hwnd, IDCANCEL);
                    return TRUE;
            }
            break;
        }
    }
    
    return FALSE;
}

//------------------------------------------------------------------------------
// Scroll Lines dialog procedure
//------------------------------------------------------------------------------
INT_PTR CALLBACK DialogManager::ScrollLinesDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static int* pScrollLines = nullptr;
    
    switch (msg) {
        case WM_INITDIALOG: {
            pScrollLines = reinterpret_cast<int*>(lParam);
            
            if (pScrollLines) {
                SetDlgItemInt(hwnd, IDC_SCROLLLINES_EDIT, *pScrollLines, FALSE);
            }
            
            SendDlgItemMessageW(hwnd, IDC_SCROLLLINES_EDIT, EM_SETSEL, 0, -1);
            
            // Center dialog
            RECT parentRect, dlgRect;
            GetWindowRect(GetParent(hwnd), &parentRect);
            GetWindowRect(hwnd, &dlgRect);
            int x = parentRect.left + (parentRect.right - parentRect.left - (dlgRect.right - dlgRect.left)) / 2;
            int y = parentRect.top + (parentRect.bottom - parentRect.top - (dlgRect.bottom - dlgRect.top)) / 2;
            SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            
            return TRUE;
        }
        
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDOK: {
                    if (pScrollLines) {
                        BOOL translated = FALSE;
                        int val = GetDlgItemInt(hwnd, IDC_SCROLLLINES_EDIT, &translated, FALSE);
                        if (translated && val >= 0 && val <= 20) {
                            *pScrollLines = val;
                            EndDialog(hwnd, IDOK);
                        } else {
                            MessageBoxW(hwnd, L"Scroll lines must be between 0 and 20.\n0 uses the system default.",
                                        L"Scroll Lines", MB_OK | MB_ICONWARNING);
                        }
                    }
                    return TRUE;
                }
                
                case IDCANCEL:
                    EndDialog(hwnd, IDCANCEL);
                    return TRUE;
            }
            break;
        }
    }
    
    return FALSE;
}

//------------------------------------------------------------------------------
// About dialog procedure
//------------------------------------------------------------------------------
INT_PTR CALLBACK DialogManager::AboutDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            // Center dialog
            RECT parentRect, dlgRect;
            HWND parent = GetParent(hwnd);
            if (parent) {
                GetWindowRect(parent, &parentRect);
            } else {
                SystemParametersInfoW(SPI_GETWORKAREA, 0, &parentRect, 0);
            }
            GetWindowRect(hwnd, &dlgRect);
            int x = parentRect.left + (parentRect.right - parentRect.left - (dlgRect.right - dlgRect.left)) / 2;
            int y = parentRect.top + (parentRect.bottom - parentRect.top - (dlgRect.bottom - dlgRect.top)) / 2;
            SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            return TRUE;
        }
        
        case WM_COMMAND: {
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwnd, LOWORD(wParam));
                return TRUE;
            }
            break;
        }
    }
    
    return FALSE;
}

//------------------------------------------------------------------------------
// Find dialog init handler
//------------------------------------------------------------------------------
void DialogManager::OnFindInit(HWND hwnd) {
    // Set initial values
    SetDlgItemTextW(hwnd, IDC_FIND_EDIT, m_findParams.searchText.c_str());
    
    CheckDlgButton(hwnd, IDC_MATCH_CASE, m_findParams.matchCase ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd, IDC_WRAP_AROUND, m_findParams.wrapAround ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd, IDC_USE_REGEX, m_findParams.useRegex ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd, m_findParams.searchUp ? IDC_DIRECTION_UP : IDC_DIRECTION_DOWN, BST_CHECKED);
    
    // Pre-populate with selected text if any
    if (m_editor) {
        std::wstring selected = m_editor->GetSelectedText();
        if (!selected.empty() && selected.length() < 256) {
            // Only use single-line selections
            if (selected.find(L'\n') == std::wstring::npos && 
                selected.find(L'\r') == std::wstring::npos) {
                SetDlgItemTextW(hwnd, IDC_FIND_EDIT, selected.c_str());
            }
        }
    }
    
    // Select all text
    SendDlgItemMessageW(hwnd, IDC_FIND_EDIT, EM_SETSEL, 0, -1);
    
    // Position near parent
    RECT parentRect;
    GetWindowRect(m_hwndParent, &parentRect);
    SetWindowPos(hwnd, nullptr, parentRect.left + 50, parentRect.top + 100, 
                 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

//------------------------------------------------------------------------------
// Find Next handler
//------------------------------------------------------------------------------
void DialogManager::OnFindNext(HWND hwnd) {
    // Get search text
    wchar_t buffer[1024] = {};
    GetDlgItemTextW(hwnd, IDC_FIND_EDIT, buffer, 1024);
    m_findParams.searchText = buffer;
    
    if (m_findParams.searchText.empty()) {
        return;
    }
    
    // Update settings
    m_findParams.matchCase = IsDlgButtonChecked(hwnd, IDC_MATCH_CASE) == BST_CHECKED;
    m_findParams.wrapAround = IsDlgButtonChecked(hwnd, IDC_WRAP_AROUND) == BST_CHECKED;
    m_findParams.useRegex = IsDlgButtonChecked(hwnd, IDC_USE_REGEX) == BST_CHECKED;
    m_findParams.searchUp = IsDlgButtonChecked(hwnd, IDC_DIRECTION_UP) == BST_CHECKED;
    
    // Save to app settings
    if (m_settings) {
        m_settings->searchMatchCase = m_findParams.matchCase;
        m_settings->searchWrapAround = m_findParams.wrapAround;
        m_settings->searchUseRegex = m_findParams.useRegex;
    }
    
    // Perform search
    if (m_editor) {
        bool found = m_editor->SearchText(
            m_findParams.searchText,
            m_findParams.matchCase,
            m_findParams.wrapAround,
            m_findParams.searchUp,
            m_findParams.useRegex
        );
        
        if (!found) {
            MessageBoxW(hwnd, L"Cannot find the specified text.", 
                        L"QNote", MB_OK | MB_ICONINFORMATION);
        }
    }
}

//------------------------------------------------------------------------------
// Find dialog close handler
//------------------------------------------------------------------------------
void DialogManager::OnFindClose(HWND hwnd) {
    DestroyWindow(hwnd);
    m_hwndFind = nullptr;
}

//------------------------------------------------------------------------------
// Replace dialog init handler
//------------------------------------------------------------------------------
void DialogManager::OnReplaceInit(HWND hwnd) {
    // Set initial values
    SetDlgItemTextW(hwnd, IDC_FIND_EDIT, m_replaceParams.searchText.c_str());
    SetDlgItemTextW(hwnd, IDC_REPLACE_EDIT, m_replaceParams.replaceText.c_str());
    
    CheckDlgButton(hwnd, IDC_MATCH_CASE, m_replaceParams.matchCase ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd, IDC_WRAP_AROUND, m_replaceParams.wrapAround ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd, IDC_USE_REGEX, m_replaceParams.useRegex ? BST_CHECKED : BST_UNCHECKED);
    
    // Pre-populate with selected text if any
    if (m_editor) {
        std::wstring selected = m_editor->GetSelectedText();
        if (!selected.empty() && selected.length() < 256) {
            if (selected.find(L'\n') == std::wstring::npos && 
                selected.find(L'\r') == std::wstring::npos) {
                SetDlgItemTextW(hwnd, IDC_FIND_EDIT, selected.c_str());
            }
        }
    }
    
    SendDlgItemMessageW(hwnd, IDC_FIND_EDIT, EM_SETSEL, 0, -1);
    
    // Position near parent
    RECT parentRect;
    GetWindowRect(m_hwndParent, &parentRect);
    SetWindowPos(hwnd, nullptr, parentRect.left + 50, parentRect.top + 100,
                 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

//------------------------------------------------------------------------------
// Replace handler
//------------------------------------------------------------------------------
void DialogManager::OnReplace(HWND hwnd) {
    // Get search and replace text
    wchar_t searchBuffer[1024] = {};
    wchar_t replaceBuffer[1024] = {};
    GetDlgItemTextW(hwnd, IDC_FIND_EDIT, searchBuffer, 1024);
    GetDlgItemTextW(hwnd, IDC_REPLACE_EDIT, replaceBuffer, 1024);
    
    m_replaceParams.searchText = searchBuffer;
    m_replaceParams.replaceText = replaceBuffer;
    m_findParams.searchText = searchBuffer;
    
    if (m_replaceParams.searchText.empty()) {
        return;
    }
    
    // Update settings
    m_replaceParams.matchCase = IsDlgButtonChecked(hwnd, IDC_MATCH_CASE) == BST_CHECKED;
    m_replaceParams.wrapAround = IsDlgButtonChecked(hwnd, IDC_WRAP_AROUND) == BST_CHECKED;
    m_replaceParams.useRegex = IsDlgButtonChecked(hwnd, IDC_USE_REGEX) == BST_CHECKED;
    
    if (m_editor) {
        // Check if current selection matches search text
        std::wstring selected = m_editor->GetSelectedText();
        bool selectionMatches = false;
        
        if (!selected.empty()) {
            if (m_replaceParams.matchCase) {
                selectionMatches = (selected == m_replaceParams.searchText);
            } else {
                std::wstring lowerSelected = selected;
                std::wstring lowerSearch = m_replaceParams.searchText;
                for (auto& c : lowerSelected) c = towlower(c);
                for (auto& c : lowerSearch) c = towlower(c);
                selectionMatches = (lowerSelected == lowerSearch);
            }
        }
        
        if (selectionMatches) {
            // Replace current selection
            m_editor->ReplaceSelection(m_replaceParams.replaceText);
        }
        
        // Find next occurrence
        bool found = m_editor->SearchText(
            m_replaceParams.searchText,
            m_replaceParams.matchCase,
            m_replaceParams.wrapAround,
            false,
            m_replaceParams.useRegex
        );
        
        if (!found && !selectionMatches) {
            MessageBoxW(hwnd, L"Cannot find the specified text.",
                        L"QNote", MB_OK | MB_ICONINFORMATION);
        }
    }
}

//------------------------------------------------------------------------------
// Replace All handler
//------------------------------------------------------------------------------
void DialogManager::OnReplaceAll(HWND hwnd) {
    // Get search and replace text
    wchar_t searchBuffer[1024] = {};
    wchar_t replaceBuffer[1024] = {};
    GetDlgItemTextW(hwnd, IDC_FIND_EDIT, searchBuffer, 1024);
    GetDlgItemTextW(hwnd, IDC_REPLACE_EDIT, replaceBuffer, 1024);
    
    m_replaceParams.searchText = searchBuffer;
    m_replaceParams.replaceText = replaceBuffer;
    
    if (m_replaceParams.searchText.empty()) {
        return;
    }
    
    m_replaceParams.matchCase = IsDlgButtonChecked(hwnd, IDC_MATCH_CASE) == BST_CHECKED;
    m_replaceParams.useRegex = IsDlgButtonChecked(hwnd, IDC_USE_REGEX) == BST_CHECKED;
    
    if (m_editor) {
        int count = m_editor->ReplaceAll(
            m_replaceParams.searchText,
            m_replaceParams.replaceText,
            m_replaceParams.matchCase,
            m_replaceParams.useRegex
        );
        
        wchar_t message[256];
        swprintf_s(message, L"Replaced %d occurrence(s).", count);
        MessageBoxW(hwnd, message, L"Replace All", MB_OK | MB_ICONINFORMATION);
    }
}

//------------------------------------------------------------------------------
// Replace dialog close handler
//------------------------------------------------------------------------------
void DialogManager::OnReplaceClose(HWND hwnd) {
    DestroyWindow(hwnd);
    m_hwndReplace = nullptr;
}

} // namespace QNote
