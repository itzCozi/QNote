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
    FontGuard() : m_font(nullptr) {}
    explicit FontGuard(HFONT font) : m_font(font) {}
    ~FontGuard() {
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
    
    HFONT get() const { return m_font; }
    void reset(HFONT font = nullptr) {
        if (m_font) DeleteObject(m_font);
        m_font = font;
    }
    HFONT release() {
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
    Editor();
    ~Editor();
    
    // Prevent copying
    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;
    
    // Initialize the edit control
    bool Create(HWND parent, HINSTANCE hInstance, const AppSettings& settings);
    
    // Destroy the edit control
    void Destroy();
    
    // Get the edit control handle
    HWND GetHandle() const { return m_hwndEdit; }
    
    // Resize the edit control
    void Resize(int x, int y, int width, int height);
    
    // Focus the edit control
    void SetFocus();
    
    // Text operations
    std::wstring GetText() const;
    void SetText(const std::wstring& text);
    void Clear();
    
    // Selection
    void GetSelection(DWORD& start, DWORD& end) const;
    void SetSelection(DWORD start, DWORD end);
    void SelectAll();
    std::wstring GetSelectedText() const;
    void ReplaceSelection(const std::wstring& text);
    
    // Clipboard operations
    bool CanUndo() const;
    bool CanRedo() const;
    void Undo();
    void Redo();
    void Cut();
    void Copy();
    void Paste();
    void Delete();
    
    // Caret position
    int GetLineCount() const;
    int GetCurrentLine() const;
    int GetCurrentColumn() const;
    int GetLineFromChar(DWORD charIndex) const;
    int GetLineIndex(int line) const;
    int GetLineLength(int line) const;
    
    // Navigation
    void GoToLine(int line);
    
    // Font
    void SetFont(const std::wstring& fontName, int fontSize, int fontWeight, bool italic);
    void ApplyZoom(int zoomPercent);
    
    // Word wrap
    void SetWordWrap(bool enable);
    bool IsWordWrapEnabled() const { return m_wordWrap; }
    
    // Tab size
    void SetTabSize(int tabSize);
    
    // Modification state
    bool IsModified() const;
    void SetModified(bool modified);
    
    // Find/Replace support
    bool SearchText(const std::wstring& searchText, bool matchCase, bool wrapAround, 
                   bool searchUp, bool useRegex, bool selectMatch = true);
    int ReplaceAll(const std::wstring& searchText, const std::wstring& replaceText,
                   bool matchCase, bool useRegex);
    
    // Line ending operations
    void SetLineEnding(LineEnding lineEnding);
    LineEnding GetLineEnding() const { return m_lineEnding; }
    
    // Encoding (for display purposes)
    void SetEncoding(TextEncoding encoding);
    TextEncoding GetEncoding() const { return m_encoding; }
    
    // Insert date/time at cursor
    void InsertDateTime();
    
    // Dark mode support
    void SetDarkMode(bool darkMode);
    
    // RTL reading order support
    void SetRTL(bool rtl);
    bool IsRTL() const { return m_rtl; }
    
    // Get character count
    int GetTextLength() const;
    
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
    
    bool m_darkMode = false;
    HBRUSH m_darkBrush = nullptr;
    
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
