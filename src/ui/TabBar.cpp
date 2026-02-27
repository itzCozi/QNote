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
    if (m_hwndDragPreview) {
        DestroyWindow(m_hwndDragPreview);
        m_hwndDragPreview = nullptr;
    }
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
    if (m_closeFont) {
        DeleteObject(m_closeFont);
        m_closeFont = nullptr;
    }
    if (m_arrowFont) {
        DeleteObject(m_arrowFont);
        m_arrowFont = nullptr;
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

    EnsureTabVisible(tab.id);
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

    ClampScrollOffset();
    Redraw();
}

//------------------------------------------------------------------------------
// Set active tab
//------------------------------------------------------------------------------
void TabBar::SetActiveTab(int tabId) {
    if (m_activeTabId == tabId) return;
    if (FindTabIndex(tabId) < 0) return;

    m_activeTabId = tabId;
    EnsureTabVisible(tabId);
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
        m_cachedTabWidth = -1;
        m_cachedPinnedTabWidth = -1;
        MoveWindow(m_hwnd, x, y, width, m_tabBarHeight, TRUE);
        ClampScrollOffset();
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

            // Check scroll arrow buttons
            {
                RECT clientRc;
                GetClientRect(m_hwnd, &clientRc);
                int scrollStep = CalcTabWidth();  // scroll by one normal tab width
                int totalContentWidth = CalcTotalTabsWidth() + m_newTabBtnWidth;
                POINT pt = { x, y };

                if (m_scrollOffset > 0) {
                    RECT leftRc = { 0, 0, m_scrollArrowWidth, clientRc.bottom - 1 };
                    if (PtInRect(&leftRc, pt)) {
                        m_scrollOffset -= scrollStep;
                        ClampScrollOffset();
                        Redraw();
                        return 0;
                    }
                }
                if (totalContentWidth - m_scrollOffset > clientRc.right) {
                    RECT rightRc = { clientRc.right - m_scrollArrowWidth, 0, clientRc.right, clientRc.bottom - 1 };
                    if (PtInRect(&rightRc, pt)) {
                        m_scrollOffset += scrollStep;
                        ClampScrollOffset();
                        Redraw();
                        return 0;
                    }
                }
            }

            // Check new tab button first
            if (NewTabButtonHitTest(x, y)) {
                Notify(TabNotification::NewTabRequested, -1);
                return 0;
            }

            // Check for close button click (not on pinned tabs)
            for (int i = 0; i < static_cast<int>(m_tabs.size()); i++) {
                if (!m_tabs[i].isPinned && CloseButtonHitTest(m_tabs[i].id, x, y)) {
                    Notify(TabNotification::TabCloseRequested, m_tabs[i].id);
                    return 0;
                }
            }

            // Check for tab click
            int hitId = TabHitTest(x, y);
            if (hitId >= 0) {
                if (hitId != m_activeTabId) {
                    m_activeTabId = hitId;
                    Notify(TabNotification::TabSelected, hitId);
                    Redraw();
                }
                // Start potential drag
                m_dragTabId = hitId;
                m_dragStartX = x;
                m_dragStartY = y;
                m_dragInitiated = false;
                m_dragging = true;
                SetCapture(m_hwnd);
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

        case WM_LBUTTONUP: {
            if (m_dragging) {
                bool wasDragInitiated = m_dragInitiated;
                int draggedId = m_dragTabId;
                m_dragging = false;
                m_dragTabId = -1;
                m_dragInitiated = false;
                ReleaseCapture();
                SetCursor(LoadCursorW(nullptr, IDC_ARROW));
                HideDragPreview();
                if (wasDragInitiated) {
                    // Check if cursor is outside the tab bar -> detach to new window
                    POINT pt;
                    GetCursorPos(&pt);
                    RECT tabBarRect;
                    GetWindowRect(m_hwnd, &tabBarRect);
                    // Allow some tolerance (expand rect by a few pixels)
                    InflateRect(&tabBarRect, 0, 10);
                    if (!PtInRect(&tabBarRect, pt) && m_tabs.size() > 1) {
                        m_lastDetachPos = pt;
                        Notify(TabNotification::TabDetached, draggedId);
                    } else {
                        Notify(TabNotification::TabReordered, m_activeTabId);
                    }
                }
            }
            return 0;
        }

        case WM_CAPTURECHANGED: {
            if (m_dragging) {
                m_dragging = false;
                m_dragTabId = -1;
                m_dragInitiated = false;
                HideDragPreview();
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

            // Handle drag reorder / tear-off
            if (m_dragging && m_dragTabId >= 0) {
                int dx = x - m_dragStartX;
                int dy = y - m_dragStartY;
                if (!m_dragInitiated && (abs(dx) > DRAG_THRESHOLD || abs(dy) > DRAG_THRESHOLD)) {
                    m_dragInitiated = true;
                }
                if (m_dragInitiated) {
                    // Check if cursor is inside or outside the tab bar
                    POINT screenPt;
                    GetCursorPos(&screenPt);
                    RECT tabBarRect;
                    GetWindowRect(m_hwnd, &tabBarRect);
                    InflateRect(&tabBarRect, 0, Scale(5));

                    if (PtInRect(&tabBarRect, screenPt)) {
                        // Inside tab bar - reorder mode
                        HideDragPreview();
                        SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));

                        int targetId = TabHitTest(x, y);
                        if (targetId >= 0 && targetId != m_dragTabId) {
                            int srcIdx = FindTabIndex(m_dragTabId);
                            int dstIdx = FindTabIndex(targetId);
                            if (srcIdx >= 0 && dstIdx >= 0) {
                                TabItem temp = m_tabs[srcIdx];
                                m_tabs.erase(m_tabs.begin() + srcIdx);
                                m_tabs.insert(m_tabs.begin() + dstIdx, temp);
                                Redraw();
                            }
                        }
                    } else if (m_tabs.size() > 1) {
                        // Outside tab bar - show tear-off preview
                        SetCursor(LoadCursorW(nullptr, IDC_HAND));
                        ShowDragPreview(m_dragTabId, screenPt.x, screenPt.y);
                    }
                    return 0;
                }
            }

            int oldHovered = m_hoveredTabId;
            bool oldCloseHovered = m_closeHovered;
            bool oldNewTabHovered = m_newTabHovered;
            bool oldLeftArrowHovered = m_leftArrowHovered;
            bool oldRightArrowHovered = m_rightArrowHovered;

            m_hoveredTabId = TabHitTest(x, y);
            m_closeHovered = false;
            m_newTabHovered = NewTabButtonHitTest(x, y);
            m_leftArrowHovered = false;
            m_rightArrowHovered = false;

            if (m_hoveredTabId >= 0) {
                // Don't show close hover on pinned tabs
                int hovIdx = FindTabIndex(m_hoveredTabId);
                if (hovIdx >= 0 && !m_tabs[hovIdx].isPinned) {
                    m_closeHovered = CloseButtonHitTest(m_hoveredTabId, x, y);
                }
            }

            // Track scroll arrow hover
            {
                RECT clientRc;
                GetClientRect(m_hwnd, &clientRc);
                int totalContentWidth = CalcTotalTabsWidth() + m_newTabBtnWidth;
                POINT pt = { x, y };

                if (m_scrollOffset > 0) {
                    RECT leftRc = { 0, 0, m_scrollArrowWidth, clientRc.bottom - 1 };
                    m_leftArrowHovered = PtInRect(&leftRc, pt) != FALSE;
                }
                if (totalContentWidth - m_scrollOffset > clientRc.right) {
                    RECT rightRc = { clientRc.right - m_scrollArrowWidth, 0, clientRc.right, clientRc.bottom - 1 };
                    m_rightArrowHovered = PtInRect(&rightRc, pt) != FALSE;
                }
            }

            if (m_hoveredTabId != oldHovered || m_closeHovered != oldCloseHovered || 
                m_newTabHovered != oldNewTabHovered ||
                m_leftArrowHovered != oldLeftArrowHovered ||
                m_rightArrowHovered != oldRightArrowHovered) {
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
            m_leftArrowHovered = false;
            m_rightArrowHovered = false;
            SendMessageW(m_hwndTooltip, TTM_POP, 0, 0);
            Redraw();
            return 0;

        case WM_MOUSEWHEEL: {
            // Scroll tabs horizontally with mouse wheel
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            m_scrollOffset -= (delta > 0) ? Scale(30) : -Scale(30);
            ClampScrollOffset();
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

    // Skip painting if client area is empty (e.g. window minimized)
    if (rc.right <= 0 || rc.bottom <= 0) {
        EndPaint(m_hwnd, &ps);
        return;
    }

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

    // Draw scroll arrows if tabs overflow
    {
        int totalContentWidth = CalcTotalTabsWidth() + m_newTabBtnWidth;

        if (m_scrollOffset > 0) {
            RECT arrowRc = { 0, 0, m_scrollArrowWidth, rc.bottom - 1 };
            DrawScrollArrow(memDC, arrowRc, true, m_leftArrowHovered);
        }
        if (totalContentWidth - m_scrollOffset > rc.right) {
            RECT arrowRc = { rc.right - m_scrollArrowWidth, 0, rc.right, rc.bottom - 1 };
            DrawScrollArrow(memDC, arrowRc, false, m_rightArrowHovered);
        }
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

    // Text area
    RECT textRc = tabRect;
    textRc.left += m_tabPadding;
    if (!tab.isPinned) {
        textRc.right -= (m_closeBtnSize + m_closeBtnMargin * 2);
    } else {
        textRc.right -= m_tabPadding;
    }

    // Draw modified indicator as a perfectly centered filled circle
    if (tab.isModified) {
        int dotRadius = Scale(3);
        int dotCenterY = (textRc.top + textRc.bottom) / 2 - 1;
        int dotCenterX = textRc.left + dotRadius;
        HBRUSH dotBrush = CreateSolidBrush(CLR_MODIFIED);
        HPEN dotPen = CreatePen(PS_SOLID, 1, CLR_MODIFIED);
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, dotBrush));
        HPEN oldPen2 = static_cast<HPEN>(SelectObject(hdc, dotPen));
        Ellipse(hdc, dotCenterX - dotRadius, dotCenterY - dotRadius,
                     dotCenterX + dotRadius + 5, dotCenterY + dotRadius + 5);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen2);
        DeleteObject(dotBrush);
        DeleteObject(dotPen);
        textRc.left += dotRadius * 2 + Scale(12);  // Space after dot
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

    // Draw close button (X) - only show on active or hovered tabs, not on pinned tabs
    if ((isActive || isHovered) && !tab.isPinned) {
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

        // Draw X - use a cached font sized to fit the button
        if (!m_closeFont) {
            m_closeFont = CreateFontW(
                -Scale(BASE_FONT_SIZE - 1), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                L"Segoe UI"
            );
        }
        HFONT prevFont = static_cast<HFONT>(SelectObject(hdc, m_closeFont));

        // Nudge rect up by 1px to visually center the multiplication sign glyph
        RECT xRc = closeRc;
        xRc.top -= 1;
        xRc.bottom -= 1;
        DrawTextW(hdc, L"\u00D7", 1, &xRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdc, prevFont);
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
    RECT btnRc = rc;
    btnRc.bottom -= 1;  // Leave room for bottom border line
    HBRUSH brush = CreateSolidBrush(isHovered ? CLR_NEWTAB_HOVER : CLR_BG);
    FillRect(hdc, &btnRc, brush);
    DeleteObject(brush);

    // Draw right border if not at the window edge
    RECT clientRc;
    GetClientRect(m_hwnd, &clientRc);
    if (rc.right < clientRc.right) {
        HPEN borderPen = CreatePen(PS_SOLID, 1, CLR_BORDER);
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
        MoveToEx(hdc, rc.right - 1, rc.top, nullptr);
        LineTo(hdc, rc.right - 1, btnRc.bottom);
        SelectObject(hdc, oldPen);
        DeleteObject(borderPen);
    }

    SetTextColor(hdc, CLR_TEXT_INACTIVE);
    RECT textRc = btnRc;
    textRc.top -= 2;
    textRc.bottom -= 2;
    DrawTextW(hdc, L"+", 1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
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

    int left = GetTabLeftOffset(index) - m_scrollOffset;
    int tabWidth = GetTabWidthByIndex(index);

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

    int left = CalcTotalTabsWidth() - m_scrollOffset;

    RECT rc;
    rc.left = left;
    rc.top = 0;
    rc.right = left + m_newTabBtnWidth;
    rc.bottom = clientRc.bottom;
    return rc;
}

//------------------------------------------------------------------------------
// Calculate consistent tab width
//------------------------------------------------------------------------------
int TabBar::CalcTabWidth() const {
    if (m_cachedTabWidth >= 0) return m_cachedTabWidth;
    RecalcCachedWidths();
    return m_cachedTabWidth;
}

//------------------------------------------------------------------------------
// Calculate pinned tab width (smaller max)
//------------------------------------------------------------------------------
int TabBar::CalcPinnedTabWidth() const {
    if (m_cachedPinnedTabWidth >= 0) return m_cachedPinnedTabWidth;
    RecalcCachedWidths();
    return m_cachedPinnedTabWidth;
}

//------------------------------------------------------------------------------
// Recompute cached tab widths
//------------------------------------------------------------------------------
void TabBar::RecalcCachedWidths() const {
    if (!m_hwnd || m_tabs.empty()) {
        m_cachedTabWidth = m_tabMaxWidth;
        m_cachedPinnedTabWidth = m_pinnedTabMaxWidth;
        return;
    }
    RECT clientRc;
    GetClientRect(m_hwnd, &clientRc);
    int totalTabs = static_cast<int>(m_tabs.size());
    int pinnedCount = 0;
    for (const auto& t : m_tabs) { if (t.isPinned) ++pinnedCount; }

    // Pinned tab width
    {
        int availWidth = clientRc.right - m_newTabBtnWidth;
        int tabWidth = availWidth / totalTabs;
        m_cachedPinnedTabWidth = (std::max)(m_tabMinWidth, (std::min)(tabWidth, m_pinnedTabMaxWidth));
    }

    // Normal tab width
    {
        int normalCount = totalTabs - pinnedCount;
        int availWidth = clientRc.right - m_newTabBtnWidth - pinnedCount * m_cachedPinnedTabWidth;
        if (normalCount <= 0) {
            m_cachedTabWidth = m_tabMaxWidth;
        } else {
            int tabWidth = availWidth / normalCount;
            m_cachedTabWidth = (std::max)(m_tabMinWidth, (std::min)(tabWidth, m_tabMaxWidth));
        }
    }
}

//------------------------------------------------------------------------------
// Get the width of a specific tab by index
//------------------------------------------------------------------------------
int TabBar::GetTabWidthByIndex(int index) const {
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) return CalcTabWidth();
    return m_tabs[index].isPinned ? CalcPinnedTabWidth() : CalcTabWidth();
}

//------------------------------------------------------------------------------
// Get the left offset of a tab by index (sum of all previous tab widths)
//------------------------------------------------------------------------------
int TabBar::GetTabLeftOffset(int index) const {
    int left = 0;
    for (int i = 0; i < index && i < static_cast<int>(m_tabs.size()); ++i) {
        left += GetTabWidthByIndex(i);
    }
    return left;
}

//------------------------------------------------------------------------------
// Calculate total width of all tabs
//------------------------------------------------------------------------------
int TabBar::CalcTotalTabsWidth() const {
    int total = 0;
    for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i) {
        total += GetTabWidthByIndex(i);
    }
    return total;
}

//------------------------------------------------------------------------------
// Clamp scroll offset to valid range
//------------------------------------------------------------------------------
void TabBar::ClampScrollOffset() {
    if (!m_hwnd) return;
    RECT clientRc;
    GetClientRect(m_hwnd, &clientRc);
    int totalContentWidth = CalcTotalTabsWidth() + m_newTabBtnWidth;
    int maxScroll = totalContentWidth - clientRc.right;
    if (maxScroll < 0) maxScroll = 0;
    m_scrollOffset = (std::max)(0, (std::min)(m_scrollOffset, maxScroll));
}

//------------------------------------------------------------------------------
// Ensure a tab is scrolled into view
//------------------------------------------------------------------------------
void TabBar::EnsureTabVisible(int tabId) {
    int idx = FindTabIndex(tabId);
    if (idx < 0 || !m_hwnd) return;
    RECT clientRc;
    GetClientRect(m_hwnd, &clientRc);
    int tabLeft = GetTabLeftOffset(idx);
    int tabRight = tabLeft + GetTabWidthByIndex(idx);
    int visibleRight = clientRc.right;
    if (tabLeft < m_scrollOffset) {
        m_scrollOffset = tabLeft;
    } else if (tabRight > m_scrollOffset + visibleRight) {
        m_scrollOffset = tabRight - visibleRight;
    }
    ClampScrollOffset();
}

//------------------------------------------------------------------------------
// Draw a scroll arrow overlay
//------------------------------------------------------------------------------
void TabBar::DrawScrollArrow(HDC hdc, const RECT& rc, bool isLeft, bool isHovered) {
    HBRUSH brush = CreateSolidBrush(CLR_TAB_HOVER);
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);

    // Draw border on the inner edge
    HPEN pen = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    if (isLeft) {
        MoveToEx(hdc, rc.right - 1, rc.top, nullptr);
        LineTo(hdc, rc.right - 1, rc.bottom);
    } else {
        MoveToEx(hdc, rc.left, rc.top, nullptr);
        LineTo(hdc, rc.left, rc.bottom);
    }
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    // Draw chevron character with a cached larger font
    if (!m_arrowFont) {
        m_arrowFont = CreateFontW(
            -Scale(20), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            L"Segoe UI"
        );
    }
    HFONT prevFont = static_cast<HFONT>(SelectObject(hdc, m_arrowFont));
    SetTextColor(hdc, CLR_TEXT_INACTIVE);

    // Measure the glyph and center it manually
    const wchar_t* glyph = isLeft ? L"\u2039" : L"\u203A";
    SIZE sz = {};
    GetTextExtentPoint32W(hdc, glyph, 1, &sz);
    int cx = rc.left + (rc.right - rc.left - sz.cx) / 2;
    int cy = rc.top + (rc.bottom - rc.top - sz.cy) / 2 - 2;
    ExtTextOutW(hdc, cx, cy, ETO_CLIPPED, &rc, glyph, 1, nullptr);

    SelectObject(hdc, prevFont);
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

    AppendMenuW(hMenu, MF_STRING, 7, L"&New Tab");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 1, tab.isPinned ? L"Unp&in Tab" : L"P&in Tab");
    AppendMenuW(hMenu, MF_STRING, 2, L"Rena&me Tab...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 3, L"&Close Tab");
    AppendMenuW(hMenu, MF_STRING, 4, L"Close &Other Tabs");
    AppendMenuW(hMenu, MF_STRING, 5, L"Close Tabs to the &Right");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 6, L"Close &All Tabs");

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
        case 7: // New Tab
            Notify(TabNotification::NewTabRequested, tabId);
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
    } else if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, RenameEditSubclassProc, subclassId);
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

        // Trim leading and trailing whitespace
        size_t start = newTitle.find_first_not_of(L" \t\r\n");
        size_t end = newTitle.find_last_not_of(L" \t\r\n");
        if (start != std::wstring::npos) {
            newTitle = newTitle.substr(start, end - start + 1);
        } else {
            newTitle.clear();
        }

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
        // Invalidate cached tab widths so they are recomputed next paint
        m_cachedTabWidth = -1;
        m_cachedPinnedTabWidth = -1;
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
    m_pinnedTabMaxWidth = Scale(BASE_PINNED_TAB_MAX_WIDTH);
    m_tabPadding = Scale(BASE_TAB_PADDING);
    m_closeBtnSize = Scale(BASE_CLOSE_BTN_SIZE);
    m_closeBtnMargin = Scale(BASE_CLOSE_BTN_MARGIN);
    m_newTabBtnWidth = Scale(BASE_NEW_TAB_BTN_WIDTH);
    m_scrollArrowWidth = Scale(BASE_SCROLL_ARROW_WIDTH);
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
    m_pinnedTabMaxWidth = Scale(BASE_PINNED_TAB_MAX_WIDTH);
    m_tabPadding = Scale(BASE_TAB_PADDING);
    m_closeBtnSize = Scale(BASE_CLOSE_BTN_SIZE);
    m_closeBtnMargin = Scale(BASE_CLOSE_BTN_MARGIN);
    m_newTabBtnWidth = Scale(BASE_NEW_TAB_BTN_WIDTH);
    m_scrollArrowWidth = Scale(BASE_SCROLL_ARROW_WIDTH);

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

    // Invalidate cached painting fonts so they get recreated at new DPI
    if (m_closeFont) { DeleteObject(m_closeFont); m_closeFont = nullptr; }
    if (m_arrowFont) { DeleteObject(m_arrowFont); m_arrowFont = nullptr; }

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

//------------------------------------------------------------------------------
// Show a semi-transparent ghost window following the cursor during tear-off drag
//------------------------------------------------------------------------------
void TabBar::ShowDragPreview(int tabId, int screenX, int screenY) {
    // Offset the preview from the cursor
    int posX = screenX + Scale(12);
    int posY = screenY + Scale(4);

    if (m_hwndDragPreview) {
        // Just reposition the existing preview
        SetWindowPos(m_hwndDragPreview, nullptr, posX, posY, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        return;
    }

    int idx = FindTabIndex(tabId);
    if (idx < 0) return;
    const auto& tab = m_tabs[idx];

    int previewWidth = Scale(180);
    int previewHeight = Scale(BASE_TAB_BAR_HEIGHT - 4);

    // Register the drag preview window class (once)
    static bool s_previewClassRegistered = false;
    if (!s_previewClassRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = m_hInstance;
        wc.lpszClassName = L"QNoteTabDragPreview";
        if (RegisterClassExW(&wc)) {
            s_previewClassRegistered = true;
        }
    }

    m_hwndDragPreview = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"QNoteTabDragPreview",
        nullptr,
        WS_POPUP,
        posX, posY, previewWidth, previewHeight,
        nullptr, nullptr, m_hInstance, nullptr
    );
    if (!m_hwndDragPreview) return;

    // Create a 32-bit DIB section for alpha-blended rendering
    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = previewWidth;
    bmi.bmiHeader.biHeight = -previewHeight;  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memDC, hBitmap));

    // Draw preview background
    RECT rc = { 0, 0, previewWidth, previewHeight };
    HBRUSH bgBrush = CreateSolidBrush(CLR_TAB_ACTIVE);
    FillRect(memDC, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Draw border
    HPEN borderPen = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN oldPen = static_cast<HPEN>(SelectObject(memDC, borderPen));
    HBRUSH oldBrushSel = static_cast<HBRUSH>(SelectObject(memDC, GetStockObject(NULL_BRUSH)));
    Rectangle(memDC, 0, 0, previewWidth, previewHeight);
    SelectObject(memDC, oldPen);
    SelectObject(memDC, oldBrushSel);
    DeleteObject(borderPen);

    // Draw tab title
    HFONT oldFont = static_cast<HFONT>(SelectObject(memDC, m_font));
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, CLR_TEXT_ACTIVE);

    RECT textRc = rc;
    textRc.left += Scale(10);
    textRc.right -= Scale(10);

    std::wstring displayText = tab.title;
    if (tab.isModified) displayText += L" \u2022";
    DrawTextW(memDC, displayText.c_str(), -1, &textRc,
              DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);

    SelectObject(memDC, oldFont);

    // Set premultiplied alpha on every pixel for semi-transparency
    constexpr BYTE alpha = 200;
    BYTE* pixelData = static_cast<BYTE*>(bits);
    for (int i = 0; i < previewWidth * previewHeight; i++) {
        pixelData[i * 4 + 0] = static_cast<BYTE>(pixelData[i * 4 + 0] * alpha / 255);
        pixelData[i * 4 + 1] = static_cast<BYTE>(pixelData[i * 4 + 1] * alpha / 255);
        pixelData[i * 4 + 2] = static_cast<BYTE>(pixelData[i * 4 + 2] * alpha / 255);
        pixelData[i * 4 + 3] = alpha;
    }

    // Apply the alpha-blended bitmap to the layered window
    POINT ptSrc = { 0, 0 };
    SIZE sizeWnd = { previewWidth, previewHeight };
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    POINT ptDst = { posX, posY };
    UpdateLayeredWindow(m_hwndDragPreview, screenDC, &ptDst, &sizeWnd,
                        memDC, &ptSrc, 0, &blend, ULW_ALPHA);

    // Cleanup GDI objects
    SelectObject(memDC, oldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);

    ShowWindow(m_hwndDragPreview, SW_SHOWNOACTIVATE);
}

//------------------------------------------------------------------------------
// Hide and destroy the drag preview ghost window
//------------------------------------------------------------------------------
void TabBar::HideDragPreview() {
    if (m_hwndDragPreview) {
        DestroyWindow(m_hwndDragPreview);
        m_hwndDragPreview = nullptr;
    }
}

} // namespace QNote
