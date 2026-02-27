//==============================================================================
// QNote - A Lightweight Notepad Clone
// MainWindowEdit.cpp - Edit menu operations (undo, redo, text transforms, etc.)
//==============================================================================

#include "MainWindow.h"
#include "resource.h"
#include <sstream>
#include <algorithm>
#include <set>

namespace QNote {

//------------------------------------------------------------------------------
// Edit menu handlers
//------------------------------------------------------------------------------
void MainWindow::OnEditUndo() {
    m_editor->Undo();
}

void MainWindow::OnEditRedo() {
    m_editor->Redo();
}

void MainWindow::OnEditCut() {
    m_editor->Cut();
}

void MainWindow::OnEditCopy() {
    m_editor->Copy();
}

void MainWindow::OnEditPaste() {
    m_editor->Paste();
}

void MainWindow::OnEditDelete() {
    m_editor->Delete();
}

void MainWindow::OnEditSelectAll() {
    m_editor->SelectAll();
}

void MainWindow::OnEditFind() {
    if (m_findBar) {
        m_findBar->Show(FindBarMode::Find);
    }
}

void MainWindow::OnEditFindNext() {
    if (m_findBar && m_findBar->IsVisible()) {
        m_findBar->FindNext();
    } else if (m_findBar) {
        // Show the find bar if not visible
        m_findBar->Show(FindBarMode::Find);
    }
}

void MainWindow::OnEditReplace() {
    if (m_findBar) {
        m_findBar->Show(FindBarMode::Replace);
    }
}

void MainWindow::OnEditGoTo() {
    int lineNumber = m_editor->GetCurrentLine() + 1;
    
    if (m_dialogManager->ShowGoToDialog(lineNumber)) {
        m_editor->GoToLine(lineNumber - 1);  // Convert to 0-based
    }
}

void MainWindow::OnEditDateTime() {
    m_editor->InsertDateTime();
}

//------------------------------------------------------------------------------
// Bookmark operations
//------------------------------------------------------------------------------
void MainWindow::OnEditToggleBookmark() {
    m_editor->ToggleBookmark();
    if (m_lineNumbersGutter && m_lineNumbersGutter->IsVisible()) {
        m_lineNumbersGutter->Update();
    }
}

void MainWindow::OnEditNextBookmark() {
    m_editor->NextBookmark();
}

void MainWindow::OnEditPrevBookmark() {
    m_editor->PrevBookmark();
}

void MainWindow::OnEditClearBookmarks() {
    m_editor->ClearBookmarks();
    if (m_lineNumbersGutter && m_lineNumbersGutter->IsVisible()) {
        m_lineNumbersGutter->Update();
    }
}

//------------------------------------------------------------------------------
// Edit -> Uppercase Selection
//------------------------------------------------------------------------------
void MainWindow::OnEditUppercase() {
    std::wstring sel = m_editor->GetSelectedText();
    if (sel.empty()) return;
    
    std::transform(sel.begin(), sel.end(), sel.begin(), ::towupper);
    m_editor->ReplaceSelection(sel);
}

//------------------------------------------------------------------------------
// Edit -> Lowercase Selection
//------------------------------------------------------------------------------
void MainWindow::OnEditLowercase() {
    std::wstring sel = m_editor->GetSelectedText();
    if (sel.empty()) return;
    
    std::transform(sel.begin(), sel.end(), sel.begin(), ::towlower);
    m_editor->ReplaceSelection(sel);
}

//------------------------------------------------------------------------------
// Edit -> Sort Lines (ascending or descending)
//------------------------------------------------------------------------------
void MainWindow::OnEditSortLines(bool ascending) {
    std::wstring text = m_editor->GetText();
    if (text.empty()) return;
    
    // Split into lines
    std::vector<std::wstring> lines;
    std::wistringstream stream(text);
    std::wstring line;
    while (std::getline(stream, line)) {
        // Remove trailing \r if present
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    
    // Sort
    if (ascending) {
        std::sort(lines.begin(), lines.end(), [](const std::wstring& a, const std::wstring& b) {
            return _wcsicmp(a.c_str(), b.c_str()) < 0;
        });
    } else {
        std::sort(lines.begin(), lines.end(), [](const std::wstring& a, const std::wstring& b) {
            return _wcsicmp(a.c_str(), b.c_str()) > 0;
        });
    }
    
    // Rejoin with \r\n (RichEdit uses \r internally, but SetText handles it)
    std::wstring result;
    result.reserve(text.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        result += lines[i];
        if (i < lines.size() - 1) {
            result += L"\r\n";
        }
    }
    
    m_editor->SelectAll();
    m_editor->ReplaceSelection(result);
}

//------------------------------------------------------------------------------
// Edit -> Trim Trailing Whitespace
//------------------------------------------------------------------------------
void MainWindow::OnEditTrimWhitespace() {
    std::wstring text = m_editor->GetText();
    if (text.empty()) return;
    
    std::vector<std::wstring> lines;
    std::wistringstream stream(text);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        // Trim trailing whitespace
        size_t end = line.find_last_not_of(L" \t");
        if (end != std::wstring::npos) {
            line = line.substr(0, end + 1);
        } else if (!line.empty()) {
            line.clear();  // Line was all whitespace
        }
        lines.push_back(line);
    }
    
    std::wstring result;
    result.reserve(text.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        result += lines[i];
        if (i < lines.size() - 1) {
            result += L"\r\n";
        }
    }
    
    m_editor->SelectAll();
    m_editor->ReplaceSelection(result);
}

//------------------------------------------------------------------------------
// Edit -> Remove Duplicate Lines
//------------------------------------------------------------------------------
void MainWindow::OnEditRemoveDuplicateLines() {
    std::wstring text = m_editor->GetText();
    if (text.empty()) return;
    
    std::vector<std::wstring> lines;
    std::set<std::wstring> seen;
    std::wistringstream stream(text);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        if (seen.insert(line).second) {
            lines.push_back(line);
        }
    }
    
    std::wstring result;
    result.reserve(text.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        result += lines[i];
        if (i < lines.size() - 1) {
            result += L"\r\n";
        }
    }
    
    m_editor->SelectAll();
    m_editor->ReplaceSelection(result);
}

//------------------------------------------------------------------------------
// Edit -> Title Case Selection
//------------------------------------------------------------------------------
void MainWindow::OnEditTitleCase() {
    std::wstring sel = m_editor->GetSelectedText();
    if (sel.empty()) return;

    bool capitalizeNext = true;
    for (size_t i = 0; i < sel.size(); ++i) {
        if (iswspace(sel[i]) || sel[i] == L'-' || sel[i] == L'_') {
            capitalizeNext = true;
        } else if (capitalizeNext) {
            sel[i] = towupper(sel[i]);
            capitalizeNext = false;
        } else {
            sel[i] = towlower(sel[i]);
        }
    }

    m_editor->ReplaceSelection(sel);
}

//------------------------------------------------------------------------------
// Edit -> Reverse Lines
//------------------------------------------------------------------------------
void MainWindow::OnEditReverseLines() {
    // Operate on selection if present, otherwise whole document
    bool hasSelection = false;
    DWORD selStart, selEnd;
    m_editor->GetSelection(selStart, selEnd);
    hasSelection = (selStart != selEnd);

    std::wstring text = hasSelection ? m_editor->GetSelectedText() : m_editor->GetText();
    if (text.empty()) return;

    // Split into lines
    std::vector<std::wstring> lines;
    std::wistringstream stream(text);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        lines.push_back(line);
    }

    // Reverse
    std::reverse(lines.begin(), lines.end());

    // Rejoin
    std::wstring result;
    result.reserve(text.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        result += lines[i];
        if (i < lines.size() - 1) result += L"\r\n";
    }

    if (hasSelection) {
        m_editor->ReplaceSelection(result);
    } else {
        m_editor->SelectAll();
        m_editor->ReplaceSelection(result);
    }
}

//------------------------------------------------------------------------------
// Edit -> Number Lines
//------------------------------------------------------------------------------
void MainWindow::OnEditNumberLines() {
    // Operate on selection if present, otherwise whole document
    bool hasSelection = false;
    DWORD selStart, selEnd;
    m_editor->GetSelection(selStart, selEnd);
    hasSelection = (selStart != selEnd);

    std::wstring text = hasSelection ? m_editor->GetSelectedText() : m_editor->GetText();
    if (text.empty()) return;

    std::vector<std::wstring> lines;
    std::wistringstream stream(text);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        lines.push_back(line);
    }

    // Determine padding width
    int width = static_cast<int>(std::to_wstring(lines.size()).size());

    std::wstring result;
    result.reserve(text.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        std::wstring num = std::to_wstring(i + 1);
        while (static_cast<int>(num.size()) < width) num = L" " + num;
        result += num + L": " + lines[i];
        if (i < lines.size() - 1) result += L"\r\n";
    }

    if (hasSelection) {
        m_editor->ReplaceSelection(result);
    } else {
        m_editor->SelectAll();
        m_editor->ReplaceSelection(result);
    }
}

//------------------------------------------------------------------------------
// Edit -> Toggle Comment (Ctrl+/)
//------------------------------------------------------------------------------
void MainWindow::OnEditToggleComment() {
    // Detect comment style from file extension
    std::wstring commentPrefix = L"// ";
    if (!m_currentFile.empty()) {
        size_t dot = m_currentFile.rfind(L'.');
        if (dot != std::wstring::npos) {
            std::wstring ext = m_currentFile.substr(dot);
            // Convert to lowercase
            for (auto& ch : ext) ch = towlower(ch);
            if (ext == L".py" || ext == L".sh" || ext == L".bash" || ext == L".yml" ||
                ext == L".yaml" || ext == L".rb" || ext == L".pl" || ext == L".r" ||
                ext == L".conf" || ext == L".ini" || ext == L".toml") {
                commentPrefix = L"# ";
            } else if (ext == L".bat" || ext == L".cmd") {
                commentPrefix = L"REM ";
            } else if (ext == L".sql") {
                commentPrefix = L"-- ";
            } else if (ext == L".html" || ext == L".xml" || ext == L".xaml") {
                commentPrefix = L"<!-- ";
            } else if (ext == L".lua") {
                commentPrefix = L"-- ";
            }
        }
    }

    // Get selected text or current line
    bool hasSelection = false;
    DWORD selStart, selEnd;
    m_editor->GetSelection(selStart, selEnd);
    hasSelection = (selStart != selEnd);

    std::wstring text;
    if (hasSelection) {
        text = m_editor->GetSelectedText();
    } else {
        // Select current line
        int line = m_editor->GetCurrentLine();
        int lineStart = m_editor->GetLineIndex(line);
        int lineLen = m_editor->GetLineLength(line);
        m_editor->SetSelection(static_cast<DWORD>(lineStart),
                               static_cast<DWORD>(lineStart + lineLen));
        text = m_editor->GetSelectedText();
    }

    if (text.empty()) return;

    // Split into lines
    std::vector<std::wstring> lines;
    std::wistringstream stream(text);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        lines.push_back(line);
    }

    // Check if all non-empty lines are already commented
    std::wstring trimmedPrefix = commentPrefix;
    // Remove trailing space for matching
    while (!trimmedPrefix.empty() && trimmedPrefix.back() == L' ')
        trimmedPrefix.pop_back();

    bool allCommented = true;
    for (const auto& l : lines) {
        // Skip blank lines
        size_t first = l.find_first_not_of(L" \t");
        if (first == std::wstring::npos) continue;
        if (l.substr(first, commentPrefix.size()) != commentPrefix &&
            l.substr(first, trimmedPrefix.size()) != trimmedPrefix) {
            allCommented = false;
            break;
        }
    }

    std::wstring result;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (allCommented) {
            // Uncomment
            size_t first = lines[i].find_first_not_of(L" \t");
            if (first != std::wstring::npos) {
                if (lines[i].substr(first, commentPrefix.size()) == commentPrefix) {
                    lines[i].erase(first, commentPrefix.size());
                } else if (lines[i].substr(first, trimmedPrefix.size()) == trimmedPrefix) {
                    lines[i].erase(first, trimmedPrefix.size());
                }
            }
        } else {
            // Comment
            size_t first = lines[i].find_first_not_of(L" \t");
            if (first != std::wstring::npos) {
                lines[i].insert(first, commentPrefix);
            }
        }
        result += lines[i];
        if (i < lines.size() - 1) result += L"\r\n";
    }

    m_editor->ReplaceSelection(result);
}

} // namespace QNote
