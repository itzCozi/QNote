//==============================================================================
// QNote - A Lightweight Notepad Clone
// MainWindowTools.cpp - Tools menu operations
//==============================================================================

#include "MainWindow.h"
#include "resource.h"
#include <CommCtrl.h>
#include <shellapi.h>
#include <sstream>
#include <algorithm>
#include <wincrypt.h>
#include <objbase.h>
#include <bcrypt.h>

#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "bcrypt.lib")

namespace QNote {

//------------------------------------------------------------------------------
// Helper: check if a string is empty or contains only whitespace
//------------------------------------------------------------------------------
static bool IsWhitespaceOnly(const std::wstring& text) {
    return text.find_first_not_of(L" \t\r\n") == std::wstring::npos;
}

//------------------------------------------------------------------------------
// Tools -> Edit Keyboard Shortcuts
//------------------------------------------------------------------------------
void MainWindow::OnToolsEditShortcuts() {
    // Get shortcuts file path
    std::wstring settingsPath = m_settingsManager->GetSettingsPath();
    size_t pos = settingsPath.rfind(L"config.ini");
    if (pos == std::wstring::npos) return;
    
    std::wstring shortcutsPath = settingsPath.substr(0, pos) + L"shortcuts.ini";
    
    // Create default file if it doesn't exist
    if (GetFileAttributesW(shortcutsPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        CreateDefaultShortcutsFile(shortcutsPath);
    }
    
    // Open the file in a new tab
    FileReadResult result = FileIO::ReadFile(shortcutsPath);
    if (result.success) {
        // Check if already open
        int existingTab = m_documentManager->FindDocumentByPath(shortcutsPath);
        if (existingTab >= 0) {
            OnTabSelected(existingTab);
        } else {
            auto* activeDoc = m_documentManager->GetActiveDocument();
            if (activeDoc && activeDoc->isNewFile && !activeDoc->isModified && 
                m_editor && IsWhitespaceOnly(m_editor->GetText())) {
                LoadFile(shortcutsPath);
            } else {
                m_documentManager->OpenDocument(
                    shortcutsPath, result.content, result.detectedEncoding, result.detectedLineEnding);
                UpdateActiveEditor();
                m_currentFile = shortcutsPath;
                m_isNewFile = false;
                UpdateTitle();
                UpdateStatusBar();
            }
        }
    }
}

//------------------------------------------------------------------------------
// Tools -> Copy File Path to Clipboard
//------------------------------------------------------------------------------
void MainWindow::OnToolsCopyFilePath() {
    if (m_currentFile.empty() || m_isNewFile) return;
    
    if (!OpenClipboard(m_hwnd)) return;
    EmptyClipboard();
    
    size_t len = (m_currentFile.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    if (hMem) {
        wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
        if (pMem) {
            wcscpy_s(pMem, m_currentFile.size() + 1, m_currentFile.c_str());
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
    }
    CloseClipboard();
    
    // Brief status bar notification
    SendMessageW(m_hwndStatus, SB_SETTEXTW, SB_PART_COUNTS,
                 reinterpret_cast<LPARAM>(L"Path copied to clipboard"));
}

//------------------------------------------------------------------------------
// Tools -> Auto-Save Options dialog
//------------------------------------------------------------------------------
struct AutoSaveDialogData {
    bool enabled;
    int intervalSeconds;
};

INT_PTR CALLBACK MainWindow::AutoSaveDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            SetWindowLongPtrW(hDlg, DWLP_USER, lParam);
            auto* data = reinterpret_cast<AutoSaveDialogData*>(lParam);
            CheckDlgButton(hDlg, IDC_AUTOSAVE_ENABLE, data->enabled ? BST_CHECKED : BST_UNCHECKED);
            SetDlgItemInt(hDlg, IDC_AUTOSAVE_INTERVAL, data->intervalSeconds, FALSE);
            // Enable/disable interval field based on checkbox
            EnableWindow(GetDlgItem(hDlg, IDC_AUTOSAVE_INTERVAL), data->enabled);
            return TRUE;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_AUTOSAVE_ENABLE: {
                    bool checked = IsDlgButtonChecked(hDlg, IDC_AUTOSAVE_ENABLE) == BST_CHECKED;
                    EnableWindow(GetDlgItem(hDlg, IDC_AUTOSAVE_INTERVAL), checked);
                    return TRUE;
                }
                case IDOK: {
                    auto* data = reinterpret_cast<AutoSaveDialogData*>(GetWindowLongPtrW(hDlg, DWLP_USER));
                    data->enabled = IsDlgButtonChecked(hDlg, IDC_AUTOSAVE_ENABLE) == BST_CHECKED;
                    BOOL success = FALSE;
                    int val = GetDlgItemInt(hDlg, IDC_AUTOSAVE_INTERVAL, &success, FALSE);
                    if (success && val >= 5 && val <= 3600) {
                        data->intervalSeconds = val;
                    } else {
                        MessageBoxW(hDlg, L"Please enter an interval between 5 and 3600 seconds.",
                                    L"QNote", MB_OK | MB_ICONWARNING);
                        return TRUE;
                    }
                    EndDialog(hDlg, IDOK);
                    return TRUE;
                }
                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

void MainWindow::OnToolsAutoSaveOptions() {
    AppSettings& settings = m_settingsManager->GetSettings();
    AutoSaveDialogData data;
    data.enabled = settings.fileAutoSave;
    data.intervalSeconds = static_cast<int>(FILEAUTOSAVE_INTERVAL / 1000);
    
    INT_PTR result = DialogBoxParamW(m_hInstance, MAKEINTRESOURCEW(IDD_AUTOSAVE),
                                      m_hwnd, AutoSaveDlgProc, reinterpret_cast<LPARAM>(&data));
    if (result == IDOK) {
        settings.fileAutoSave = data.enabled;
        (void)m_settingsManager->Save();
        
        // Restart or stop the file auto-save timer
        KillTimer(m_hwnd, TIMER_FILEAUTOSAVE);
        if (data.enabled) {
            SetTimer(m_hwnd, TIMER_FILEAUTOSAVE, static_cast<UINT>(data.intervalSeconds * 1000), nullptr);
        }
    }
}

//------------------------------------------------------------------------------
// Tools -> URL Encode Selection
//------------------------------------------------------------------------------
void MainWindow::OnToolsUrlEncode() {
    std::wstring sel = m_editor->GetSelectedText();
    if (sel.empty()) return;
    
    // Convert to UTF-8 for encoding
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, sel.c_str(), static_cast<int>(sel.size()), nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 0) return;
    std::string utf8(utf8Len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, sel.c_str(), static_cast<int>(sel.size()), &utf8[0], utf8Len, nullptr, nullptr);
    
    std::string encoded;
    encoded.reserve(utf8.size() * 3);
    for (unsigned char c : utf8) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += static_cast<char>(c);
        } else {
            char hex[4];
            sprintf_s(hex, "%%%02X", c);
            encoded += hex;
        }
    }
    
    // Convert back to wide string
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, encoded.c_str(), static_cast<int>(encoded.size()), nullptr, 0);
    if (wideLen <= 0) return;
    std::wstring result(wideLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, encoded.c_str(), static_cast<int>(encoded.size()), &result[0], wideLen);
    
    m_editor->ReplaceSelection(result);
}

//------------------------------------------------------------------------------
// Tools -> URL Decode Selection
//------------------------------------------------------------------------------
void MainWindow::OnToolsUrlDecode() {
    std::wstring sel = m_editor->GetSelectedText();
    if (sel.empty()) return;
    
    // Convert to UTF-8 for decoding
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, sel.c_str(), static_cast<int>(sel.size()), nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 0) return;
    std::string utf8(utf8Len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, sel.c_str(), static_cast<int>(sel.size()), &utf8[0], utf8Len, nullptr, nullptr);
    
    std::string decoded;
    decoded.reserve(utf8.size());
    for (size_t i = 0; i < utf8.size(); ++i) {
        if (utf8[i] == '%' && i + 2 < utf8.size()) {
            char hex[3] = { utf8[i + 1], utf8[i + 2], '\0' };
            char* end = nullptr;
            long val = strtol(hex, &end, 16);
            if (end == hex + 2) {
                decoded += static_cast<char>(val);
                i += 2;
                continue;
            }
        }
        if (utf8[i] == '+') {
            decoded += ' ';
        } else {
            decoded += utf8[i];
        }
    }
    
    // Convert back to wide string
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, decoded.c_str(), static_cast<int>(decoded.size()), nullptr, 0);
    if (wideLen <= 0) return;
    std::wstring result(wideLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, decoded.c_str(), static_cast<int>(decoded.size()), &result[0], wideLen);
    
    m_editor->ReplaceSelection(result);
}

//------------------------------------------------------------------------------
// Tools -> Base64 Encode Selection
//------------------------------------------------------------------------------
void MainWindow::OnToolsBase64Encode() {
    std::wstring sel = m_editor->GetSelectedText();
    if (sel.empty()) return;
    
    // Convert to UTF-8
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, sel.c_str(), static_cast<int>(sel.size()), nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 0) return;
    std::string utf8(utf8Len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, sel.c_str(), static_cast<int>(sel.size()), &utf8[0], utf8Len, nullptr, nullptr);
    
    // Use CryptBinaryToStringA for Base64
    DWORD b64Len = 0;
    if (!CryptBinaryToStringA(reinterpret_cast<const BYTE*>(utf8.data()), static_cast<DWORD>(utf8.size()),
                               CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &b64Len)) {
        return;
    }
    std::string b64(b64Len, '\0');
    if (!CryptBinaryToStringA(reinterpret_cast<const BYTE*>(utf8.data()), static_cast<DWORD>(utf8.size()),
                               CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &b64[0], &b64Len)) {
        return;
    }
    b64.resize(b64Len);
    
    // Convert to wide string
    std::wstring result(b64.begin(), b64.end());
    m_editor->ReplaceSelection(result);
}

//------------------------------------------------------------------------------
// Tools -> Base64 Decode Selection
//------------------------------------------------------------------------------
void MainWindow::OnToolsBase64Decode() {
    std::wstring sel = m_editor->GetSelectedText();
    if (sel.empty()) return;
    
    // Convert selection to narrow string (Base64 is ASCII)
    std::string b64;
    b64.reserve(sel.size());
    for (wchar_t wc : sel) {
        b64 += static_cast<char>(wc & 0x7F);
    }
    
    // Decode
    DWORD binLen = 0;
    if (!CryptStringToBinaryA(b64.c_str(), static_cast<DWORD>(b64.size()),
                               CRYPT_STRING_BASE64, nullptr, &binLen, nullptr, nullptr)) {
        MessageBoxW(m_hwnd, L"Invalid Base64 data.", L"QNote", MB_OK | MB_ICONWARNING);
        return;
    }
    std::vector<BYTE> bin(binLen);
    if (!CryptStringToBinaryA(b64.c_str(), static_cast<DWORD>(b64.size()),
                               CRYPT_STRING_BASE64, bin.data(), &binLen, nullptr, nullptr)) {
        MessageBoxW(m_hwnd, L"Failed to decode Base64 data.", L"QNote", MB_OK | MB_ICONWARNING);
        return;
    }
    
    // Convert from UTF-8 to wide string
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(bin.data()),
                                       static_cast<int>(binLen), nullptr, 0);
    if (wideLen <= 0) return;
    std::wstring result(wideLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(bin.data()),
                         static_cast<int>(binLen), &result[0], wideLen);
    
    m_editor->ReplaceSelection(result);
}

//------------------------------------------------------------------------------
// Tools -> Insert Lorem Ipsum
//------------------------------------------------------------------------------
void MainWindow::OnToolsInsertLorem() {
    m_editor->ReplaceSelection(L"Lorem ipsum dolor sit amet, consectetur adipiscing elit.");
}

//------------------------------------------------------------------------------
// Tools -> Split Lines at Column
//------------------------------------------------------------------------------
INT_PTR CALLBACK MainWindow::SplitLinesDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG:
            SetWindowLongPtrW(hDlg, DWLP_USER, lParam);
            SetDlgItemInt(hDlg, IDC_SPLITLINES_WIDTH, 80, FALSE);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK: {
                    auto* pWidth = reinterpret_cast<int*>(GetWindowLongPtrW(hDlg, DWLP_USER));
                    BOOL success = FALSE;
                    int val = GetDlgItemInt(hDlg, IDC_SPLITLINES_WIDTH, &success, FALSE);
                    if (success && val >= 1 && val <= 999) {
                        *pWidth = val;
                        EndDialog(hDlg, IDOK);
                    } else {
                        MessageBoxW(hDlg, L"Please enter a width between 1 and 999.",
                                    L"QNote", MB_OK | MB_ICONWARNING);
                    }
                    return TRUE;
                }
                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

void MainWindow::OnToolsSplitLines() {
    int maxWidth = 80;
    INT_PTR result = DialogBoxParamW(m_hInstance, MAKEINTRESOURCEW(IDD_SPLITLINES),
                                      m_hwnd, SplitLinesDlgProc, reinterpret_cast<LPARAM>(&maxWidth));
    if (result != IDOK) return;
    
    // Operate on selection if present, otherwise on whole document
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
        
        // Split this line if it exceeds maxWidth
        while (static_cast<int>(line.size()) > maxWidth) {
            // Try to break at a space
            int breakPos = maxWidth;
            for (int i = maxWidth - 1; i > 0; --i) {
                if (line[i] == L' ' || line[i] == L'\t') {
                    breakPos = i + 1;
                    break;
                }
            }
            lines.push_back(line.substr(0, breakPos));
            line = line.substr(breakPos);
            // Trim leading spaces from the continuation
            size_t firstNonSpace = line.find_first_not_of(L' ');
            if (firstNonSpace != std::wstring::npos && firstNonSpace > 0) {
                line = line.substr(firstNonSpace);
            }
        }
        lines.push_back(line);
    }
    
    std::wstring resultText;
    for (size_t i = 0; i < lines.size(); ++i) {
        resultText += lines[i];
        if (i < lines.size() - 1) resultText += L"\r\n";
    }
    
    if (hasSelection) {
        m_editor->ReplaceSelection(resultText);
    } else {
        m_editor->SelectAll();
        m_editor->ReplaceSelection(resultText);
    }
}

//------------------------------------------------------------------------------
// Tools -> Tabs to Spaces
//------------------------------------------------------------------------------
void MainWindow::OnToolsTabsToSpaces() {
    std::wstring text = m_editor->GetText();
    if (text.empty()) return;
    
    int tabSize = m_settingsManager->GetSettings().tabSize;
    std::wstring spaces(tabSize, L' ');
    
    std::wstring result;
    result.reserve(text.size());
    for (wchar_t ch : text) {
        if (ch == L'\t') {
            result += spaces;
        } else {
            result += ch;
        }
    }
    
    if (result != text) {
        m_editor->SelectAll();
        m_editor->ReplaceSelection(result);
    }
}

//------------------------------------------------------------------------------
// Tools -> Spaces to Tabs
//------------------------------------------------------------------------------
void MainWindow::OnToolsSpacesToTabs() {
    std::wstring text = m_editor->GetText();
    if (text.empty()) return;
    
    int tabSize = m_settingsManager->GetSettings().tabSize;
    std::wstring spaces(tabSize, L' ');
    
    // Replace leading spaces with tabs on each line
    std::vector<std::wstring> lines;
    std::wistringstream stream(text);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        
        std::wstring newLine;
        size_t i = 0;
        // Convert leading spaces to tabs
        while (i + tabSize <= line.size()) {
            bool allSpaces = true;
            for (int j = 0; j < tabSize; ++j) {
                if (line[i + j] != L' ') { allSpaces = false; break; }
            }
            if (allSpaces) {
                newLine += L'\t';
                i += tabSize;
            } else {
                break;
            }
        }
        newLine += line.substr(i);
        lines.push_back(newLine);
    }
    
    std::wstring result;
    for (size_t i = 0; i < lines.size(); ++i) {
        result += lines[i];
        if (i < lines.size() - 1) result += L"\r\n";
    }
    
    if (result != text) {
        m_editor->SelectAll();
        m_editor->ReplaceSelection(result);
    }
}

//------------------------------------------------------------------------------
// Tools -> Word Count
//------------------------------------------------------------------------------
void MainWindow::OnToolsWordCount() {
    std::wstring text = m_editor->GetText();
    
    int charCount = static_cast<int>(text.size());
    int charNoSpaces = 0;
    int wordCount = 0;
    int lineCount = 1;
    int paragraphCount = 0;
    bool inWord = false;
    bool lastWasNewline = false;
    
    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t ch = text[i];
        
        if (ch != L' ' && ch != L'\t' && ch != L'\r' && ch != L'\n') {
            charNoSpaces++;
        }
        
        if (ch == L'\n') {
            lineCount++;
            if (lastWasNewline) {
                paragraphCount++;
            }
            lastWasNewline = true;
        } else if (ch != L'\r') {
            lastWasNewline = false;
        }
        
        if (ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n') {
            if (inWord) {
                wordCount++;
                inWord = false;
            }
        } else {
            inWord = true;
        }
    }
    if (inWord) wordCount++;
    if (charCount > 0) paragraphCount++; // count last paragraph
    
    // Also count selection stats
    std::wstring sel = m_editor->GetSelectedText();
    std::wstring selInfo;
    if (!sel.empty()) {
        int selChars = static_cast<int>(sel.size());
        int selWords = 0;
        bool selInWord = false;
        for (wchar_t ch : sel) {
            if (ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n') {
                if (selInWord) { selWords++; selInWord = false; }
            } else {
                selInWord = true;
            }
        }
        if (selInWord) selWords++;
        
        wchar_t selBuf[128];
        swprintf_s(selBuf, L"\n\nSelection:\n  Characters: %d\n  Words: %d", selChars, selWords);
        selInfo = selBuf;
    }
    
    wchar_t msg[512];
    swprintf_s(msg, 
        L"Document Statistics:\n"
        L"  Characters (with spaces): %d\n"
        L"  Characters (no spaces): %d\n"
        L"  Words: %d\n"
        L"  Lines: %d\n"
        L"  Paragraphs: %d%s",
        charCount, charNoSpaces, wordCount, lineCount, paragraphCount, selInfo.c_str());
    
    MessageBoxW(m_hwnd, msg, L"Word Count - QNote", MB_OK | MB_ICONINFORMATION);
}

//------------------------------------------------------------------------------
// Tools -> Remove Blank Lines
//------------------------------------------------------------------------------
void MainWindow::OnToolsRemoveBlankLines() {
    std::wstring text = m_editor->GetText();
    if (text.empty()) return;
    
    std::vector<std::wstring> lines;
    std::wistringstream stream(text);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        // Keep the line only if it's not blank
        if (line.find_first_not_of(L" \t") != std::wstring::npos) {
            lines.push_back(line);
        }
    }
    
    std::wstring result;
    for (size_t i = 0; i < lines.size(); ++i) {
        result += lines[i];
        if (i < lines.size() - 1) result += L"\r\n";
    }
    
    m_editor->SelectAll();
    m_editor->ReplaceSelection(result);
}

//------------------------------------------------------------------------------
// Tools -> Join Lines
//------------------------------------------------------------------------------
void MainWindow::OnToolsJoinLines() {
    std::wstring sel = m_editor->GetSelectedText();
    if (sel.empty()) return;
    
    std::wstring result;
    result.reserve(sel.size());
    bool lastWasNewline = false;
    for (size_t i = 0; i < sel.size(); ++i) {
        wchar_t ch = sel[i];
        if (ch == L'\r') continue; // skip CR
        if (ch == L'\n') {
            if (!lastWasNewline) {
                result += L' '; // replace first newline with space
            }
            lastWasNewline = true;
        } else {
            lastWasNewline = false;
            result += ch;
        }
    }
    
    m_editor->ReplaceSelection(result);
}

//------------------------------------------------------------------------------
// Tools -> Format JSON (pretty-print)
//------------------------------------------------------------------------------
void MainWindow::OnToolsFormatJson() {
    // Operate on selection or whole document
    bool hasSelection = false;
    DWORD selStart, selEnd;
    m_editor->GetSelection(selStart, selEnd);
    hasSelection = (selStart != selEnd);
    
    std::wstring text = hasSelection ? m_editor->GetSelectedText() : m_editor->GetText();
    if (text.empty()) return;
    
    // Simple JSON pretty-printer
    std::wstring result;
    result.reserve(text.size() * 2);
    int indent = 0;
    bool inString = false;
    bool escaped = false;
    
    auto addNewline = [&]() {
        result += L"\r\n";
        for (int i = 0; i < indent; ++i) result += L"    ";
    };
    
    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t ch = text[i];
        
        if (escaped) {
            result += ch;
            escaped = false;
            continue;
        }
        
        if (ch == L'\\' && inString) {
            result += ch;
            escaped = true;
            continue;
        }
        
        if (ch == L'"') {
            inString = !inString;
            result += ch;
            continue;
        }
        
        if (inString) {
            result += ch;
            continue;
        }
        
        // Skip existing whitespace outside strings
        if (ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n') {
            continue;
        }
        
        if (ch == L'{' || ch == L'[') {
            result += ch;
            // Check if empty object/array
            size_t next = i + 1;
            while (next < text.size() && (text[next] == L' ' || text[next] == L'\t' || 
                   text[next] == L'\r' || text[next] == L'\n')) next++;
            if (next < text.size() && ((ch == L'{' && text[next] == L'}') || 
                                        (ch == L'[' && text[next] == L']'))) {
                result += text[next];
                i = next;
            } else {
                indent++;
                addNewline();
            }
        } else if (ch == L'}' || ch == L']') {
            indent--;
            if (indent < 0) indent = 0;
            addNewline();
            result += ch;
        } else if (ch == L',') {
            result += ch;
            addNewline();
        } else if (ch == L':') {
            result += L": ";
        } else {
            result += ch;
        }
    }
    
    if (hasSelection) {
        m_editor->ReplaceSelection(result);
    } else {
        m_editor->SelectAll();
        m_editor->ReplaceSelection(result);
    }
}

//------------------------------------------------------------------------------
// Tools -> Minify JSON
//------------------------------------------------------------------------------
void MainWindow::OnToolsMinifyJson() {
    bool hasSelection = false;
    DWORD selStart, selEnd;
    m_editor->GetSelection(selStart, selEnd);
    hasSelection = (selStart != selEnd);
    
    std::wstring text = hasSelection ? m_editor->GetSelectedText() : m_editor->GetText();
    if (text.empty()) return;
    
    std::wstring result;
    result.reserve(text.size());
    bool inString = false;
    bool escaped = false;
    
    for (wchar_t ch : text) {
        if (escaped) {
            result += ch;
            escaped = false;
            continue;
        }
        if (ch == L'\\' && inString) {
            result += ch;
            escaped = true;
            continue;
        }
        if (ch == L'"') {
            inString = !inString;
            result += ch;
            continue;
        }
        if (inString) {
            result += ch;
            continue;
        }
        // Skip whitespace outside strings
        if (ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n') {
            continue;
        }
        result += ch;
    }
    
    if (hasSelection) {
        m_editor->ReplaceSelection(result);
    } else {
        m_editor->SelectAll();
        m_editor->ReplaceSelection(result);
    }
}

//------------------------------------------------------------------------------
// Tools -> Open Terminal Here
//------------------------------------------------------------------------------
void MainWindow::OnToolsOpenTerminal() {
    std::wstring dir;
    if (!m_currentFile.empty() && !m_isNewFile) {
        size_t pos = m_currentFile.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
            dir = m_currentFile.substr(0, pos);
        }
    }
    
    if (dir.empty()) {
        wchar_t cwd[MAX_PATH] = {};
        GetCurrentDirectoryW(MAX_PATH, cwd);
        dir = cwd;
    }
    
    // Open PowerShell in the directory
    ShellExecuteW(nullptr, L"open", L"powershell.exe", nullptr, dir.c_str(), SW_SHOWNORMAL);
}

//------------------------------------------------------------------------------
// Tools -> Settings
//------------------------------------------------------------------------------
void MainWindow::OnToolsSettings() {
    AppSettings& settings = m_settingsManager->GetSettings();
    AppSettings oldSettings = settings;
    
    if (SettingsWindow::Show(m_hwnd, m_hInstance, settings)) {
        // Apply font changes
        if (settings.fontName != oldSettings.fontName ||
            settings.fontSize != oldSettings.fontSize ||
            settings.fontWeight != oldSettings.fontWeight ||
            settings.fontItalic != oldSettings.fontItalic) {
            // Apply to all editors (all tabs)
            if (m_documentManager) {
                m_documentManager->ApplySettingsToAllEditors(settings);
            }
        }
        
        // Apply editor behavior changes (to all tabs)
        if (settings.wordWrap != oldSettings.wordWrap ||
            settings.tabSize != oldSettings.tabSize ||
            settings.scrollLines != oldSettings.scrollLines ||
            settings.rightToLeft != oldSettings.rightToLeft ||
            settings.showWhitespace != oldSettings.showWhitespace ||
            settings.zoomLevel != oldSettings.zoomLevel) {
            if (m_documentManager) {
                m_documentManager->ApplySettingsToAllEditors(settings);
            }
        }
        
        // Apply view changes
        if (settings.showStatusBar != oldSettings.showStatusBar) {
            ShowWindow(m_hwndStatus, settings.showStatusBar ? SW_SHOW : SW_HIDE);
            ResizeControls();
        }
        if (settings.showLineNumbers != oldSettings.showLineNumbers) {
            if (m_lineNumbersGutter && m_editor) {
                m_lineNumbersGutter->SetFont(m_editor->GetFont());
                m_lineNumbersGutter->Show(settings.showLineNumbers);
            }
            ResizeControls();
        }
        
        // Apply auto-save timer changes
        if (settings.saveStyle != oldSettings.saveStyle ||
            settings.autoSaveDelayMs != oldSettings.autoSaveDelayMs) {
            KillTimer(m_hwnd, TIMER_REALSAVE);
            if (settings.saveStyle == SaveStyle::AutoSave) {
                SetTimer(m_hwnd, TIMER_REALSAVE, settings.autoSaveDelayMs, nullptr);
            }
        }
        
        UpdateStatusBar();
        UpdateMenuState();
    }
}

//------------------------------------------------------------------------------
// Tools -> Calculate Selection
//------------------------------------------------------------------------------
void MainWindow::OnToolsCalculate() {
    std::wstring sel = m_editor->GetSelectedText();
    if (sel.empty()) return;

    // Simple expression evaluator: supports +, -, *, /, parentheses, decimals
    // Convert to narrow string for parsing
    std::string expr;
    for (wchar_t ch : sel) {
        if (ch < 128) expr += static_cast<char>(ch);
    }

    // Remove whitespace
    expr.erase(std::remove_if(expr.begin(), expr.end(), ::isspace), expr.end());
    if (expr.empty()) return;

    // Shunting-yard algorithm for evaluation
    std::vector<double> values;
    std::vector<char> ops;

    auto precedence = [](char op) -> int {
        if (op == '+' || op == '-') return 1;
        if (op == '*' || op == '/') return 2;
        return 0;
    };

    auto applyOp = [](double a, double b, char op) -> double {
        switch (op) {
            case '+': return a + b;
            case '-': return a - b;
            case '*': return a * b;
            case '/': return (b != 0) ? a / b : 0;
            default: return 0;
        }
    };

    auto evaluate = [&]() {
        if (values.size() < 2 || ops.empty()) return;
        double b = values.back(); values.pop_back();
        double a = values.back(); values.pop_back();
        char op = ops.back(); ops.pop_back();
        values.push_back(applyOp(a, b, op));
    };

    bool valid = true;
    for (size_t i = 0; i < expr.size() && valid; ) {
        char ch = expr[i];

        if (isdigit(ch) || ch == '.') {
            // Parse number
            size_t end;
            double num = 0;
            try {
                num = std::stod(expr.substr(i), &end);
            } catch (...) {
                valid = false;
                break;
            }
            values.push_back(num);
            i += end;
        } else if (ch == '(') {
            ops.push_back(ch);
            i++;
        } else if (ch == ')') {
            while (!ops.empty() && ops.back() != '(') evaluate();
            if (!ops.empty()) ops.pop_back(); // remove '('
            i++;
        } else if (ch == '+' || ch == '-' || ch == '*' || ch == '/') {
            // Handle unary minus
            if (ch == '-' && (i == 0 || expr[i - 1] == '(' || expr[i - 1] == '+' ||
                expr[i - 1] == '-' || expr[i - 1] == '*' || expr[i - 1] == '/')) {
                // Unary minus: parse the next number as negative
                size_t end;
                double num = 0;
                try {
                    num = std::stod(expr.substr(i), &end);
                } catch (...) {
                    valid = false;
                    break;
                }
                values.push_back(num);
                i += end;
                continue;
            }
            while (!ops.empty() && ops.back() != '(' &&
                   precedence(ops.back()) >= precedence(ch)) {
                evaluate();
            }
            ops.push_back(ch);
            i++;
        } else {
            valid = false;
        }
    }

    while (!ops.empty() && valid) evaluate();

    if (!valid || values.size() != 1) {
        MessageBoxW(m_hwnd, L"Could not evaluate the selected expression.",
                    L"Calculate", MB_OK | MB_ICONWARNING);
        return;
    }

    double result = values.back();

    // Format result
    wchar_t buf[64];
    if (result == static_cast<int64_t>(result) && std::abs(result) < 1e15) {
        swprintf_s(buf, L"%lld", static_cast<int64_t>(result));
    } else {
        swprintf_s(buf, L"%.10g", result);
    }

    // Show result and offer to insert
    std::wstring msg = sel + L" = " + buf;
    int answer = MessageBoxW(m_hwnd, (msg + L"\n\nReplace selection with result?").c_str(),
                             L"Calculate", MB_YESNO | MB_ICONINFORMATION);
    if (answer == IDYES) {
        m_editor->ReplaceSelection(buf);
    }
}

//------------------------------------------------------------------------------
// Tools -> Insert GUID/UUID
//------------------------------------------------------------------------------
void MainWindow::OnToolsInsertGuid() {
    GUID guid;
    if (FAILED(CoCreateGuid(&guid))) {
        MessageBoxW(m_hwnd, L"Failed to generate GUID.", L"QNote", MB_OK | MB_ICONERROR);
        return;
    }

    wchar_t buf[40];
    swprintf_s(buf, L"%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
               guid.Data1, guid.Data2, guid.Data3,
               guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
               guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

    m_editor->ReplaceSelection(buf);
}

//------------------------------------------------------------------------------
// Tools -> Insert File Path (browse for a file)
//------------------------------------------------------------------------------
void MainWindow::OnToolsInsertFilePath() {
    wchar_t filePath[MAX_PATH] = {};
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"All Files\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        m_editor->ReplaceSelection(filePath);
    }
}

//------------------------------------------------------------------------------
// Tools -> Convert Line Endings in Selection
//------------------------------------------------------------------------------
void MainWindow::OnToolsConvertEolSelection() {
    std::wstring sel = m_editor->GetSelectedText();
    if (sel.empty()) {
        MessageBoxW(m_hwnd, L"Please select text first.", L"Convert Line Endings", MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Ask which EOL to convert to
    int choice = MessageBoxW(m_hwnd,
        L"Convert line endings in selection to:\n\n"
        L"[Yes] = CRLF (Windows)\n"
        L"[No] = LF (Unix)\n"
        L"[Cancel] = Cancel",
        L"Convert Line Endings", MB_YESNOCANCEL | MB_ICONQUESTION);

    if (choice == IDCANCEL) return;

    // Normalize to LF first
    std::wstring result;
    result.reserve(sel.size());
    for (size_t i = 0; i < sel.size(); ++i) {
        if (sel[i] == L'\r') {
            if (i + 1 < sel.size() && sel[i + 1] == L'\n') ++i; // skip CR in CRLF
            result += L'\n'; // normalize to LF
        } else {
            result += sel[i];
        }
    }

    if (choice == IDYES) {
        // Convert LF to CRLF efficiently using find-and-replace
        std::wstring crlf;
        crlf.reserve(result.size() + result.size() / 4);  // Estimate ~25% growth
        size_t prev = 0;
        size_t pos = 0;
        while ((pos = result.find(L'\n', prev)) != std::wstring::npos) {
            crlf.append(result, prev, pos - prev);
            crlf.append(L"\r\n");
            prev = pos + 1;
        }
        crlf.append(result, prev, result.size() - prev);
        result = std::move(crlf);
    }
    // choice == IDNO: keep as LF

    m_editor->ReplaceSelection(result);
}

//------------------------------------------------------------------------------
// Tools -> Checksum (MD5/SHA-256 of selection or file)
//------------------------------------------------------------------------------
void MainWindow::OnToolsChecksum() {
    // Get text to hash - selection or whole file
    bool hasSelection = false;
    DWORD selStart, selEnd;
    m_editor->GetSelection(selStart, selEnd);
    hasSelection = (selStart != selEnd);

    std::wstring text = hasSelection ? m_editor->GetSelectedText() : m_editor->GetText();
    if (text.empty()) {
        MessageBoxW(m_hwnd, L"Nothing to hash.", L"Checksum", MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Convert to UTF-8 bytes for hashing
    int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                  nullptr, 0, nullptr, nullptr);
    std::vector<BYTE> utf8(len);
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        reinterpret_cast<char*>(utf8.data()), len, nullptr, nullptr);

    // Compute SHA-256 using BCrypt
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    std::wstring sha256Str, md5Str;

    // SHA-256
    if (BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
        DWORD hashLen = 0, resultLen = 0;
        BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLen),
                          sizeof(hashLen), &resultLen, 0);
        std::vector<BYTE> hashBuf(hashLen);

        if (BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0))) {
            BCryptHashData(hHash, utf8.data(), static_cast<ULONG>(utf8.size()), 0);
            BCryptFinishHash(hHash, hashBuf.data(), hashLen, 0);
            BCryptDestroyHash(hHash);

            // Convert to hex string
            for (DWORD i = 0; i < hashLen; ++i) {
                wchar_t hex[4];
                swprintf_s(hex, L"%02x", hashBuf[i]);
                sha256Str += hex;
            }
        }
        BCryptCloseAlgorithmProvider(hAlg, 0);
    }

    // MD5
    hAlg = nullptr;
    if (BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_MD5_ALGORITHM, nullptr, 0))) {
        DWORD hashLen = 0, resultLen = 0;
        BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLen),
                          sizeof(hashLen), &resultLen, 0);
        std::vector<BYTE> hashBuf(hashLen);

        if (BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0))) {
            BCryptHashData(hHash, utf8.data(), static_cast<ULONG>(utf8.size()), 0);
            BCryptFinishHash(hHash, hashBuf.data(), hashLen, 0);
            BCryptDestroyHash(hHash);

            for (DWORD i = 0; i < hashLen; ++i) {
                wchar_t hex[4];
                swprintf_s(hex, L"%02x", hashBuf[i]);
                md5Str += hex;
            }
        }
        BCryptCloseAlgorithmProvider(hAlg, 0);
    }

    std::wstring msg = L"MD5:     " + md5Str + L"\nSHA-256: " + sha256Str +
                       L"\n\nSource: " + (hasSelection ? L"Selection" : L"Entire document") +
                       L"\n\nCopy SHA-256 to clipboard?";

    int answer = MessageBoxW(m_hwnd, msg.c_str(), L"Checksum", MB_YESNO | MB_ICONINFORMATION);
    if (answer == IDYES) {
        // Copy SHA-256 to clipboard
        if (OpenClipboard(m_hwnd)) {
            EmptyClipboard();
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (sha256Str.size() + 1) * sizeof(wchar_t));
            if (hMem) {
                wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
                wcscpy_s(pMem, sha256Str.size() + 1, sha256Str.c_str());
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
            CloseClipboard();
        }
    }
}

//------------------------------------------------------------------------------
// Tools -> Run Selection as Command
//------------------------------------------------------------------------------
void MainWindow::OnToolsRunSelection() {
    std::wstring sel = m_editor->GetSelectedText();
    if (sel.empty()) return;

    // Trim whitespace
    size_t start = sel.find_first_not_of(L" \t\r\n");
    size_t end = sel.find_last_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return;
    sel = sel.substr(start, end - start + 1);

    // Run via cmd.exe and capture output
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) return;
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;

    std::wstring cmdLine = L"cmd.exe /C " + sel;

    PROCESS_INFORMATION pi = {};
    BOOL created = CreateProcessW(nullptr, &cmdLine[0], nullptr, nullptr, TRUE,
                                  CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hWritePipe);

    std::string output;
    if (created) {
        // Read pipe output BEFORE waiting for process to avoid deadlock
        // (child may block writing to a full pipe buffer)
        char buf[4096];
        DWORD bytesRead;
        while (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buf[bytesRead] = 0;
            output += buf;
        }

        WaitForSingleObject(pi.hProcess, 10000); // 10 second timeout
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    CloseHandle(hReadPipe);

    // Convert output to wide string
    int wideLen = MultiByteToWideChar(CP_OEMCP, 0, output.c_str(), static_cast<int>(output.size()),
                                      nullptr, 0);
    std::wstring wideOutput(wideLen, 0);
    MultiByteToWideChar(CP_OEMCP, 0, output.c_str(), static_cast<int>(output.size()),
                        &wideOutput[0], wideLen);

    // Show output in a message box (or insert into document)
    if (wideOutput.empty()) wideOutput = L"(no output)";

    std::wstring msg = L"Command: " + sel + L"\n\nOutput:\n" + wideOutput +
                       L"\n\nInsert output at cursor?";
    int answer = MessageBoxW(m_hwnd, msg.c_str(), L"Run Selection", MB_YESNO | MB_ICONINFORMATION);
    if (answer == IDYES) {
        // Move cursor to end of selection and insert
        DWORD s, e;
        m_editor->GetSelection(s, e);
        m_editor->SetSelection(e, e);
        m_editor->ReplaceSelection((L"\r\n" + wideOutput));
    }
}

} // namespace QNote
