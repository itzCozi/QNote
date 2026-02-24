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
#include <ShellScalingApi.h>
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
    
    // Update font to match editor (extracts font properties and creates own copy)
    void SetFont(HFONT font) noexcept;
    
    // Handle editor scroll notification
    void OnEditorScroll() noexcept;

    // DPI update (call when monitor DPI changes)
    void UpdateDPI(UINT newDpi);
    
private:
    // Window procedure
    static LRESULT CALLBACK GutterWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Paint the gutter
    void OnPaint();
    
    // Calculate required width based on line count
    void CalculateWidth();

    // DPI helpers
    void InitializeDPI();
    int Scale(int value) const noexcept { return MulDiv(value, m_dpi, 96); }

    // Font management
    void RecreateFont();
    void UpdateFontMetrics();
    
private:
    HWND m_hwndGutter = nullptr;
    HWND m_hwndParent = nullptr;
    HINSTANCE m_hInstance = nullptr;
    Editor* m_editor = nullptr;
    
    HFONT m_font = nullptr;         // Owned by gutter - must be deleted
    bool m_ownsFont = false;          // Whether we created the font ourselves
    LOGFONTW m_logFont = {};          // Stored font properties for recreation
    int m_baseFontHeight = 0;         // Unscaled font height (at 96 DPI)
    int m_dpi = 96;                   // Current DPI
    int m_width = 50;                 // Current gutter width
    int m_charWidth = 8;              // Width of a single character
    int m_lineHeight = 16;            // Height of a line
    int m_leftPadding = 6;            // Left padding for line numbers
    int m_rightPadding = 10;          // Right padding for line numbers
    int m_minWidth = 48;              // Minimum gutter width
    bool m_visible = false;
    
    // Base layout constants (scaled by DPI)
    static constexpr int BASE_LEFT_PADDING = 6;
    static constexpr int BASE_RIGHT_PADDING = 10;
    static constexpr int BASE_MIN_WIDTH = 48;
    static constexpr int BASE_DEFAULT_WIDTH = 50;
    
    // Colors
    COLORREF m_bgColor = RGB(240, 240, 240);
    COLORREF m_textColor = RGB(100, 100, 100);
    COLORREF m_borderColor = RGB(200, 200, 200);
    
    // Window class name
    static constexpr wchar_t GUTTER_CLASS[] = L"QNoteLineNumbersGutter";
    static bool s_classRegistered;
};

} // namespace QNote
