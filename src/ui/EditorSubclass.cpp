//==============================================================================
// QNote - A Lightweight Notepad Clone
// EditorSubclass.cpp - Edit control subclass procedure and utility methods
//==============================================================================

#include "Editor.h"
#include "../resources/resource.h"
#include <CommCtrl.h>
#include <windowsx.h>

namespace QNote {

// Subclass ID for edit control
static constexpr UINT_PTR EDIT_SUBCLASS_ID = 1;

//------------------------------------------------------------------------------
// Edit control subclass procedure
//------------------------------------------------------------------------------
LRESULT CALLBACK Editor::EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, 
                                           LPARAM lParam, UINT_PTR subclassId, 
                                           DWORD_PTR refData) {
    Editor* editor = reinterpret_cast<Editor*>(refData);
    
    switch (msg) {
        case WM_PAINT: {
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            if (editor->m_showWhitespace || editor->m_spellCheckEnabled) {
                HDC hdc = GetDC(hwnd);
                if (editor->m_showWhitespace) {
                    editor->DrawWhitespace(hdc);
                }
                if (editor->m_spellCheckEnabled) {
                    editor->DrawSpellCheck(hdc);
                }
                ReleaseDC(hwnd, hdc);
            }
            // Sync line-number gutter on every repaint so it tracks
            // smooth/animated scrolling that continues after WM_MOUSEWHEEL.
            if (editor->m_scrollCallback) {
                editor->m_scrollCallback(editor->m_scrollCallbackData);
            }
            return result;
        }

        case WM_LBUTTONDOWN: {
            // Mouse click repositions cursor - seal the undo group
            editor->SealUndoGroup();
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            return result;
        }

        case WM_LBUTTONUP: {
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            if (editor->m_scrollCallback) {
                editor->m_scrollCallback(editor->m_scrollCallbackData);
            }
            return result;
        }

        case WM_MOUSEWHEEL: {
            // Handle Ctrl+Wheel for zoom
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                short delta = GET_WHEEL_DELTA_WPARAM(wParam);
                int newZoom = editor->m_zoomPercent + (delta > 0 ? 10 : -10);
                editor->ApplyZoom(newZoom);
                
                // Notify parent of zoom change
                PostMessageW(GetParent(hwnd), WM_APP_UPDATESTATUS, 0, 0);
                return 0;
            }
            // Custom scroll lines: if set, manually scroll the editor instead
            // of letting the default handler use the system setting.
            if (editor->m_scrollLines > 0) {
                short delta = GET_WHEEL_DELTA_WPARAM(wParam);
                int linesToScroll = -((int)delta / WHEEL_DELTA) * editor->m_scrollLines;
                SendMessageW(hwnd, EM_LINESCROLL, 0, linesToScroll);
                if (editor->m_scrollCallback) {
                    editor->m_scrollCallback(editor->m_scrollCallbackData);
                }
                return 0;
            }
            // Fall through to default handler, then notify scroll
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            if (editor->m_scrollCallback) {
                editor->m_scrollCallback(editor->m_scrollCallbackData);
            }
            return result;
        }
        
        case WM_VSCROLL:
        case WM_HSCROLL: {
            // Handle scroll, then notify callback
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            if (editor->m_scrollCallback) {
                editor->m_scrollCallback(editor->m_scrollCallbackData);
            }
            return result;
        }
        
        case WM_KEYDOWN: {
            bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            bool altDown = (GetKeyState(VK_MENU) & 0x8000) != 0;
            
            // --- Undo/Redo handling ---
            
            // Ctrl+Z: Undo (also handled via accelerator table → IDM_EDIT_UNDO)
            if (ctrlDown && !shiftDown && !altDown && wParam == 'Z') {
                editor->Undo();
                return 0;
            }
            
            // Ctrl+Shift+Z: Redo (VSCode standard)
            if (ctrlDown && shiftDown && !altDown && wParam == 'Z') {
                editor->Redo();
                return 0;
            }
            
            // --- Backspace / Delete with undo tracking ---
            
            if (!ctrlDown && !altDown && wParam == VK_BACK) {
                editor->PushUndoCheckpoint(EditAction::Deleting);
                LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
                editor->m_spellDirty = true;
                if (editor->m_scrollCallback) editor->m_scrollCallback(editor->m_scrollCallbackData);
                return result;
            }
            
            if (!ctrlDown && !altDown && wParam == VK_DELETE) {
                editor->PushUndoCheckpoint(EditAction::Deleting);
                LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
                editor->m_spellDirty = true;
                if (editor->m_scrollCallback) editor->m_scrollCallback(editor->m_scrollCallbackData);
                return result;
            }
            
            // Ctrl+Backspace: delete word left
            if (ctrlDown && !altDown && wParam == VK_BACK) {
                editor->PushUndoCheckpoint(EditAction::Other);
                LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
                editor->m_spellDirty = true;
                if (editor->m_scrollCallback) editor->m_scrollCallback(editor->m_scrollCallbackData);
                return result;
            }
            
            // Ctrl+Delete: delete word right
            if (ctrlDown && !altDown && wParam == VK_DELETE) {
                editor->PushUndoCheckpoint(EditAction::Other);
                LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
                editor->m_spellDirty = true;
                if (editor->m_scrollCallback) editor->m_scrollCallback(editor->m_scrollCallbackData);
                return result;
            }
            
            // --- VS Code-like keyboard shortcuts ---
            
            // Ctrl+Shift+K: Delete line
            if (ctrlDown && shiftDown && !altDown && wParam == 'K') {
                editor->DeleteLine();
                if (editor->m_scrollCallback) editor->m_scrollCallback(editor->m_scrollCallbackData);
                return 0;
            }
            
            // Ctrl+D: Duplicate line/selection
            if (ctrlDown && !shiftDown && !altDown && wParam == 'D') {
                editor->DuplicateLine();
                if (editor->m_scrollCallback) editor->m_scrollCallback(editor->m_scrollCallbackData);
                return 0;
            }
            
            // Ctrl+L: Select line
            if (ctrlDown && !shiftDown && !altDown && wParam == 'L') {
                editor->SelectLine();
                return 0;
            }
            
            // Alt+Up: Move line up
            if (altDown && !ctrlDown && !shiftDown && wParam == VK_UP) {
                editor->MoveLineUp();
                if (editor->m_scrollCallback) editor->m_scrollCallback(editor->m_scrollCallbackData);
                return 0;
            }
            
            // Alt+Down: Move line down
            if (altDown && !ctrlDown && !shiftDown && wParam == VK_DOWN) {
                editor->MoveLineDown();
                if (editor->m_scrollCallback) editor->m_scrollCallback(editor->m_scrollCallbackData);
                return 0;
            }
            
            // Alt+Shift+Up: Copy line up
            if (altDown && shiftDown && !ctrlDown && wParam == VK_UP) {
                editor->CopyLineUp();
                if (editor->m_scrollCallback) editor->m_scrollCallback(editor->m_scrollCallbackData);
                return 0;
            }
            
            // Alt+Shift+Down: Copy line down
            if (altDown && shiftDown && !ctrlDown && wParam == VK_DOWN) {
                editor->CopyLineDown();
                if (editor->m_scrollCallback) editor->m_scrollCallback(editor->m_scrollCallbackData);
                return 0;
            }
            
            // Ctrl+Enter: Insert line below
            if (ctrlDown && !shiftDown && !altDown && wParam == VK_RETURN) {
                editor->InsertLineBelow();
                if (editor->m_scrollCallback) editor->m_scrollCallback(editor->m_scrollCallbackData);
                return 0;
            }
            
            // Ctrl+Shift+Enter: Insert line above
            if (ctrlDown && shiftDown && !altDown && wParam == VK_RETURN) {
                editor->InsertLineAbove();
                if (editor->m_scrollCallback) editor->m_scrollCallback(editor->m_scrollCallbackData);
                return 0;
            }
            
            // Tab/Shift+Tab: Indent/Unindent
            if (!ctrlDown && !altDown && wParam == VK_TAB) {
                if (shiftDown) {
                    // Shift+Tab: Always unindent
                    editor->UnindentSelection();
                    if (editor->m_scrollCallback) editor->m_scrollCallback(editor->m_scrollCallbackData);
                    return 0;
                } else {
                    // Tab: Indent if there's any selection
                    DWORD tabSelStart, tabSelEnd;
                    editor->GetSelection(tabSelStart, tabSelEnd);
                    if (tabSelStart != tabSelEnd) {
                        editor->IndentSelection();
                        if (editor->m_scrollCallback) editor->m_scrollCallback(editor->m_scrollCallbackData);
                        return 0;
                    }
                    // Otherwise fall through for normal tab insertion
                }
            }
            
            // Home: Smart Home (toggle first non-whitespace / column 0)
            if (!ctrlDown && !altDown && wParam == VK_HOME) {
                editor->SealUndoGroup();
                editor->SmartHome(shiftDown);
                return 0;
            }
            
            // Arrow keys, Page Up/Down, Ctrl+Home/End: seal undo group
            if (wParam == VK_UP || wParam == VK_DOWN || wParam == VK_LEFT || wParam == VK_RIGHT ||
                wParam == VK_PRIOR || wParam == VK_NEXT ||
                (ctrlDown && (wParam == VK_HOME || wParam == VK_END))) {
                editor->SealUndoGroup();
            }
            
            // --- Default handling ---
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            // Arrow keys, Page Up/Down, Home/End can cause scrolling
            if (wParam == VK_UP || wParam == VK_DOWN || wParam == VK_PRIOR || 
                wParam == VK_NEXT || wParam == VK_HOME || wParam == VK_END) {
                if (editor->m_scrollCallback) {
                    editor->m_scrollCallback(editor->m_scrollCallbackData);
                }
            }
            return result;
        }
        
        case EN_CHANGE:
            // WM_CUT has the same value; both should trigger line number update
            {
                LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
                editor->m_spellDirty = true;
                if (editor->m_scrollCallback) {
                    editor->m_scrollCallback(editor->m_scrollCallbackData);
                }
                return result;
            }

        case WM_PASTE: {
            // Push undo checkpoint before paste
            editor->PushUndoCheckpoint(EditAction::Other);
            // Paste may insert multiple lines; update line numbers after paste
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            editor->m_spellDirty = true;
            if (editor->m_scrollCallback) {
                editor->m_scrollCallback(editor->m_scrollCallbackData);
            }
            return result;
        }

        case WM_CHAR: {
            // Suppress control characters from keyboard shortcuts handled in WM_KEYDOWN
            // 0x04 = Ctrl+D, 0x0A = Ctrl+Enter(LF), 0x0B = Ctrl+K, 0x0C = Ctrl+L
            // 0x1A = Ctrl+Z, 0x19 = Ctrl+Y (undo/redo handled in WM_KEYDOWN)
            if (wParam == 0x04 || wParam == 0x0A || wParam == 0x0B || wParam == 0x0C ||
                wParam == 0x1A || wParam == 0x19) {
                return 0;
            }
            
            // Push undo checkpoint for character input
            wchar_t ch = static_cast<wchar_t>(wParam);
            if (ch == L'\b') {
                // Backspace already handled in WM_KEYDOWN - suppress WM_CHAR echo
                return 0;
            } else if (ch == L'\r') {
                editor->PushUndoCheckpoint(EditAction::Typing, ch);
            } else if (ch >= L' ' || ch == L'\t') {
                editor->PushUndoCheckpoint(EditAction::Typing, ch);
            }
            
            // Auto-indent: when Enter is pressed, copy leading whitespace from current line
            if (wParam == L'\r') {
                // Get current line's leading whitespace before the newline is inserted
                int curLine = static_cast<int>(SendMessageW(hwnd, EM_LINEFROMCHAR, static_cast<WPARAM>(-1), 0));
                int lineStart = static_cast<int>(SendMessageW(hwnd, EM_LINEINDEX, curLine, 0));
                int lineLen = static_cast<int>(SendMessageW(hwnd, EM_LINELENGTH, lineStart, 0));

                std::wstring whitespace;
                if (lineLen > 0) {
                    std::vector<wchar_t> buf(lineLen + 2, 0);
                    *reinterpret_cast<WORD*>(buf.data()) = static_cast<WORD>(lineLen + 1);
                    SendMessageW(hwnd, EM_GETLINE, curLine, reinterpret_cast<LPARAM>(buf.data()));
                    for (int j = 0; j < lineLen; ++j) {
                        if (buf[j] == L' ' || buf[j] == L'\t') {
                            whitespace += buf[j];
                        } else {
                            break;
                        }
                    }
                }

                // Let default insert the newline
                LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);

                // Insert the leading whitespace after the newline
                if (!whitespace.empty()) {
                    SendMessageW(hwnd, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(whitespace.c_str()));
                }

                if (editor->m_scrollCallback) {
                    editor->m_scrollCallback(editor->m_scrollCallbackData);
                }
                editor->m_spellDirty = true;
                return result;
            }

            // Content change may affect line numbers
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            editor->m_spellDirty = true;
            if (editor->m_scrollCallback) {
                editor->m_scrollCallback(editor->m_scrollCallbackData);
            }
            return result;
        }

        case WM_CONTEXTMENU: {
            int screenX = GET_X_LPARAM(lParam);
            int screenY = GET_Y_LPARAM(lParam);

            // Handle keyboard-invoked context menu (coordinates are -1, -1)
            if (screenX == -1 && screenY == -1) {
                DWORD selStart, selEnd;
                editor->GetSelection(selStart, selEnd);
                POINTL pt = {};
                SendMessageW(hwnd, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&pt), selStart);
                POINT sp = { pt.x, pt.y };
                ClientToScreen(hwnd, &sp);
                screenX = sp.x;
                screenY = sp.y;
            }

            // Get character position from click
            POINT clientPt = { screenX, screenY };
            ScreenToClient(hwnd, &clientPt);
            LRESULT charPos = SendMessageW(hwnd, EM_CHARFROMPOS, 0, reinterpret_cast<LPARAM>(&clientPt));
            int charIndex = static_cast<int>(charPos);

            // Build context menu
            HMENU hMenu = CreatePopupMenu();

            if (editor->m_spellCheckEnabled && editor->m_spellChecker.IsAvailable() && charIndex >= 0) {
                // Find word at click position
                int line = static_cast<int>(SendMessageW(hwnd, EM_LINEFROMCHAR, charIndex, 0));
                int lineStart = static_cast<int>(SendMessageW(hwnd, EM_LINEINDEX, line, 0));
                int lineLen = static_cast<int>(SendMessageW(hwnd, EM_LINELENGTH, lineStart, 0));

                if (lineLen > 0) {
                    std::vector<wchar_t> buf(lineLen + 2, 0);
                    *reinterpret_cast<WORD*>(buf.data()) = static_cast<WORD>(lineLen + 1);
                    SendMessageW(hwnd, EM_GETLINE, line, reinterpret_cast<LPARAM>(buf.data()));

                    int posInLine = charIndex - lineStart;
                    if (posInLine >= 0 && posInLine < lineLen && iswalpha(buf[posInLine])) {
                        // Walk backward to find word start
                        int ws = posInLine;
                        while (ws > 0 && (iswalpha(buf[ws - 1]) || buf[ws - 1] == L'\''))
                            ws--;
                        // Walk forward to find word end
                        int we = posInLine;
                        while (we < lineLen - 1 && (iswalpha(buf[we + 1]) || buf[we + 1] == L'\''))
                            we++;

                        std::wstring word(buf.data() + ws, we - ws + 1);
                        if (!word.empty() && !editor->m_spellChecker.CheckWord(word)) {
                            editor->m_rightClickWord = word;
                            editor->m_rightClickStart = static_cast<DWORD>(lineStart + ws);
                            editor->m_rightClickLen = static_cast<DWORD>(we - ws + 1);
                            editor->m_rightClickSuggestions = editor->m_spellChecker.GetSuggestions(word, 5);

                            if (!editor->m_rightClickSuggestions.empty()) {
                                for (int i = 0; i < static_cast<int>(editor->m_rightClickSuggestions.size()); i++) {
                                    AppendMenuW(hMenu, MF_STRING, IDM_SPELL_SUGGEST_BASE + i,
                                                editor->m_rightClickSuggestions[i].c_str());
                                }
                                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                            } else {
                                AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"(No suggestions)");
                                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                            }
                            AppendMenuW(hMenu, MF_STRING, IDM_SPELL_ADDWORD, L"Add to Dictionary");
                            AppendMenuW(hMenu, MF_STRING, IDM_SPELL_IGNORE, L"Ignore Word");
                            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                        }
                    }
                }
            }

            // Standard edit context menu items
            DWORD selStart, selEnd;
            editor->GetSelection(selStart, selEnd);
            bool hasSelection = (selStart != selEnd);
            bool canPaste = IsClipboardFormatAvailable(CF_UNICODETEXT) ||
                            IsClipboardFormatAvailable(CF_TEXT);

            AppendMenuW(hMenu, MF_STRING | (editor->CanUndo() ? 0 : MF_GRAYED), IDM_EDIT_UNDO, L"&Undo");
            AppendMenuW(hMenu, MF_STRING | (editor->CanRedo() ? 0 : MF_GRAYED), IDM_EDIT_REDO, L"&Redo");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING | (hasSelection ? 0 : MF_GRAYED), IDM_EDIT_CUT, L"Cu&t");
            AppendMenuW(hMenu, MF_STRING | (hasSelection ? 0 : MF_GRAYED), IDM_EDIT_COPY, L"&Copy");
            AppendMenuW(hMenu, MF_STRING | (canPaste ? 0 : MF_GRAYED), IDM_EDIT_PASTE, L"&Paste");
            AppendMenuW(hMenu, MF_STRING | (hasSelection ? 0 : MF_GRAYED), IDM_EDIT_DELETE, L"&Delete");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, IDM_EDIT_SELECTALL, L"Select &All");

            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                     screenX, screenY, 0, hwnd, nullptr);
            DestroyMenu(hMenu);

            if (cmd >= IDM_SPELL_SUGGEST_BASE &&
                cmd < IDM_SPELL_SUGGEST_BASE + static_cast<int>(editor->m_rightClickSuggestions.size())) {
                // Replace misspelled word with selected suggestion
                int idx = cmd - IDM_SPELL_SUGGEST_BASE;
                editor->PushUndoCheckpoint(EditAction::Other);
                editor->SetSelection(editor->m_rightClickStart,
                                     editor->m_rightClickStart + editor->m_rightClickLen);
                editor->ReplaceSelection(editor->m_rightClickSuggestions[idx]);
                editor->m_spellDirty = true;
                InvalidateRect(hwnd, nullptr, FALSE);
            } else if (cmd == IDM_SPELL_ADDWORD) {
                editor->m_spellChecker.AddWord(editor->m_rightClickWord);
                editor->m_spellDirty = true;
                InvalidateRect(hwnd, nullptr, FALSE);
            } else if (cmd == IDM_SPELL_IGNORE) {
                editor->m_spellChecker.IgnoreWord(editor->m_rightClickWord);
                editor->m_spellDirty = true;
                InvalidateRect(hwnd, nullptr, FALSE);
            } else if (cmd) {
                switch (cmd) {
                    case IDM_EDIT_UNDO: editor->Undo(); break;
                    case IDM_EDIT_REDO: editor->Redo(); break;
                    case IDM_EDIT_CUT: editor->Cut(); break;
                    case IDM_EDIT_COPY: editor->Copy(); break;
                    case IDM_EDIT_PASTE: editor->Paste(); break;
                    case IDM_EDIT_DELETE: editor->Delete(); break;
                    case IDM_EDIT_SELECTALL: editor->SelectAll(); break;
                }
            }
            return 0;
        }
    }
    
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

//------------------------------------------------------------------------------
// Set spell check enabled
//------------------------------------------------------------------------------
void Editor::SetSpellCheck(bool enable) {
    m_spellCheckEnabled = enable;
    if (enable && !m_spellChecker.IsAvailable()) {
        if (!m_spellChecker.Initialize()) {
            m_spellCheckEnabled = false;
        }
    }
    m_spellDirty = true;
    if (m_hwndEdit) {
        InvalidateRect(m_hwndEdit, nullptr, FALSE);
    }
}

//------------------------------------------------------------------------------
// Draw spell check wavy underlines for misspelled words
//------------------------------------------------------------------------------
void Editor::DrawSpellCheck(HDC hdc) {
    if (!m_hwndEdit || !m_spellCheckEnabled || !m_spellChecker.IsAvailable()) return;
    if (!m_font.get()) return;

    int firstLine = GetFirstVisibleLine();

    RECT clientRect;
    GetClientRect(m_hwndEdit, &clientRect);

    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, m_font.get()));
    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);

    int visibleLines = (clientRect.bottom - clientRect.top) / tm.tmHeight + 2;
    int totalLines = GetLineCount();
    int lastLine = (std::min)(firstLine + visibleLines - 1, totalLines - 1);

    // Check cache: recompute only when visible range or text changed
    if (m_spellDirty || firstLine != m_spellCacheFirstLine || lastLine != m_spellCacheLastLine) {
        m_spellCacheWords.clear();

        // Get visible text range
        int rangeStart = GetLineIndex(firstLine);
        int rangeEndLine = (std::min)(lastLine, totalLines - 1);
        int rangeEnd = GetLineIndex(rangeEndLine) + GetLineLength(rangeEndLine);

        if (rangeEnd > rangeStart) {
            // Extract visible text using EM_GETTEXTRANGE
            int textLen = rangeEnd - rangeStart;
            std::vector<wchar_t> buffer(textLen + 1, 0);
            TEXTRANGEW tr = {};
            tr.chrg.cpMin = rangeStart;
            tr.chrg.cpMax = rangeEnd;
            tr.lpstrText = buffer.data();
            LRESULT len = SendMessageW(m_hwndEdit, EM_GETTEXTRANGE, 0, reinterpret_cast<LPARAM>(&tr));

            if (len > 0) {
                std::wstring visibleText(buffer.data(), static_cast<size_t>(len));
                m_spellCacheWords = m_spellChecker.CheckText(visibleText, static_cast<DWORD>(rangeStart));
            }
        }

        m_spellCacheFirstLine = firstLine;
        m_spellCacheLastLine = lastLine;
        m_spellDirty = false;
    }

    // Draw wavy red underlines for each misspelled word
    HPEN wavePen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, wavePen));

    for (const auto& word : m_spellCacheWords) {
        POINTL ptStart = {};
        SendMessageW(m_hwndEdit, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&ptStart), word.startPos);

        POINTL ptEnd = {};
        DWORD endPos = word.startPos + word.length;
        SendMessageW(m_hwndEdit, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&ptEnd), endPos);

        // If end position is at or before start, estimate it
        if (ptEnd.x <= ptStart.x) {
            ptEnd.x = ptStart.x + static_cast<int>(word.length) * tm.tmAveCharWidth;
            ptEnd.y = ptStart.y;
        }

        // Clip to client area
        if (ptStart.y < clientRect.top - tm.tmHeight || ptStart.y > clientRect.bottom) continue;
        if (ptEnd.x <= clientRect.left || ptStart.x >= clientRect.right) continue;

        int startX = (std::max)(static_cast<int>(ptStart.x), static_cast<int>(clientRect.left));
        int endX = (std::min)(static_cast<int>(ptEnd.x), static_cast<int>(clientRect.right));
        int baseY = ptStart.y + tm.tmAscent + 2;

        // Draw zigzag wave pattern
        MoveToEx(hdc, startX, baseY, nullptr);
        for (int x = startX + 1; x <= endX; x++) {
            int phase = (x - startX) % 4;
            int y;
            switch (phase) {
                case 0: y = baseY; break;
                case 1: y = baseY + 1; break;
                case 2: y = baseY; break;
                case 3: y = baseY - 1; break;
                default: y = baseY; break;
            }
            LineTo(hdc, x, y);
        }
    }

    SelectObject(hdc, oldPen);
    DeleteObject(wavePen);
    SelectObject(hdc, oldFont);
}

//------------------------------------------------------------------------------
// Set show whitespace
//------------------------------------------------------------------------------
void Editor::SetShowWhitespace(bool enable) noexcept {
    m_showWhitespace = enable;
    if (m_hwndEdit) {
        InvalidateRect(m_hwndEdit, nullptr, FALSE);
    }
}

//------------------------------------------------------------------------------
// Draw whitespace glyphs (dots for spaces, arrows for tabs)
//------------------------------------------------------------------------------
void Editor::DrawWhitespace(HDC hdc) {
    if (!m_hwndEdit || !m_font.get()) return;
    
    int firstLine = GetFirstVisibleLine();
    
    RECT clientRect;
    GetClientRect(m_hwndEdit, &clientRect);
    
    // Get font metrics
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, m_font.get()));
    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    
    int visibleLines = (clientRect.bottom - clientRect.top) / tm.tmHeight + 2;
    int totalLines = GetLineCount();
    
    COLORREF wsColor = RGB(180, 180, 180);
    HPEN wsPen = CreatePen(PS_SOLID, 1, wsColor);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, wsPen));
    
    for (int i = 0; i < visibleLines && (firstLine + i) < totalLines; i++) {
        int line = firstLine + i;
        int lineStart = GetLineIndex(line);
        int lineLen = GetLineLength(line);
        
        if (lineLen == 0) continue;
        
        // Get line text
        std::vector<wchar_t> buf(lineLen + 2, 0);
        *reinterpret_cast<WORD*>(buf.data()) = static_cast<WORD>(lineLen + 1);
        SendMessageW(m_hwndEdit, EM_GETLINE, line, reinterpret_cast<LPARAM>(buf.data()));
        
        for (int j = 0; j < lineLen; j++) {
            if (buf[j] == L' ' || buf[j] == L'\t') {
                POINTL pt = {};
                SendMessageW(m_hwndEdit, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&pt), lineStart + j);
                
                if (pt.x >= clientRect.left && pt.x < clientRect.right && 
                    pt.y >= clientRect.top && pt.y < clientRect.bottom) {
                    if (buf[j] == L' ') {
                        // Draw centered dot for space
                        int dotX = pt.x + tm.tmAveCharWidth / 2;
                        int dotY = pt.y + tm.tmHeight / 2;
                        RECT dotRect = { dotX - 1, dotY - 1, dotX + 1, dotY + 1 };
                        HBRUSH dotBrush = CreateSolidBrush(wsColor);
                        FillRect(hdc, &dotRect, dotBrush);
                        DeleteObject(dotBrush);
                    } else {
                        // Draw right arrow for tab
                        int arrowY = pt.y + tm.tmHeight / 2;
                        int arrowStart = pt.x + 2;
                        int arrowEnd = pt.x + tm.tmAveCharWidth * m_tabSize - 2;
                        if (arrowEnd > arrowStart + 4) {
                            MoveToEx(hdc, arrowStart, arrowY, nullptr);
                            LineTo(hdc, arrowEnd, arrowY);
                            // Arrow head
                            MoveToEx(hdc, arrowEnd - 3, arrowY - 3, nullptr);
                            LineTo(hdc, arrowEnd + 1, arrowY);
                            MoveToEx(hdc, arrowEnd - 3, arrowY + 3, nullptr);
                            LineTo(hdc, arrowEnd + 1, arrowY);
                        }
                    }
                }
            }
        }
    }
    
    SelectObject(hdc, oldPen);
    DeleteObject(wsPen);
    SelectObject(hdc, oldFont);
}

//------------------------------------------------------------------------------
// Toggle bookmark on current line
//------------------------------------------------------------------------------
void Editor::ToggleBookmark() {
    int line = GetCurrentLine();
    auto it = m_bookmarks.find(line);
    if (it != m_bookmarks.end()) {
        m_bookmarks.erase(it);
    } else {
        m_bookmarks.insert(line);
    }
    // Notify parent for gutter update
    PostMessageW(m_hwndParent, WM_APP_UPDATESTATUS, 0, 0);
}

//------------------------------------------------------------------------------
// Jump to next bookmark
//------------------------------------------------------------------------------
void Editor::NextBookmark() {
    if (m_bookmarks.empty()) return;
    
    int curLine = GetCurrentLine();
    auto it = m_bookmarks.upper_bound(curLine);
    if (it == m_bookmarks.end()) {
        it = m_bookmarks.begin(); // Wrap around
    }
    GoToLine(*it);
}

//------------------------------------------------------------------------------
// Jump to previous bookmark
//------------------------------------------------------------------------------
void Editor::PrevBookmark() {
    if (m_bookmarks.empty()) return;
    
    int curLine = GetCurrentLine();
    auto it = m_bookmarks.lower_bound(curLine);
    if (it == m_bookmarks.begin()) {
        it = m_bookmarks.end();
    }
    --it;
    GoToLine(*it);
}

//------------------------------------------------------------------------------
// Clear all bookmarks
//------------------------------------------------------------------------------
void Editor::ClearBookmarks() {
    m_bookmarks.clear();
    PostMessageW(m_hwndParent, WM_APP_UPDATESTATUS, 0, 0);
}

//------------------------------------------------------------------------------
// Set bookmarks (for document restore)
//------------------------------------------------------------------------------
void Editor::SetBookmarks(const std::set<int>& bookmarks) {
    m_bookmarks = bookmarks;
}

} // namespace QNote
