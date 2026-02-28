//==============================================================================
// QNote - A Lightweight Notepad Clone
// CharacterMap.cpp - Character Map / Emoji Picker popup window
//==============================================================================

#include "CharacterMap.h"
#include <windowsx.h>
#include <CommCtrl.h>
#include <algorithm>
#include <cctype>

namespace QNote {

bool CharacterMap::s_classRegistered = false;

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
CharacterMap::~CharacterMap() {
    Close();
}

//------------------------------------------------------------------------------
// Build the default character categories
//------------------------------------------------------------------------------
void CharacterMap::BuildCategories() {
    m_allCategories.clear();

    // --- Common Symbols ---
    {
        CharCategory cat;
        cat.name = L"Common Symbols";
        cat.entries = {
            { L"\u00A9", L"Copyright" },
            { L"\u00AE", L"Registered" },
            { L"\u2122", L"Trademark" },
            { L"\u00B0", L"Degree" },
            { L"\u00B1", L"Plus-Minus" },
            { L"\u00B7", L"Middle Dot" },
            { L"\u2022", L"Bullet" },
            { L"\u2026", L"Ellipsis" },
            { L"\u2013", L"En Dash" },
            { L"\u2014", L"Em Dash" },
            { L"\u2018", L"Left Single Quote" },
            { L"\u2019", L"Right Single Quote" },
            { L"\u201C", L"Left Double Quote" },
            { L"\u201D", L"Right Double Quote" },
            { L"\u00AB", L"Left Guillemet" },
            { L"\u00BB", L"Right Guillemet" },
            { L"\u00A7", L"Section" },
            { L"\u00B6", L"Pilcrow" },
            { L"\u2020", L"Dagger" },
            { L"\u2021", L"Double Dagger" },
            { L"\u00A6", L"Broken Bar" },
            { L"\u2030", L"Per Mille" },
            { L"\u2032", L"Prime" },
            { L"\u2033", L"Double Prime" },
        };
        m_allCategories.push_back(std::move(cat));
    }

    // --- Currency ---
    {
        CharCategory cat;
        cat.name = L"Currency";
        cat.entries = {
            { L"\u0024", L"Dollar" },
            { L"\u20AC", L"Euro" },
            { L"\u00A3", L"Pound" },
            { L"\u00A5", L"Yen" },
            { L"\u00A2", L"Cent" },
            { L"\u20B9", L"Rupee" },
            { L"\u20BD", L"Ruble" },
            { L"\u20A9", L"Won" },
            { L"\u20BA", L"Lira" },
            { L"\u20B1", L"Peso" },
            { L"\u20BF", L"Bitcoin" },
            { L"\u00A4", L"Currency Sign" },
        };
        m_allCategories.push_back(std::move(cat));
    }

    // --- Math ---
    {
        CharCategory cat;
        cat.name = L"Math & Logic";
        cat.entries = {
            { L"\u00D7", L"Multiply" },
            { L"\u00F7", L"Divide" },
            { L"\u2260", L"Not Equal" },
            { L"\u2264", L"Less or Equal" },
            { L"\u2265", L"Greater or Equal" },
            { L"\u2248", L"Approximately" },
            { L"\u221A", L"Square Root" },
            { L"\u221E", L"Infinity" },
            { L"\u2211", L"Summation" },
            { L"\u220F", L"Product" },
            { L"\u222B", L"Integral" },
            { L"\u2202", L"Partial Differential" },
            { L"\u0394", L"Delta" },
            { L"\u03C0", L"Pi" },
            { L"\u00B2", L"Superscript 2" },
            { L"\u00B3", L"Superscript 3" },
            { L"\u00BD", L"One Half" },
            { L"\u00BC", L"One Quarter" },
            { L"\u00BE", L"Three Quarters" },
            { L"\u2153", L"One Third" },
            { L"\u2154", L"Two Thirds" },
            { L"\u2200", L"For All" },
            { L"\u2203", L"There Exists" },
            { L"\u2208", L"Element Of" },
            { L"\u2209", L"Not Element Of" },
            { L"\u2229", L"Intersection" },
            { L"\u222A", L"Union" },
            { L"\u2227", L"Logical And" },
            { L"\u2228", L"Logical Or" },
            { L"\u00AC", L"Not" },
        };
        m_allCategories.push_back(std::move(cat));
    }

    // --- Arrows ---
    {
        CharCategory cat;
        cat.name = L"Arrows";
        cat.entries = {
            { L"\u2190", L"Left Arrow" },
            { L"\u2191", L"Up Arrow" },
            { L"\u2192", L"Right Arrow" },
            { L"\u2193", L"Down Arrow" },
            { L"\u2194", L"Left Right Arrow" },
            { L"\u2195", L"Up Down Arrow" },
            { L"\u21D0", L"Left Double Arrow" },
            { L"\u21D2", L"Right Double Arrow" },
            { L"\u21D4", L"Left Right Double Arrow" },
            { L"\u2196", L"North West Arrow" },
            { L"\u2197", L"North East Arrow" },
            { L"\u2198", L"South East Arrow" },
            { L"\u2199", L"South West Arrow" },
            { L"\u21B5", L"Return Arrow" },
            { L"\u21BA", L"Anticlockwise Arrow" },
            { L"\u21BB", L"Clockwise Arrow" },
        };
        m_allCategories.push_back(std::move(cat));
    }

    // --- Box Drawing ---
    {
        CharCategory cat;
        cat.name = L"Box Drawing";
        cat.entries = {
            { L"\u2500", L"Light Horizontal" },
            { L"\u2502", L"Light Vertical" },
            { L"\u250C", L"Light Down Right" },
            { L"\u2510", L"Light Down Left" },
            { L"\u2514", L"Light Up Right" },
            { L"\u2518", L"Light Up Left" },
            { L"\u251C", L"Light Vertical Right" },
            { L"\u2524", L"Light Vertical Left" },
            { L"\u252C", L"Light Down Horizontal" },
            { L"\u2534", L"Light Up Horizontal" },
            { L"\u253C", L"Light Vertical Horizontal" },
            { L"\u2550", L"Double Horizontal" },
            { L"\u2551", L"Double Vertical" },
            { L"\u2554", L"Double Down Right" },
            { L"\u2557", L"Double Down Left" },
            { L"\u255A", L"Double Up Right" },
            { L"\u255D", L"Double Up Left" },
            { L"\u2588", L"Full Block" },
            { L"\u2591", L"Light Shade" },
            { L"\u2592", L"Medium Shade" },
            { L"\u2593", L"Dark Shade" },
        };
        m_allCategories.push_back(std::move(cat));
    }

    // --- Checkmarks & Stars ---
    {
        CharCategory cat;
        cat.name = L"Checkmarks & Stars";
        cat.entries = {
            { L"\u2713", L"Check Mark" },
            { L"\u2714", L"Heavy Check Mark" },
            { L"\u2717", L"Ballot X" },
            { L"\u2718", L"Heavy Ballot X" },
            { L"\u2605", L"Black Star" },
            { L"\u2606", L"White Star" },
            { L"\u2665", L"Heart" },
            { L"\u2666", L"Diamond" },
            { L"\u2663", L"Club" },
            { L"\u2660", L"Spade" },
            { L"\u266A", L"Eighth Note" },
            { L"\u266B", L"Beamed Notes" },
            { L"\u263A", L"Smiley" },
            { L"\u2639", L"Frowning" },
            { L"\u261E", L"Pointing Right" },
            { L"\u261C", L"Pointing Left" },
            { L"\u2602", L"Umbrella" },
            { L"\u2603", L"Snowman" },
            { L"\u2600", L"Sun" },
            { L"\u2601", L"Cloud" },
        };
        m_allCategories.push_back(std::move(cat));
    }

    // --- Greek Letters ---
    {
        CharCategory cat;
        cat.name = L"Greek Letters";
        cat.entries = {
            { L"\u0391", L"Alpha (upper)" },
            { L"\u0392", L"Beta (upper)" },
            { L"\u0393", L"Gamma (upper)" },
            { L"\u0394", L"Delta (upper)" },
            { L"\u0395", L"Epsilon (upper)" },
            { L"\u03A3", L"Sigma (upper)" },
            { L"\u03A9", L"Omega (upper)" },
            { L"\u03B1", L"Alpha (lower)" },
            { L"\u03B2", L"Beta (lower)" },
            { L"\u03B3", L"Gamma (lower)" },
            { L"\u03B4", L"Delta (lower)" },
            { L"\u03B5", L"Epsilon (lower)" },
            { L"\u03B6", L"Zeta (lower)" },
            { L"\u03B7", L"Eta (lower)" },
            { L"\u03B8", L"Theta (lower)" },
            { L"\u03BB", L"Lambda (lower)" },
            { L"\u03BC", L"Mu (lower)" },
            { L"\u03C0", L"Pi (lower)" },
            { L"\u03C3", L"Sigma (lower)" },
            { L"\u03C6", L"Phi (lower)" },
            { L"\u03C9", L"Omega (lower)" },
        };
        m_allCategories.push_back(std::move(cat));
    }

    // --- Emoji (BMP only for wide compatibility) ---
    {
        CharCategory cat;
        cat.name = L"Emoji & Pictographs";
        cat.entries = {
            { L"\u2764", L"Red Heart" },
            { L"\u270C", L"Victory Hand" },
            { L"\u270D", L"Writing Hand" },
            { L"\u270E", L"Pencil" },
            { L"\u2709", L"Envelope" },
            { L"\u260E", L"Telephone" },
            { L"\u2615", L"Hot Beverage" },
            { L"\u231A", L"Watch" },
            { L"\u231B", L"Hourglass" },
            { L"\u2328", L"Keyboard" },
            { L"\u23F0", L"Alarm Clock" },
            { L"\u23F3", L"Hourglass Flowing" },
            { L"\u2702", L"Scissors" },
            { L"\u2708", L"Airplane" },
            { L"\u2744", L"Snowflake" },
            { L"\u2728", L"Sparkles" },
            { L"\u26A0", L"Warning" },
            { L"\u26A1", L"Lightning" },
            { L"\u267B", L"Recycling" },
            { L"\u2699", L"Gear" },
            { L"\u269B", L"Atom" },
            { L"\u2B50", L"Star" },
            { L"\u2B55", L"Circle" },
            { L"\u274C", L"Cross Mark" },
        };
        m_allCategories.push_back(std::move(cat));
    }

    m_categories = m_allCategories;
}

//------------------------------------------------------------------------------
// Show the character map window
//------------------------------------------------------------------------------
bool CharacterMap::Show(HWND parent, HINSTANCE hInstance,
                        std::function<void(const std::wstring&)> insertCallback) {
    // If already open, just bring to front
    if (m_hwnd && IsWindow(m_hwnd)) {
        SetForegroundWindow(m_hwnd);
        return true;
    }

    m_hwndParent = parent;
    m_hInstance = hInstance;
    m_insertCallback = std::move(insertCallback);

    BuildCategories();

    // Register class once
    if (!s_classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = WINDOW_CLASS;
        RegisterClassExW(&wc);
        s_classRegistered = true;
    }

    // Position near parent
    RECT parentRect;
    GetWindowRect(parent, &parentRect);
    int x = parentRect.left + 50;
    int y = parentRect.top + 50;

    m_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        WINDOW_CLASS,
        L"Character Map",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        x, y, WINDOW_W, WINDOW_H,
        parent,
        nullptr,
        hInstance,
        this);

    if (!m_hwnd) return false;

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    return true;
}

//------------------------------------------------------------------------------
// Close
//------------------------------------------------------------------------------
void CharacterMap::Close() noexcept {
    if (m_hwnd && IsWindow(m_hwnd)) {
        DestroyWindow(m_hwnd);
    }
    m_hwnd = nullptr;

    if (m_fontGrid)   { DeleteObject(m_fontGrid);   m_fontGrid = nullptr; }
    if (m_fontHeader) { DeleteObject(m_fontHeader); m_fontHeader = nullptr; }
    if (m_fontSearch) { DeleteObject(m_fontSearch); m_fontSearch = nullptr; }
    if (m_fontStatus) { DeleteObject(m_fontStatus); m_fontStatus = nullptr; }
}

//------------------------------------------------------------------------------
// IsVisible
//------------------------------------------------------------------------------
bool CharacterMap::IsVisible() const noexcept {
    return m_hwnd && IsWindow(m_hwnd) && ::IsWindowVisible(m_hwnd);
}

//------------------------------------------------------------------------------
// IsDialogMessage
//------------------------------------------------------------------------------
bool CharacterMap::IsDialogMessage(MSG* pMsg) noexcept {
    if (!m_hwnd || !IsWindow(m_hwnd)) return false;
    if (pMsg->hwnd != m_hwnd && !IsChild(m_hwnd, pMsg->hwnd)) return false;

    // Handle Tab key to move focus from search to grid
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_TAB) {
        if (GetFocus() == m_hwndSearch) {
            ::SetFocus(m_hwnd);
            if (m_selectedIndex < 0 && !m_flatEntries.empty()) {
                m_selectedIndex = 0;
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
            return true;
        }
    }

    // Arrow keys on grid
    if (pMsg->message == WM_KEYDOWN && pMsg->hwnd == m_hwnd) {
        OnKeyDown(pMsg->wParam);
        return true;
    }

    return false;
}

//------------------------------------------------------------------------------
// Window procedure
//------------------------------------------------------------------------------
LRESULT CALLBACK CharacterMap::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    CharacterMap* pThis = nullptr;

    if (msg == WM_NCCREATE) {
        auto* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<CharacterMap*>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<CharacterMap*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

//------------------------------------------------------------------------------
// Recalculate layout (flattened entries + rects)
//------------------------------------------------------------------------------
static void LayoutEntries(const std::vector<CharCategory>& categories,
                          int clientWidth, int cellSize, int headerH, int searchH,
                          std::vector<CharacterMap::FlatEntry>& flat,
                          int& contentHeight, int& colCount) {
    flat.clear();
    int y = 0;
    colCount = std::max(1, clientWidth / cellSize);

    for (int ci = 0; ci < static_cast<int>(categories.size()); ci++) {
        const auto& cat = categories[ci];
        // Category header
        y += headerH;

        int col = 0;
        for (int ei = 0; ei < static_cast<int>(cat.entries.size()); ei++) {
            CharacterMap::FlatEntry fe;
            fe.catIndex = ci;
            fe.entryIndex = ei;
            fe.cellRect.left = col * cellSize;
            fe.cellRect.top = y + searchH;
            fe.cellRect.right = fe.cellRect.left + cellSize;
            fe.cellRect.bottom = fe.cellRect.top + cellSize;
            flat.push_back(fe);

            col++;
            if (col >= colCount) {
                col = 0;
                y += cellSize;
            }
        }
        if (col > 0) y += cellSize;  // finish partial row
        y += 4;  // padding between categories
    }
    contentHeight = y + searchH;
}

//------------------------------------------------------------------------------
// HandleMessage
//------------------------------------------------------------------------------
LRESULT CharacterMap::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Create fonts
        m_fontGrid = CreateFontW(22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Symbol");
        m_fontHeader = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        m_fontSearch = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        m_fontStatus = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

        // Create search bar
        m_hwndSearch = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            4, 4, WINDOW_W - 12, SEARCH_BAR_H - 6,
            m_hwnd, reinterpret_cast<HMENU>(1001),
            m_hInstance, nullptr);
        SendMessageW(m_hwndSearch, WM_SETFONT, reinterpret_cast<WPARAM>(m_fontSearch), TRUE);
        SendMessageW(m_hwndSearch, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Search characters..."));

        OnSize();
        return 0;
    }

    case WM_SIZE:
        OnSize();
        return 0;

    case WM_COMMAND:
        // Search edit changed
        if (LOWORD(wParam) == 1001 && HIWORD(wParam) == EN_CHANGE) {
            wchar_t buf[128] = {};
            GetWindowTextW(m_hwndSearch, buf, 128);
            UpdateSearch(buf);
        }
        return 0;

    case WM_PAINT:
        OnPaint();
        return 0;

    case WM_MOUSEMOVE:
        OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_LBUTTONDOWN:
        OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_LBUTTONDBLCLK:
        OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        ScrollTo(m_scrollY - delta);
        return 0;
    }

    case WM_VSCROLL: {
        SCROLLINFO si = {};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(m_hwnd, SB_VERT, &si);

        int pos = si.nPos;
        switch (LOWORD(wParam)) {
            case SB_LINEUP:   pos -= CELL_SIZE; break;
            case SB_LINEDOWN: pos += CELL_SIZE; break;
            case SB_PAGEUP:   pos -= si.nPage; break;
            case SB_PAGEDOWN: pos += si.nPage; break;
            case SB_THUMBTRACK: pos = si.nTrackPos; break;
        }
        ScrollTo(pos);
        return 0;
    }

    case WM_KEYDOWN:
        OnKeyDown(wParam);
        return 0;

    case WM_DESTROY:
        m_hwnd = nullptr;
        return 0;

    default:
        break;
    }
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

//------------------------------------------------------------------------------
// OnSize
//------------------------------------------------------------------------------
void CharacterMap::OnSize() {
    if (!m_hwnd) return;

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int cw = rc.right - rc.left;

    // Reposition search bar
    if (m_hwndSearch) {
        MoveWindow(m_hwndSearch, 4, 4, cw - 8, SEARCH_BAR_H - 6, TRUE);
    }

    // Recalculate layout
    LayoutEntries(m_categories, cw, CELL_SIZE, CATEGORY_HEADER_H, SEARCH_BAR_H,
                  m_flatEntries, m_contentHeight, m_colCount);

    UpdateScrollBar();
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

//------------------------------------------------------------------------------
// UpdateScrollBar
//------------------------------------------------------------------------------
void CharacterMap::UpdateScrollBar() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int visibleH = (rc.bottom - rc.top) - SEARCH_BAR_H - STATUSBAR_H;

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = m_contentHeight;
    si.nPage = visibleH;
    si.nPos = m_scrollY;
    SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
}

//------------------------------------------------------------------------------
// ScrollTo
//------------------------------------------------------------------------------
void CharacterMap::ScrollTo(int pos) {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int visibleH = (rc.bottom - rc.top) - SEARCH_BAR_H - STATUSBAR_H;
    int maxScroll = std::max(0, m_contentHeight - visibleH);

    m_scrollY = std::clamp(pos, 0, maxScroll);
    UpdateScrollBar();
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

//------------------------------------------------------------------------------
// HitTest - return flat entry index under (x, y) in client coords
//------------------------------------------------------------------------------
int CharacterMap::HitTest(int x, int y) const {
    // Convert from client to content coords
    int cy = y + m_scrollY - SEARCH_BAR_H;

    for (int i = 0; i < static_cast<int>(m_flatEntries.size()); i++) {
        const auto& fe = m_flatEntries[i];
        RECT r = fe.cellRect;
        r.top -= SEARCH_BAR_H;
        r.bottom -= SEARCH_BAR_H;
        if (x >= r.left && x < r.right && cy >= (r.top) && cy < (r.bottom)) {
            return i;
        }
    }
    return -1;
}

//------------------------------------------------------------------------------
// OnPaint
//------------------------------------------------------------------------------
void CharacterMap::OnPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int cw = rc.right;
    int ch = rc.bottom;

    // Double buffer
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, cw, ch);
    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDC, memBmp));

    // Background
    HBRUSH bgBrush = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
    FillRect(memDC, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Set up for drawing
    SetBkMode(memDC, TRANSPARENT);
    int offsetY = SEARCH_BAR_H - m_scrollY;

    // Draw category headers + cells
    HFONT oldFont = static_cast<HFONT>(SelectObject(memDC, m_fontHeader));

    int flatIdx = 0;
    for (int ci = 0; ci < static_cast<int>(m_categories.size()); ci++) {
        const auto& cat = m_categories[ci];

        // Find first entry for this category to get Y position
        int headerY = 0;
        for (int fi = flatIdx; fi < static_cast<int>(m_flatEntries.size()); fi++) {
            if (m_flatEntries[fi].catIndex == ci) {
                headerY = m_flatEntries[fi].cellRect.top - CATEGORY_HEADER_H;
                break;
            }
        }

        // Draw header
        RECT headerRect = { 4, headerY + offsetY, cw - 4, headerY + CATEGORY_HEADER_H + offsetY };
        if (headerRect.bottom > SEARCH_BAR_H && headerRect.top < ch - STATUSBAR_H) {
            SetTextColor(memDC, GetSysColor(COLOR_GRAYTEXT));
            SelectObject(memDC, m_fontHeader);
            DrawTextW(memDC, cat.name.c_str(), static_cast<int>(cat.name.size()),
                      &headerRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        }

        // Draw cells
        SelectObject(memDC, m_fontGrid);
        for (int ei = 0; ei < static_cast<int>(cat.entries.size()); ei++) {
            if (flatIdx >= static_cast<int>(m_flatEntries.size())) break;
            const auto& fe = m_flatEntries[flatIdx];

            RECT cellRect = fe.cellRect;
            cellRect.top += offsetY;
            cellRect.bottom += offsetY;

            // Clip to visible area
            if (cellRect.bottom > SEARCH_BAR_H && cellRect.top < ch - STATUSBAR_H) {
                bool isSelected = (flatIdx == m_selectedIndex);
                bool isHover = (flatIdx == m_hoverIndex);

                if (isSelected) {
                    HBRUSH selBrush = CreateSolidBrush(GetSysColor(COLOR_HIGHLIGHT));
                    FillRect(memDC, &cellRect, selBrush);
                    DeleteObject(selBrush);
                    SetTextColor(memDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
                } else if (isHover) {
                    HBRUSH hovBrush = CreateSolidBrush(GetSysColor(COLOR_MENUHILIGHT));
                    FillRect(memDC, &cellRect, hovBrush);
                    DeleteObject(hovBrush);
                    SetTextColor(memDC, GetSysColor(COLOR_WINDOWTEXT));
                } else {
                    SetTextColor(memDC, GetSysColor(COLOR_WINDOWTEXT));
                }

                // Draw character centered
                const auto& entry = cat.entries[ei];
                DrawTextW(memDC, entry.ch.c_str(), static_cast<int>(entry.ch.size()),
                          &cellRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

                // Draw subtle border
                HPEN pen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNFACE));
                HPEN oldPen = static_cast<HPEN>(SelectObject(memDC, pen));
                HBRUSH nullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
                HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(memDC, nullBrush));
                Rectangle(memDC, cellRect.left, cellRect.top, cellRect.right, cellRect.bottom);
                SelectObject(memDC, oldPen);
                SelectObject(memDC, oldBr);
                DeleteObject(pen);
            }

            flatIdx++;
        }
    }

    // Draw status bar at bottom
    RECT statusRect = { 0, ch - STATUSBAR_H, cw, ch };
    HBRUSH statusBrush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    FillRect(memDC, &statusRect, statusBrush);
    DeleteObject(statusBrush);

    SelectObject(memDC, m_fontStatus);
    SetTextColor(memDC, GetSysColor(COLOR_BTNTEXT));

    // Show hovered/selected character info
    int infoIdx = (m_hoverIndex >= 0) ? m_hoverIndex : m_selectedIndex;
    if (infoIdx >= 0 && infoIdx < static_cast<int>(m_flatEntries.size())) {
        const auto& fe = m_flatEntries[infoIdx];
        const auto& entry = m_categories[fe.catIndex].entries[fe.entryIndex];

        wchar_t statusText[256];
        // Get Unicode codepoint
        wchar_t firstChar = entry.ch[0];
        if (entry.ch.size() == 1) {
            swprintf_s(statusText, L"  %s  U+%04X  %s", entry.ch.c_str(),
                       static_cast<unsigned>(firstChar), entry.name.c_str());
        } else {
            swprintf_s(statusText, L"  %s  %s", entry.ch.c_str(), entry.name.c_str());
        }
        RECT textRect = statusRect;
        textRect.left += 4;
        DrawTextW(memDC, statusText, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    } else {
        RECT textRect = statusRect;
        textRect.left += 4;
        DrawTextW(memDC, L"  Click a character to insert it", -1, &textRect,
                  DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    }

    SelectObject(memDC, oldFont);

    BitBlt(hdc, 0, 0, cw, ch, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);

    EndPaint(m_hwnd, &ps);
}

//------------------------------------------------------------------------------
// OnMouseMove
//------------------------------------------------------------------------------
void CharacterMap::OnMouseMove(int x, int y) {
    int idx = HitTest(x, y);
    if (idx != m_hoverIndex) {
        m_hoverIndex = idx;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

//------------------------------------------------------------------------------
// OnLButtonDown - insert the clicked character
//------------------------------------------------------------------------------
void CharacterMap::OnLButtonDown(int x, int y) {
    int idx = HitTest(x, y);
    if (idx >= 0 && idx < static_cast<int>(m_flatEntries.size())) {
        m_selectedIndex = idx;
        const auto& fe = m_flatEntries[idx];
        const auto& entry = m_categories[fe.catIndex].entries[fe.entryIndex];

        if (m_insertCallback) {
            m_insertCallback(entry.ch);
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

//------------------------------------------------------------------------------
// OnKeyDown - keyboard navigation
//------------------------------------------------------------------------------
void CharacterMap::OnKeyDown(WPARAM vk) {
    if (m_flatEntries.empty()) return;

    int total = static_cast<int>(m_flatEntries.size());

    switch (vk) {
    case VK_RIGHT:
        m_selectedIndex = std::min(m_selectedIndex + 1, total - 1);
        break;
    case VK_LEFT:
        m_selectedIndex = std::max(m_selectedIndex - 1, 0);
        break;
    case VK_DOWN:
        m_selectedIndex = std::min(m_selectedIndex + m_colCount, total - 1);
        break;
    case VK_UP:
        m_selectedIndex = std::max(m_selectedIndex - m_colCount, 0);
        break;
    case VK_HOME:
        m_selectedIndex = 0;
        break;
    case VK_END:
        m_selectedIndex = total - 1;
        break;
    case VK_RETURN:
    case VK_SPACE:
        if (m_selectedIndex >= 0 && m_selectedIndex < total) {
            const auto& fe = m_flatEntries[m_selectedIndex];
            const auto& entry = m_categories[fe.catIndex].entries[fe.entryIndex];
            if (m_insertCallback) {
                m_insertCallback(entry.ch);
            }
        }
        break;
    case VK_ESCAPE:
        Close();
        if (m_hwndParent) ::SetFocus(m_hwndParent);
        return;
    default:
        return;
    }

    EnsureSelectedVisible();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

//------------------------------------------------------------------------------
// EnsureSelectedVisible
//------------------------------------------------------------------------------
void CharacterMap::EnsureSelectedVisible() {
    if (m_selectedIndex < 0 || m_selectedIndex >= static_cast<int>(m_flatEntries.size())) return;

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int visibleH = (rc.bottom - rc.top) - SEARCH_BAR_H - STATUSBAR_H;

    const auto& fe = m_flatEntries[m_selectedIndex];
    int cellTop = fe.cellRect.top - SEARCH_BAR_H;
    int cellBot = fe.cellRect.bottom - SEARCH_BAR_H;

    if (cellTop < m_scrollY) {
        ScrollTo(cellTop);
    } else if (cellBot > m_scrollY + visibleH) {
        ScrollTo(cellBot - visibleH);
    }
}

//------------------------------------------------------------------------------
// UpdateSearch - filter categories by search string
//------------------------------------------------------------------------------
void CharacterMap::UpdateSearch(const std::wstring& query) {
    if (query.empty()) {
        m_categories = m_allCategories;
    } else {
        // Case-insensitive search
        std::wstring lower;
        lower.resize(query.size());
        for (size_t i = 0; i < query.size(); i++) {
            lower[i] = static_cast<wchar_t>(towlower(query[i]));
        }

        m_categories.clear();
        for (const auto& cat : m_allCategories) {
            CharCategory filtered;
            filtered.name = cat.name;
            for (const auto& entry : cat.entries) {
                // Match against name (case-insensitive) or the character itself
                std::wstring lowerName;
                lowerName.resize(entry.name.size());
                for (size_t i = 0; i < entry.name.size(); i++) {
                    lowerName[i] = static_cast<wchar_t>(towlower(entry.name[i]));
                }

                if (lowerName.find(lower) != std::wstring::npos ||
                    entry.ch.find(query) != std::wstring::npos) {
                    filtered.entries.push_back(entry);
                }
            }
            if (!filtered.entries.empty()) {
                m_categories.push_back(std::move(filtered));
            }
        }
    }

    m_selectedIndex = -1;
    m_hoverIndex = -1;
    m_scrollY = 0;
    OnSize();
}

} // namespace QNote
