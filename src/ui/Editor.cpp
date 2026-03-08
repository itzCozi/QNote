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
    m_autoCompleteBraces = settings.autoCompleteBraces;
    
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
    
    // Enable EN_CHANGE, EN_SCROLL, and EN_LINK notifications
    DWORD eventMask = static_cast<DWORD>(SendMessageW(m_hwndEdit, EM_GETEVENTMASK, 0, 0));
    SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, eventMask | ENM_CHANGE | ENM_SCROLL | ENM_LINK);

    // Detect and underline URLs automatically
    SendMessageW(m_hwndEdit, EM_AUTOURLDETECT, AURL_ENABLEURL, 0);
    
    // Create and set font
    m_font.reset(CreateEditorFont());
    if (m_font.get()) {
        SendMessageW(m_hwndEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_font.get()), TRUE);
        ApplyCharFormat();
    }
    
    // Set tab stops
    SetTabSize(m_tabSize);
    
    // Add a small left inset so text doesn't sit flush against the gutter border
    ApplyTextInset();
    
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
        // Re-apply left text inset after resize (EM_SETRECT is size-dependent)
        ApplyTextInset();
        // Re-highlight after resize since visible line range may have changed
        if (m_syntaxHighlightEnabled && m_language != Language::None) {
            m_syntaxDirty = true;
            ApplySyntaxHighlighting();
        }
    }
}

//------------------------------------------------------------------------------
// Apply left text inset so text doesn't sit flush against the gutter border
//------------------------------------------------------------------------------
void Editor::ApplyTextInset() noexcept {
    if (!m_hwndEdit) return;
    RECT rc;
    GetClientRect(m_hwndEdit, &rc);
    rc.left += 6;   // 6 pixels of padding on the left
    SendMessageW(m_hwndEdit, EM_SETRECT, 0, reinterpret_cast<LPARAM>(&rc));
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
// Stream-set large text in chunks so the UI stays responsive.
// Clears the control, then appends text in ~2 MB wide-char slices with
// message-loop pumping between each slice.
//------------------------------------------------------------------------------
void Editor::SetTextStreamed(const std::wstring& text, HWND hwndStatus) {
    if (!m_hwndEdit) return;

    // Suppress EN_CHANGE for the entire load
    DWORD oldMask = static_cast<DWORD>(SendMessageW(m_hwndEdit, EM_GETEVENTMASK, 0, 0));
    SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask & ~ENM_CHANGE);

    // Disable redraw during the load
    SendMessageW(m_hwndEdit, WM_SETREDRAW, FALSE, 0);

    // Clear existing content
    SetWindowTextW(m_hwndEdit, L"");

    // Append in slices
    static constexpr size_t SLICE_CHARS = 2 * 1024 * 1024 / sizeof(wchar_t); // ~2 MB
    size_t totalChars = text.size();
    size_t offset = 0;
    int lastPercent = -1;

    while (offset < totalChars) {
        size_t len = (std::min)(SLICE_CHARS, totalChars - offset);

        // Move caret to end
        CHARRANGE cr;
        cr.cpMin = -1;
        cr.cpMax = -1;
        SendMessageW(m_hwndEdit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&cr));

        // Build a null-terminated slice
        std::wstring slice(text.data() + offset, len);
        SendMessageW(m_hwndEdit, EM_REPLACESEL, FALSE,
                     reinterpret_cast<LPARAM>(slice.c_str()));
        offset += len;

        // Status bar progress
        int pct = static_cast<int>(offset * 100 / totalChars);
        if (hwndStatus && pct != lastPercent) {
            lastPercent = pct;
            wchar_t buf[64];
            swprintf_s(buf, L"Rendering... %d%%", pct);
            SendMessageW(hwndStatus, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(buf));
        }

        // Pump messages
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    // Re-enable redraw and repaint
    SendMessageW(m_hwndEdit, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_hwndEdit, nullptr, TRUE);

    ApplyCharFormat();
    SetModified(false);
    SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask);

    ClearUndoHistory();
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
// Can undo (custom stack — incremental/diff-based)
//------------------------------------------------------------------------------
bool Editor::CanUndo() const noexcept {
    return !m_undoStack.empty() || m_hasPendingSnapshot;
}

//------------------------------------------------------------------------------
// Can redo (custom stack)
//------------------------------------------------------------------------------
bool Editor::CanRedo() const noexcept {
    return !m_redoStack.empty();
}

//------------------------------------------------------------------------------
// Compute the minimal diff between two strings.
// Finds the common prefix and common suffix, yielding the changed region.
//------------------------------------------------------------------------------
void Editor::ComputeDelta(const std::wstring& before, const std::wstring& after,
                          DWORD& pos, std::wstring& removedText,
                          std::wstring& insertedText) {
    size_t minLen = (std::min)(before.size(), after.size());

    // Common prefix
    size_t prefixLen = 0;
    while (prefixLen < minLen && before[prefixLen] == after[prefixLen]) {
        ++prefixLen;
    }

    // Common suffix (not overlapping with prefix)
    size_t suffixLen = 0;
    size_t maxSuffix = minLen - prefixLen;
    while (suffixLen < maxSuffix &&
           before[before.size() - 1 - suffixLen] == after[after.size() - 1 - suffixLen]) {
        ++suffixLen;
    }

    pos = static_cast<DWORD>(prefixLen);
    removedText = before.substr(prefixLen, before.size() - prefixLen - suffixLen);
    insertedText = after.substr(prefixLen, after.size() - prefixLen - suffixLen);
}

//------------------------------------------------------------------------------
// Finalize the pending snapshot: diff it against the current editor text
// and push a compact UndoDelta onto the undo stack.
//------------------------------------------------------------------------------
void Editor::FinalizePendingSnapshot() {
    if (!m_hasPendingSnapshot || !m_hwndEdit) return;

    std::wstring currentText = GetText();

    // If nothing changed, just discard the pending snapshot
    if (currentText == m_pendingSnapshot) {
        m_hasPendingSnapshot = false;
        m_pendingSnapshot.clear();
        m_pendingSnapshot.shrink_to_fit();
        return;
    }

    // Compute the minimal diff
    UndoDelta delta;
    ComputeDelta(m_pendingSnapshot, currentText,
                 delta.changePos, delta.oldText, delta.newText);

    delta.selStartBefore = m_pendingSelStart;
    delta.selEndBefore   = m_pendingSelEnd;
    delta.firstVisibleLineBefore = m_pendingFirstVisibleLine;

    // "After" state is the current editor state
    GetSelection(delta.selStartAfter, delta.selEndAfter);
    delta.firstVisibleLineAfter = GetFirstVisibleLine();

    size_t deltaBytes = (delta.oldText.size() + delta.newText.size()) * sizeof(wchar_t);
    m_undoMemoryUsage += deltaBytes;
    m_undoStack.push_back(std::move(delta));

    // Enforce max undo levels and memory cap
    while (m_undoStack.size() > 1 &&
           (static_cast<int>(m_undoStack.size()) > MAX_UNDO_LEVELS ||
            m_undoMemoryUsage > MAX_UNDO_MEMORY_BYTES)) {
        auto& front = m_undoStack.front();
        m_undoMemoryUsage -= (front.oldText.size() + front.newText.size()) * sizeof(wchar_t);
        m_undoStack.erase(m_undoStack.begin());
    }

    // Clear the pending snapshot — it's been consumed
    m_hasPendingSnapshot = false;
    m_pendingSnapshot.clear();
    m_pendingSnapshot.shrink_to_fit();
}

//------------------------------------------------------------------------------
// Undo - apply the top delta in reverse via targeted replacement
//------------------------------------------------------------------------------
void Editor::Undo() {
    if (!m_hwndEdit) return;

    // Finalize any pending snapshot so it becomes an undoable delta
    FinalizePendingSnapshot();

    if (m_undoStack.empty()) return;

    m_suppressUndo = true;

    // Pop from undo stack
    UndoDelta delta = std::move(m_undoStack.back());
    size_t deltaBytes = (delta.oldText.size() + delta.newText.size()) * sizeof(wchar_t);
    m_undoMemoryUsage -= deltaBytes;
    m_undoStack.pop_back();

    // Suppress EN_CHANGE during text restoration and prevent flicker
    DWORD oldMask = static_cast<DWORD>(SendMessageW(m_hwndEdit, EM_GETEVENTMASK, 0, 0));
    SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask & ~ENM_CHANGE);
    SendMessageW(m_hwndEdit, WM_SETREDRAW, FALSE, 0);

    // Targeted replacement: select the range that was inserted, replace
    // with the old text.  Positions from ComputeDelta are in GetText()
    // string-index space which matches RichEdit 4.1 character positions
    // (both use \r for line breaks).
    DWORD replaceEnd = delta.changePos + static_cast<DWORD>(delta.newText.size());
    SendMessageW(m_hwndEdit, EM_SETSEL, delta.changePos, replaceEnd);
    SendMessageW(m_hwndEdit, EM_REPLACESEL, FALSE,
                 reinterpret_cast<LPARAM>(delta.oldText.c_str()));

    // Restore selection to "before" state
    SendMessageW(m_hwndEdit, EM_SETSEL, delta.selStartBefore, delta.selEndBefore);

    // Restore scroll position
    int currentFirst = GetFirstVisibleLine();
    if (delta.firstVisibleLineBefore != currentFirst) {
        SendMessageW(m_hwndEdit, EM_LINESCROLL, 0,
                     delta.firstVisibleLineBefore - currentFirst);
    }
    SendMessageW(m_hwndEdit, EM_SCROLLCARET, 0, 0);

    // Re-enable redraw and notifications
    SendMessageW(m_hwndEdit, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_hwndEdit, nullptr, FALSE);
    SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask);

    SetModified(true);

    // Re-apply syntax highlighting around the changed region
    if (m_syntaxHighlightEnabled && m_language != Language::None) {
        m_syntaxDirty = true;
        m_highlightedFirstLine = -1;
        m_highlightedLastLine = -1;
        ApplySyntaxHighlighting();
    }

    // Push to redo stack (same delta — direction is determined by the stack)
    m_redoMemoryUsage += deltaBytes;
    m_redoStack.push_back(std::move(delta));

    // Enforce redo stack memory cap
    while (m_redoStack.size() > 1 && m_redoMemoryUsage > MAX_UNDO_MEMORY_BYTES) {
        auto& front = m_redoStack.front();
        m_redoMemoryUsage -= (front.oldText.size() + front.newText.size()) * sizeof(wchar_t);
        m_redoStack.erase(m_redoStack.begin());
    }

    m_lastEditAction = EditAction::None;
    m_suppressUndo = false;

    // Notify scroll callback for line numbers update
    if (m_scrollCallback) {
        m_scrollCallback(m_scrollCallbackData);
    }
}

//------------------------------------------------------------------------------
// Redo - apply the top delta forward via targeted replacement
//------------------------------------------------------------------------------
void Editor::Redo() {
    if (m_redoStack.empty() || !m_hwndEdit) return;

    m_suppressUndo = true;

    // Pop from redo stack
    UndoDelta delta = std::move(m_redoStack.back());
    size_t deltaBytes = (delta.oldText.size() + delta.newText.size()) * sizeof(wchar_t);
    m_redoMemoryUsage -= deltaBytes;
    m_redoStack.pop_back();

    // Suppress EN_CHANGE during text restoration and prevent flicker
    DWORD oldMask = static_cast<DWORD>(SendMessageW(m_hwndEdit, EM_GETEVENTMASK, 0, 0));
    SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask & ~ENM_CHANGE);
    SendMessageW(m_hwndEdit, WM_SETREDRAW, FALSE, 0);

    // Targeted replacement: select the range with old text, replace with new
    DWORD replaceEnd = delta.changePos + static_cast<DWORD>(delta.oldText.size());
    SendMessageW(m_hwndEdit, EM_SETSEL, delta.changePos, replaceEnd);
    SendMessageW(m_hwndEdit, EM_REPLACESEL, FALSE,
                 reinterpret_cast<LPARAM>(delta.newText.c_str()));

    // Restore selection to "after" state
    SendMessageW(m_hwndEdit, EM_SETSEL, delta.selStartAfter, delta.selEndAfter);

    // Restore scroll position
    int currentFirst = GetFirstVisibleLine();
    if (delta.firstVisibleLineAfter != currentFirst) {
        SendMessageW(m_hwndEdit, EM_LINESCROLL, 0,
                     delta.firstVisibleLineAfter - currentFirst);
    }
    SendMessageW(m_hwndEdit, EM_SCROLLCARET, 0, 0);

    // Re-enable redraw and notifications
    SendMessageW(m_hwndEdit, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_hwndEdit, nullptr, FALSE);
    SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask);

    SetModified(true);

    // Re-apply syntax highlighting around the changed region
    if (m_syntaxHighlightEnabled && m_language != Language::None) {
        m_syntaxDirty = true;
        m_highlightedFirstLine = -1;
        m_highlightedLastLine = -1;
        ApplySyntaxHighlighting();
    }

    // Push to undo stack (same delta)
    m_undoMemoryUsage += deltaBytes;
    m_undoStack.push_back(std::move(delta));

    m_lastEditAction = EditAction::None;
    m_suppressUndo = false;

    // Notify scroll callback for line numbers update
    if (m_scrollCallback) {
        m_scrollCallback(m_scrollCallbackData);
    }
}

//------------------------------------------------------------------------------
// Push undo checkpoint - captures state before a change (lazy diff)
// Groups consecutive same-type edits together like VSCode:
// - Consecutive typing groups until: enter, pause >2s, or cursor jump
// - Consecutive deleting groups until: pause >2s or cursor jump
// - "Other" actions (paste, cut, line ops) always start a new group
//
// Instead of storing the full document text, we capture a pending snapshot.
// The snapshot is later diffed against the post-edit text to produce a
// compact UndoDelta (see FinalizePendingSnapshot).
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

    // Finalize the previous pending snapshot into a delta before starting
    // a new group (this diffs the old snapshot against the current text).
    FinalizePendingSnapshot();

    // Capture a new pending snapshot (the "before" state for this group)
    m_pendingSnapshot = GetText();
    GetSelection(m_pendingSelStart, m_pendingSelEnd);
    m_pendingFirstVisibleLine = GetFirstVisibleLine();
    m_hasPendingSnapshot = true;

    // Clear redo stack on new edit
    m_redoStack.clear();
    m_redoMemoryUsage = 0;
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
    m_redoMemoryUsage = 0;
    m_lastEditAction = EditAction::None;
    m_lastEditTime = 0;
    m_hasPendingSnapshot = false;
    m_pendingSnapshot.clear();
    m_pendingSnapshot.shrink_to_fit();
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
        // Save scroll position so zoom doesn't jump to cursor
        int firstVisibleLine = GetFirstVisibleLine();

        // Suppress EN_CHANGE so zoom changes don't mark the document modified
        DWORD oldMask = static_cast<DWORD>(SendMessageW(m_hwndEdit, EM_GETEVENTMASK, 0, 0));
        SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask & ~ENM_CHANGE);
        bool wasModified = IsModified();

        // Suppress redraws to prevent the control from scrolling to the caret
        SendMessageW(m_hwndEdit, WM_SETREDRAW, FALSE, 0);

        SendMessageW(m_hwndEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_font.get()), FALSE);
        ApplyCharFormat();

        // Re-apply syntax highlighting to visible text only (fast path).
        // ApplyCharFormat resets all colors, so we just need to re-color
        // what's on screen.  A full-file pass is too expensive during zoom.
        if (m_syntaxHighlightEnabled && m_language != Language::None) {
            m_syntaxDirty = true;
            m_highlightedFirstLine = -1;
            m_highlightedLastLine = -1;
            ApplySyntaxHighlighting(false);  // visible chunk only
        }

        // Restore scroll position before re-enabling redraws
        int currentFirst = GetFirstVisibleLine();
        if (currentFirst != firstVisibleLine) {
            SendMessageW(m_hwndEdit, EM_LINESCROLL, 0, firstVisibleLine - currentFirst);
        }

        SendMessageW(m_hwndEdit, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(m_hwndEdit, nullptr, TRUE);

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
// Get cached word count (recomputes only when text has changed)
//------------------------------------------------------------------------------
int Editor::GetWordCount() {
    if (!m_wordCountDirty) {
        return m_cachedWordCount;
    }
    m_wordCountDirty = false;

    if (!m_hwndEdit) {
        m_cachedWordCount = 0;
        return 0;
    }

    int lineCount = GetLineCount();
    int wordCount = 0;
    bool inWord = false;

    for (int i = 0; i < lineCount; ++i) {
        // EM_GETLINE: first word of buffer = max chars
        wchar_t buf[4096];
        *reinterpret_cast<WORD*>(buf) = static_cast<WORD>(sizeof(buf) / sizeof(wchar_t) - 1);
        int len = static_cast<int>(SendMessageW(m_hwndEdit, EM_GETLINE, i, reinterpret_cast<LPARAM>(buf)));
        for (int j = 0; j < len; ++j) {
            wchar_t c = buf[j];
            if (c == L' ' || c == L'\t' || c == L'\r' || c == L'\n') {
                inWord = false;
            } else if (!inWord) {
                inWord = true;
                ++wordCount;
            }
        }
        inWord = false;  // line boundary = word break
    }

    m_cachedWordCount = wordCount;
    return wordCount;
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

//------------------------------------------------------------------------------
// Set syntax highlighting enabled
//------------------------------------------------------------------------------
void Editor::SetSyntaxHighlighting(bool enable) {
    m_syntaxHighlightEnabled = enable;
    m_syntaxDirty = true;
    m_highlightedFirstLine = -1;
    m_highlightedLastLine = -1;
    if (enable) {
        m_language = SyntaxHighlighter::DetectLanguage(m_filePath);
        ApplySyntaxHighlighting(true);
    } else {
        m_language = Language::None;
        // Reset all text to default color
        if (m_hwndEdit) {
            DWORD oldMask = static_cast<DWORD>(SendMessageW(m_hwndEdit, EM_GETEVENTMASK, 0, 0));
            SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask & ~ENM_CHANGE);

            // Select all and clear syntax colors (restore auto-color)
            CHARRANGE savedSel;
            SendMessageW(m_hwndEdit, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&savedSel));
            SendMessageW(m_hwndEdit, WM_SETREDRAW, FALSE, 0);

            CHARRANGE allRange = { 0, -1 };
            SendMessageW(m_hwndEdit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&allRange));

            CHARFORMAT2W cf = {};
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_COLOR;
            cf.dwEffects = CFE_AUTOCOLOR;
            SendMessageW(m_hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));

            SendMessageW(m_hwndEdit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&savedSel));
            SendMessageW(m_hwndEdit, WM_SETREDRAW, TRUE, 0);
            InvalidateRect(m_hwndEdit, nullptr, FALSE);

            SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask);
        }
    }
}

//------------------------------------------------------------------------------
// Set file path (for language detection)
//------------------------------------------------------------------------------
void Editor::SetFilePath(std::wstring_view filePath) {
    m_filePath = filePath;
    Language newLang = SyntaxHighlighter::DetectLanguage(m_filePath);
    if (newLang != m_language) {
        m_language = newLang;
        m_syntaxDirty = true;
        m_highlightedFirstLine = -1;
        m_highlightedLastLine = -1;
        if (m_syntaxHighlightEnabled) {
            ApplySyntaxHighlighting(true);
        }
    }
}

//------------------------------------------------------------------------------
// Schedule syntax highlighting (debounced via timer)
//------------------------------------------------------------------------------
void Editor::ScheduleSyntaxHighlighting() {
    if (!m_syntaxHighlightEnabled || m_language == Language::None || !m_hwndEdit) return;
    m_syntaxDirty = true;
    // Invalidate the line cache so the next pass re-highlights all visible text
    m_highlightedFirstLine = -1;
    m_highlightedLastLine = -1;
    // Debounce: 50ms batches rapid events (keystrokes, thumb scrolling)
    SetTimer(m_hwndEdit, TIMER_SYNTAXHIGHLIGHT, 50, nullptr);
}

//------------------------------------------------------------------------------
// Apply syntax highlighting to visible text
// Highlights in 100-line chunks with midpoint pre-fetch: when the viewport
// scrolls past the halfway mark of the current chunk, the next chunk is loaded.
//------------------------------------------------------------------------------
static constexpr int HIGHLIGHT_CHUNK_LINES = 100;

void Editor::ApplySyntaxHighlighting(bool fullFile) {
    if (!m_hwndEdit || !m_syntaxHighlightEnabled || m_language == Language::None) return;
    if (!m_syntaxDirty) return;
    m_syntaxDirty = false;

    // Kill any pending timer
    KillTimer(m_hwndEdit, TIMER_SYNTAXHIGHLIGHT);

    int firstLine = GetFirstVisibleLine();
    int totalLines = GetLineCount();

    int chunkFirst, chunkLast;

    if (fullFile) {
        // Highlight the entire file (used on open, tab switch, toggle)
        chunkFirst = 0;
        chunkLast = totalLines - 1;
    } else {
        // Chunked mode for scrolling: 100 lines from the current viewport
        // with midpoint pre-fetch to reduce re-highlight frequency.
        if (m_highlightedFirstLine >= 0 && m_highlightedLastLine >= 0) {
            int chunkSize = m_highlightedLastLine - m_highlightedFirstLine + 1;
            int midpoint = m_highlightedFirstLine + chunkSize / 2;
            bool viewportCovered = (firstLine >= m_highlightedFirstLine && firstLine < midpoint);
            if (viewportCovered) {
                return;  // Still in the first half of the chunk — no work needed
            }
        }
        chunkFirst = firstLine;
        chunkLast = (std::min)(firstLine + HIGHLIGHT_CHUNK_LINES - 1, totalLines - 1);
    }

    // Suppress EN_CHANGE and redraw during formatting.
    // Also save the modify flag — EM_SETCHARFORMAT marks the control as
    // modified even though we're only changing colors, not text content.
    DWORD oldMask = static_cast<DWORD>(SendMessageW(m_hwndEdit, EM_GETEVENTMASK, 0, 0));
    SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask & ~ENM_CHANGE);
    bool wasModified = IsModified();
    SendMessageW(m_hwndEdit, WM_SETREDRAW, FALSE, 0);

    // Save current selection and scroll position
    CHARRANGE savedSel;
    SendMessageW(m_hwndEdit, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&savedSel));
    POINT savedScroll;
    SendMessageW(m_hwndEdit, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&savedScroll));

    // Get text range for the chunk
    int rangeStart = GetLineIndex(chunkFirst);
    int rangeEnd = GetLineIndex(chunkLast) + GetLineLength(chunkLast);
    if (rangeEnd <= rangeStart) {
        SendMessageW(m_hwndEdit, WM_SETREDRAW, TRUE, 0);
        SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask);
        return;
    }

    int textLen = rangeEnd - rangeStart;
    std::vector<wchar_t> buf(textLen + 1, 0);
    TEXTRANGEW tr = {};
    tr.chrg.cpMin = rangeStart;
    tr.chrg.cpMax = rangeEnd;
    tr.lpstrText = buf.data();
    SendMessageW(m_hwndEdit, EM_GETTEXTRANGE, 0, reinterpret_cast<LPARAM>(&tr));

    std::wstring_view visibleText(buf.data(), textLen);

    // Reset chunk range to default color first
    CHARRANGE resetRange = { rangeStart, rangeEnd };
    SendMessageW(m_hwndEdit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&resetRange));
    
    CHARFORMAT2W cfDefault = {};
    cfDefault.cbSize = sizeof(cfDefault);
    cfDefault.dwMask = CFM_COLOR;
    cfDefault.dwEffects = 0; // clear CFE_AUTOCOLOR
    cfDefault.crTextColor = LightPlusColors::Default;
    SendMessageW(m_hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cfDefault));

    // Tokenize
    auto tokens = m_syntaxHighlighter.Tokenize(visibleText, m_language, rangeStart);

    // Apply colors, coalescing adjacent tokens with the same color into
    // single EM_SETCHARFORMAT calls.  This reduces the number of Win32
    // round-trips from O(tokens) to O(color-runs), a significant speedup
    // on large files and slower machines.
    CHARFORMAT2W cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.dwEffects = 0;

    // Sort tokens by position (they should already be in order, but ensure it)
    // Then merge adjacent tokens with the same color.
    COLORREF currentColor = 0;
    int runStart = -1;
    int runEnd = -1;

    auto flushRun = [&]() {
        if (runStart >= 0 && runEnd > runStart) {
            CHARRANGE tokenRange = { runStart, runEnd };
            SendMessageW(m_hwndEdit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&tokenRange));
            cf.crTextColor = currentColor;
            SendMessageW(m_hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));
        }
        runStart = -1;
        runEnd = -1;
    };

    for (const auto& token : tokens) {
        if (token.length <= 0) continue;
        // Skip tokens that use the default color -- the chunk was
        // already reset to Default above, so re-applying it is wasted work.
        if (token.type == TokenType::Default || token.type == TokenType::Punctuation ||
            token.type == TokenType::Operator) {
            flushRun();
            continue;
        }

        COLORREF color = SyntaxHighlighter::GetTokenColor(token.type);
        int tokenEnd = token.start + token.length;

        // Coalesce with current run if same color and adjacent/contiguous
        if (color == currentColor && token.start == runEnd) {
            runEnd = tokenEnd;
        } else {
            flushRun();
            currentColor = color;
            runStart = token.start;
            runEnd = tokenEnd;
        }
    }
    flushRun();

    // Update cache with the chunk we just highlighted
    m_highlightedFirstLine = chunkFirst;
    m_highlightedLastLine = chunkLast;

    // Restore selection and scroll position
    SendMessageW(m_hwndEdit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&savedSel));
    SendMessageW(m_hwndEdit, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&savedScroll));

    // Re-enable redraw and repaint
    SendMessageW(m_hwndEdit, WM_SETREDRAW, TRUE, 0);
    // Use RedrawWindow with RDW_UPDATENOW to force an immediate synchronous
    // repaint after WM_SETREDRAW toggling.  Without RDW_UPDATENOW, RichEdit
    // defers painting and colors don't appear until the next scroll event.
    RedrawWindow(m_hwndEdit, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);

    // Restore the modify flag so formatting-only changes don't mark the
    // document as user-modified, then re-enable EN_CHANGE.
    SetModified(wasModified);
    SendMessageW(m_hwndEdit, EM_SETEVENTMASK, 0, oldMask);

    // Sync line-number gutter after highlighting (scroll position is now final)
    if (m_scrollCallback) {
        m_scrollCallback(m_scrollCallbackData);
    }
}

} // namespace QNote

