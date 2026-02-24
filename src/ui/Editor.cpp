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
    
    // Set undo limit to 100 levels
    SendMessageW(m_hwndEdit, EM_SETUNDOLIMIT, UNDO_LIMIT, 0);
    
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
        SetWindowTextW(m_hwndEdit, std::wstring(text).c_str());
        ApplyCharFormat();
        SetModified(false);
    }
}

//------------------------------------------------------------------------------
// Clear text
//------------------------------------------------------------------------------
void Editor::Clear() noexcept {
    if (m_hwndEdit) {
        SetWindowTextW(m_hwndEdit, L"");
        SetModified(false);
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
    DWORD start, end;
    GetSelection(start, end);
    
    if (start == end) return L"";
    
    std::wstring text = GetText();
    if (end > text.size()) end = static_cast<DWORD>(text.size());
    if (start > end) std::swap(start, end);
    
    return text.substr(start, end - start);
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
// Can undo
//------------------------------------------------------------------------------
bool Editor::CanUndo() const noexcept {
    if (m_hwndEdit) {
        return SendMessageW(m_hwndEdit, EM_CANUNDO, 0, 0) != 0;
    }
    return false;
}

//------------------------------------------------------------------------------
// Can redo (RichEdit supports full redo)
//------------------------------------------------------------------------------
bool Editor::CanRedo() const noexcept {
    if (m_hwndEdit) {
        return SendMessageW(m_hwndEdit, EM_CANREDO, 0, 0) != 0;
    }
    return false;
}

//------------------------------------------------------------------------------
// Undo
//------------------------------------------------------------------------------
void Editor::Undo() noexcept {
    if (m_hwndEdit) {
        SendMessageW(m_hwndEdit, EM_UNDO, 0, 0);
    }
}

//------------------------------------------------------------------------------
// Redo (RichEdit supports full redo)
//------------------------------------------------------------------------------
void Editor::Redo() noexcept {
    if (m_hwndEdit) {
        SendMessageW(m_hwndEdit, EM_REDO, 0, 0);
    }
}

//------------------------------------------------------------------------------
// Cut
//------------------------------------------------------------------------------
void Editor::Cut() noexcept {
    if (m_hwndEdit) {
        SendMessageW(m_hwndEdit, WM_CUT, 0, 0);
    }
}

//------------------------------------------------------------------------------
// Copy
//------------------------------------------------------------------------------
void Editor::Copy() noexcept {
    if (m_hwndEdit) {
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
        SendMessageW(m_hwndEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_font.get()), TRUE);
        ApplyCharFormat();
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
        SendMessageW(m_hwndEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_font.get()), TRUE);
        ApplyCharFormat();
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
// Find text
//------------------------------------------------------------------------------
bool Editor::SearchText(std::wstring_view searchText, bool matchCase, bool wrapAround,
                        bool searchUp, bool useRegex, bool selectMatch) {
    if (searchText.empty() || !m_hwndEdit) {
        return false;
    }
    
    std::wstring text = GetText();
    if (text.empty()) {
        return false;
    }
    
    DWORD selStart, selEnd;
    GetSelection(selStart, selEnd);
    
    // Start search from after current selection (or before if searching up)
    size_t searchStart = searchUp ? (selStart > 0 ? selStart - 1 : 0) : selEnd;
    
    size_t foundPos = std::wstring::npos;
    size_t foundLen = searchText.length();
    
    const std::wstring searchStr(searchText);
    
    if (useRegex) {
        try {
            std::wregex::flag_type flags = std::regex::ECMAScript;
            if (!matchCase) {
                flags |= std::regex::icase;
            }
            std::wregex regex(searchStr, flags);
            
            std::wsmatch match;
            if (searchUp) {
                // Search backwards - find all matches up to searchStart
                std::wstring searchRegion = text.substr(0, searchStart);
                auto it = searchRegion.cbegin();
                size_t lastPos = std::wstring::npos;
                while (std::regex_search(it, searchRegion.cend(), match, regex)) {
                    lastPos = std::distance(searchRegion.cbegin(), it) + match.position();
                    foundLen = match.length();
                    it = match[0].second;
                }
                foundPos = lastPos;
                
                // Wrap around if needed
                if (foundPos == std::wstring::npos && wrapAround) {
                    std::wstring wrapRegion = text.substr(searchStart);
                    it = wrapRegion.cbegin();
                    while (std::regex_search(it, wrapRegion.cend(), match, regex)) {
                        lastPos = searchStart + std::distance(wrapRegion.cbegin(), it) + match.position();
                        foundLen = match.length();
                        it = match[0].second;
                    }
                    foundPos = lastPos;
                }
            } else {
                std::wstring searchRegion = text.substr(searchStart);
                if (std::regex_search(searchRegion, match, regex)) {
                    foundPos = searchStart + match.position();
                    foundLen = match.length();
                } else if (wrapAround) {
                    // Wrap around
                    searchRegion = text.substr(0, searchStart);
                    if (std::regex_search(searchRegion, match, regex)) {
                        foundPos = match.position();
                        foundLen = match.length();
                    }
                }
            }
        } catch (const std::regex_error&) {
            // Invalid regex
            return false;
        }
    } else {
        // Plain text search
        if (searchUp) {
            if (matchCase) {
                foundPos = text.rfind(searchStr, searchStart > 0 ? searchStart - 1 : 0);
            } else {
                // Case-insensitive search backwards
                std::wstring lowerText = text;
                std::wstring lowerSearch = searchStr;
                for (auto& c : lowerText) c = towlower(c);
                for (auto& c : lowerSearch) c = towlower(c);
                foundPos = lowerText.rfind(lowerSearch, searchStart > 0 ? searchStart - 1 : 0);
            }
            
            if (foundPos == std::wstring::npos && wrapAround) {
                // Wrap to end
                if (matchCase) {
                    foundPos = text.rfind(searchStr);
                } else {
                    std::wstring lowerText = text;
                    std::wstring lowerSearch = searchStr;
                    for (auto& c : lowerText) c = towlower(c);
                    for (auto& c : lowerSearch) c = towlower(c);
                    foundPos = lowerText.rfind(lowerSearch);
                }
            }
        } else {
            if (matchCase) {
                foundPos = text.find(searchStr, searchStart);
            } else {
                // Case-insensitive search
                std::wstring lowerText = text;
                std::wstring lowerSearch = searchStr;
                for (auto& c : lowerText) c = towlower(c);
                for (auto& c : lowerSearch) c = towlower(c);
                foundPos = lowerText.find(lowerSearch, searchStart);
            }
            
            if (foundPos == std::wstring::npos && wrapAround) {
                // Wrap to beginning
                if (matchCase) {
                    foundPos = text.find(searchStr, 0);
                } else {
                    std::wstring lowerText = text;
                    std::wstring lowerSearch = searchStr;
                    for (auto& c : lowerText) c = towlower(c);
                    for (auto& c : lowerSearch) c = towlower(c);
                    foundPos = lowerText.find(lowerSearch, 0);
                }
            }
        }
    }
    
    if (foundPos != std::wstring::npos && selectMatch) {
        SetSelection(static_cast<DWORD>(foundPos), static_cast<DWORD>(foundPos + foundLen));
        return true;
    }
    
    return foundPos != std::wstring::npos;
}

//------------------------------------------------------------------------------
// Replace all occurrences
//------------------------------------------------------------------------------
int Editor::ReplaceAll(std::wstring_view searchText, std::wstring_view replaceText,
                       bool matchCase, bool useRegex) {
    if (searchText.empty() || !m_hwndEdit) {
        return 0;
    }
    
    std::wstring text = GetText();
    if (text.empty()) {
        return 0;
    }
    
    const std::wstring searchStr(searchText);
    const std::wstring replaceStr(replaceText);
    
    int count = 0;
    std::wstring result;
    
    if (useRegex) {
        try {
            std::wregex::flag_type flags = std::regex::ECMAScript;
            if (!matchCase) {
                flags |= std::regex::icase;
            }
            std::wregex regex(searchStr, flags);
            
            // Count matches
            auto begin = std::wsregex_iterator(text.begin(), text.end(), regex);
            auto end = std::wsregex_iterator();
            count = static_cast<int>(std::distance(begin, end));
            
            // Replace all
            result = std::regex_replace(text, regex, replaceStr);
        } catch (const std::regex_error&) {
            return 0;
        }
    } else {
        result.reserve(text.size());
        size_t pos = 0;
        size_t searchLen = searchStr.length();
        
        std::wstring searchLower;
        std::wstring textLower;
        if (!matchCase) {
            searchLower = searchStr;
            textLower = text;
            for (auto& c : searchLower) c = towlower(c);
            for (auto& c : textLower) c = towlower(c);
        }
        
        while (pos < text.size()) {
            size_t found;
            if (matchCase) {
                found = text.find(searchStr, pos);
            } else {
                found = textLower.find(searchLower, pos);
            }
            
            if (found == std::wstring::npos) {
                result.append(text.substr(pos));
                break;
            }
            
            result.append(text.substr(pos, found - pos));
            result.append(replaceStr);
            pos = found + searchLen;
            count++;
        }
    }
    
    if (count > 0) {
        SetText(result);
        SetModified(true);
    }
    
    return count;
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
// Recreate edit control (for word wrap toggle)
//------------------------------------------------------------------------------
void Editor::RecreateControl() {
    if (!m_hwndEdit) return;
    
    // Save state
    std::wstring text = GetText();
    DWORD selStart, selEnd;
    GetSelection(selStart, selEnd);
    bool modified = IsModified();
    
    // Get current position
    RECT rect;
    GetWindowRect(m_hwndEdit, &rect);
    MapWindowPoints(HWND_DESKTOP, m_hwndParent, reinterpret_cast<LPPOINT>(&rect), 2);
    
    // Remove subclass before destroying
    RemoveWindowSubclass(m_hwndEdit, EditSubclassProc, EDIT_SUBCLASS_ID);
    DestroyWindow(m_hwndEdit);
    
    // Create new control with updated style
    DWORD style = WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_NOHIDESEL;
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
        
        // Set undo limit to 100 levels
        SendMessageW(m_hwndEdit, EM_SETUNDOLIMIT, UNDO_LIMIT, 0);
        
        // Apply font
        if (m_font.get()) {
            SendMessageW(m_hwndEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_font.get()), TRUE);
            ApplyCharFormat();
        }
        
        // Set tab stops
        SetTabSize(m_tabSize);
        
        // Restore state
        SetWindowTextW(m_hwndEdit, text.c_str());
        SetSelection(selStart, selEnd);
        SetModified(modified);
        
        // Re-subclass
        SetWindowSubclass(m_hwndEdit, EditSubclassProc, EDIT_SUBCLASS_ID,
                          reinterpret_cast<DWORD_PTR>(this));
        
        ::SetFocus(m_hwndEdit);
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
// Edit control subclass procedure
//------------------------------------------------------------------------------
LRESULT CALLBACK Editor::EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, 
                                           LPARAM lParam, UINT_PTR subclassId, 
                                           DWORD_PTR refData) {
    Editor* editor = reinterpret_cast<Editor*>(refData);
    
    switch (msg) {
        case WM_PAINT: {
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            if (editor->m_showWhitespace) {
                HDC hdc = GetDC(hwnd);
                editor->DrawWhitespace(hdc);
                ReleaseDC(hwnd, hdc);
            }
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
            // Handle key, then notify for potential scroll
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
                if (editor->m_scrollCallback) {
                    editor->m_scrollCallback(editor->m_scrollCallbackData);
                }
                return result;
            }

        case WM_PASTE: {
            // Paste may insert multiple lines; update line numbers after paste
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            if (editor->m_scrollCallback) {
                editor->m_scrollCallback(editor->m_scrollCallbackData);
            }
            return result;
        }

        case WM_CHAR: {
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
                return result;
            }

            // Content change may affect line numbers
            LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
            if (editor->m_scrollCallback) {
                editor->m_scrollCallback(editor->m_scrollCallbackData);
            }
            return result;
        }
    }
    
    return DefSubclassProc(hwnd, msg, wParam, lParam);
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
