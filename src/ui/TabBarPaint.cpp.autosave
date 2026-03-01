//==============================================================================
// QNote - A Lightweight Notepad Clone
// TabBarPaint.cpp - Tab bar painting, hit testing, and layout calculations
//==============================================================================

#include "TabBar.h"
#include <algorithm>

namespace QNote {

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
        int dotCenterY = (textRc.top + textRc.bottom) / 2;
        int dotCenterX = textRc.left + dotRadius;
        HBRUSH dotBrush = CreateSolidBrush(CLR_MODIFIED);
        HPEN dotPen = CreatePen(PS_SOLID, 1, CLR_MODIFIED);
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, dotBrush));
        HPEN oldPen2 = static_cast<HPEN>(SelectObject(hdc, dotPen));
        Ellipse(hdc, dotCenterX - dotRadius, dotCenterY - dotRadius,
                     dotCenterX + dotRadius + 3, dotCenterY + dotRadius + 3);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen2);
        DeleteObject(dotBrush);
        DeleteObject(dotPen);
        textRc.left += dotRadius * 2 + Scale(10);  // Space after dot
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

} // namespace QNote
