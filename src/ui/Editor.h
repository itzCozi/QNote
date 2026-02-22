//==============================================================================
// QNote - A Lightweight Notepad Clone
// Editor.h - Edit control wrapper and text operations
//==============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include "Settings.h"

namespace QNote {

//------------------------------------------------------------------------------
// Undo/Redo action types
//------------------------------------------------------------------------------
enum class UndoActionType {
    Insert,
    Delete,
    Replace
};

//------------------------------------------------------------------------------
// Undo/Redo action structure
//------------------------------------------------------------------------------
struct UndoAction {
    UndoActionType type;
    DWORD startPos;
    DWORD endPos;
    std::wstring oldText;
    std::wstring newText;
};

//------------------------------------------------------------------------------
// RAII wrapper for GDI fonts
//------------------------------------------------------------------------------
class FontGuard {
public:
    FontGuard() noexcept : m_font(nullptr) {}
    explicit FontGuard(HFONT font) noexcept : m_font(font) {}
    ~FontGuard() noexcept {
        if (m_font) {
            DeleteObject(m_font);
        }
    }
    
    FontGuard(const FontGuard&) = delete;
    FontGuard& operator=(const FontGuard&) = delete;
    
    FontGuard(FontGuard&& other) noexcept : m_font(other.m_font) {
        other.m_font = nullptr;
    }
    
    FontGuard& operator=(FontGuard&& other) noexcept {
        if (this != &other) {
            if (m_font) DeleteObject(m_font);
            m_font = other.m_font;
            other.m_font = nullptr;
        }
        return *this;
    }
    
    [[nodiscard]] HFONT get() const noexcept { return m_font; }
    void reset(HFONT font = nullptr) noexcept {
        if (m_font) DeleteObject(m_font);
        m_font = font;
    }
    HFONT release() noexcept {
        HFONT f = m_font;
        m_font = nullptr;
        return f;
    }
    
private:
    HFONT m_font;
};

//------------------------------------------------------------------------------
// Editor control wrapper class
//------------------------------------------------------------------------------

class Editor {
public:
    Editor() noexcept = default;
    ~Editor();
    
    // Prevent copying
    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;
    
    // Initialize the edit control
    [[nodiscard]] bool Create(HWND parent, HINSTANCE hInstance, const AppSettings& settings);
    
    // Destroy the edit control
    void Destroy() noexcept;
    
    // Get the edit control handle
    [[nodiscard]] HWND GetHandle() const noexcept { return m_hwndEdit; }
    
    // Resize the edit control
    void Resize(int x, int y, int width, int height) noexcept;
    
    // Focus the edit control
    void SetFocus() noexcept;
    
    // Text operations
    [[nodiscard]] std::wstring GetText() const;
    void SetText(std::wstring_view text);
    void Clear() noexcept;
    
    // Selection
    void GetSelection(DWORD& start, DWORD& end) const noexcept;
    void SetSelection(DWORD start, DWORD end) noexcept;
    void SelectAll() noexcept;
    [[nodiscard]] std::wstring GetSelectedText() const;
    void ReplaceSelection(std::wstring_view text);
    
    // Clipboard operations
    [[nodiscard]] bool CanUndo() const noexcept;
    [[nodiscard]] bool CanRedo() const noexcept;
    void Undo() noexcept;
    void Redo() noexcept;
    void Cut() noexcept;
    void Copy() noexcept;
    void Paste() noexcept;
    void Delete() noexcept;
    
    // Caret position
    [[nodiscard]] int GetLineCount() const noexcept;
    [[nodiscard]] int GetCurrentLine() const noexcept;
    [[nodiscard]] int GetCurrentColumn() const noexcept;
    [[nodiscard]] int GetLineFromChar(DWORD charIndex) const noexcept;
    [[nodiscard]] int GetLineIndex(int line) const noexcept;
    [[nodiscard]] int GetLineLength(int line) const noexcept;
    
    // Navigation
    void GoToLine(int line) noexcept;
    
    // Font
    void SetFont(std::wstring_view fontName, int fontSize, int fontWeight, bool italic);
    void ApplyZoom(int zoomPercent) noexcept;
    
    // Word wrap
    void SetWordWrap(bool enable);
    [[nodiscard]] bool IsWordWrapEnabled() const noexcept { return m_wordWrap; }
    
    // Tab size
    void SetTabSize(int tabSize) noexcept;
    
    // Modification state
    [[nodiscard]] bool IsModified() const noexcept;
    void SetModified(bool modified) noexcept;
    
    // Find/Replace support
    [[nodiscard]] bool SearchText(std::wstring_view searchText, bool matchCase, bool wrapAround, 
                   bool searchUp, bool useRegex, bool selectMatch = true);
    [[nodiscard]] int ReplaceAll(std::wstring_view searchText, std::wstring_view replaceText,
                   bool matchCase, bool useRegex);
    
    // Line ending operations
    void SetLineEnding(LineEnding lineEnding) noexcept;
    [[nodiscard]] LineEnding GetLineEnding() const noexcept { return m_lineEnding; }
    
    // Encoding (for display purposes)
    void SetEncoding(TextEncoding encoding) noexcept;
    [[nodiscard]] TextEncoding GetEncoding() const noexcept { return m_encoding; }
    
    // Insert date/time at cursor
    void InsertDateTime();
    
    // RTL reading order support
    void SetRTL(bool rtl) noexcept;
    [[nodiscard]] bool IsRTL() const noexcept { return m_rtl; }
    
    // Get character count
    [[nodiscard]] int GetTextLength() const noexcept;
    
private:
    // Recreate edit control (needed for word wrap toggle)
    void RecreateControl();
    
    // Create font from current settings
    HFONT CreateEditorFont();
    
    // Subclass procedure for additional functionality
    static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, 
                                              LPARAM lParam, UINT_PTR subclassId, 
                                              DWORD_PTR refData);
    
private:
    HWND m_hwndEdit = nullptr;
    HWND m_hwndParent = nullptr;
    HINSTANCE m_hInstance = nullptr;
    
    FontGuard m_font;
    std::wstring m_fontName = L"Consolas";
    int m_baseFontSize = 11;
    int m_fontWeight = FW_NORMAL;
    bool m_fontItalic = false;
    int m_zoomPercent = 100;
    
    bool m_wordWrap = false;
    int m_tabSize = 4;
    
    LineEnding m_lineEnding = LineEnding::CRLF;
    TextEncoding m_encoding = TextEncoding::UTF8;
    
    bool m_rtl = false;
    
    // Undo/redo stacks (beyond what Windows provides)
    std::vector<UndoAction> m_undoStack;
    std::vector<UndoAction> m_redoStack;
    static constexpr size_t MAX_UNDO_LEVELS = 100;
    
    // Current search state
    std::wstring m_lastSearchText;
    DWORD m_lastSearchPos = 0;
};

} // namespace QNote
