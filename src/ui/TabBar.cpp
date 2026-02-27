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

} // namespace QNote

