//==============================================================================
// QNote - A Lightweight Notepad Clone
// Editor.h - RichEdit control wrapper and text operations
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
#include <string_view>
#include <vector>
#include <memory>
#include <set>
#include "Settings.h"
#include "SpellChecker.h"
#include "SyntaxHighlighter.h"

namespace QNote {

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
// Undo/Redo action types for grouping
//------------------------------------------------------------------------------
enum class EditAction { None, Typing, Deleting, Other };

//------------------------------------------------------------------------------
// Undo checkpoint - captures full editor state at a point in time
//------------------------------------------------------------------------------
struct UndoCheckpoint {
    std::wstring text;
    DWORD selStart = 0;
    DWORD selEnd = 0;
    int firstVisibleLine = 0;
};

//------------------------------------------------------------------------------
// Editor control wrapper class (custom multi-level undo/redo)
//------------------------------------------------------------------------------

class Editor {
public:
    Editor() noexcept = default;
    ~Editor();
    
    // Prevent copying
    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;
    
    // Initialize RichEdit library (call once at app startup)
    static bool InitializeRichEdit();
    static void UninitializeRichEdit();
    
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
    
    // Stream-set large text in chunks, pumping the message loop between
    // chunks so the UI stays responsive.  Use for content >100 MB.
    void SetTextStreamed(const std::wstring& text, HWND hwndStatus = nullptr);
    
    void Clear() noexcept;
    
    // Selection
    void GetSelection(DWORD& start, DWORD& end) const noexcept;
    void SetSelection(DWORD start, DWORD end) noexcept;
    void SelectAll() noexcept;
    [[nodiscard]] std::wstring GetSelectedText() const;
    void ReplaceSelection(std::wstring_view text);
    
    // Undo/Redo (custom multi-level, VSCode-style)
    [[nodiscard]] bool CanUndo() const noexcept;
    [[nodiscard]] bool CanRedo() const noexcept;
    void Undo();
    void Redo();
    void PushUndoCheckpoint(EditAction action, wchar_t ch = 0);
    void ClearUndoHistory();
    void SealUndoGroup();
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
    
    // Get cached word count (only recomputed when text changes)
    [[nodiscard]] int GetWordCount();
    
    // Get font for line numbers gutter
    [[nodiscard]] HFONT GetFont() const noexcept { return m_font.get(); }
    
    // Get first visible line (for line numbers sync)
    [[nodiscard]] int GetFirstVisibleLine() const noexcept;
    
    // Scroll lines per wheel notch (0 = system default)
    void SetScrollLines(int lines) noexcept;
    [[nodiscard]] int GetScrollLines() const noexcept { return m_scrollLines; }
    
    // Scroll notification callback
    using ScrollCallback = void(*)(void* userData);
    void SetScrollCallback(ScrollCallback callback, void* userData) noexcept;
    
    // Show whitespace
    void SetShowWhitespace(bool enable) noexcept;
    [[nodiscard]] bool IsShowWhitespace() const noexcept { return m_showWhitespace; }
    
    // Bookmarks
    void ToggleBookmark();
    void NextBookmark();
    void PrevBookmark();
    void ClearBookmarks();
    [[nodiscard]] bool HasBookmarks() const noexcept { return !m_bookmarks.empty(); }
    [[nodiscard]] const std::set<int>& GetBookmarks() const noexcept { return m_bookmarks; }
    void SetBookmarks(const std::set<int>& bookmarks);
    
    // Spell check
    void SetSpellCheck(bool enable);
    [[nodiscard]] bool IsSpellCheckEnabled() const noexcept { return m_spellCheckEnabled; }
    
    // Line operations (VS Code-like editing)
    void DeleteLine();
    void DuplicateLine();
    void MoveLineUp();
    void MoveLineDown();
    void CopyLineUp();
    void CopyLineDown();
    void InsertLineBelow();
    void InsertLineAbove();
    void SelectLine();
    void IndentSelection();
    void UnindentSelection();
    void SmartHome(bool extendSelection);
    
    // Auto-complete braces/quotes
    void SetAutoCompleteBraces(bool enable) noexcept { m_autoCompleteBraces = enable; }
    [[nodiscard]] bool IsAutoCompleteBracesEnabled() const noexcept { return m_autoCompleteBraces; }
    
    // Syntax highlighting
    void SetSyntaxHighlighting(bool enable);
    [[nodiscard]] bool IsSyntaxHighlightingEnabled() const noexcept { return m_syntaxHighlightEnabled; }
    void SetFilePath(std::wstring_view filePath);
    void ApplySyntaxHighlighting();
    void ScheduleSyntaxHighlighting();
    
private:
    // Recreate edit control (needed for word wrap toggle)
    void RecreateControl();
    
    // Create font from current settings
    HFONT CreateEditorFont();
    
    // Apply font to all text via EM_SETCHARFORMAT (RichEdit-proper)
    void ApplyCharFormat();
    
    // Subclass procedure for additional functionality
    static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, 
                                              LPARAM lParam, UINT_PTR subclassId, 
                                              DWORD_PTR refData);
    
    // Paint helpers for overlays
    void DrawWhitespace(HDC hdc);
    void DrawSpellCheck(HDC hdc);
    
    // Helper to get line text content (without line ending)
    [[nodiscard]] std::wstring GetLineText(int line) const;
    
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
    
    // Show whitespace
    bool m_showWhitespace = false;
    
    // Auto-complete braces/quotes
    bool m_autoCompleteBraces = true;
    
    // Syntax highlighting
    bool m_syntaxHighlightEnabled = false;
    bool m_syntaxDirty = true;
    Language m_language = Language::None;
    SyntaxHighlighter m_syntaxHighlighter;
    std::wstring m_filePath;
    
    // Bookmarks
    std::set<int> m_bookmarks;
    
    // Spell check
    bool m_spellCheckEnabled = false;
    SpellChecker m_spellChecker;
    bool m_spellDirty = true;
    int m_spellCacheFirstLine = -1;
    int m_spellCacheLastLine = -1;
    std::vector<MisspelledWord> m_spellCacheWords;
    std::wstring m_rightClickWord;
    DWORD m_rightClickStart = 0;
    DWORD m_rightClickLen = 0;
    std::vector<std::wstring> m_rightClickSuggestions;
    
    int m_scrollLines = 0;  // Lines per wheel notch (0 = system default)
    
    // Cached word count
    int m_cachedWordCount = 0;
    bool m_wordCountDirty = true;
    
    // Scroll notification callback
    ScrollCallback m_scrollCallback = nullptr;
    void* m_scrollCallbackData = nullptr;
    
    // Custom undo/redo system
    std::vector<UndoCheckpoint> m_undoStack;
    std::vector<UndoCheckpoint> m_redoStack;
    EditAction m_lastEditAction = EditAction::None;
    DWORD m_lastEditTime = 0;
    bool m_suppressUndo = false;
    static constexpr int MAX_UNDO_LEVELS = 100;
    static constexpr size_t MAX_UNDO_MEMORY_BYTES = 50 * 1024 * 1024;  // 50 MB cap
    size_t m_undoMemoryUsage = 0;
    size_t m_redoMemoryUsage = 0;
    
    // RichEdit library handle
    static HMODULE s_hRichEditLib;
    
    // Current search state
    std::wstring m_lastSearchText;
    DWORD m_lastSearchPos = 0;
};

} // namespace QNote
