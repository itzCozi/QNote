//==============================================================================
// QNote - A Lightweight Notepad Clone
// FindBar.cpp - Chrome-style inline find/replace bar implementation
//==============================================================================

#include "FindBar.h"
#include "Editor.h"
#include <CommCtrl.h>
#include <regex>
#include <windowsx.h>

#pragma comment(lib, "comctl32.lib")

namespace QNote {

// Static member initialization
bool FindBar::s_classRegistered = false;

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
FindBar::~FindBar() {
    Destroy();
}

//------------------------------------------------------------------------------
// Create the find bar
//------------------------------------------------------------------------------
bool FindBar::Create(HWND parent, HINSTANCE hInstance, Editor* editor) {
    m_hwndParent = parent;
    m_hInstance = hInstance;
    m_editor = editor;
    
    // Register window class if needed
    if (!s_classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = 0;
        wc.lpfnWndProc = FindBarProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1);
        wc.lpszClassName = FINDBAR_CLASS;
        
        if (RegisterClassExW(&wc)) {
            s_classRegistered = true;
        } else {
            return false;
        }
    }
    
    // Create container window
    m_hwndContainer = CreateWindowExW(
        0,
        FINDBAR_CLASS,
        L"",
        WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 100, FINDBAR_HEIGHT,
        parent,
        nullptr,
        hInstance,
        this
    );
    
    if (!m_hwndContainer) {
        return false;
    }
    
    // Create font
    NONCLIENTMETRICSW ncm = {};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    m_font = CreateFontIndirectW(&ncm.lfMessageFont);
    
    // Create child controls
    CreateControls();
    
    return true;
}

//------------------------------------------------------------------------------
// Destroy the find bar
//------------------------------------------------------------------------------
void FindBar::Destroy() noexcept {
    if (m_font) {
        DeleteObject(m_font);
        m_font = nullptr;
    }
    
    if (m_hwndContainer) {
        DestroyWindow(m_hwndContainer);
        m_hwndContainer = nullptr;
    }
    
    // Reset control handles
    m_hwndSearchEdit = nullptr;
    m_hwndReplaceEdit = nullptr;
    m_hwndMatchCount = nullptr;
    m_hwndPrevBtn = nullptr;
    m_hwndNextBtn = nullptr;
    m_hwndCloseBtn = nullptr;
    m_hwndReplaceBtn = nullptr;
    m_hwndReplaceAllBtn = nullptr;
    m_hwndCaseBtn = nullptr;
    m_hwndWholeWordBtn = nullptr;
    m_hwndRegexBtn = nullptr;
}

//------------------------------------------------------------------------------
// Show the find bar
//------------------------------------------------------------------------------
void FindBar::Show(FindBarMode mode) {
    if (!m_hwndContainer) return;
    
    m_mode = mode;
    m_visible = true;
    
    // Show/hide replace row
    if (m_hwndReplaceEdit) {
        ShowWindow(m_hwndReplaceEdit, mode == FindBarMode::Replace ? SW_SHOW : SW_HIDE);
    }
    if (m_hwndReplaceLabel) {
        ShowWindow(m_hwndReplaceLabel, mode == FindBarMode::Replace ? SW_SHOW : SW_HIDE);
    }
    if (m_hwndReplaceBtn) {
        ShowWindow(m_hwndReplaceBtn, mode == FindBarMode::Replace ? SW_SHOW : SW_HIDE);
    }
    if (m_hwndReplaceAllBtn) {
        ShowWindow(m_hwndReplaceAllBtn, mode == FindBarMode::Replace ? SW_SHOW : SW_HIDE);
    }
    
    ShowWindow(m_hwndContainer, SW_SHOW);
    
    // Notify parent to resize
    SendMessageW(m_hwndParent, WM_SIZE, 0, 0);
    
    // Populate from selection and focus
    PopulateFromSelection();
    Focus();
}

//------------------------------------------------------------------------------
// Hide the find bar
//------------------------------------------------------------------------------
void FindBar::Hide() {
    if (!m_hwndContainer) return;
    
    m_visible = false;
    ShowWindow(m_hwndContainer, SW_HIDE);
    
    // Notify parent to resize and return focus to editor
    SendMessageW(m_hwndParent, WM_SIZE, 0, 0);
    
    if (m_editor) {
        m_editor->SetFocus();
    }
}

//------------------------------------------------------------------------------
// Set mode (Find or Replace)
//------------------------------------------------------------------------------
void FindBar::SetMode(FindBarMode mode) {
    if (m_mode == mode) return;
    
    m_mode = mode;
    
    // Show/hide replace controls
    if (m_hwndReplaceEdit) {
        ShowWindow(m_hwndReplaceEdit, mode == FindBarMode::Replace ? SW_SHOW : SW_HIDE);
    }
    if (m_hwndReplaceLabel) {
        ShowWindow(m_hwndReplaceLabel, mode == FindBarMode::Replace ? SW_SHOW : SW_HIDE);
    }
    if (m_hwndReplaceBtn) {
        ShowWindow(m_hwndReplaceBtn, mode == FindBarMode::Replace ? SW_SHOW : SW_HIDE);
    }
    if (m_hwndReplaceAllBtn) {
        ShowWindow(m_hwndReplaceAllBtn, mode == FindBarMode::Replace ? SW_SHOW : SW_HIDE);
    }
    
    // Resize container
    if (m_visible) {
        SendMessageW(m_hwndParent, WM_SIZE, 0, 0);
    }
}

//------------------------------------------------------------------------------
// Resize the find bar
//------------------------------------------------------------------------------
void FindBar::Resize(int x, int y, int width) noexcept {
    if (!m_hwndContainer || !m_visible) return;
    
    int height = GetHeight();
    SetWindowPos(m_hwndContainer, nullptr, x, y, width, height, SWP_NOZORDER);
    
    LayoutControls();
}

//------------------------------------------------------------------------------
// Get the height of the find bar
//------------------------------------------------------------------------------
int FindBar::GetHeight() const noexcept {
    if (!m_visible) return 0;
    return (m_mode == FindBarMode::Replace) ? FINDBAR_HEIGHT_REPLACE : FINDBAR_HEIGHT;
}

//------------------------------------------------------------------------------
// Set focus to the search box
//------------------------------------------------------------------------------
void FindBar::Focus() {
    if (m_hwndSearchEdit) {
        ::SetFocus(m_hwndSearchEdit);
        SendMessageW(m_hwndSearchEdit, EM_SETSEL, 0, -1);
    }
}

//------------------------------------------------------------------------------
// Handle keyboard shortcuts
//------------------------------------------------------------------------------
bool FindBar::HandleKeyDown(WPARAM vKey) {
    if (!m_visible) return false;
    
    switch (vKey) {
        case VK_ESCAPE:
            Hide();
            return true;
            
        case VK_RETURN:
            if (GetKeyState(VK_SHIFT) & 0x8000) {
                FindPrevious();
            } else {
                FindNext();
            }
            return true;
            
        case VK_F3:
            if (GetKeyState(VK_SHIFT) & 0x8000) {
                FindPrevious();
            } else {
                FindNext();
            }
            return true;
    }
    
    return false;
}

//------------------------------------------------------------------------------
// Check if message is for find bar dialogs
//------------------------------------------------------------------------------
bool FindBar::IsDialogMessage(MSG* pMsg) noexcept {
    if (!m_hwndContainer || !m_visible) return false;
    
    // Handle Tab navigation within the find bar
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_TAB) {
        HWND hwndFocus = GetFocus();
        if (IsChild(m_hwndContainer, hwndFocus)) {
            // Let Tab navigate between controls
            HWND hwndNext = GetNextDlgTabItem(m_hwndContainer, hwndFocus, 
                                               GetKeyState(VK_SHIFT) & 0x8000);
            if (hwndNext) {
                ::SetFocus(hwndNext);
                return true;
            }
        }
    }
    
    // Handle Enter for search
    if (pMsg->message == WM_KEYDOWN) {
        HWND hwndFocus = GetFocus();
        if (hwndFocus == m_hwndSearchEdit || hwndFocus == m_hwndReplaceEdit) {
            if (pMsg->wParam == VK_RETURN) {
                if (hwndFocus == m_hwndSearchEdit) {
                    if (GetKeyState(VK_SHIFT) & 0x8000) {
                        FindPrevious();
                    } else {
                        FindNext();
                    }
                } else if (hwndFocus == m_hwndReplaceEdit) {
                    ReplaceNext();
                }
                return true;
            }
            if (pMsg->wParam == VK_ESCAPE) {
                Hide();
                return true;
            }
        }
    }
    
    return ::IsDialogMessageW(m_hwndContainer, pMsg) != FALSE;
}

//------------------------------------------------------------------------------
// Find next match
//------------------------------------------------------------------------------
void FindBar::FindNext() {
    DoFind(true);
}

//------------------------------------------------------------------------------
// Find previous match
//------------------------------------------------------------------------------
void FindBar::FindPrevious() {
    DoFind(false);
}

//------------------------------------------------------------------------------
// Perform find operation
//------------------------------------------------------------------------------
bool FindBar::DoFind(bool forward) {
    if (!m_editor) return false;
    
    std::wstring searchText = GetSearchText();
    if (searchText.empty()) return false;
    
    // Build search pattern
    std::wstring pattern = searchText;
    
    // If whole word matching is enabled, wrap pattern with word boundaries
    if (m_options.wholeWord && !m_options.useRegex) {
        // For plain text with whole word, we need to use regex
        pattern = L"\\b" + std::wstring(searchText) + L"\\b";
    }
    
    bool found = m_editor->SearchText(
        m_options.wholeWord ? pattern : searchText,
        m_options.matchCase,
        true,  // wrap around
        !forward,  // searchUp
        m_options.useRegex || m_options.wholeWord,  // use regex if whole word
        true   // select match
    );
    
    UpdateMatchCount();
    
    if (!found) {
        // Flash the search box to indicate not found
        FLASHWINFO fwi = {};
        fwi.cbSize = sizeof(fwi);
        fwi.hwnd = m_hwndContainer;
        fwi.dwFlags = FLASHW_ALL;
        fwi.uCount = 2;
        fwi.dwTimeout = 100;
        FlashWindowEx(&fwi);
    }
    
    return found;
}

//------------------------------------------------------------------------------
// Replace next match
//------------------------------------------------------------------------------
void FindBar::ReplaceNext() {
    if (!m_editor) return;
    
    std::wstring searchText = GetSearchText();
    std::wstring replaceText = GetReplaceText();
    
    if (searchText.empty()) return;
    
    // Check if current selection matches search
    std::wstring selected = m_editor->GetSelectedText();
    bool selectionMatches = false;
    
    if (m_options.useRegex || m_options.wholeWord) {
        try {
            std::wregex::flag_type flags = std::regex::ECMAScript;
            if (!m_options.matchCase) {
                flags |= std::regex::icase;
            }
            
            std::wstring pattern = searchText;
            if (m_options.wholeWord && !m_options.useRegex) {
                pattern = L"\\b" + searchText + L"\\b";
            }
            
            std::wregex regex(pattern, flags);
            selectionMatches = std::regex_match(selected, regex);
        } catch (const std::regex_error&) {
            return;
        }
    } else {
        if (m_options.matchCase) {
            selectionMatches = (selected == searchText);
        } else {
            // Case-insensitive comparison
            std::wstring lowerSel = selected;
            std::wstring lowerSearch = searchText;
            for (auto& c : lowerSel) c = towlower(c);
            for (auto& c : lowerSearch) c = towlower(c);
            selectionMatches = (lowerSel == lowerSearch);
        }
    }
    
    if (selectionMatches) {
        // Replace the selection
        if (m_options.useRegex) {
            try {
                std::wregex::flag_type flags = std::regex::ECMAScript;
                if (!m_options.matchCase) {
                    flags |= std::regex::icase;
                }
                std::wregex regex(searchText, flags);
                std::wstring replaced = std::regex_replace(selected, regex, replaceText);
                m_editor->ReplaceSelection(replaced);
            } catch (const std::regex_error&) {
                return;
            }
        } else {
            m_editor->ReplaceSelection(replaceText);
        }
    }
    
    // Find next match
    FindNext();
}

//------------------------------------------------------------------------------
// Replace all matches
//------------------------------------------------------------------------------
void FindBar::ReplaceAll() {
    if (!m_editor) return;
    
    std::wstring searchText = GetSearchText();
    std::wstring replaceText = GetReplaceText();
    
    if (searchText.empty()) return;
    
    // Build search pattern for whole word
    std::wstring pattern = searchText;
    if (m_options.wholeWord && !m_options.useRegex) {
        pattern = L"\\b" + searchText + L"\\b";
    }
    
    int count = m_editor->ReplaceAll(
        m_options.wholeWord ? pattern : searchText,
        replaceText,
        m_options.matchCase,
        m_options.useRegex || m_options.wholeWord
    );
    
    // Update match count display
    if (m_hwndMatchCount) {
        wchar_t buf[64];
        if (count > 0) {
            swprintf_s(buf, L"%d replaced", count);
        } else {
            wcscpy_s(buf, L"No matches");
        }
        SetWindowTextW(m_hwndMatchCount, buf);
    }
}

//------------------------------------------------------------------------------
// Get current search text
//------------------------------------------------------------------------------
std::wstring FindBar::GetSearchText() const {
    if (!m_hwndSearchEdit) return L"";
    
    int len = GetWindowTextLengthW(m_hwndSearchEdit);
    if (len == 0) return L"";
    
    std::wstring text(len + 1, L'\0');
    GetWindowTextW(m_hwndSearchEdit, text.data(), len + 1);
    text.resize(len);
    
    return text;
}

//------------------------------------------------------------------------------
// Get current replace text
//------------------------------------------------------------------------------
std::wstring FindBar::GetReplaceText() const {
    if (!m_hwndReplaceEdit) return L"";
    
    int len = GetWindowTextLengthW(m_hwndReplaceEdit);
    if (len == 0) return L"";
    
    std::wstring text(len + 1, L'\0');
    GetWindowTextW(m_hwndReplaceEdit, text.data(), len + 1);
    text.resize(len);
    
    return text;
}

//------------------------------------------------------------------------------
// Populate search box with selected text
//------------------------------------------------------------------------------
void FindBar::PopulateFromSelection() {
    if (!m_editor || !m_hwndSearchEdit) return;
    
    std::wstring selected = m_editor->GetSelectedText();
    if (!selected.empty() && selected.find(L'\n') == std::wstring::npos) {
        // Only use single-line selections
        SetWindowTextW(m_hwndSearchEdit, selected.c_str());
        SendMessageW(m_hwndSearchEdit, EM_SETSEL, 0, -1);
    }
    
    UpdateMatchCount();
}

//------------------------------------------------------------------------------
// Window procedure for the find bar container
//------------------------------------------------------------------------------
LRESULT CALLBACK FindBar::FindBarProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    FindBar* pThis = nullptr;
    
    if (msg == WM_NCCREATE) {
        auto* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<FindBar*>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<FindBar*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    
    switch (msg) {
        case WM_COMMAND:
            if (pThis) {
                pThis->OnCommand(LOWORD(wParam), HIWORD(wParam), reinterpret_cast<HWND>(lParam));
                return 0;
            }
            break;
            
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Draw bottom border
            RECT rc;
            GetClientRect(hwnd, &rc);
            HPEN hPen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
            HPEN hOldPen = SelectPen(hdc, hPen);
            MoveToEx(hdc, 0, rc.bottom - 1, nullptr);
            LineTo(hdc, rc.right, rc.bottom - 1);
            SelectPen(hdc, hOldPen);
            DeleteObject(hPen);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_CTLCOLORSTATIC:
            // Make static controls use the dialog background
            SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_3DFACE));
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

//------------------------------------------------------------------------------
// Handle commands from child controls
//------------------------------------------------------------------------------
void FindBar::OnCommand(WORD id, WORD code, HWND hwndCtl) {
    switch (id) {
        case ID_SEARCH_EDIT:
            if (code == EN_CHANGE) {
                UpdateMatchCount();
                UpdateButtonStates();
            }
            break;
            
        case ID_PREV_BTN:
            if (code == BN_CLICKED) {
                FindPrevious();
            }
            break;
            
        case ID_NEXT_BTN:
            if (code == BN_CLICKED) {
                FindNext();
            }
            break;
            
        case ID_CLOSE_BTN:
            if (code == BN_CLICKED) {
                Hide();
            }
            break;
            
        case ID_REPLACE_BTN:
            if (code == BN_CLICKED) {
                ReplaceNext();
            }
            break;
            
        case ID_REPLACE_ALL_BTN:
            if (code == BN_CLICKED) {
                ReplaceAll();
            }
            break;
            
        case ID_CASE_BTN:
            if (code == BN_CLICKED) {
                m_options.matchCase = !m_options.matchCase;
                // Update button state visually
                SendMessageW(m_hwndCaseBtn, BM_SETCHECK, 
                             m_options.matchCase ? BST_CHECKED : BST_UNCHECKED, 0);
                UpdateMatchCount();
            }
            break;
            
        case ID_WHOLEWORD_BTN:
            if (code == BN_CLICKED) {
                m_options.wholeWord = !m_options.wholeWord;
                SendMessageW(m_hwndWholeWordBtn, BM_SETCHECK,
                             m_options.wholeWord ? BST_CHECKED : BST_UNCHECKED, 0);
                UpdateMatchCount();
            }
            break;
            
        case ID_REGEX_BTN:
            if (code == BN_CLICKED) {
                m_options.useRegex = !m_options.useRegex;
                SendMessageW(m_hwndRegexBtn, BM_SETCHECK,
                             m_options.useRegex ? BST_CHECKED : BST_UNCHECKED, 0);
                // Disable whole word when regex is enabled
                if (m_options.useRegex) {
                    EnableWindow(m_hwndWholeWordBtn, FALSE);
                } else {
                    EnableWindow(m_hwndWholeWordBtn, TRUE);
                }
                UpdateMatchCount();
            }
            break;
    }
}

//------------------------------------------------------------------------------
// Create child controls
//------------------------------------------------------------------------------
void FindBar::CreateControls() {
    if (!m_hwndContainer) return;
    
    int x = MARGIN;
    int y = (FINDBAR_HEIGHT - CONTROL_HEIGHT) / 2;
    
    // Search edit
    m_hwndSearchEdit = CreateEdit(x, y, EDIT_WIDTH, CONTROL_HEIGHT, ID_SEARCH_EDIT);
    x += EDIT_WIDTH + SPACING;
    
    // Match count label
    m_hwndMatchCount = CreateWindowExW(
        0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        x, y, 80, CONTROL_HEIGHT,
        m_hwndContainer,
        nullptr,
        m_hInstance,
        nullptr
    );
    ApplyFont(m_hwndMatchCount);
    x += 80 + SPACING;
    
    // Previous button
    m_hwndPrevBtn = CreateButton(x, y, BUTTON_WIDTH, CONTROL_HEIGHT, L"\x25B2", ID_PREV_BTN);
    x += BUTTON_WIDTH + 2;
    
    // Next button
    m_hwndNextBtn = CreateButton(x, y, BUTTON_WIDTH, CONTROL_HEIGHT, L"\x25BC", ID_NEXT_BTN);
    x += BUTTON_WIDTH + SPACING * 2;
    
    // Case sensitivity toggle
    m_hwndCaseBtn = CreateButton(x, y, 32, CONTROL_HEIGHT, L"Aa", ID_CASE_BTN, true);
    x += 32 + 2;
    
    // Whole word toggle
    m_hwndWholeWordBtn = CreateButton(x, y, 32, CONTROL_HEIGHT, L"W", ID_WHOLEWORD_BTN, true);
    x += 32 + 2;
    
    // Regex toggle
    m_hwndRegexBtn = CreateButton(x, y, 32, CONTROL_HEIGHT, L".*", ID_REGEX_BTN, true);
    x += 32 + SPACING * 2;
    
    // Close button (positioned at right edge, will be handled in LayoutControls)
    m_hwndCloseBtn = CreateButton(0, y, BUTTON_WIDTH, CONTROL_HEIGHT, L"\x2715", ID_CLOSE_BTN);
    
    // Replace row (positioned below search row)
    int y2 = FINDBAR_HEIGHT + (FINDBAR_HEIGHT - CONTROL_HEIGHT) / 2 - SPACING;
    x = MARGIN;
    
    // Replace edit
    m_hwndReplaceEdit = CreateEdit(x, y2, EDIT_WIDTH, CONTROL_HEIGHT, ID_REPLACE_EDIT);
    ShowWindow(m_hwndReplaceEdit, SW_HIDE);
    x += EDIT_WIDTH + SPACING;
    
    // Replace button
    m_hwndReplaceBtn = CreateButton(x, y2, 60, CONTROL_HEIGHT, L"Replace", ID_REPLACE_BTN);
    ShowWindow(m_hwndReplaceBtn, SW_HIDE);
    x += 60 + SPACING;
    
    // Replace All button
    m_hwndReplaceAllBtn = CreateButton(x, y2, 80, CONTROL_HEIGHT, L"Replace All", ID_REPLACE_ALL_BTN);
    ShowWindow(m_hwndReplaceAllBtn, SW_HIDE);
    
    // Set tooltips for toggle buttons
    // Note: For a full implementation, you would use TOOLTIPS_CLASS
}

//------------------------------------------------------------------------------
// Create a button
//------------------------------------------------------------------------------
HWND FindBar::CreateButton(int x, int y, int width, int height, const wchar_t* text, int id, bool isToggle) {
    DWORD style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_CENTER | BS_VCENTER;
    if (isToggle) {
        style |= BS_CHECKBOX | BS_PUSHLIKE;
    }
    
    HWND hwnd = CreateWindowExW(
        0, L"BUTTON", text,
        style,
        x, y, width, height,
        m_hwndContainer,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        m_hInstance,
        nullptr
    );
    
    ApplyFont(hwnd);
    return hwnd;
}

//------------------------------------------------------------------------------
// Create an edit control
//------------------------------------------------------------------------------
HWND FindBar::CreateEdit(int x, int y, int width, int height, int id) {
    HWND hwnd = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        x, y, width, height,
        m_hwndContainer,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        m_hInstance,
        nullptr
    );
    
    ApplyFont(hwnd);
    return hwnd;
}

//------------------------------------------------------------------------------
// Apply font to a control
//------------------------------------------------------------------------------
void FindBar::ApplyFont(HWND hwnd) {
    if (hwnd && m_font) {
        SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
    }
}

//------------------------------------------------------------------------------
// Layout controls based on current size
//------------------------------------------------------------------------------
void FindBar::LayoutControls() {
    if (!m_hwndContainer) return;
    
    RECT rc;
    GetClientRect(m_hwndContainer, &rc);
    int width = rc.right;
    
    // Position close button at right edge
    if (m_hwndCloseBtn) {
        int y = (FINDBAR_HEIGHT - CONTROL_HEIGHT) / 2;
        SetWindowPos(m_hwndCloseBtn, nullptr, 
                     width - MARGIN - BUTTON_WIDTH, y, 
                     BUTTON_WIDTH, CONTROL_HEIGHT, 
                     SWP_NOZORDER);
    }
}

//------------------------------------------------------------------------------
// Update match count display
//------------------------------------------------------------------------------
void FindBar::UpdateMatchCount() {
    if (!m_hwndMatchCount || !m_editor) return;
    
    std::wstring searchText = GetSearchText();
    if (searchText.empty()) {
        SetWindowTextW(m_hwndMatchCount, L"");
        m_lastMatchCount = 0;
        return;
    }
    
    // Count matches in the document
    std::wstring text = m_editor->GetText();
    int count = 0;
    
    std::wstring pattern = searchText;
    if (m_options.wholeWord && !m_options.useRegex) {
        pattern = L"\\b" + searchText + L"\\b";
    }
    
    if (m_options.useRegex || m_options.wholeWord) {
        try {
            std::wregex::flag_type flags = std::regex::ECMAScript;
            if (!m_options.matchCase) {
                flags |= std::regex::icase;
            }
            std::wregex regex(pattern, flags);
            auto begin = std::wsregex_iterator(text.begin(), text.end(), regex);
            auto end = std::wsregex_iterator();
            count = static_cast<int>(std::distance(begin, end));
        } catch (const std::regex_error&) {
            SetWindowTextW(m_hwndMatchCount, L"Invalid regex");
            return;
        }
    } else {
        // Simple text search count
        std::wstring searchLower, textLower;
        if (!m_options.matchCase) {
            searchLower = searchText;
            textLower = text;
            for (auto& c : searchLower) c = towlower(c);
            for (auto& c : textLower) c = towlower(c);
        }
        
        const std::wstring& haystack = m_options.matchCase ? text : textLower;
        const std::wstring& needle = m_options.matchCase ? searchText : searchLower;
        
        size_t pos = 0;
        while ((pos = haystack.find(needle, pos)) != std::wstring::npos) {
            ++count;
            pos += needle.length();
        }
    }
    
    m_lastMatchCount = count;
    
    wchar_t buf[64];
    if (count == 0) {
        wcscpy_s(buf, L"No results");
    } else if (count == 1) {
        wcscpy_s(buf, L"1 match");
    } else {
        swprintf_s(buf, L"%d matches", count);
    }
    
    SetWindowTextW(m_hwndMatchCount, buf);
}

//------------------------------------------------------------------------------
// Update button states
//------------------------------------------------------------------------------
void FindBar::UpdateButtonStates() {
    std::wstring searchText = GetSearchText();
    bool hasText = !searchText.empty();
    
    EnableWindow(m_hwndPrevBtn, hasText);
    EnableWindow(m_hwndNextBtn, hasText);
    EnableWindow(m_hwndReplaceBtn, hasText);
    EnableWindow(m_hwndReplaceAllBtn, hasText);
}

} // namespace QNote
