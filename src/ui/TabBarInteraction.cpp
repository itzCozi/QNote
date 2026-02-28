//==============================================================================
// QNote - A Lightweight Notepad Clone
// TabBarInteraction.cpp - Tab bar context menu, rename, DPI, and drag preview
//==============================================================================

#include "TabBar.h"
#include <windowsx.h>
#include <CommCtrl.h>
#include <ShellScalingApi.h>
#include <algorithm>

#pragma comment(lib, "Shcore.lib")

namespace QNote {

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
    if (!hBitmap || !bits) {
        DeleteDC(memDC);
        ReleaseDC(nullptr, screenDC);
        DestroyWindow(m_hwndDragPreview);
        m_hwndDragPreview = nullptr;
        return;
    }
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
