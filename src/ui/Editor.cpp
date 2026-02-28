//==============================================================================
// QNote - A Lightweight Notepad Clone
// Editor.cpp - RichEdit control wrapper and text operations implementation
//==============================================================================

#include "Editor.h"
#include "resource.h"
#include <CommCtrl.h>
#include <Richedit.h>
#include <regex>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Msimg32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace QNote {

// Subclass ID for edit control
static constexpr UINT_PTR EDIT_SUBCLASS_ID = 1;

// Static member initialization
HMODULE Editor::s_hRichEditLib = nullptr;

//------------------------------------------------------------------------------
// Initialize RichEdit library
//------------------------------------------------------------------------------
bool Editor::InitializeRichEdit() {
    if (s_hRichEditLib) {
        return true;  // Already loaded
    }
    
    // Load RichEdit 4.1 (Msftedit.dll) for best compatibility
    s_hRichEditLib = LoadLibraryW(L"Msftedit.dll");
    if (!s_hRichEditLib) {
        // Fallback to older RichEdit
        s_hRichEditLib = LoadLibraryW(L"Riched20.dll");
    }
    
    return s_hRichEditLib != nullptr;
}

//------------------------------------------------------------------------------
// Uninitialize RichEdit library
//------------------------------------------------------------------------------
void Editor::UninitializeRichEdit() {
    if (s_hRichEditLib) {
        FreeLibrary(s_hRichEditLib);
        s_hRichEditLib = nullptr;
    }
}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
Editor::~Editor() {
    Destroy();
}

//------------------------------------------------------------------------------
// Create the edit control
//------------------------------------------------------------------------------
bool Editor::Create(HWND parent, HINSTANCE hInstance, const AppSettings& settings) {
    m_hwndParent = parent;
    m_hInstance = hInstance;
    
    // Store settings
    m_fontName = settings.fontName;
    m_baseFontSize = settings.fontSize;
    m_fontWeight = settings.fontWeight;
    m_fontItalic = settings.fontItalic;
    m_wordWrap = settings.wordWrap;
    m_tabSize = settings.tabSize;
    m_zoomPercent = settings.zoomLevel;
    m_rtl = settings.rightToLeft;
    m_scrollLines = settings.scrollLines;
    
    // Determine edit control style
    DWORD style = WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_NOHIDESEL;
    if (!m_wordWrap) {
        style |= ES_AUTOHSCROLL | WS_HSCROLL;
    }
    
    // Determine extended style (RTL support, no border)
    DWORD exStyle = 0;
    if (m_rtl) {
        exStyle |= WS_EX_RTLREADING | WS_EX_RIGHT;
    }
    
    // Create the RichEdit control
    m_hwndEdit = CreateWindowExW(
        exStyle,
        MSFTEDIT_CLASS,  // RichEdit 4.1 class
        L"",
        style,
        0, 0, 100, 100,  // Will be resized later
        parent,
        nullptr,
        hInstance,
        nullptr
    );
    
    if (!m_hwndEdit) {
        return false;
    }
    
    // Set text limit to maximum (RichEdit uses different message)
    SendMessageW(m_hwndEdit, EM_EXLIMITTEXT, 0, 0x7FFFFFFE);
    
    // Disable built-in undo (we use our own multi-level system)
    SendMessageW(m_hwndEdit, EM_SETUNDOLIMIT, 0, 0);
    
    // Enable EN_CHANGE notifications (required for RichEdit controls)
    DWORD eventMask = static_cast<DWORD>(SendMessageW(m_hwndEdit, EM_GETEVENTMASK, 0, 0));
    SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, eventMask | ENM_CHANGE | ENM_SCROLL);
    
    // Create and set font
    m_font.reset(CreateEditorFont());
    if (m_font.get()) {
        SendMessageW(m_hwndEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_font.get()), TRUE);
        ApplyCharFormat();
    }
    
    // Set tab stops
    SetTabSize(m_tabSize);
    
    // Subclass for additional handling
    SetWindowSubclass(m_hwndEdit, EditSubclassProc, EDIT_SUBCLASS_ID, 
                      reinterpret_cast<DWORD_PTR>(this));
    
    return true;
}

//------------------------------------------------------------------------------
// Destroy the edit control  
//------------------------------------------------------------------------------
void Editor::Destroy() noexcept {
    if (m_hwndEdit) {
        RemoveWindowSubclass(m_hwndEdit, EditSubclassProc, EDIT_SUBCLASS_ID);
        DestroyWindow(m_hwndEdit);
        m_hwndEdit = nullptr;
    }
}

//------------------------------------------------------------------------------
// Resize the edit control
//------------------------------------------------------------------------------
void Editor::Resize(int x, int y, int width, int height) noexcept {
    if (m_hwndEdit) {
        SetWindowPos(m_hwndEdit, nullptr, x, y, width, height, SWP_NOZORDER);
    }
}

//------------------------------------------------------------------------------
// Focus the edit control
//------------------------------------------------------------------------------
void Editor::SetFocus() noexcept {
    if (m_hwndEdit) {
        ::SetFocus(m_hwndEdit);
    }
}

//------------------------------------------------------------------------------
// Get text from edit control
//------------------------------------------------------------------------------
std::wstring Editor::GetText() const {
    if (!m_hwndEdit) return L"";
    
    int len = GetWindowTextLengthW(m_hwndEdit);
    if (len == 0) return L"";
    
    std::wstring text(len + 1, L'\0');
    GetWindowTextW(m_hwndEdit, text.data(), len + 1);
    text.resize(len);
    
    return text;
}

//------------------------------------------------------------------------------
// Set text in edit control
//------------------------------------------------------------------------------
void Editor::SetText(std::wstring_view text) {
    if (m_hwndEdit) {
        // Temporarily suppress EN_CHANGE notifications so that programmatic
        // text loads are not misinterpreted as user edits.
        DWORD oldMask = static_cast<DWORD>(SendMessageW(m_hwndEdit, EM_GETEVENTMASK, 0, 0));
        SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask & ~ENM_CHANGE);

        SetWindowTextW(m_hwndEdit, std::wstring(text).c_str());
        ApplyCharFormat();
        SetModified(false);

        SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask);

        // Clear undo history - this is a fresh document load
        ClearUndoHistory();
    }
}

//------------------------------------------------------------------------------
// Clear text
//------------------------------------------------------------------------------
void Editor::Clear() noexcept {
    if (m_hwndEdit) {
        // Suppress EN_CHANGE so clearing is not treated as a user edit
        DWORD oldMask = static_cast<DWORD>(SendMessageW(m_hwndEdit, EM_GETEVENTMASK, 0, 0));
        SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask & ~ENM_CHANGE);

        SetWindowTextW(m_hwndEdit, L"");
        SetModified(false);

        SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask);

        // Clear undo history
        ClearUndoHistory();
    }
}

//------------------------------------------------------------------------------
// Get selection range
//------------------------------------------------------------------------------
void Editor::GetSelection(DWORD& start, DWORD& end) const noexcept {
    if (m_hwndEdit) {
        SendMessageW(m_hwndEdit, EM_GETSEL, reinterpret_cast<WPARAM>(&start), 
                     reinterpret_cast<LPARAM>(&end));
    } else {
        start = end = 0;
    }
}

//------------------------------------------------------------------------------
// Set selection range
//------------------------------------------------------------------------------
void Editor::SetSelection(DWORD start, DWORD end) noexcept {
    if (m_hwndEdit) {
        SendMessageW(m_hwndEdit, EM_SETSEL, start, end);
        SendMessageW(m_hwndEdit, EM_SCROLLCARET, 0, 0);
    }
}

//------------------------------------------------------------------------------
// Select all text
//------------------------------------------------------------------------------
void Editor::SelectAll() noexcept {
    SetSelection(0, static_cast<DWORD>(-1));
}

//------------------------------------------------------------------------------
// Get selected text
//------------------------------------------------------------------------------
std::wstring Editor::GetSelectedText() const {
    if (!m_hwndEdit) return L"";
    
    DWORD start, end;
    GetSelection(start, end);
    
    if (start == end) return L"";
    if (start > end) std::swap(start, end);
    
    // Use EM_GETTEXTRANGE to read only the selected portion
    int len = end - start;
    std::vector<wchar_t> buf(len + 1, 0);
    TEXTRANGEW tr = {};
    tr.chrg.cpMin = start;
    tr.chrg.cpMax = end;
    tr.lpstrText = buf.data();
    LRESULT actual = SendMessageW(m_hwndEdit, EM_GETTEXTRANGE, 0, reinterpret_cast<LPARAM>(&tr));
    
    return std::wstring(buf.data(), static_cast<size_t>(actual));
}

//------------------------------------------------------------------------------
// Replace selection with text
//------------------------------------------------------------------------------
void Editor::ReplaceSelection(std::wstring_view text) {
    if (m_hwndEdit) {
        SendMessageW(m_hwndEdit, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(std::wstring(text).c_str()));
    }
}

//------------------------------------------------------------------------------
// Can undo (custom stack)
//------------------------------------------------------------------------------
bool Editor::CanUndo() const noexcept {
    return !m_undoStack.empty();
}

//------------------------------------------------------------------------------
// Can redo (custom stack)
//------------------------------------------------------------------------------
bool Editor::CanRedo() const noexcept {
    return !m_redoStack.empty();
}

//------------------------------------------------------------------------------
// Undo - restore previous checkpoint
//------------------------------------------------------------------------------
void Editor::Undo() {
    if (m_undoStack.empty() || !m_hwndEdit) return;

    m_suppressUndo = true;

    // Save current state to redo stack
    UndoCheckpoint current;
    current.text = GetText();
    GetSelection(current.selStart, current.selEnd);
    current.firstVisibleLine = GetFirstVisibleLine();
    m_redoStack.push_back(std::move(current));

    // Pop from undo stack
    UndoCheckpoint& cp = m_undoStack.back();

    // Suppress EN_CHANGE during text restoration
    DWORD oldMask = static_cast<DWORD>(SendMessageW(m_hwndEdit, EM_GETEVENTMASK, 0, 0));
    SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask & ~ENM_CHANGE);

    SetWindowTextW(m_hwndEdit, cp.text.c_str());
    ApplyCharFormat();

    // Restore selection
    SendMessageW(m_hwndEdit, EM_SETSEL, cp.selStart, cp.selEnd);

    // Restore scroll position
    int currentFirst = GetFirstVisibleLine();
    if (cp.firstVisibleLine != currentFirst) {
        SendMessageW(m_hwndEdit, EM_LINESCROLL, 0, cp.firstVisibleLine - currentFirst);
    }
    SendMessageW(m_hwndEdit, EM_SCROLLCARET, 0, 0);

    // Re-enable notifications
    SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask);

    // Mark modified (unless we're back to original text)
    SetModified(true);

    m_undoStack.pop_back();
    m_lastEditAction = EditAction::None;
    m_suppressUndo = false;

    // Notify scroll callback for line numbers update
    if (m_scrollCallback) {
        m_scrollCallback(m_scrollCallbackData);
    }
}

//------------------------------------------------------------------------------
// Redo - restore next checkpoint
//------------------------------------------------------------------------------
void Editor::Redo() {
    if (m_redoStack.empty() || !m_hwndEdit) return;

    m_suppressUndo = true;

    // Save current state to undo stack
    UndoCheckpoint current;
    current.text = GetText();
    GetSelection(current.selStart, current.selEnd);
    current.firstVisibleLine = GetFirstVisibleLine();
    m_undoStack.push_back(std::move(current));

    // Pop from redo stack
    UndoCheckpoint& cp = m_redoStack.back();

    // Suppress EN_CHANGE during text restoration
    DWORD oldMask = static_cast<DWORD>(SendMessageW(m_hwndEdit, EM_GETEVENTMASK, 0, 0));
    SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask & ~ENM_CHANGE);

    SetWindowTextW(m_hwndEdit, cp.text.c_str());
    ApplyCharFormat();

    // Restore selection
    SendMessageW(m_hwndEdit, EM_SETSEL, cp.selStart, cp.selEnd);

    // Restore scroll position
    int currentFirst = GetFirstVisibleLine();
    if (cp.firstVisibleLine != currentFirst) {
        SendMessageW(m_hwndEdit, EM_LINESCROLL, 0, cp.firstVisibleLine - currentFirst);
    }
    SendMessageW(m_hwndEdit, EM_SCROLLCARET, 0, 0);

    // Re-enable notifications
    SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask);

    SetModified(true);

    m_redoStack.pop_back();
    m_lastEditAction = EditAction::None;
    m_suppressUndo = false;

    // Notify scroll callback for line numbers update
    if (m_scrollCallback) {
        m_scrollCallback(m_scrollCallbackData);
    }
}

//------------------------------------------------------------------------------
// Push undo checkpoint - captures state before a change
// Groups consecutive same-type edits together like VSCode:
// - Consecutive typing groups until: enter, pause >2s, or cursor jump
// - Consecutive deleting groups until: pause >2s or cursor jump
// - "Other" actions (paste, cut, line ops) always start a new group
//------------------------------------------------------------------------------
void Editor::PushUndoCheckpoint(EditAction action, wchar_t ch) {
    if (m_suppressUndo || !m_hwndEdit) return;

    DWORD now = GetTickCount();

    // Determine if this edit should be grouped with the previous one
    bool newGroup = true;

    if (action == m_lastEditAction && action != EditAction::Other) {
        // Same action type - check for grouping
        DWORD elapsed = now - m_lastEditTime;

        if (action == EditAction::Typing) {
            // Group typing together unless:
            // - Enter/newline (always starts new group)
            // - Pause > 2 seconds
            if (ch != L'\r' && ch != L'\n' && elapsed < 2000) {
                newGroup = false;  // Continue grouping
            }
        } else if (action == EditAction::Deleting) {
            // Group deleting together unless pause > 2 seconds
            if (elapsed < 2000) {
                newGroup = false;
            }
        }
    }

    m_lastEditAction = action;
    m_lastEditTime = now;

    if (!newGroup) return;  // Extend current group, don't push new checkpoint

    // Capture current state
    UndoCheckpoint cp;
    cp.text = GetText();
    GetSelection(cp.selStart, cp.selEnd);
    cp.firstVisibleLine = GetFirstVisibleLine();

    m_undoMemoryUsage += cp.text.size() * sizeof(wchar_t);
    m_undoStack.push_back(std::move(cp));

    // Enforce max undo levels and memory cap
    while (m_undoStack.size() > 1 &&
           (static_cast<int>(m_undoStack.size()) > MAX_UNDO_LEVELS ||
            m_undoMemoryUsage > MAX_UNDO_MEMORY_BYTES)) {
        m_undoMemoryUsage -= m_undoStack.front().text.size() * sizeof(wchar_t);
        m_undoStack.erase(m_undoStack.begin());
    }

    // Clear redo stack on new edit
    m_redoStack.clear();
}

//------------------------------------------------------------------------------
// Seal the current undo group - forces the next edit to start a new group
//------------------------------------------------------------------------------
void Editor::SealUndoGroup() {
    m_lastEditAction = EditAction::None;
}

//------------------------------------------------------------------------------
// Clear all undo/redo history
//------------------------------------------------------------------------------
void Editor::ClearUndoHistory() {
    m_undoStack.clear();
    m_redoStack.clear();
    m_undoMemoryUsage = 0;
    m_lastEditAction = EditAction::None;
    m_lastEditTime = 0;
}

//------------------------------------------------------------------------------
// Cut (VS Code: cuts entire line when no selection)
//------------------------------------------------------------------------------
void Editor::Cut() noexcept {
    if (!m_hwndEdit) return;
    
    PushUndoCheckpoint(EditAction::Other);
    
    DWORD start, end;
    GetSelection(start, end);
    if (start == end) {
        // No selection - select the entire line including line ending
        int line = GetLineFromChar(start);
        int lineCount = GetLineCount();
        int lineStart = GetLineIndex(line);
        
        if (line + 1 < lineCount) {
            SetSelection(lineStart, GetLineIndex(line + 1));
        } else if (line > 0) {
            // Last line: include preceding line break
            int prevEnd = GetLineIndex(line - 1) + GetLineLength(line - 1);
            SetSelection(prevEnd, GetTextLength());
        } else {
            SetSelection(0, GetTextLength());
        }
    }
    
    SendMessageW(m_hwndEdit, WM_CUT, 0, 0);
}

//------------------------------------------------------------------------------
// Copy (VS Code: copies entire line when no selection)
//------------------------------------------------------------------------------
void Editor::Copy() noexcept {
    if (!m_hwndEdit) return;
    
    DWORD start, end;
    GetSelection(start, end);
    
    if (start == end) {
        // No selection - temporarily select line, copy, then restore cursor
        int line = GetLineFromChar(start);
        int lineCount = GetLineCount();
        int lineStart = GetLineIndex(line);
        int selectEnd = (line + 1 < lineCount) ? GetLineIndex(line + 1) : GetTextLength();
        
        SetSelection(lineStart, selectEnd);
        SendMessageW(m_hwndEdit, WM_COPY, 0, 0);
        SetSelection(start, end);
    } else {
        SendMessageW(m_hwndEdit, WM_COPY, 0, 0);
    }
}

//------------------------------------------------------------------------------
// Paste
//------------------------------------------------------------------------------
void Editor::Paste() noexcept {
    if (m_hwndEdit) {
        SendMessageW(m_hwndEdit, WM_PASTE, 0, 0);
    }
}

//------------------------------------------------------------------------------
// Delete selection
//------------------------------------------------------------------------------
void Editor::Delete() noexcept {
    if (m_hwndEdit) {
        SendMessageW(m_hwndEdit, WM_CLEAR, 0, 0);
    }
}

//------------------------------------------------------------------------------
// Get line count
//------------------------------------------------------------------------------
int Editor::GetLineCount() const noexcept {
    if (m_hwndEdit) {
        return static_cast<int>(SendMessageW(m_hwndEdit, EM_GETLINECOUNT, 0, 0));
    }
    return 0;
}

//------------------------------------------------------------------------------
// Get current line (0-based)
//------------------------------------------------------------------------------
int Editor::GetCurrentLine() const noexcept {
    if (!m_hwndEdit) return 0;
    
    DWORD start, end;
    GetSelection(start, end);
    return GetLineFromChar(start);
}

//------------------------------------------------------------------------------
// Get current column (0-based)
//------------------------------------------------------------------------------
int Editor::GetCurrentColumn() const noexcept {
    if (!m_hwndEdit) return 0;
    
    DWORD start, end;
    GetSelection(start, end);
    
    int line = GetLineFromChar(start);
    int lineStart = GetLineIndex(line);
    
    return start - lineStart;
}

//------------------------------------------------------------------------------
// Get line from character index
//------------------------------------------------------------------------------
int Editor::GetLineFromChar(DWORD charIndex) const noexcept {
    if (m_hwndEdit) {
        return static_cast<int>(SendMessageW(m_hwndEdit, EM_LINEFROMCHAR, charIndex, 0));
    }
    return 0;
}

//------------------------------------------------------------------------------
// Get character index of line start
//------------------------------------------------------------------------------
int Editor::GetLineIndex(int line) const noexcept {
    if (m_hwndEdit) {
        return static_cast<int>(SendMessageW(m_hwndEdit, EM_LINEINDEX, line, 0));
    }
    return 0;
}

//------------------------------------------------------------------------------
// Get line length
//------------------------------------------------------------------------------
int Editor::GetLineLength(int line) const noexcept {
    if (m_hwndEdit) {
        int lineIndex = GetLineIndex(line);
        return static_cast<int>(SendMessageW(m_hwndEdit, EM_LINELENGTH, lineIndex, 0));
    }
    return 0;
}

//------------------------------------------------------------------------------
// Go to line
//------------------------------------------------------------------------------
void Editor::GoToLine(int line) noexcept {
    if (!m_hwndEdit) return;
    
    // Validate line number
    int lineCount = GetLineCount();
    if (line < 0) line = 0;
    if (line >= lineCount) line = lineCount - 1;
    
    // Get character index of line start
    int charIndex = GetLineIndex(line);
    
    // Set selection to line start
    SetSelection(charIndex, charIndex);
}

//------------------------------------------------------------------------------
// Set font
//------------------------------------------------------------------------------
void Editor::SetFont(std::wstring_view fontName, int fontSize, int fontWeight, bool italic) {
    m_fontName = fontName;
    m_baseFontSize = fontSize;
    m_fontWeight = fontWeight;
    m_fontItalic = italic;
    
    // Recreate font with zoom applied
    m_font.reset(CreateEditorFont());
    if (m_hwndEdit && m_font.get()) {
        // Suppress EN_CHANGE so font changes don't mark the document modified
        DWORD oldMask = static_cast<DWORD>(SendMessageW(m_hwndEdit, EM_GETEVENTMASK, 0, 0));
        SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask & ~ENM_CHANGE);
        bool wasModified = IsModified();

        SendMessageW(m_hwndEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_font.get()), TRUE);
        ApplyCharFormat();

        SetModified(wasModified);
        SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask);
    }
}

//------------------------------------------------------------------------------
// Apply zoom level
//------------------------------------------------------------------------------
void Editor::ApplyZoom(int zoomPercent) noexcept {
    if (zoomPercent < 25) zoomPercent = 25;
    if (zoomPercent > 500) zoomPercent = 500;
    
    m_zoomPercent = zoomPercent;
    
    // Recreate font with new zoom
    m_font.reset(CreateEditorFont());
    if (m_hwndEdit && m_font.get()) {
        // Suppress EN_CHANGE so zoom changes don't mark the document modified
        DWORD oldMask = static_cast<DWORD>(SendMessageW(m_hwndEdit, EM_GETEVENTMASK, 0, 0));
        SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask & ~ENM_CHANGE);
        bool wasModified = IsModified();

        SendMessageW(m_hwndEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_font.get()), TRUE);
        ApplyCharFormat();

        SetModified(wasModified);
        SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask);
    }
}

//------------------------------------------------------------------------------
// Set word wrap
//------------------------------------------------------------------------------
void Editor::SetWordWrap(bool enable) {
    if (m_wordWrap != enable) {
        m_wordWrap = enable;
        RecreateControl();
    }
}

//------------------------------------------------------------------------------
// Set tab size
//------------------------------------------------------------------------------
void Editor::SetTabSize(int tabSize) noexcept {
    if (tabSize < 1) tabSize = 1;
    if (tabSize > 16) tabSize = 16;
    
    m_tabSize = tabSize;
    
    if (m_hwndEdit) {
        // Set tab stops in dialog units (about 4 dialog units per character)
        int tabStop = tabSize * 4;
        SendMessageW(m_hwndEdit, EM_SETTABSTOPS, 1, reinterpret_cast<LPARAM>(&tabStop));
        InvalidateRect(m_hwndEdit, nullptr, TRUE);
    }
}

//------------------------------------------------------------------------------
// Check if modified
//------------------------------------------------------------------------------
bool Editor::IsModified() const noexcept {
    if (m_hwndEdit) {
        return SendMessageW(m_hwndEdit, EM_GETMODIFY, 0, 0) != 0;
    }
    return false;
}

//------------------------------------------------------------------------------
// Set modified state
//------------------------------------------------------------------------------
void Editor::SetModified(bool modified) noexcept {
    if (m_hwndEdit) {
        SendMessageW(m_hwndEdit, EM_SETMODIFY, modified, 0);
    }
}

//------------------------------------------------------------------------------
// Set line ending type
//------------------------------------------------------------------------------
void Editor::SetLineEnding(LineEnding lineEnding) noexcept {
    m_lineEnding = lineEnding;
}

//------------------------------------------------------------------------------
// Set encoding
//------------------------------------------------------------------------------
void Editor::SetEncoding(TextEncoding encoding) noexcept {
    m_encoding = encoding;
}

//------------------------------------------------------------------------------
// Insert date/time at cursor
//------------------------------------------------------------------------------
void Editor::InsertDateTime() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    std::tm localTime = {};
#ifdef _WIN32
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif
    
    std::wostringstream ss;
    ss << std::put_time(&localTime, L"%H:%M %Y-%m-%d");
    
    PushUndoCheckpoint(EditAction::Other);
    ReplaceSelection(ss.str());
}

//------------------------------------------------------------------------------
// Set right-to-left reading order
//------------------------------------------------------------------------------
void Editor::SetRTL(bool rtl) noexcept {
    m_rtl = rtl;
    
    if (m_hwndEdit) {
        LONG_PTR exStyle = GetWindowLongPtrW(m_hwndEdit, GWL_EXSTYLE);
        if (rtl) {
            exStyle |= WS_EX_RTLREADING | WS_EX_RIGHT;
        } else {
            exStyle &= ~(WS_EX_RTLREADING | WS_EX_RIGHT);
        }
        SetWindowLongPtrW(m_hwndEdit, GWL_EXSTYLE, exStyle);
        InvalidateRect(m_hwndEdit, nullptr, TRUE);
    }
}

//------------------------------------------------------------------------------
// Get text length
//------------------------------------------------------------------------------
int Editor::GetTextLength() const noexcept {
    if (m_hwndEdit) {
        return GetWindowTextLengthW(m_hwndEdit);
    }
    return 0;
}

//------------------------------------------------------------------------------
// Get first visible line
//------------------------------------------------------------------------------
int Editor::GetFirstVisibleLine() const noexcept {
    if (m_hwndEdit) {
        return static_cast<int>(SendMessageW(m_hwndEdit, EM_GETFIRSTVISIBLELINE, 0, 0));
    }
    return 0;
}

//------------------------------------------------------------------------------
// Set scroll notification callback
//------------------------------------------------------------------------------
void Editor::SetScrollCallback(ScrollCallback callback, void* userData) noexcept {
    m_scrollCallback = callback;
    m_scrollCallbackData = userData;
}

//------------------------------------------------------------------------------
// Set scroll lines per wheel notch (0 = system default)
//------------------------------------------------------------------------------
void Editor::SetScrollLines(int lines) noexcept {
    m_scrollLines = (lines < 0) ? 0 : (lines > 20) ? 20 : lines;
}

//------------------------------------------------------------------------------
// Recreate edit control (for word wrap toggle)
//------------------------------------------------------------------------------
void Editor::RecreateControl() {
    if (!m_hwndEdit) return;
    
    // Save state
    std::wstring text = GetText();
    DWORD selStart, selEnd;
    GetSelection(selStart, selEnd);
    bool modified = IsModified();
    bool wasVisible = IsWindowVisible(m_hwndEdit) != FALSE;
    
    // Get current position
    RECT rect;
    GetWindowRect(m_hwndEdit, &rect);
    MapWindowPoints(HWND_DESKTOP, m_hwndParent, reinterpret_cast<LPPOINT>(&rect), 2);
    
    // Remove subclass before destroying
    RemoveWindowSubclass(m_hwndEdit, EditSubclassProc, EDIT_SUBCLASS_ID);
    DestroyWindow(m_hwndEdit);
    
    // Create new control with updated style (preserve visibility)
    DWORD style = WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_NOHIDESEL;
    if (wasVisible) style |= WS_VISIBLE;
    if (!m_wordWrap) {
        style |= ES_AUTOHSCROLL | WS_HSCROLL;
    }
    
    // Determine extended style (RTL support, no border)
    DWORD exStyle = 0;
    if (m_rtl) {
        exStyle |= WS_EX_RTLREADING | WS_EX_RIGHT;
    }
    
    m_hwndEdit = CreateWindowExW(
        exStyle,
        MSFTEDIT_CLASS,  // RichEdit 4.1 class
        L"",
        style,
        rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
        m_hwndParent,
        nullptr,
        m_hInstance,
        nullptr
    );
    
    if (m_hwndEdit) {
        // Set unlimited text (RichEdit uses different message)
        SendMessageW(m_hwndEdit, EM_EXLIMITTEXT, 0, 0x7FFFFFFE);
        
        // Disable built-in undo (we use our own multi-level system)
        SendMessageW(m_hwndEdit, EM_SETUNDOLIMIT, 0, 0);
        
        // Apply font
        if (m_font.get()) {
            SendMessageW(m_hwndEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_font.get()), TRUE);
            ApplyCharFormat();
        }
        
        // Set tab stops
        SetTabSize(m_tabSize);
        
        // Suppress EN_CHANGE during text restore
        DWORD eventMask = static_cast<DWORD>(SendMessageW(m_hwndEdit, EM_GETEVENTMASK, 0, 0));
        SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, eventMask & ~ENM_CHANGE);
        
        // Restore state
        SetWindowTextW(m_hwndEdit, text.c_str());
        SetSelection(selStart, selEnd);
        SetModified(modified);
        
        // Re-enable EN_CHANGE notifications
        SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, eventMask | ENM_CHANGE | ENM_SCROLL);

        // Re-subclass
        SetWindowSubclass(m_hwndEdit, EditSubclassProc, EDIT_SUBCLASS_ID,
                          reinterpret_cast<DWORD_PTR>(this));
        
        if (wasVisible) {
            ::SetFocus(m_hwndEdit);
        }
    }
}

//------------------------------------------------------------------------------
// Apply font to all text via CHARFORMAT (RichEdit-proper method)
// WM_SETFONT alone does not reliably set the font for all text in RichEdit.
//------------------------------------------------------------------------------
void Editor::ApplyCharFormat() {
    if (!m_hwndEdit) return;
    
    int fontSize = MulDiv(m_baseFontSize, m_zoomPercent, 100);
    if (fontSize < 6) fontSize = 6;
    if (fontSize > 144) fontSize = 144;
    
    CHARFORMAT2W cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_FACE | CFM_SIZE | CFM_WEIGHT | CFM_ITALIC | CFM_CHARSET;
    cf.yHeight = fontSize * 20;  // twips (1/20 of a point)
    cf.wWeight = static_cast<WORD>(m_fontWeight);
    cf.bCharSet = DEFAULT_CHARSET;
    if (m_fontItalic) cf.dwEffects |= CFE_ITALIC;
    wcsncpy_s(cf.szFaceName, m_fontName.c_str(), LF_FACESIZE - 1);
    
    // Set as default format and apply to all existing text
    SendMessageW(m_hwndEdit, EM_SETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&cf));
    SendMessageW(m_hwndEdit, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&cf));
}

//------------------------------------------------------------------------------
// Create HFONT from current settings
//------------------------------------------------------------------------------
HFONT Editor::CreateEditorFont() {
    // Calculate zoomed font size
    int fontSize = MulDiv(m_baseFontSize, m_zoomPercent, 100);
    if (fontSize < 6) fontSize = 6;
    if (fontSize > 144) fontSize = 144;
    
    // Get DPI for proper font scaling
    HDC hdc = GetDC(m_hwndEdit ? m_hwndEdit : m_hwndParent);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(m_hwndEdit ? m_hwndEdit : m_hwndParent, hdc);
    
    // Create font
    LOGFONTW lf = {};
    lf.lfHeight = -MulDiv(fontSize, dpi, 72);
    lf.lfWeight = m_fontWeight;
    lf.lfItalic = m_fontItalic ? TRUE : FALSE;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    wcsncpy_s(lf.lfFaceName, m_fontName.c_str(), LF_FACESIZE - 1);
    
    HFONT font = CreateFontIndirectW(&lf);
    
    // Fallback to Courier New if font creation failed
    if (!font) {
        wcsncpy_s(lf.lfFaceName, L"Courier New", LF_FACESIZE - 1);
        font = CreateFontIndirectW(&lf);
    }
    
    return font;
}

} // namespace QNote

