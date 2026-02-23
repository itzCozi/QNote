//==============================================================================
// QNote - A Lightweight Notepad Clone
// LineNumbersGutter.cpp - Line numbers gutter control implementation
//==============================================================================

#include "LineNumbersGutter.h"
#include "Editor.h"
#include <algorithm>

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
    m_font = font;
    
    if (m_font && m_hwndGutter) {
        // Get font metrics
        HDC hdc = GetDC(m_hwndGutter);
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, m_font));
        
        TEXTMETRICW tm;
        GetTextMetricsW(hdc, &tm);
        m_charWidth = tm.tmAveCharWidth;
        m_lineHeight = tm.tmHeight;
        
        SelectObject(hdc, oldFont);
        ReleaseDC(m_hwndGutter, hdc);
        
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
    
    // Minimum width
    if (m_width < 40) {
        m_width = 40;
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
        
        // Draw line numbers
        for (int i = 0; i < visibleLines && (firstVisibleLine + i) < totalLines; i++) {
            int lineNum = firstVisibleLine + i + 1;  // 1-based line numbers
            
            wchar_t buffer[16];
            wsprintfW(buffer, L"%d", lineNum);
            
            RECT textRect;
            textRect.left = 4;
            textRect.top = i * m_lineHeight;
            textRect.right = rc.right - 8;
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

} // namespace QNote
