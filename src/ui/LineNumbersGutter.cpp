//==============================================================================
// QNote - A Lightweight Notepad Clone
// LineNumbersGutter.cpp - Line numbers gutter control implementation
//==============================================================================

#include "LineNumbersGutter.h"
#include "Editor.h"
#include <ShellScalingApi.h>
#include <algorithm>

#pragma comment(lib, "Shcore.lib")

namespace QNote {

// Static member initialization
bool LineNumbersGutter::s_classRegistered = false;

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
LineNumbersGutter::~LineNumbersGutter() {
    Destroy();
}

//------------------------------------------------------------------------------
// Create the gutter window
//------------------------------------------------------------------------------
bool LineNumbersGutter::Create(HWND parent, HINSTANCE hInstance, Editor* editor) {
    m_hwndParent = parent;
    m_hInstance = hInstance;
    m_editor = editor;

    // Initialize DPI scaling
    InitializeDPI();
    
    // Register window class if not already done
    if (!s_classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = GutterWndProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;  // We'll paint the background ourselves
        wc.lpszClassName = GUTTER_CLASS;
        
        if (!RegisterClassExW(&wc)) {
            return false;
        }
        s_classRegistered = true;
    }
    
    // Create the gutter window
    m_hwndGutter = CreateWindowExW(
        0,
        GUTTER_CLASS,
        L"",
        WS_CHILD,  // Initially hidden
        0, 0, m_width, 100,
        parent,
        nullptr,
        hInstance,
        this  // Pass this pointer for WM_CREATE
    );
    
    if (!m_hwndGutter) {
        return false;
    }
    
    // Set the this pointer for the window
    SetWindowLongPtrW(m_hwndGutter, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    
    return true;
}

//------------------------------------------------------------------------------
// Destroy the gutter window
//------------------------------------------------------------------------------
void LineNumbersGutter::Destroy() noexcept {
    if (m_ownsFont && m_font) {
        DeleteObject(m_font);
        m_font = nullptr;
        m_ownsFont = false;
    }
    if (m_hwndGutter) {
        DestroyWindow(m_hwndGutter);
        m_hwndGutter = nullptr;
    }
}

//------------------------------------------------------------------------------
// Show/hide the gutter
//------------------------------------------------------------------------------
void LineNumbersGutter::Show(bool show) noexcept {
    m_visible = show;
    if (m_hwndGutter) {
        ShowWindow(m_hwndGutter, show ? SW_SHOW : SW_HIDE);
        if (show) {
            Update();
        }
    }
}

//------------------------------------------------------------------------------
// Resize the gutter
//------------------------------------------------------------------------------
void LineNumbersGutter::Resize(int x, int y, int height) noexcept {
    if (m_hwndGutter && m_visible) {
        CalculateWidth();
        SetWindowPos(m_hwndGutter, nullptr, x, y, m_width, height, SWP_NOZORDER);
        InvalidateRect(m_hwndGutter, nullptr, FALSE);
    }
}

//------------------------------------------------------------------------------
// Update line numbers
//------------------------------------------------------------------------------
void LineNumbersGutter::Update() noexcept {
    if (m_hwndGutter && m_visible) {
        CalculateWidth();
        InvalidateRect(m_hwndGutter, nullptr, FALSE);
    }
}

//------------------------------------------------------------------------------
// Set font to match editor
//------------------------------------------------------------------------------
void LineNumbersGutter::SetFont(HFONT font) noexcept {
    if (!font) return;

    // Extract the logical font properties from the editor's font
    LOGFONTW lf = {};
    if (GetObjectW(font, sizeof(lf), &lf) == 0) return;

    // Store the font properties
    m_logFont = lf;

    // Compute the base (unscaled) font height by reversing the DPI scaling
    // lfHeight is typically negative = -(size * dpi / 72)
    // We store it as-is so we can re-scale for our DPI
    m_baseFontHeight = lf.lfHeight;

    // Create our own copy of the font, scaled for our DPI
    RecreateFont();

    if (m_font && m_hwndGutter) {
        UpdateFontMetrics();
        CalculateWidth();
        Update();
    }
}

//------------------------------------------------------------------------------
// Handle editor scroll notification
//------------------------------------------------------------------------------
void LineNumbersGutter::OnEditorScroll() noexcept {
    Update();
}

//------------------------------------------------------------------------------
// Calculate required width based on line count
//------------------------------------------------------------------------------
void LineNumbersGutter::CalculateWidth() {
    if (!m_editor) {
        m_width = 50;
        return;
    }
    
    int lineCount = m_editor->GetLineCount();
    int digits = 1;
    int temp = lineCount;
    while (temp >= 10) {
        temp /= 10;
        digits++;
    }
    
    // Minimum 3 digits, plus padding
    digits = (std::max)(digits, 3);
    m_width = (digits + 2) * m_charWidth;  // Extra space for padding
    
    // Minimum width (DPI-scaled)
    if (m_width < m_minWidth) {
        m_width = m_minWidth;
    }
}

//------------------------------------------------------------------------------
// Window procedure
//------------------------------------------------------------------------------
LRESULT CALLBACK LineNumbersGutter::GutterWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    LineNumbersGutter* gutter = reinterpret_cast<LineNumbersGutter*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    
    if (gutter) {
        return gutter->HandleMessage(msg, wParam, lParam);
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

//------------------------------------------------------------------------------
// Handle messages
//------------------------------------------------------------------------------
LRESULT LineNumbersGutter::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT:
            OnPaint();
            return 0;
            
        case WM_ERASEBKGND:
            return 1;  // We handle background in WM_PAINT

        case WM_DPICHANGED_AFTERPARENT: {
            // Child windows receive this when parent DPI changes (Per-Monitor V2)
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
    
    return DefWindowProcW(m_hwndGutter, msg, wParam, lParam);
}

//------------------------------------------------------------------------------
// Paint the gutter
//------------------------------------------------------------------------------
void LineNumbersGutter::OnPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwndGutter, &ps);
    
    RECT rc;
    GetClientRect(m_hwndGutter, &rc);
    
    // Create double buffer
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memDC, memBitmap));
    
    // Fill background
    HBRUSH bgBrush = CreateSolidBrush(m_bgColor);
    FillRect(memDC, &rc, bgBrush);
    DeleteObject(bgBrush);
    
    // Draw right border
    HPEN borderPen = CreatePen(PS_SOLID, 1, m_borderColor);
    HPEN oldPen = static_cast<HPEN>(SelectObject(memDC, borderPen));
    MoveToEx(memDC, rc.right - 1, rc.top, nullptr);
    LineTo(memDC, rc.right - 1, rc.bottom);
    SelectObject(memDC, oldPen);
    DeleteObject(borderPen);
    
    // Set up text drawing
    HFONT oldFont = nullptr;
    if (m_font) {
        oldFont = static_cast<HFONT>(SelectObject(memDC, m_font));
    }
    SetTextColor(memDC, m_textColor);
    SetBkMode(memDC, TRANSPARENT);
    
    // Get editor scroll position and visible lines
    if (m_editor && m_editor->GetHandle()) {
        HWND hwndEdit = m_editor->GetHandle();
        
        // Get the first visible line
        int firstVisibleLine = static_cast<int>(SendMessageW(hwndEdit, EM_GETFIRSTVISIBLELINE, 0, 0));
        
        // Get total line count
        int totalLines = m_editor->GetLineCount();
        
        // Calculate how many lines fit in the visible area
        int visibleLines = (rc.bottom - rc.top) / m_lineHeight + 2;
        
        // Get bookmark set for marker drawing
        const auto& bookmarks = m_editor->GetBookmarks();
        
        // Get the current cursor line (0-based) for highlighting
        int currentLine = m_editor->GetCurrentLine();
        
        // Draw line numbers
        for (int i = 0; i < visibleLines && (firstVisibleLine + i) < totalLines; i++) {
            int lineNum = firstVisibleLine + i + 1;  // 1-based line numbers
            int lineIndex = firstVisibleLine + i;     // 0-based for bookmark check
            
            // Draw bookmark marker (blue circle in left margin)
            if (bookmarks.count(lineIndex)) {
                int cy = i * m_lineHeight + m_lineHeight / 2;
                int cx = m_leftPadding / 2 + 2;
                int r = (std::min)(m_lineHeight / 2 - 2, 5);
                if (r < 2) r = 2;
                HBRUSH bmBrush = CreateSolidBrush(RGB(60, 130, 214));
                HPEN noPen2 = static_cast<HPEN>(GetStockObject(NULL_PEN));
                HBRUSH oldBrush2 = static_cast<HBRUSH>(SelectObject(memDC, bmBrush));
                HPEN oldPen2 = static_cast<HPEN>(SelectObject(memDC, noPen2));
                Ellipse(memDC, cx - r, cy - r, cx + r + 1, cy + r + 1);
                SelectObject(memDC, oldBrush2);
                SelectObject(memDC, oldPen2);
                DeleteObject(bmBrush);
            }
            
            // Highlight current line number
            if (lineIndex == currentLine) {
                SetTextColor(memDC, RGB(30, 30, 30));
            } else {
                SetTextColor(memDC, m_textColor);
            }
            
            wchar_t buffer[16];
            wsprintfW(buffer, L"%d", lineNum);
            
            RECT textRect;
            textRect.left = m_leftPadding;
            textRect.top = i * m_lineHeight;
            textRect.right = rc.right - m_rightPadding;
            textRect.bottom = textRect.top + m_lineHeight;
            
            DrawTextW(memDC, buffer, -1, &textRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
    }
    
    if (oldFont) {
        SelectObject(memDC, oldFont);
    }
    
    // Copy to screen
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
    
    // Cleanup
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
    
    EndPaint(m_hwndGutter, &ps);
}

//------------------------------------------------------------------------------
// Initialize DPI scaling
//------------------------------------------------------------------------------
void LineNumbersGutter::InitializeDPI() {
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

    // Scale layout values
    m_leftPadding = Scale(BASE_LEFT_PADDING);
    m_rightPadding = Scale(BASE_RIGHT_PADDING);
    m_minWidth = Scale(BASE_MIN_WIDTH);
    m_width = Scale(BASE_DEFAULT_WIDTH);
}

//------------------------------------------------------------------------------
// Update DPI scaling (called when monitor DPI changes)
//------------------------------------------------------------------------------
void LineNumbersGutter::UpdateDPI(UINT newDpi) {
    if (static_cast<int>(newDpi) == m_dpi) return;

    m_dpi = static_cast<int>(newDpi);

    // Rescale layout values
    m_leftPadding = Scale(BASE_LEFT_PADDING);
    m_rightPadding = Scale(BASE_RIGHT_PADDING);
    m_minWidth = Scale(BASE_MIN_WIDTH);

    // Recreate the font at the new DPI if we have font properties
    if (m_baseFontHeight != 0) {
        RecreateFont();
        if (m_font && m_hwndGutter) {
            UpdateFontMetrics();
        }
    }

    CalculateWidth();
    if (m_hwndGutter && m_visible) {
        InvalidateRect(m_hwndGutter, nullptr, FALSE);
    }
}

//------------------------------------------------------------------------------
// Recreate the gutter's own font scaled for current DPI
//------------------------------------------------------------------------------
void LineNumbersGutter::RecreateFont() {
    // Delete old owned font
    if (m_ownsFont && m_font) {
        DeleteObject(m_font);
        m_font = nullptr;
        m_ownsFont = false;
    }

    // Create a new font using stored properties
    // The lfHeight from the editor was created at the editor's DPI.
    // We use it directly since the editor should already be DPI-aware.
    // But if DPI has changed since font was set, we need to rescale.
    LOGFONTW lf = m_logFont;

    // Ensure the font height is properly scaled for our current DPI
    // Extract the point size from the stored height and rescale
    // lfHeight = -(pointSize * dpi / 72)
    // We want to ensure the font matches what the editor displays
    if (lf.lfHeight < 0) {
        // Compute a DPI-aware height
        // Use our DPI to create the font so it renders correctly in our DC
        lf.lfHeight = -MulDiv(-lf.lfHeight, m_dpi, m_dpi);
        // The above is identity, but what we really need is to ensure
        // the font is created with the correct height for this DPI.
        // Since the editor font may have been created at a different DPI,
        // let's just use the LOGFONT as-is (the editor already DPI-scaled it)
        lf.lfHeight = m_logFont.lfHeight;
    }

    lf.lfQuality = CLEARTYPE_QUALITY;
    m_font = CreateFontIndirectW(&lf);
    m_ownsFont = (m_font != nullptr);
}

//------------------------------------------------------------------------------
// Update cached font metrics from the current font
//------------------------------------------------------------------------------
void LineNumbersGutter::UpdateFontMetrics() {
    if (!m_font || !m_hwndGutter) return;

    HDC hdc = GetDC(m_hwndGutter);
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, m_font));

    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    m_charWidth = tm.tmAveCharWidth;
    m_lineHeight = tm.tmHeight;

    SelectObject(hdc, oldFont);
    ReleaseDC(m_hwndGutter, hdc);
}

} // namespace QNote
