//==============================================================================
// QNote - A Lightweight Notepad Clone
// LineNumbersGutter.h - Line numbers gutter control
//==============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Richedit.h>
#include <string>

namespace QNote {

class Editor;

//------------------------------------------------------------------------------
// Line numbers gutter control
//------------------------------------------------------------------------------
class LineNumbersGutter {
public:
    LineNumbersGutter() noexcept = default;
    ~LineNumbersGutter();
    
    // Prevent copying
    LineNumbersGutter(const LineNumbersGutter&) = delete;
    LineNumbersGutter& operator=(const LineNumbersGutter&) = delete;
    
    // Create the gutter window
    [[nodiscard]] bool Create(HWND parent, HINSTANCE hInstance, Editor* editor);
    
    // Destroy the gutter window
    void Destroy() noexcept;
    
    // Get the gutter handle
    [[nodiscard]] HWND GetHandle() const noexcept { return m_hwndGutter; }
    
    // Visibility
    void Show(bool show) noexcept;
    [[nodiscard]] bool IsVisible() const noexcept { return m_visible; }
    
    // Resize the gutter
    void Resize(int x, int y, int height) noexcept;
    
    // Get gutter width
    [[nodiscard]] int GetWidth() const noexcept { return m_visible ? m_width : 0; }
    
    // Update line numbers (call when editor content or scroll changes)
    void Update() noexcept;
    
    // Update font to match editor
    void SetFont(HFONT font) noexcept;
    
    // Handle editor scroll notification
    void OnEditorScroll() noexcept;
    
private:
    // Window procedure
    static LRESULT CALLBACK GutterWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Paint the gutter
    void OnPaint();
    
    // Calculate required width based on line count
    void CalculateWidth();
    
private:
    HWND m_hwndGutter = nullptr;
    HWND m_hwndParent = nullptr;
    HINSTANCE m_hInstance = nullptr;
    Editor* m_editor = nullptr;
    
    HFONT m_font = nullptr;
    int m_width = 50;           // Current gutter width
    int m_charWidth = 8;        // Width of a single character
    int m_lineHeight = 16;      // Height of a line
    bool m_visible = false;
    
    // Colors
    COLORREF m_bgColor = RGB(240, 240, 240);
    COLORREF m_textColor = RGB(100, 100, 100);
    COLORREF m_borderColor = RGB(200, 200, 200);
    
    // Window class name
    static constexpr wchar_t GUTTER_CLASS[] = L"QNoteLineNumbersGutter";
    static bool s_classRegistered;
};

} // namespace QNote
