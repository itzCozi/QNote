//==============================================================================
// QNote - A Lightweight Notepad Clone
// EditorLineOps.cpp - Line manipulation operations
//==============================================================================

#include "Editor.h"

namespace QNote {

//------------------------------------------------------------------------------
// Get text of a specific line (0-based, without line ending)
//------------------------------------------------------------------------------
std::wstring Editor::GetLineText(int line) const {
    if (!m_hwndEdit) return L"";
    int lineLen = GetLineLength(line);
    if (lineLen == 0) return L"";
    
    std::vector<wchar_t> buf(lineLen + 2, 0);
    *reinterpret_cast<WORD*>(buf.data()) = static_cast<WORD>(lineLen + 1);
    SendMessageW(m_hwndEdit, EM_GETLINE, line, reinterpret_cast<LPARAM>(buf.data()));
    return std::wstring(buf.data(), lineLen);
}

//------------------------------------------------------------------------------
// Delete current line(s) - Ctrl+Shift+K
//------------------------------------------------------------------------------
void Editor::DeleteLine() {
    if (!m_hwndEdit) return;
    
    PushUndoCheckpoint(EditAction::Other);
    
    DWORD selStart, selEnd;
    GetSelection(selStart, selEnd);
    
    int startLine = GetLineFromChar(selStart);
    int endLine = GetLineFromChar(selEnd);
    int lineCount = GetLineCount();
    
    // Adjust if selection ends at the very start of a line
    if (selEnd > selStart && selEnd == static_cast<DWORD>(GetLineIndex(endLine)) && endLine > startLine) {
        endLine--;
    }
    
    int blockStart = GetLineIndex(startLine);
    
    if (endLine + 1 < lineCount) {
        SetSelection(blockStart, GetLineIndex(endLine + 1));
    } else if (startLine > 0) {
        int prevEnd = GetLineIndex(startLine - 1) + GetLineLength(startLine - 1);
        SetSelection(prevEnd, GetTextLength());
    } else {
        SetSelection(0, GetTextLength());
    }
    
    ReplaceSelection(L"");
}

//------------------------------------------------------------------------------
// Duplicate current line(s) below - Ctrl+D / Alt+Shift+Down
//------------------------------------------------------------------------------
void Editor::DuplicateLine() {
    if (!m_hwndEdit) return;
    
    PushUndoCheckpoint(EditAction::Other);
    
    DWORD selStart, selEnd;
    GetSelection(selStart, selEnd);
    
    int startLine = GetLineFromChar(selStart);
    int endLine = GetLineFromChar(selEnd);
    int lineCount = GetLineCount();
    
    if (selEnd > selStart && selEnd == static_cast<DWORD>(GetLineIndex(endLine)) && endLine > startLine) {
        endLine--;
    }
    
    // Read block text via temporary selection
    int blockStart = GetLineIndex(startLine);
    int blockEnd = (endLine + 1 < lineCount) ? GetLineIndex(endLine + 1) : GetTextLength();
    
    SetSelection(blockStart, blockEnd);
    std::wstring blockText = GetSelectedText();
    SetSelection(selStart, selEnd);
    
    bool isLastLine = (endLine + 1 >= lineCount);
    
    // Insert duplicate after block
    SetSelection(blockEnd, blockEnd);
    if (isLastLine) {
        ReplaceSelection(L"\r" + blockText);
    } else {
        ReplaceSelection(blockText);
    }
    
    // Position cursor in the duplicate at same relative offset
    int newBlockStart = blockEnd + (isLastLine ? 1 : 0);
    int relStart = static_cast<int>(selStart) - blockStart;
    int relEnd = static_cast<int>(selEnd) - blockStart;
    SetSelection(newBlockStart + relStart, newBlockStart + relEnd);
}

//------------------------------------------------------------------------------
// Move current line(s) up - Alt+Up
//------------------------------------------------------------------------------
void Editor::MoveLineUp() {
    if (!m_hwndEdit) return;
    
    PushUndoCheckpoint(EditAction::Other);
    
    DWORD selStart, selEnd;
    GetSelection(selStart, selEnd);
    
    int startLine = GetLineFromChar(selStart);
    int endLine = GetLineFromChar(selEnd);
    int lineCount = GetLineCount();
    
    if (selEnd > selStart && selEnd == static_cast<DWORD>(GetLineIndex(endLine)) && endLine > startLine) {
        endLine--;
    }
    
    if (startLine == 0) return;
    
    int prevLineStart = GetLineIndex(startLine - 1);
    int blockStart = GetLineIndex(startLine);
    int blockEnd = (endLine + 1 < lineCount) ? GetLineIndex(endLine + 1) : GetTextLength();
    
    // Read the entire range (previous line + block)
    SetSelection(prevLineStart, blockEnd);
    std::wstring entireRange = GetSelectedText();
    
    int splitPos = blockStart - prevLineStart;
    std::wstring prevText = entireRange.substr(0, splitPos);
    std::wstring blockText = entireRange.substr(splitPos);
    
    // Adjust newlines when block is at the end of file
    if (endLine + 1 >= lineCount) {
        if (!blockText.empty() && blockText.back() != L'\r') blockText += L'\r';
        if (!prevText.empty() && prevText.back() == L'\r') prevText.pop_back();
    }
    
    ReplaceSelection(blockText + prevText);
    
    // Restore selection on moved block
    int relStart = static_cast<int>(selStart) - blockStart;
    int relEnd = static_cast<int>(selEnd) - blockStart;
    SetSelection(prevLineStart + relStart, prevLineStart + relEnd);
}

//------------------------------------------------------------------------------
// Move current line(s) down - Alt+Down
//------------------------------------------------------------------------------
void Editor::MoveLineDown() {
    if (!m_hwndEdit) return;
    
    PushUndoCheckpoint(EditAction::Other);
    
    DWORD selStart, selEnd;
    GetSelection(selStart, selEnd);
    
    int startLine = GetLineFromChar(selStart);
    int endLine = GetLineFromChar(selEnd);
    int lineCount = GetLineCount();
    
    if (selEnd > selStart && selEnd == static_cast<DWORD>(GetLineIndex(endLine)) && endLine > startLine) {
        endLine--;
    }
    
    if (endLine >= lineCount - 1) return;
    
    int blockStart = GetLineIndex(startLine);
    int blockEnd = GetLineIndex(endLine + 1);
    int nextLineEnd = (endLine + 2 < lineCount) ? GetLineIndex(endLine + 2) : GetTextLength();
    
    // Read the entire range (block + next line)
    SetSelection(blockStart, nextLineEnd);
    std::wstring entireRange = GetSelectedText();
    
    int splitPos = blockEnd - blockStart;
    std::wstring blockText = entireRange.substr(0, splitPos);
    std::wstring nextText = entireRange.substr(splitPos);
    
    // Adjust newlines when next line is the last
    if (endLine + 2 >= lineCount) {
        if (!nextText.empty() && nextText.back() != L'\r') nextText += L'\r';
        if (!blockText.empty() && blockText.back() == L'\r') blockText.pop_back();
    }
    
    ReplaceSelection(nextText + blockText);
    
    // Restore selection on moved block
    int newBlockStart = blockStart + static_cast<int>(nextText.length());
    int relStart = static_cast<int>(selStart) - blockStart;
    int relEnd = static_cast<int>(selEnd) - blockStart;
    SetSelection(newBlockStart + relStart, newBlockStart + relEnd);
}

//------------------------------------------------------------------------------
// Copy current line(s) up - Alt+Shift+Up
//------------------------------------------------------------------------------
void Editor::CopyLineUp() {
    if (!m_hwndEdit) return;
    
    PushUndoCheckpoint(EditAction::Other);
    
    DWORD selStart, selEnd;
    GetSelection(selStart, selEnd);
    
    int startLine = GetLineFromChar(selStart);
    int endLine = GetLineFromChar(selEnd);
    int lineCount = GetLineCount();
    
    if (selEnd > selStart && selEnd == static_cast<DWORD>(GetLineIndex(endLine)) && endLine > startLine) {
        endLine--;
    }
    
    int blockStart = GetLineIndex(startLine);
    int blockEnd = (endLine + 1 < lineCount) ? GetLineIndex(endLine + 1) : GetTextLength();
    
    SetSelection(blockStart, blockEnd);
    std::wstring blockText = GetSelectedText();
    
    bool endsWithNewline = !blockText.empty() && blockText.back() == L'\r';
    
    // Insert copy above the block
    SetSelection(blockStart, blockStart);
    ReplaceSelection(endsWithNewline ? blockText : (blockText + L"\r"));
    
    // Cursor stays at original relative position (now on the upper copy)
    int relStart = static_cast<int>(selStart) - blockStart;
    int relEnd = static_cast<int>(selEnd) - blockStart;
    SetSelection(blockStart + relStart, blockStart + relEnd);
}

//------------------------------------------------------------------------------
// Copy current line(s) down - Alt+Shift+Down
//------------------------------------------------------------------------------
void Editor::CopyLineDown() {
    if (!m_hwndEdit) return;
    
    PushUndoCheckpoint(EditAction::Other);
    
    DWORD selStart, selEnd;
    GetSelection(selStart, selEnd);
    
    int startLine = GetLineFromChar(selStart);
    int endLine = GetLineFromChar(selEnd);
    int lineCount = GetLineCount();
    
    if (selEnd > selStart && selEnd == static_cast<DWORD>(GetLineIndex(endLine)) && endLine > startLine) {
        endLine--;
    }
    
    int blockStart = GetLineIndex(startLine);
    int blockEnd = (endLine + 1 < lineCount) ? GetLineIndex(endLine + 1) : GetTextLength();
    
    SetSelection(blockStart, blockEnd);
    std::wstring blockText = GetSelectedText();
    
    bool isLastLine = (endLine + 1 >= lineCount);
    
    // Insert duplicate after block
    SetSelection(blockEnd, blockEnd);
    if (isLastLine) {
        ReplaceSelection(L"\r" + blockText);
    } else {
        ReplaceSelection(blockText);
    }
    
    // Position cursor on the lower copy
    int newBlockStart = blockEnd + (isLastLine ? 1 : 0);
    int relStart = static_cast<int>(selStart) - blockStart;
    int relEnd = static_cast<int>(selEnd) - blockStart;
    SetSelection(newBlockStart + relStart, newBlockStart + relEnd);
}

//------------------------------------------------------------------------------
// Insert blank line below with auto-indent - Ctrl+Enter
//------------------------------------------------------------------------------
void Editor::InsertLineBelow() {
    if (!m_hwndEdit) return;
    
    PushUndoCheckpoint(EditAction::Other);
    
    int curLine = GetCurrentLine();
    int lineStart = GetLineIndex(curLine);
    int lineLen = GetLineLength(curLine);
    
    // Copy leading whitespace for auto-indent
    std::wstring lineText = GetLineText(curLine);
    std::wstring indent;
    for (wchar_t ch : lineText) {
        if (ch == L' ' || ch == L'\t') indent += ch;
        else break;
    }
    
    // Insert newline + indent at end of current line
    SetSelection(lineStart + lineLen, lineStart + lineLen);
    ReplaceSelection(L"\r" + indent);
}

//------------------------------------------------------------------------------
// Insert blank line above with auto-indent - Ctrl+Shift+Enter
//------------------------------------------------------------------------------
void Editor::InsertLineAbove() {
    if (!m_hwndEdit) return;
    
    PushUndoCheckpoint(EditAction::Other);
    
    int curLine = GetCurrentLine();
    int lineStart = GetLineIndex(curLine);
    
    // Copy leading whitespace for auto-indent
    std::wstring lineText = GetLineText(curLine);
    std::wstring indent;
    for (wchar_t ch : lineText) {
        if (ch == L' ' || ch == L'\t') indent += ch;
        else break;
    }
    
    // Insert indent + newline at start of current line
    SetSelection(lineStart, lineStart);
    ReplaceSelection(indent + L"\r");
    
    // Position cursor at end of indent on the new line
    int pos = lineStart + static_cast<int>(indent.length());
    SetSelection(pos, pos);
}

//------------------------------------------------------------------------------
// Select current line - Ctrl+L
//------------------------------------------------------------------------------
void Editor::SelectLine() {
    if (!m_hwndEdit) return;
    
    int curLine = GetCurrentLine();
    int lineStart = GetLineIndex(curLine);
    int lineCount = GetLineCount();
    
    int selectEnd = (curLine + 1 < lineCount) ? GetLineIndex(curLine + 1) : GetTextLength();
    SetSelection(lineStart, selectEnd);
}

//------------------------------------------------------------------------------
// Indent selected lines (add tab) - Tab with selection
//------------------------------------------------------------------------------
void Editor::IndentSelection() {
    if (!m_hwndEdit) return;
    
    PushUndoCheckpoint(EditAction::Other);
    
    DWORD selStart, selEnd;
    GetSelection(selStart, selEnd);
    
    int startLine = GetLineFromChar(selStart);
    int endLine = GetLineFromChar(selEnd);
    
    if (selEnd > selStart && selEnd == static_cast<DWORD>(GetLineIndex(endLine)) && endLine > startLine) {
        endLine--;
    }
    
    // Add tab at start of each line, working bottom-up to preserve positions
    for (int line = endLine; line >= startLine; line--) {
        int ls = GetLineIndex(line);
        SetSelection(ls, ls);
        ReplaceSelection(L"\t");
    }
    
    // Reselect the affected lines
    int newStart = GetLineIndex(startLine);
    int newEnd = (endLine + 1 < GetLineCount()) ? GetLineIndex(endLine + 1) : GetTextLength();
    SetSelection(newStart, newEnd);
}

//------------------------------------------------------------------------------
// Unindent selected lines (remove one indent level) - Shift+Tab
//------------------------------------------------------------------------------
void Editor::UnindentSelection() {
    if (!m_hwndEdit) return;
    
    PushUndoCheckpoint(EditAction::Other);
    
    DWORD selStart, selEnd;
    GetSelection(selStart, selEnd);
    
    int startLine = GetLineFromChar(selStart);
    int endLine = GetLineFromChar(selEnd);
    
    // For single cursor, unindent current line
    if (selStart == selEnd) {
        startLine = endLine = GetCurrentLine();
    } else if (selEnd > selStart && selEnd == static_cast<DWORD>(GetLineIndex(endLine)) && endLine > startLine) {
        endLine--;
    }
    
    // Remove leading indent from each line, working bottom-up
    for (int line = endLine; line >= startLine; line--) {
        int ls = GetLineIndex(line);
        int ll = GetLineLength(line);
        if (ll == 0) continue;
        
        std::wstring lt = GetLineText(line);
        
        if (lt[0] == L'\t') {
            SetSelection(ls, ls + 1);
            ReplaceSelection(L"");
        } else if (lt[0] == L' ') {
            int spaces = 0;
            for (int j = 0; j < m_tabSize && j < ll; j++) {
                if (lt[j] == L' ') spaces++;
                else break;
            }
            if (spaces > 0) {
                SetSelection(ls, ls + spaces);
                ReplaceSelection(L"");
            }
        }
    }
    
    // Reselect the affected lines
    int newStart = GetLineIndex(startLine);
    int newEnd = (endLine + 1 < GetLineCount()) ? GetLineIndex(endLine + 1) : GetTextLength();
    SetSelection(newStart, newEnd);
}

//------------------------------------------------------------------------------
// Smart Home: toggle between first non-whitespace and column 0
//------------------------------------------------------------------------------
void Editor::SmartHome(bool extendSelection) {
    if (!m_hwndEdit) return;
    
    DWORD selStart, selEnd;
    GetSelection(selStart, selEnd);
    
    // Determine caret position
    DWORD caretPos = selStart;
    if (selStart != selEnd) {
        int caretLine = static_cast<int>(SendMessageW(m_hwndEdit, EM_LINEFROMCHAR, static_cast<WPARAM>(-1), 0));
        int endLine = GetLineFromChar(selEnd);
        if (caretLine == endLine && caretLine != GetLineFromChar(selStart)) {
            caretPos = selEnd;
        }
    }
    
    int line = GetLineFromChar(caretPos);
    int lineStart = GetLineIndex(line);
    int lineLen = GetLineLength(line);
    int currentCol = static_cast<int>(caretPos) - lineStart;
    
    // Find first non-whitespace position
    int firstNonWS = 0;
    if (lineLen > 0) {
        std::wstring lineText = GetLineText(line);
        for (int j = 0; j < lineLen; j++) {
            if (lineText[j] != L' ' && lineText[j] != L'\t') {
                firstNonWS = j;
                break;
            }
        }
    }
    
    int targetCol = (currentCol == firstNonWS) ? 0 : firstNonWS;
    DWORD targetPos = static_cast<DWORD>(lineStart + targetCol);
    
    if (extendSelection) {
        DWORD anchor = (caretPos == selStart) ? selEnd : selStart;
        if (selStart == selEnd) anchor = caretPos;
        SendMessageW(m_hwndEdit, EM_SETSEL, anchor, targetPos);
        SendMessageW(m_hwndEdit, EM_SCROLLCARET, 0, 0);
    } else {
        SetSelection(targetPos, targetPos);
    }
}

} // namespace QNote
