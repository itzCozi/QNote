//==============================================================================
// QNote - A Lightweight Notepad Clone
// TabBar.cpp - Notepad++-style tab bar implementation
//==============================================================================

#include "TabBar.h"
#include <windowsx.h>
#include <CommCtrl.h>
#include <ShellScalingApi.h>
#include <algorithm>

#pragma comment(lib, "Shcore.lib")

namespace QNote {

bool TabBar::s_classRegistered = false;

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
TabBar::~TabBar() {
    Destroy();
}

//------------------------------------------------------------------------------
// Create the tab bar window
//------------------------------------------------------------------------------
bool TabBar::Create(HWND parent, HINSTANCE hInstance) {
    m_hwndParent = parent;
    m_hInstance = hInstance;

    // Initialize DPI scaling
    InitializeDPI();

    // Register the window class once
    if (!s_classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = TabBarProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;  // We paint everything ourselves
        wc.lpszClassName = TAB_CLASS;

        if (!RegisterClassExW(&wc)) {
            return false;
        }
        s_classRegistered = true;
    }

    RECT parentRc;
    GetClientRect(parent, &parentRc);

    m_hwnd = CreateWindowExW(
        0,
        TAB_CLASS,
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, parentRc.right, m_tabBarHeight,
        parent,
        nullptr,
        hInstance,
        this
    );

    if (!m_hwnd) return false;

    // Create the font
    m_font = CreateFontW(
        -Scale(BASE_FONT_SIZE), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );

    // Create tooltip control
    m_hwndTooltip = CreateWindowExW(
        WS_EX_TOPMOST,
        TOOLTIPS_CLASSW,
        nullptr,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        m_hwnd,
        nullptr,
        hInstance,
        nullptr
    );

    if (m_hwndTooltip) {
        // Associate tooltip with tab bar
        TOOLINFOW ti = {};
        ti.cbSize = sizeof(ti);
        ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
        ti.hwnd = m_hwnd;
        ti.uId = reinterpret_cast<UINT_PTR>(m_hwnd);
        ti.lpszText = LPSTR_TEXTCALLBACKW;
        SendMessageW(m_hwndTooltip, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&ti));
        // Disable automatic tooltip display - we control it with our own 3s timer
        SendMessageW(m_hwndTooltip, TTM_SETDELAYTIME, TTDT_INITIAL, 32767);
    }

    return true;
}

//------------------------------------------------------------------------------
// Destroy
//------------------------------------------------------------------------------
void TabBar::Destroy() noexcept {
    KillTimer(m_hwnd, TOOLTIP_TIMER_ID);
    if (m_hwndTooltip) {
        DestroyWindow(m_hwndTooltip);
        m_hwndTooltip = nullptr;
    }
    if (m_hwndRenameEdit) {
        DestroyWindow(m_hwndRenameEdit);
        m_hwndRenameEdit = nullptr;
    }
    if (m_font) {
        DeleteObject(m_font);
        m_font = nullptr;
    }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

//------------------------------------------------------------------------------
// Add a new tab, returns the tab id
//------------------------------------------------------------------------------
int TabBar::AddTab(const std::wstring& title, const std::wstring& filePath, bool pinned) {
    TabItem tab;
    tab.id = m_nextTabId++;
    tab.title = title;
    tab.filePath = filePath;
    tab.isPinned = pinned;
    tab.isModified = false;

    // Insert pinned tabs at the front, unpinned at the end
    if (pinned) {
        // Find the last pinned tab
        size_t insertPos = 0;
        for (size_t i = 0; i < m_tabs.size(); i++) {
            if (m_tabs[i].isPinned) insertPos = i + 1;
        }
        m_tabs.insert(m_tabs.begin() + insertPos, tab);
    } else {
        m_tabs.push_back(tab);
    }

    Redraw();
    return tab.id;
}

//------------------------------------------------------------------------------
// Remove a tab by id
//------------------------------------------------------------------------------
void TabBar::RemoveTab(int tabId) {
    int idx = FindTabIndex(tabId);
    if (idx < 0) return;

    bool wasActive = (tabId == m_activeTabId);
    m_tabs.erase(m_tabs.begin() + idx);

    if (wasActive && !m_tabs.empty()) {
        // Select the tab at the same index or the last one
        int newIdx = (std::min)(idx, static_cast<int>(m_tabs.size()) - 1);
        m_activeTabId = m_tabs[newIdx].id;
        Notify(TabNotification::TabSelected, m_activeTabId);
    } else if (m_tabs.empty()) {
        m_activeTabId = -1;
    }

    Redraw();
}

//------------------------------------------------------------------------------
// Set active tab
//------------------------------------------------------------------------------
void TabBar::SetActiveTab(int tabId) {
    if (m_activeTabId == tabId) return;
    if (FindTabIndex(tabId) < 0) return;

    m_activeTabId = tabId;
    Redraw();
}

//------------------------------------------------------------------------------
// Tab property setters
//------------------------------------------------------------------------------
void TabBar::SetTabTitle(int tabId, const std::wstring& title) {
    int idx = FindTabIndex(tabId);
    if (idx >= 0) {
        m_tabs[idx].title = title;
        Redraw();
    }
}

void TabBar::SetTabFilePath(int tabId, const std::wstring& filePath) {
    int idx = FindTabIndex(tabId);
    if (idx >= 0) {
        m_tabs[idx].filePath = filePath;
    }
}

void TabBar::SetTabModified(int tabId, bool modified) {
    int idx = FindTabIndex(tabId);
    if (idx >= 0 && m_tabs[idx].isModified != modified) {
        m_tabs[idx].isModified = modified;
        Redraw();
    }
}

void TabBar::SetTabPinned(int tabId, bool pinned) {
    int idx = FindTabIndex(tabId);
    if (idx < 0) return;

    if (m_tabs[idx].isPinned == pinned) return;
    m_tabs[idx].isPinned = pinned;

    // Move pinned tabs to the front, unpinned to after pinned section
    TabItem tab = m_tabs[idx];
    m_tabs.erase(m_tabs.begin() + idx);

    if (pinned) {
        size_t insertPos = 0;
        for (size_t i = 0; i < m_tabs.size(); i++) {
            if (m_tabs[i].isPinned) insertPos = i + 1;
        }
        m_tabs.insert(m_tabs.begin() + insertPos, tab);
    } else {
        // Insert after last pinned tab
        size_t insertPos = 0;
        for (size_t i = 0; i < m_tabs.size(); i++) {
            if (m_tabs[i].isPinned) insertPos = i + 1;
        }
        m_tabs.insert(m_tabs.begin() + insertPos, tab);
    }

    Redraw();
}

//------------------------------------------------------------------------------
// Query tab properties
//------------------------------------------------------------------------------
const TabItem* TabBar::GetTab(int tabId) const {
    int idx = FindTabIndex(tabId);
    if (idx >= 0) return &m_tabs[idx];
    return nullptr;
}

const TabItem* TabBar::GetActiveTab() const {
    return GetTab(m_activeTabId);
}

std::vector<int> TabBar::GetAllTabIds() const {
    std::vector<int> ids;
    ids.reserve(m_tabs.size());
    for (const auto& tab : m_tabs) {
        ids.push_back(tab.id);
    }
    return ids;
}

int TabBar::GetTabIdByIndex(int index) const {
    if (index >= 0 && index < static_cast<int>(m_tabs.size())) {
        return m_tabs[index].id;
    }
    return -1;
}

//------------------------------------------------------------------------------
// Resize
//------------------------------------------------------------------------------
void TabBar::Resize(int x, int y, int width) {
    if (m_hwnd) {
        MoveWindow(m_hwnd, x, y, width, m_tabBarHeight, TRUE);
    }
}

//------------------------------------------------------------------------------
// Window procedure (static)
//------------------------------------------------------------------------------
LRESULT CALLBACK TabBar::TabBarProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    TabBar* pThis = nullptr;

    if (msg == WM_NCCREATE) {
        auto* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<TabBar*>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<TabBar*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

//------------------------------------------------------------------------------
// Message handler
//------------------------------------------------------------------------------
LRESULT TabBar::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT:
            OnPaint();
            return 0;

        case WM_ERASEBKGND:
            return 1;  // We handle background in WM_PAINT

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            // Check new tab button first
            if (NewTabButtonHitTest(x, y)) {
                Notify(TabNotification::NewTabRequested, -1);
                return 0;
            }

            // Check for close button click
            for (int i = 0; i < static_cast<int>(m_tabs.size()); i++) {
                if (CloseButtonHitTest(m_tabs[i].id, x, y)) {
                    Notify(TabNotification::TabCloseRequested, m_tabs[i].id);
                    return 0;
                }
            }

            // Check for tab click
            int hitId = TabHitTest(x, y);
            if (hitId >= 0 && hitId != m_activeTabId) {
                m_activeTabId = hitId;
                Notify(TabNotification::TabSelected, hitId);
                Redraw();
            }
            return 0;
        }

        case WM_LBUTTONDBLCLK: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            int hitId = TabHitTest(x, y);
            if (hitId >= 0) {
                BeginRename(hitId);
            }
            return 0;
        }

        case WM_RBUTTONUP: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            int hitId = TabHitTest(x, y);
            if (hitId >= 0) {
                POINT pt = { x, y };
                ClientToScreen(m_hwnd, &pt);
                ShowContextMenu(hitId, pt.x, pt.y);
            }
            return 0;
        }

        case WM_MBUTTONDOWN: {
            // Middle-click to close tab
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            int hitId = TabHitTest(x, y);
            if (hitId >= 0) {
                Notify(TabNotification::TabCloseRequested, hitId);
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            if (!m_trackingMouse) {
                TRACKMOUSEEVENT tme = {};
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = m_hwnd;
                TrackMouseEvent(&tme);
                m_trackingMouse = true;
            }

            int oldHovered = m_hoveredTabId;
            bool oldCloseHovered = m_closeHovered;
            bool oldNewTabHovered = m_newTabHovered;

            m_hoveredTabId = TabHitTest(x, y);
            m_closeHovered = false;
            m_newTabHovered = NewTabButtonHitTest(x, y);

            if (m_hoveredTabId >= 0) {
                m_closeHovered = CloseButtonHitTest(m_hoveredTabId, x, y);
            }

            if (m_hoveredTabId != oldHovered || m_closeHovered != oldCloseHovered || 
                m_newTabHovered != oldNewTabHovered) {
                Redraw();
            }

            // Update tooltip if hovered tab changed
            if (m_hoveredTabId != m_tooltipTabId) {
                // Kill any existing timer and reset tooltip state
                KillTimer(m_hwnd, TOOLTIP_TIMER_ID);
                m_tooltipReady = false;
                SendMessageW(m_hwndTooltip, TTM_POP, 0, 0);

                m_tooltipTabId = m_hoveredTabId;

                // Start 3-second timer if hovering over a valid tab
                if (m_tooltipTabId >= 0) {
                    SetTimer(m_hwnd, TOOLTIP_TIMER_ID, TOOLTIP_DELAY_MS, nullptr);
                }
            }
            return 0;
        }

        case WM_MOUSELEAVE:
            KillTimer(m_hwnd, TOOLTIP_TIMER_ID);
            m_trackingMouse = false;
            m_hoveredTabId = -1;
            m_tooltipTabId = -1;
            m_tooltipReady = false;
            m_closeHovered = false;
            m_newTabHovered = false;
            SendMessageW(m_hwndTooltip, TTM_POP, 0, 0);
            Redraw();
            return 0;

        case WM_MOUSEWHEEL: {
            // Scroll tabs horizontally with mouse wheel
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            m_scrollOffset -= (delta > 0) ? Scale(30) : -Scale(30);
            if (m_scrollOffset < 0) m_scrollOffset = 0;
            Redraw();
            return 0;
        }

        case WM_TIMER:
            if (wParam == TOOLTIP_TIMER_ID) {
                KillTimer(m_hwnd, TOOLTIP_TIMER_ID);
                if (m_tooltipTabId >= 0) {
                    m_tooltipReady = true;
                    UpdateTooltip();
                }
                return 0;
            }
            break;

        case WM_NOTIFY: {
            auto* pnmh = reinterpret_cast<NMHDR*>(lParam);
            if (pnmh->hwndFrom == m_hwndTooltip && pnmh->code == TTN_GETDISPINFOW) {
                auto* pdi = reinterpret_cast<NMTTDISPINFOW*>(lParam);
                // Only show tooltip after 3-second hover delay
                if (m_tooltipReady && m_tooltipTabId >= 0) {
                    const TabItem* tab = GetTab(m_tooltipTabId);
                    if (tab) {
                        static std::wstring tooltipText;
                        if (!tab->filePath.empty()) {
                            tooltipText = tab->filePath;
                        } else {
                            tooltipText = tab->title + L" (Untitled)";
                        }
                        pdi->lpszText = const_cast<wchar_t*>(tooltipText.c_str());
                    }
                }
                return 0;
            }
            break;
        }

        case WM_COMMAND:
            if (HIWORD(wParam) == EN_KILLFOCUS && reinterpret_cast<HWND>(lParam) == m_hwndRenameEdit) {
                EndRename(true);
                return 0;
            }
            break;

        case WM_DPICHANGED_AFTERPARENT: {
            // Child windows receive this when parent DPI changes (Per-Monitor V2)
            // Re-query DPI from the parent window
            HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
            if (hUser32) {
                using GetDpiForWindowFunc = UINT(WINAPI*)(HWND);
                auto pGetDpiForWindow = reinterpret_cast<GetDpiForWindowFunc>(
                    GetProcAddress(hUser32, "GetDpiForWindow"));
                if (pGetDpiForWindow && m_hwndParent) {
                    UINT newDpi = pGetDpiForWindow(m_hwndParent);
                    UpdateDPI(newDpi);
                }
            }
            return 0;
        }
    }

    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

//------------------------------------------------------------------------------
// Paint
//------------------------------------------------------------------------------
void TabBar::OnPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);

    RECT rc;
    GetClientRect(m_hwnd, &rc);

    // Double buffer
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memDC, memBitmap));

    // Fill background
    HBRUSH bgBrush = CreateSolidBrush(CLR_BG);
    FillRect(memDC, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Draw bottom border line
    HPEN borderPen = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN oldPen = static_cast<HPEN>(SelectObject(memDC, borderPen));
    MoveToEx(memDC, 0, rc.bottom - 1, nullptr);
    LineTo(memDC, rc.right, rc.bottom - 1);
    SelectObject(memDC, oldPen);
    DeleteObject(borderPen);

    // Set font
    HFONT oldFont = nullptr;
    if (m_font) {
        oldFont = static_cast<HFONT>(SelectObject(memDC, m_font));
    }

    SetBkMode(memDC, TRANSPARENT);

    // Draw each tab
    for (int i = 0; i < static_cast<int>(m_tabs.size()); i++) {
        RECT tabRc = GetTabRect(i);

        // Skip tabs scrolled out of view
        if (tabRc.right <= 0 || tabRc.left >= rc.right) continue;

        bool isActive = (m_tabs[i].id == m_activeTabId);
        bool isHovered = (m_tabs[i].id == m_hoveredTabId);
        bool closeHovered = isHovered && m_closeHovered;

        DrawTab(memDC, tabRc, m_tabs[i], isActive, isHovered, closeHovered);
    }

    // Draw new tab button
    RECT newBtnRc = GetNewTabButtonRect();
    if (newBtnRc.left < rc.right) {
        DrawNewTabButton(memDC, newBtnRc, m_newTabHovered);
    }

    // Restore font
    if (oldFont) {
        SelectObject(memDC, oldFont);
    }

    // Blit to screen
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);

    EndPaint(m_hwnd, &ps);
}

//------------------------------------------------------------------------------
// Draw a single tab
//------------------------------------------------------------------------------
void TabBar::DrawTab(HDC hdc, const RECT& rc, const TabItem& tab, bool isActive, 
                     bool isHovered, bool isCloseHovered) {
    // Tab background
    COLORREF bgColor = isActive ? CLR_TAB_ACTIVE : (isHovered ? CLR_TAB_HOVER : CLR_TAB_INACTIVE);
    HBRUSH brush = CreateSolidBrush(bgColor);
    RECT tabRect = rc;
    tabRect.bottom -= 1;
    FillRect(hdc, &tabRect, brush);
    DeleteObject(brush);

    // Active tab indicator line at the top
    if (isActive) {
        HBRUSH accentBrush = CreateSolidBrush(CLR_ACTIVE_LINE);
        RECT lineRc = { rc.left, rc.top, rc.right, rc.top + 2 };
        FillRect(hdc, &lineRc, accentBrush);
        DeleteObject(accentBrush);
    }

    // Text area
    RECT textRc = tabRect;
    textRc.left += m_tabPadding;
    textRc.right -= (m_closeBtnSize + m_closeBtnMargin * 2);

    // Draw modified indicator as a perfectly centered filled circle
    if (tab.isModified) {
        int dotRadius = Scale(3);
        int dotCenterY = (textRc.top + textRc.bottom) / 2;
        int dotCenterX = textRc.left + dotRadius;
        HBRUSH dotBrush = CreateSolidBrush(CLR_MODIFIED);
        HPEN dotPen = CreatePen(PS_SOLID, 1, CLR_MODIFIED);
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, dotBrush));
        HPEN oldPen2 = static_cast<HPEN>(SelectObject(hdc, dotPen));
        Ellipse(hdc, dotCenterX - dotRadius, dotCenterY - dotRadius,
                     dotCenterX + dotRadius, dotCenterY + dotRadius);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen2);
        DeleteObject(dotBrush);
        DeleteObject(dotPen);
        textRc.left += dotRadius * 2 + Scale(5);  // Space after dot
    }

    // Build display text
    std::wstring displayText;
    if (tab.isPinned) {
        displayText = L"\U0001F4CC ";  // Pushpin emoji
    }
    displayText += tab.title;

    // Text color
    COLORREF textColor = isActive ? CLR_TEXT_ACTIVE : CLR_TEXT_INACTIVE;
    if (tab.isModified) {
        textColor = CLR_MODIFIED;
    }
    SetTextColor(hdc, textColor);

    // Draw text
    DrawTextW(hdc, displayText.c_str(), static_cast<int>(displayText.length()), 
              &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    // Draw close button (X) - only show on active or hovered tabs
    if (isActive || isHovered) {
        RECT closeRc = GetCloseButtonRect(tabRect);

        if (isCloseHovered) {
            // Red background on hover
            HBRUSH closeBg = CreateSolidBrush(CLR_CLOSE_HOVER);
            FillRect(hdc, &closeRc, closeBg);
            DeleteObject(closeBg);
            SetTextColor(hdc, RGB(255, 255, 255));
        } else {
            SetTextColor(hdc, CLR_CLOSE_X);
        }

        // Draw X - use a dedicated font sized to fit the button
        HFONT closeFont = CreateFontW(
            -Scale(BASE_FONT_SIZE - 1), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            L"Segoe UI"
        );
        HFONT prevFont = static_cast<HFONT>(SelectObject(hdc, closeFont));

        // Nudge rect up by 1px to visually center the multiplication sign glyph
        RECT xRc = closeRc;
        xRc.top -= 1;
        xRc.bottom -= 1;
        DrawTextW(hdc, L"\u00D7", 1, &xRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdc, prevFont);
        DeleteObject(closeFont);
    }

    // Right border between tabs
    HPEN sepPen = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, sepPen));
    MoveToEx(hdc, rc.right - 1, rc.top, nullptr);
    LineTo(hdc, rc.right - 1, rc.bottom);
    SelectObject(hdc, oldPen);
    DeleteObject(sepPen);
}

//------------------------------------------------------------------------------
// Draw new tab (+) button
//------------------------------------------------------------------------------
void TabBar::DrawNewTabButton(HDC hdc, const RECT& rc, bool isHovered) {
    if (isHovered) {
        HBRUSH brush = CreateSolidBrush(CLR_NEWTAB_HOVER);
        FillRect(hdc, &rc, brush);
        DeleteObject(brush);
    }

    SetTextColor(hdc, CLR_TEXT_INACTIVE);
    DrawTextW(hdc, L"+", 1, const_cast<RECT*>(&rc), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

//------------------------------------------------------------------------------
// Hit testing - returns tab id at point, or -1
//------------------------------------------------------------------------------
int TabBar::TabHitTest(int x, int y) const {
    for (int i = 0; i < static_cast<int>(m_tabs.size()); i++) {
        RECT rc = GetTabRect(i);
        POINT pt = { x, y };
        if (PtInRect(&rc, pt)) {
            return m_tabs[i].id;
        }
    }
    return -1;
}

//------------------------------------------------------------------------------
// Close button hit test
//------------------------------------------------------------------------------
bool TabBar::CloseButtonHitTest(int tabId, int x, int y) const {
    int idx = FindTabIndex(tabId);
    if (idx < 0) return false;

    RECT tabRc = GetTabRect(idx);
    tabRc.top += 2;
    tabRc.bottom -= 1;
    RECT closeRc = GetCloseButtonRect(tabRc);
    POINT pt = { x, y };
    return PtInRect(&closeRc, pt) != FALSE;
}

//------------------------------------------------------------------------------
// New tab button hit test
//------------------------------------------------------------------------------
bool TabBar::NewTabButtonHitTest(int x, int y) const {
    RECT rc = GetNewTabButtonRect();
    POINT pt = { x, y };
    return PtInRect(&rc, pt) != FALSE;
}

//------------------------------------------------------------------------------
// Get the rectangle for a tab by index
//------------------------------------------------------------------------------
RECT TabBar::GetTabRect(int index) const {
    if (!m_hwnd) return { 0, 0, 0, 0 };

    RECT clientRc;
    GetClientRect(m_hwnd, &clientRc);

    // Calculate tab width based on available space
    int totalTabs = static_cast<int>(m_tabs.size());
    int availWidth = clientRc.right - m_newTabBtnWidth;

    int tabWidth = (totalTabs > 0) ? availWidth / totalTabs : m_tabMaxWidth;
    tabWidth = (std::max)(m_tabMinWidth, (std::min)(tabWidth, m_tabMaxWidth));

    int left = index * tabWidth - m_scrollOffset;

    RECT rc;
    rc.left = left;
    rc.top = 0;
    rc.right = left + tabWidth;
    rc.bottom = clientRc.bottom;

    return rc;
}

//------------------------------------------------------------------------------
// Get close button rect within a tab rect
//------------------------------------------------------------------------------
RECT TabBar::GetCloseButtonRect(const RECT& tabRect) const {
    RECT rc;
    rc.right = tabRect.right - m_closeBtnMargin;
    rc.left = rc.right - m_closeBtnSize;
    rc.top = tabRect.top + (tabRect.bottom - tabRect.top - m_closeBtnSize) / 2;
    rc.bottom = rc.top + m_closeBtnSize;
    return rc;
}

//------------------------------------------------------------------------------
// Get new tab (+) button rect
//------------------------------------------------------------------------------
RECT TabBar::GetNewTabButtonRect() const {
    if (!m_hwnd) return { 0, 0, 0, 0 };

    RECT clientRc;
    GetClientRect(m_hwnd, &clientRc);

    int totalTabs = static_cast<int>(m_tabs.size());
    int availWidth = clientRc.right - m_newTabBtnWidth;
    int tabWidth = (totalTabs > 0) ? availWidth / totalTabs : m_tabMaxWidth;
    tabWidth = (std::max)(m_tabMinWidth, (std::min)(tabWidth, m_tabMaxWidth));

    int left = totalTabs * tabWidth - m_scrollOffset;

    RECT rc;
    rc.left = left;
    rc.top = 0;
    rc.right = left + m_newTabBtnWidth;
    rc.bottom = clientRc.bottom;
    return rc;
}

//------------------------------------------------------------------------------
// Context menu
//------------------------------------------------------------------------------
void TabBar::ShowContextMenu(int tabId, int x, int y) {
    m_contextMenuTabId = tabId;
    int idx = FindTabIndex(tabId);
    if (idx < 0) return;

    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    const TabItem& tab = m_tabs[idx];

    AppendMenuW(hMenu, MF_STRING, 1, tab.isPinned ? L"Unpin Tab" : L"Pin Tab");
    AppendMenuW(hMenu, MF_STRING, 2, L"Rename Tab...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 3, L"Close Tab");
    AppendMenuW(hMenu, MF_STRING, 4, L"Close Other Tabs");
    AppendMenuW(hMenu, MF_STRING, 5, L"Close Tabs to the Right");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 6, L"Close All Tabs");

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, x, y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    switch (cmd) {
        case 1: // Pin/Unpin
            Notify(TabNotification::TabPinToggled, tabId);
            break;
        case 2: // Rename
            BeginRename(tabId);
            break;
        case 3: // Close
            Notify(TabNotification::TabCloseRequested, tabId);
            break;
        case 4: // Close Others
            Notify(TabNotification::CloseOthers, tabId);
            break;
        case 5: // Close to Right
            Notify(TabNotification::CloseToRight, tabId);
            break;
        case 6: // Close All
            Notify(TabNotification::CloseAll, tabId);
            break;
    }
}

//------------------------------------------------------------------------------
// Edit subclass procedure for rename (static)
//------------------------------------------------------------------------------
static LRESULT CALLBACK RenameEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, 
                                                LPARAM lParam, UINT_PTR subclassId,
                                                DWORD_PTR refData) {
    TabBar* self = reinterpret_cast<TabBar*>(refData);
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            self->EndRename(true);
            return 0;
        } else if (wParam == VK_ESCAPE) {
            self->EndRename(false);
            return 0;
        }
    } else if (msg == WM_KILLFOCUS) {
        self->EndRename(true);
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

//------------------------------------------------------------------------------
// Begin inline rename
//------------------------------------------------------------------------------
void TabBar::BeginRename(int tabId) {
    int idx = FindTabIndex(tabId);
    if (idx < 0) return;

    m_renamingTabId = tabId;

    RECT tabRc = GetTabRect(idx);

    // Create an edit control over the tab
    if (m_hwndRenameEdit) {
        DestroyWindow(m_hwndRenameEdit);
    }

    m_hwndRenameEdit = CreateWindowExW(
        0, L"EDIT", m_tabs[idx].title.c_str(),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        tabRc.left + Scale(4), tabRc.top + Scale(4),
        tabRc.right - tabRc.left - m_closeBtnSize - m_closeBtnMargin * 2 - Scale(8),
        tabRc.bottom - tabRc.top - Scale(8),
        m_hwnd, nullptr, m_hInstance, nullptr
    );

    if (m_hwndRenameEdit) {
        if (m_font) {
            SendMessageW(m_hwndRenameEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
        }
        SendMessageW(m_hwndRenameEdit, EM_SETSEL, 0, -1);  // Select all
        ::SetFocus(m_hwndRenameEdit);

        // Subclass the edit control to catch Enter/Escape
        SetWindowSubclass(m_hwndRenameEdit, RenameEditSubclassProc, 0, 
                          reinterpret_cast<DWORD_PTR>(this));
    }
}

//------------------------------------------------------------------------------
// End inline rename
//------------------------------------------------------------------------------
void TabBar::EndRename(bool accept) {
    if (!m_hwndRenameEdit || m_renamingTabId < 0) return;

    if (accept) {
        wchar_t buf[256] = {};
        GetWindowTextW(m_hwndRenameEdit, buf, 256);
        std::wstring newTitle = buf;

        if (!newTitle.empty()) {
            int idx = FindTabIndex(m_renamingTabId);
            if (idx >= 0) {
                m_tabs[idx].title = newTitle;
                Notify(TabNotification::TabRenamed, m_renamingTabId);
            }
        }
    }

    DestroyWindow(m_hwndRenameEdit);
    m_hwndRenameEdit = nullptr;
    m_renamingTabId = -1;
    Redraw();
}

//------------------------------------------------------------------------------
// Notify callback
//------------------------------------------------------------------------------
void TabBar::Notify(TabNotification notification, int tabId) {
    if (m_callback) {
        m_callback(notification, tabId);
    }
}

//------------------------------------------------------------------------------
// Find tab index by id
//------------------------------------------------------------------------------
int TabBar::FindTabIndex(int tabId) const {
    for (int i = 0; i < static_cast<int>(m_tabs.size()); i++) {
        if (m_tabs[i].id == tabId) return i;
    }
    return -1;
}

//------------------------------------------------------------------------------
// Invalidate for repaint
//------------------------------------------------------------------------------
void TabBar::Redraw() noexcept {
    if (m_hwnd) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

//------------------------------------------------------------------------------
// Initialize DPI scaling
//------------------------------------------------------------------------------
void TabBar::InitializeDPI() {
    // Try to get per-monitor DPI (Windows 8.1+)
    HMODULE hShcore = GetModuleHandleW(L"Shcore.dll");
    if (hShcore) {
        using GetDpiForMonitorFunc = HRESULT(WINAPI*)(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*);
        auto pGetDpiForMonitor = reinterpret_cast<GetDpiForMonitorFunc>(
            GetProcAddress(hShcore, "GetDpiForMonitor"));
        if (pGetDpiForMonitor && m_hwndParent) {
            HMONITOR hMon = MonitorFromWindow(m_hwndParent, MONITOR_DEFAULTTONEAREST);
            UINT dpiX = 96, dpiY = 96;
            if (SUCCEEDED(pGetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
                m_dpi = static_cast<int>(dpiX);
            }
        }
    }
    
    // Fallback: get system DPI from DC
    if (m_dpi == 96) {
        HDC hdc = GetDC(nullptr);
        if (hdc) {
            m_dpi = GetDeviceCaps(hdc, LOGPIXELSX);
            ReleaseDC(nullptr, hdc);
        }
    }

    // Scale all layout values
    m_tabBarHeight = Scale(BASE_TAB_BAR_HEIGHT);
    m_tabMinWidth = Scale(BASE_TAB_MIN_WIDTH);
    m_tabMaxWidth = Scale(BASE_TAB_MAX_WIDTH);
    m_tabPadding = Scale(BASE_TAB_PADDING);
    m_closeBtnSize = Scale(BASE_CLOSE_BTN_SIZE);
    m_closeBtnMargin = Scale(BASE_CLOSE_BTN_MARGIN);
    m_newTabBtnWidth = Scale(BASE_NEW_TAB_BTN_WIDTH);
}

//------------------------------------------------------------------------------
// Update DPI scaling (called when monitor DPI changes)
//------------------------------------------------------------------------------
void TabBar::UpdateDPI(UINT newDpi) {
    if (static_cast<int>(newDpi) == m_dpi) return;

    m_dpi = static_cast<int>(newDpi);

    // Rescale all layout values
    m_tabBarHeight = Scale(BASE_TAB_BAR_HEIGHT);
    m_tabMinWidth = Scale(BASE_TAB_MIN_WIDTH);
    m_tabMaxWidth = Scale(BASE_TAB_MAX_WIDTH);
    m_tabPadding = Scale(BASE_TAB_PADDING);
    m_closeBtnSize = Scale(BASE_CLOSE_BTN_SIZE);
    m_closeBtnMargin = Scale(BASE_CLOSE_BTN_MARGIN);
    m_newTabBtnWidth = Scale(BASE_NEW_TAB_BTN_WIDTH);

    // Recreate the font at the new DPI
    if (m_font) {
        DeleteObject(m_font);
    }
    m_font = CreateFontW(
        -Scale(BASE_FONT_SIZE), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );

    // Resize the tab bar window to the new height
    if (m_hwnd) {
        RECT rc;
        GetWindowRect(m_hwnd, &rc);
        MapWindowPoints(HWND_DESKTOP, GetParent(m_hwnd), reinterpret_cast<LPPOINT>(&rc), 2);
        MoveWindow(m_hwnd, rc.left, rc.top, rc.right - rc.left, m_tabBarHeight, TRUE);
        Redraw();
    }
}

//------------------------------------------------------------------------------
// Update tooltip text
//------------------------------------------------------------------------------
void TabBar::UpdateTooltip() {
    if (!m_hwndTooltip) return;

    // Force tooltip to update by triggering a pop and re-activate
    SendMessageW(m_hwndTooltip, TTM_POP, 0, 0);
    if (m_tooltipTabId >= 0) {
        SendMessageW(m_hwndTooltip, TTM_POPUP, 0, 0);
    }
}

} // namespace QNote
