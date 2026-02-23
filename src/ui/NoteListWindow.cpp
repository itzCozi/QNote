//==============================================================================
// QNote - A Lightweight Notepad Clone
// NoteListWindow.cpp - Note browser implementation
//==============================================================================

#include "NoteListWindow.h"
#include <dwmapi.h>
#include <algorithm>
#include <sstream>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")

namespace QNote {

//------------------------------------------------------------------------------
// DWM dark title bar (Windows 10 1809+)
//------------------------------------------------------------------------------
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

//------------------------------------------------------------------------------
// NoteListWindow Implementation
//------------------------------------------------------------------------------

NoteListWindow::NoteListWindow() = default;

NoteListWindow::~NoteListWindow() {
    if (m_hFont) {
        DeleteObject(m_hFont);
    }
    if (m_hBoldFont) {
        DeleteObject(m_hBoldFont);
    }
    if (m_hImageList) {
        ImageList_Destroy(m_hImageList);
    }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
}

bool NoteListWindow::Create(HINSTANCE hInstance, HWND parentWindow, NoteStore* noteStore) {
    m_hInstance = hInstance;
    m_hwndParent = parentWindow;
    m_noteStore = noteStore;
    
    // Initialize common controls
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);
    
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = WINDOW_CLASS;
    
    if (!RegisterClassExW(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }
    
    // Get parent window position for initial placement
    RECT parentRect;
    GetWindowRect(parentWindow, &parentRect);
    int x = parentRect.right + 20;
    int y = parentRect.top;
    
    // Create window
    m_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        WINDOW_CLASS,
        L"Notes - QNote",
        WS_OVERLAPPEDWINDOW,
        x, y, DEFAULT_WIDTH, DEFAULT_HEIGHT,
        nullptr,  // No parent - independent window
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

void NoteListWindow::Show() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_SHOW);
        RefreshList();
    }
}

void NoteListWindow::Hide() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

void NoteListWindow::Toggle() {
    if (IsVisible()) {
        Hide();
    } else {
        Show();
    }
}

bool NoteListWindow::IsVisible() const {
    return m_hwnd && IsWindowVisible(m_hwnd);
}

void NoteListWindow::SetViewMode(NoteListViewMode mode) {
    if (m_viewMode != mode) {
        m_viewMode = mode;
        UpdateViewModeUI();
        RefreshList();
    }
}

void NoteListWindow::RefreshList() {
    PopulateList();
    UpdateStatusText();
}

void NoteListWindow::Search(const std::wstring& query) {
    m_currentSearch = query;
    if (!query.empty()) {
        m_viewMode = NoteListViewMode::Search;
    }
    UpdateViewModeUI();
    RefreshList();
}

void NoteListWindow::ClearSearch() {
    m_currentSearch.clear();
    if (m_hwndSearch) {
        SetWindowTextW(m_hwndSearch, L"");
    }
    if (m_viewMode == NoteListViewMode::Search) {
        m_viewMode = NoteListViewMode::AllNotes;
    }
    UpdateViewModeUI();
    RefreshList();
}

LRESULT CALLBACK NoteListWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    NoteListWindow* pThis = nullptr;
    
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = static_cast<NoteListWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<NoteListWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    
    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT NoteListWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
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
            
        case WM_NOTIFY:
            OnNotify(reinterpret_cast<NMHDR*>(lParam));
            return 0;
            
        case WM_CTLCOLOREDIT:
            return OnCtlColorEdit(reinterpret_cast<HDC>(wParam), reinterpret_cast<HWND>(lParam));
            
        case WM_CTLCOLORSTATIC:
            return OnCtlColorStatic(reinterpret_cast<HDC>(wParam), reinterpret_cast<HWND>(lParam));
            
        case WM_CLOSE:
            Hide();
            return 0;
    }
    
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

void NoteListWindow::OnCreate() {
    // Create fonts
    m_hFont = CreateFontW(
        -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );
    
    m_hBoldFont = CreateFontW(
        -13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );
    
    CreateControls();
}

void NoteListWindow::OnDestroy() {
    // Cleanup handled in destructor
}

void NoteListWindow::OnSize(int width, int height) {
    int y = PADDING;
    
    // View mode buttons row
    int btnWidth = (width - PADDING * 4) / 3;
    if (m_hwndAllBtn) {
        SetWindowPos(m_hwndAllBtn, nullptr, PADDING, y, btnWidth, TOOLBAR_HEIGHT, SWP_NOZORDER);
    }
    if (m_hwndPinnedBtn) {
        SetWindowPos(m_hwndPinnedBtn, nullptr, PADDING * 2 + btnWidth, y, btnWidth, TOOLBAR_HEIGHT, SWP_NOZORDER);
    }
    if (m_hwndTimelineBtn) {
        SetWindowPos(m_hwndTimelineBtn, nullptr, PADDING * 3 + btnWidth * 2, y, btnWidth, TOOLBAR_HEIGHT, SWP_NOZORDER);
    }
    y += TOOLBAR_HEIGHT + PADDING;
    
    // Search row
    int searchBtnWidth = 60;
    int clearBtnWidth = 50;
    int searchEditWidth = width - PADDING * 4 - searchBtnWidth - clearBtnWidth;
    
    if (m_hwndSearch) {
        SetWindowPos(m_hwndSearch, nullptr, PADDING, y, searchEditWidth, SEARCH_HEIGHT, SWP_NOZORDER);
    }
    if (m_hwndSearchBtn) {
        SetWindowPos(m_hwndSearchBtn, nullptr, PADDING * 2 + searchEditWidth, y, searchBtnWidth, SEARCH_HEIGHT, SWP_NOZORDER);
    }
    if (m_hwndClearBtn) {
        SetWindowPos(m_hwndClearBtn, nullptr, PADDING * 3 + searchEditWidth + searchBtnWidth, y, clearBtnWidth, SEARCH_HEIGHT, SWP_NOZORDER);
    }
    y += SEARCH_HEIGHT + PADDING;
    
    // List view
    int listHeight = height - y - STATUS_HEIGHT - PADDING;
    if (m_hwndList) {
        SetWindowPos(m_hwndList, nullptr, PADDING, y, width - PADDING * 2, listHeight, SWP_NOZORDER);
    }
    
    // Status bar
    y += listHeight + PADDING / 2;
    if (m_hwndStatus) {
        SetWindowPos(m_hwndStatus, nullptr, PADDING, y, width - PADDING * 2, STATUS_HEIGHT, SWP_NOZORDER);
    }
}

void NoteListWindow::OnCommand(WORD id, WORD code, HWND hwndCtl) {
    switch (id) {
        case IDC_ALL_BTN:
            SetViewMode(NoteListViewMode::AllNotes);
            break;
            
        case IDC_PINNED_BTN:
            SetViewMode(NoteListViewMode::Pinned);
            break;
            
        case IDC_TIMELINE_BTN:
            SetViewMode(NoteListViewMode::Timeline);
            break;
            
        case IDC_SEARCH_BTN:
            if (m_hwndSearch) {
                int len = GetWindowTextLengthW(m_hwndSearch);
                if (len > 0) {
                    std::wstring query(len + 1, L'\0');
                    GetWindowTextW(m_hwndSearch, &query[0], len + 1);
                    query.resize(len);
                    Search(query);
                }
            }
            break;
            
        case IDC_CLEAR_BTN:
            ClearSearch();
            break;
            
        case IDC_SEARCH_EDIT:
            if (code == EN_CHANGE) {
                // Could implement real-time search here
            }
            break;
    }
}

void NoteListWindow::OnNotify(NMHDR* pnmh) {
    if (pnmh->hwndFrom == m_hwndList) {
        switch (pnmh->code) {
            case NM_DBLCLK: {
                NMITEMACTIVATE* pnmia = reinterpret_cast<NMITEMACTIVATE*>(pnmh);
                if (pnmia->iItem >= 0) {
                    OnListItemActivated(pnmia->iItem);
                }
                break;
            }
            
            case NM_RCLICK: {
                NMITEMACTIVATE* pnmia = reinterpret_cast<NMITEMACTIVATE*>(pnmh);
                if (pnmia->iItem >= 0) {
                    OnListItemRightClick(pnmia->iItem, pnmia->ptAction);
                }
                break;
            }
            
            case LVN_KEYDOWN: {
                NMLVKEYDOWN* pnkd = reinterpret_cast<NMLVKEYDOWN*>(pnmh);
                if (pnkd->wVKey == VK_RETURN) {
                    int sel = ListView_GetNextItem(m_hwndList, -1, LVNI_SELECTED);
                    if (sel >= 0) {
                        OnListItemActivated(sel);
                    }
                } else if (pnkd->wVKey == VK_DELETE) {
                    DeleteSelectedNotes();
                } else if (pnkd->wVKey == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                    // Ctrl+A: Select all items
                    SelectAllItems();
                } else if (pnkd->wVKey == 'P' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                    // Ctrl+P: Pin/unpin selected items
                    PinSelectedNotes();
                }
                break;
            }
            
            case LVN_ITEMCHANGED: {
                NMLISTVIEW* pnmlv = reinterpret_cast<NMLISTVIEW*>(pnmh);
                // Update status when selection changes
                if (pnmlv->uChanged & LVIF_STATE) {
                    if ((pnmlv->uOldState ^ pnmlv->uNewState) & LVIS_SELECTED) {
                        UpdateStatusText();
                    }
                }
                break;
            }
        }
    }
}

LRESULT NoteListWindow::OnCtlColorEdit(HDC hdc, HWND hwndEdit) {
    return DefWindowProcW(m_hwnd, WM_CTLCOLOREDIT, reinterpret_cast<WPARAM>(hdc), 
                          reinterpret_cast<LPARAM>(hwndEdit));
}

LRESULT NoteListWindow::OnCtlColorStatic(HDC hdc, HWND hwndStatic) {
    return DefWindowProcW(m_hwnd, WM_CTLCOLORSTATIC, reinterpret_cast<WPARAM>(hdc), 
                          reinterpret_cast<LPARAM>(hwndStatic));
}

void NoteListWindow::PopulateList() {
    if (!m_hwndList || !m_noteStore) return;
    
    // Clear current items
    ListView_DeleteAllItems(m_hwndList);
    m_displayedNotes.clear();
    
    // Get notes based on view mode
    NoteFilter filter;
    
    switch (m_viewMode) {
        case NoteListViewMode::AllNotes:
            filter.sortBy = NoteFilter::SortBy::UpdatedDesc;
            break;
            
        case NoteListViewMode::Pinned:
            filter.pinnedOnly = true;
            filter.sortBy = NoteFilter::SortBy::UpdatedDesc;
            break;
            
        case NoteListViewMode::Timeline:
            filter.sortBy = NoteFilter::SortBy::CreatedDesc;
            break;
            
        case NoteListViewMode::Search:
            filter.searchQuery = m_currentSearch;
            filter.sortBy = NoteFilter::SortBy::UpdatedDesc;
            break;
    }
    
    m_displayedNotes = m_noteStore->GetNotes(filter);
    
    // Add to list view
    for (size_t i = 0; i < m_displayedNotes.size(); ++i) {
        AddNoteToList(m_displayedNotes[i], static_cast<int>(i));
    }
}

void NoteListWindow::AddNoteToList(const Note& note, int index) {
    if (!m_hwndList) return;
    
    LVITEMW lvi = {};
    lvi.mask = LVIF_TEXT | LVIF_PARAM;
    lvi.iItem = index;
    lvi.iSubItem = 0;
    
    // Title with pin indicator
    std::wstring title = note.GetDisplayTitle();
    if (note.isPinned) {
        title = L"\u2605 " + title;  // Star symbol
    }
    lvi.pszText = const_cast<LPWSTR>(title.c_str());
    lvi.lParam = reinterpret_cast<LPARAM>(&m_displayedNotes[index]);
    
    int itemIndex = ListView_InsertItem(m_hwndList, &lvi);
    
    // Set date column
    std::wstring dateStr;
    if (m_viewMode == NoteListViewMode::Timeline) {
        dateStr = FormatTimestamp(note.createdAt);
    } else {
        dateStr = FormatTimestamp(note.updatedAt);
    }
    ListView_SetItemText(m_hwndList, itemIndex, 1, const_cast<LPWSTR>(dateStr.c_str()));
}

void NoteListWindow::OnListItemActivated(int index) {
    if (index < 0 || index >= static_cast<int>(m_displayedNotes.size())) return;
    
    const Note& note = m_displayedNotes[index];
    if (m_openCallback) {
        m_openCallback(note);
    }
}

void NoteListWindow::OnListItemRightClick(int index, POINT pt) {
    if (index < 0 || index >= static_cast<int>(m_displayedNotes.size())) return;
    
    // Convert to screen coordinates
    ClientToScreen(m_hwndList, &pt);
    ShowContextMenu(index, pt);
}

void NoteListWindow::ShowContextMenu(int index, POINT pt) {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;
    
    std::vector<int> selected = GetSelectedIndices();
    bool multiSelect = selected.size() > 1;
    
    if (!multiSelect) {
        const Note& note = m_displayedNotes[index];
        AppendMenuW(hMenu, MF_STRING, IDM_CTX_OPEN, L"&Open");
        AppendMenuW(hMenu, MF_STRING, IDM_CTX_PIN, note.isPinned ? L"&Unpin" : L"&Pin");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, IDM_CTX_DELETE, L"&Delete");
    } else {
        // Multi-selection context menu
        std::wstring pinText = L"&Pin/Unpin Selected (" + std::to_wstring(selected.size()) + L")";
        std::wstring deleteText = L"&Delete Selected (" + std::to_wstring(selected.size()) + L")";
        AppendMenuW(hMenu, MF_STRING, IDM_CTX_PIN, pinText.c_str());
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, IDM_CTX_DELETE, deleteText.c_str());
    }
    
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwnd, nullptr);
    
    switch (cmd) {
        case IDM_CTX_OPEN:
            OnContextMenuOpen(index);
            break;
        case IDM_CTX_PIN:
            if (multiSelect) {
                PinSelectedNotes();
            } else {
                OnContextMenuPin(index);
            }
            break;
        case IDM_CTX_DELETE:
            if (multiSelect) {
                DeleteSelectedNotes();
            } else {
                OnContextMenuDelete(index);
            }
            break;
    }
    
    DestroyMenu(hMenu);
}

void NoteListWindow::OnContextMenuPin(int index) {
    if (index < 0 || index >= static_cast<int>(m_displayedNotes.size())) return;
    
    const std::wstring& noteId = m_displayedNotes[index].id;
    if (m_noteStore) {
        m_noteStore->TogglePin(noteId);
        RefreshList();
    }
}

void NoteListWindow::OnContextMenuDelete(int index) {
    if (index < 0 || index >= static_cast<int>(m_displayedNotes.size())) return;
    
    const Note& note = m_displayedNotes[index];
    
    // Confirm deletion
    std::wstring msg = L"Delete \"" + note.GetDisplayTitle() + L"\"?";
    if (MessageBoxW(m_hwnd, msg.c_str(), L"Delete Note", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        if (m_deleteCallback) {
            m_deleteCallback(note.id);
        }
        if (m_noteStore) {
            m_noteStore->DeleteNote(note.id);
        }
        RefreshList();
    }
}

void NoteListWindow::OnContextMenuOpen(int index) {
    OnListItemActivated(index);
}

std::vector<int> NoteListWindow::GetSelectedIndices() const {
    std::vector<int> indices;
    if (!m_hwndList) return indices;
    
    int index = -1;
    while ((index = ListView_GetNextItem(m_hwndList, index, LVNI_SELECTED)) != -1) {
        indices.push_back(index);
    }
    return indices;
}

void NoteListWindow::SelectAllItems() {
    if (!m_hwndList) return;
    
    int count = ListView_GetItemCount(m_hwndList);
    for (int i = 0; i < count; ++i) {
        ListView_SetItemState(m_hwndList, i, LVIS_SELECTED, LVIS_SELECTED);
    }
}

void NoteListWindow::DeleteSelectedNotes() {
    if (!m_hwndList || !m_noteStore) return;
    
    std::vector<int> selected = GetSelectedIndices();
    if (selected.empty()) return;
    
    // Confirm deletion
    std::wstring msg;
    if (selected.size() == 1) {
        msg = L"Delete \"" + m_displayedNotes[selected[0]].GetDisplayTitle() + L"\"?";
    } else {
        msg = L"Delete " + std::to_wstring(selected.size()) + L" selected notes?";
    }
    
    if (MessageBoxW(m_hwnd, msg.c_str(), L"Delete Notes", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        // Collect IDs first (indices will become invalid during deletion)
        std::vector<std::wstring> idsToDelete;
        for (int idx : selected) {
            if (idx >= 0 && idx < static_cast<int>(m_displayedNotes.size())) {
                idsToDelete.push_back(m_displayedNotes[idx].id);
            }
        }
        
        // Delete notes
        for (const auto& id : idsToDelete) {
            if (m_deleteCallback) {
                m_deleteCallback(id);
            }
            m_noteStore->DeleteNote(id);
        }
        
        RefreshList();
    }
}

void NoteListWindow::PinSelectedNotes() {
    if (!m_hwndList || !m_noteStore) return;
    
    std::vector<int> selected = GetSelectedIndices();
    if (selected.empty()) return;
    
    // Toggle pin state for all selected notes
    for (int idx : selected) {
        if (idx >= 0 && idx < static_cast<int>(m_displayedNotes.size())) {
            m_noteStore->TogglePin(m_displayedNotes[idx].id);
        }
    }
    
    RefreshList();
}

void NoteListWindow::CreateControls() {
    // View mode buttons
    m_hwndAllBtn = CreateWindowExW(
        0, L"BUTTON", L"All Notes",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 100, TOOLBAR_HEIGHT,
        m_hwnd, reinterpret_cast<HMENU>(IDC_ALL_BTN), m_hInstance, nullptr
    );
    
    m_hwndPinnedBtn = CreateWindowExW(
        0, L"BUTTON", L"Pinned",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 100, TOOLBAR_HEIGHT,
        m_hwnd, reinterpret_cast<HMENU>(IDC_PINNED_BTN), m_hInstance, nullptr
    );
    
    m_hwndTimelineBtn = CreateWindowExW(
        0, L"BUTTON", L"Timeline",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 100, TOOLBAR_HEIGHT,
        m_hwnd, reinterpret_cast<HMENU>(IDC_TIMELINE_BTN), m_hInstance, nullptr
    );
    
    // Search controls
    m_hwndSearch = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 200, SEARCH_HEIGHT,
        m_hwnd, reinterpret_cast<HMENU>(IDC_SEARCH_EDIT), m_hInstance, nullptr
    );
    
    // Set placeholder text
    SendMessageW(m_hwndSearch, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"Search notes..."));
    
    m_hwndSearchBtn = CreateWindowExW(
        0, L"BUTTON", L"Search",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 60, SEARCH_HEIGHT,
        m_hwnd, reinterpret_cast<HMENU>(IDC_SEARCH_BTN), m_hInstance, nullptr
    );
    
    m_hwndClearBtn = CreateWindowExW(
        0, L"BUTTON", L"Clear",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 50, SEARCH_HEIGHT,
        m_hwnd, reinterpret_cast<HMENU>(IDC_CLEAR_BTN), m_hInstance, nullptr
    );
    
    // List view
    m_hwndList = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
        0, 0, 400, 300,
        m_hwnd, reinterpret_cast<HMENU>(IDC_NOTE_LIST), m_hInstance, nullptr
    );
    
    // Set extended styles
    ListView_SetExtendedListViewStyle(m_hwndList, 
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    
    // Add columns
    LVCOLUMNW lvc = {};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    
    lvc.iSubItem = 0;
    lvc.pszText = const_cast<LPWSTR>(L"Title");
    lvc.cx = 280;
    ListView_InsertColumn(m_hwndList, 0, &lvc);
    
    lvc.iSubItem = 1;
    lvc.pszText = const_cast<LPWSTR>(L"Date");
    lvc.cx = 140;
    ListView_InsertColumn(m_hwndList, 1, &lvc);
    
    // Status label
    m_hwndStatus = CreateWindowExW(
        0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 300, STATUS_HEIGHT,
        m_hwnd, nullptr, m_hInstance, nullptr
    );
    
    // Set fonts
    if (m_hFont) {
        SendMessageW(m_hwndAllBtn, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
        SendMessageW(m_hwndPinnedBtn, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
        SendMessageW(m_hwndTimelineBtn, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
        SendMessageW(m_hwndSearch, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
        SendMessageW(m_hwndSearchBtn, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
        SendMessageW(m_hwndClearBtn, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
        SendMessageW(m_hwndStatus, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
    }
    
    UpdateViewModeUI();
}

void NoteListWindow::UpdateViewModeUI() {
    // Update window title based on mode
    std::wstring windowTitle;
    switch (m_viewMode) {
        case NoteListViewMode::AllNotes:
            windowTitle = L"All Notes - QNote";
            break;
        case NoteListViewMode::Pinned:
            windowTitle = L"Pinned Notes - QNote";
            break;
        case NoteListViewMode::Timeline:
            windowTitle = L"Timeline - QNote";
            break;
        case NoteListViewMode::Search:
            windowTitle = L"Search Results - QNote";
            break;
    }
    SetWindowTextW(m_hwnd, windowTitle.c_str());
}

void NoteListWindow::UpdateStatusText() {
    if (!m_hwndStatus) return;
    
    std::wstringstream ss;
    
    std::vector<int> selected = GetSelectedIndices();
    if (selected.size() > 1) {
        ss << selected.size() << L" selected | ";
    }
    
    ss << m_displayedNotes.size() << L" note(s)";
    
    if (!m_currentSearch.empty()) {
        ss << L" matching \"" << m_currentSearch << L"\"";
    }
    
    ss << L" | Ctrl+A: Select All, Ctrl+P: Pin, Del: Delete";
    
    SetWindowTextW(m_hwndStatus, ss.str().c_str());
}

std::wstring NoteListWindow::GetDateGroupHeader(time_t timestamp) const {
    time_t now = std::time(nullptr);
    time_t todayMidnight = GetMidnight(now);
    time_t noteMidnight = GetMidnight(timestamp);
    
    if (noteMidnight == todayMidnight) {
        return L"Today";
    } else if (noteMidnight == todayMidnight - 24 * 60 * 60) {
        return L"Yesterday";
    } else if (noteMidnight >= todayMidnight - 7 * 24 * 60 * 60) {
        return L"This Week";
    } else if (noteMidnight >= todayMidnight - 30 * 24 * 60 * 60) {
        return L"This Month";
    } else {
        return FormatTimestamp(timestamp, false);
    }
}

} // namespace QNote
