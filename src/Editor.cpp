//==============================================================================
// QNote - A Lightweight Notepad Clone
// Editor.cpp - Edit control wrapper and text operations implementation
//==============================================================================

#include "Editor.h"
#include "resource.h"
#include <CommCtrl.h>
#include <regex>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "comctl32.lib")

namespace QNote {

// Subclass ID for edit control
static constexpr UINT_PTR EDIT_SUBCLASS_ID = 1;

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
Editor::Editor() {
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
    
    // Determine extended style (RTL support)
    DWORD exStyle = WS_EX_CLIENTEDGE;
    if (m_rtl) {
        exStyle |= WS_EX_RTLREADING | WS_EX_RIGHT;
    }
    
    // Create the edit control
    m_hwndEdit = CreateWindowExW(
        exStyle,
        L"EDIT",
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
    
    // Set text limit to maximum
    SendMessageW(m_hwndEdit, EM_SETLIMITTEXT, 0, 0);
    
    // Create and set font
    m_font.reset(CreateEditorFont());
    if (m_font.get()) {
        SendMessageW(m_hwndEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_font.get()), TRUE);
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
void Editor::Destroy() {
    if (m_hwndEdit) {
        RemoveWindowSubclass(m_hwndEdit, EditSubclassProc, EDIT_SUBCLASS_ID);
        DestroyWindow(m_hwndEdit);
        m_hwndEdit = nullptr;
    }
    
    if (m_darkBrush) {
        DeleteObject(m_darkBrush);
        m_darkBrush = nullptr;
    }
}

//------------------------------------------------------------------------------
// Resize the edit control
//------------------------------------------------------------------------------
void Editor::Resize(int x, int y, int width, int height) {
    if (m_hwndEdit) {
        SetWindowPos(m_hwndEdit, nullptr, x, y, width, height, SWP_NOZORDER);
    }
}

//------------------------------------------------------------------------------
// Focus the edit control
//------------------------------------------------------------------------------
void Editor::SetFocus() {
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
void Editor::SetText(const std::wstring& text) {
    if (m_hwndEdit) {
        SetWindowTextW(m_hwndEdit, text.c_str());
        SetModified(false);
    }
}

//------------------------------------------------------------------------------
// Clear text
//------------------------------------------------------------------------------
void Editor::Clear() {
    SetText(L"");
}

//------------------------------------------------------------------------------
// Get selection range
//------------------------------------------------------------------------------
void Editor::GetSelection(DWORD& start, DWORD& end) const {
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
void Editor::SetSelection(DWORD start, DWORD end) {
    if (m_hwndEdit) {
        SendMessageW(m_hwndEdit, EM_SETSEL, start, end);
        SendMessageW(m_hwndEdit, EM_SCROLLCARET, 0, 0);
    }
}

//------------------------------------------------------------------------------
// Select all text
//------------------------------------------------------------------------------
void Editor::SelectAll() {
    SetSelection(0, -1);
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
void Editor::ReplaceSelection(const std::wstring& text) {
    if (m_hwndEdit) {
        SendMessageW(m_hwndEdit, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(text.c_str()));
    }
}

//------------------------------------------------------------------------------
// Can undo
//------------------------------------------------------------------------------
bool Editor::CanUndo() const {
    if (m_hwndEdit) {
        return SendMessageW(m_hwndEdit, EM_CANUNDO, 0, 0) != 0;
    }
    return false;
}

//------------------------------------------------------------------------------
// Can redo (Windows edit control has limited redo - single level)
//------------------------------------------------------------------------------
bool Editor::CanRedo() const {
    // Standard edit control doesn't support redo well
    // We would need RichEdit for proper redo support
    return false;
}

//------------------------------------------------------------------------------
// Undo
//------------------------------------------------------------------------------
void Editor::Undo() {
    if (m_hwndEdit) {
        SendMessageW(m_hwndEdit, EM_UNDO, 0, 0);
    }
}

//------------------------------------------------------------------------------
// Redo
//------------------------------------------------------------------------------
void Editor::Redo() {
    // Standard edit control doesn't support redo
    // Would need to use RichEdit control for this
}

//------------------------------------------------------------------------------
// Cut
//------------------------------------------------------------------------------
void Editor::Cut() {
    if (m_hwndEdit) {
        SendMessageW(m_hwndEdit, WM_CUT, 0, 0);
    }
}

//------------------------------------------------------------------------------
// Copy
//------------------------------------------------------------------------------
void Editor::Copy() {
    if (m_hwndEdit) {
        SendMessageW(m_hwndEdit, WM_COPY, 0, 0);
    }
}

//------------------------------------------------------------------------------
// Paste
//------------------------------------------------------------------------------
void Editor::Paste() {
    if (m_hwndEdit) {
        SendMessageW(m_hwndEdit, WM_PASTE, 0, 0);
    }
}

//------------------------------------------------------------------------------
// Delete selection
//------------------------------------------------------------------------------
void Editor::Delete() {
    if (m_hwndEdit) {
        SendMessageW(m_hwndEdit, WM_CLEAR, 0, 0);
    }
}

//------------------------------------------------------------------------------
// Get line count
//------------------------------------------------------------------------------
int Editor::GetLineCount() const {
    if (m_hwndEdit) {
        return static_cast<int>(SendMessageW(m_hwndEdit, EM_GETLINECOUNT, 0, 0));
    }
    return 0;
}

//------------------------------------------------------------------------------
// Get current line (0-based)
//------------------------------------------------------------------------------
int Editor::GetCurrentLine() const {
    if (!m_hwndEdit) return 0;
    
    DWORD start, end;
    GetSelection(start, end);
    return GetLineFromChar(start);
}

//------------------------------------------------------------------------------
// Get current column (0-based)
//------------------------------------------------------------------------------
int Editor::GetCurrentColumn() const {
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
int Editor::GetLineFromChar(DWORD charIndex) const {
    if (m_hwndEdit) {
        return static_cast<int>(SendMessageW(m_hwndEdit, EM_LINEFROMCHAR, charIndex, 0));
    }
    return 0;
}

//------------------------------------------------------------------------------
// Get character index of line start
//------------------------------------------------------------------------------
int Editor::GetLineIndex(int line) const {
    if (m_hwndEdit) {
        return static_cast<int>(SendMessageW(m_hwndEdit, EM_LINEINDEX, line, 0));
    }
    return 0;
}

//------------------------------------------------------------------------------
// Get line length
//------------------------------------------------------------------------------
int Editor::GetLineLength(int line) const {
    if (m_hwndEdit) {
        int lineIndex = GetLineIndex(line);
        return static_cast<int>(SendMessageW(m_hwndEdit, EM_LINELENGTH, lineIndex, 0));
    }
    return 0;
}

//------------------------------------------------------------------------------
// Go to line
//------------------------------------------------------------------------------
void Editor::GoToLine(int line) {
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
void Editor::SetFont(const std::wstring& fontName, int fontSize, int fontWeight, bool italic) {
    m_fontName = fontName;
    m_baseFontSize = fontSize;
    m_fontWeight = fontWeight;
    m_fontItalic = italic;
    
    // Recreate font with zoom applied
    m_font.reset(CreateEditorFont());
    if (m_hwndEdit && m_font.get()) {
        SendMessageW(m_hwndEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_font.get()), TRUE);
    }
}

//------------------------------------------------------------------------------
// Apply zoom level
//------------------------------------------------------------------------------
void Editor::ApplyZoom(int zoomPercent) {
    if (zoomPercent < 25) zoomPercent = 25;
    if (zoomPercent > 500) zoomPercent = 500;
    
    m_zoomPercent = zoomPercent;
    
    // Recreate font with new zoom
    m_font.reset(CreateEditorFont());
    if (m_hwndEdit && m_font.get()) {
        SendMessageW(m_hwndEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_font.get()), TRUE);
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
void Editor::SetTabSize(int tabSize) {
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
bool Editor::IsModified() const {
    if (m_hwndEdit) {
        return SendMessageW(m_hwndEdit, EM_GETMODIFY, 0, 0) != 0;
    }
    return false;
}

//------------------------------------------------------------------------------
// Set modified state
//------------------------------------------------------------------------------
void Editor::SetModified(bool modified) {
    if (m_hwndEdit) {
        SendMessageW(m_hwndEdit, EM_SETMODIFY, modified, 0);
    }
}

//------------------------------------------------------------------------------
// Find text
//------------------------------------------------------------------------------
bool Editor::SearchText(const std::wstring& searchText, bool matchCase, bool wrapAround,
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
    
    if (useRegex) {
        try {
            std::wregex::flag_type flags = std::regex::ECMAScript;
            if (!matchCase) {
                flags |= std::regex::icase;
            }
            std::wregex regex(searchText, flags);
            
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
                foundPos = text.rfind(searchText, searchStart > 0 ? searchStart - 1 : 0);
            } else {
                // Case-insensitive search backwards
                std::wstring lowerText = text;
                std::wstring lowerSearch = searchText;
                for (auto& c : lowerText) c = towlower(c);
                for (auto& c : lowerSearch) c = towlower(c);
                foundPos = lowerText.rfind(lowerSearch, searchStart > 0 ? searchStart - 1 : 0);
            }
            
            if (foundPos == std::wstring::npos && wrapAround) {
                // Wrap to end
                if (matchCase) {
                    foundPos = text.rfind(searchText);
                } else {
                    std::wstring lowerText = text;
                    std::wstring lowerSearch = searchText;
                    for (auto& c : lowerText) c = towlower(c);
                    for (auto& c : lowerSearch) c = towlower(c);
                    foundPos = lowerText.rfind(lowerSearch);
                }
            }
        } else {
            if (matchCase) {
                foundPos = text.find(searchText, searchStart);
            } else {
                // Case-insensitive search
                std::wstring lowerText = text;
                std::wstring lowerSearch = searchText;
                for (auto& c : lowerText) c = towlower(c);
                for (auto& c : lowerSearch) c = towlower(c);
                foundPos = lowerText.find(lowerSearch, searchStart);
            }
            
            if (foundPos == std::wstring::npos && wrapAround) {
                // Wrap to beginning
                if (matchCase) {
                    foundPos = text.find(searchText, 0);
                } else {
                    std::wstring lowerText = text;
                    std::wstring lowerSearch = searchText;
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
int Editor::ReplaceAll(const std::wstring& searchText, const std::wstring& replaceText,
                       bool matchCase, bool useRegex) {
    if (searchText.empty() || !m_hwndEdit) {
        return 0;
    }
    
    std::wstring text = GetText();
    if (text.empty()) {
        return 0;
    }
    
    int count = 0;
    std::wstring result;
    
    if (useRegex) {
        try {
            std::wregex::flag_type flags = std::regex::ECMAScript;
            if (!matchCase) {
                flags |= std::regex::icase;
            }
            std::wregex regex(searchText, flags);
            
            // Count matches
            auto begin = std::wsregex_iterator(text.begin(), text.end(), regex);
            auto end = std::wsregex_iterator();
            count = static_cast<int>(std::distance(begin, end));
            
            // Replace all
            result = std::regex_replace(text, regex, replaceText);
        } catch (const std::regex_error&) {
            return 0;
        }
    } else {
        result.reserve(text.size());
        size_t pos = 0;
        size_t searchLen = searchText.length();
        
        std::wstring searchLower;
        std::wstring textLower;
        if (!matchCase) {
            searchLower = searchText;
            textLower = text;
            for (auto& c : searchLower) c = towlower(c);
            for (auto& c : textLower) c = towlower(c);
        }
        
        while (pos < text.size()) {
            size_t found;
            if (matchCase) {
                found = text.find(searchText, pos);
            } else {
                found = textLower.find(searchLower, pos);
            }
            
            if (found == std::wstring::npos) {
                result.append(text.substr(pos));
                break;
            }
            
            result.append(text.substr(pos, found - pos));
            result.append(replaceText);
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
void Editor::SetLineEnding(LineEnding lineEnding) {
    m_lineEnding = lineEnding;
}

//------------------------------------------------------------------------------
// Set encoding
//------------------------------------------------------------------------------
void Editor::SetEncoding(TextEncoding encoding) {
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
// Set dark mode
//------------------------------------------------------------------------------
void Editor::SetDarkMode(bool darkMode) {
    m_darkMode = darkMode;
    
    if (m_darkBrush) {
        DeleteObject(m_darkBrush);
        m_darkBrush = nullptr;
    }
    
    if (darkMode) {
        m_darkBrush = CreateSolidBrush(RGB(30, 30, 30));
    }
    
    if (m_hwndEdit) {
        InvalidateRect(m_hwndEdit, nullptr, TRUE);
    }
}

//------------------------------------------------------------------------------
// Set right-to-left reading order
//------------------------------------------------------------------------------
void Editor::SetRTL(bool rtl) {
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
int Editor::GetTextLength() const {
    if (m_hwndEdit) {
        return GetWindowTextLengthW(m_hwndEdit);
    }
    return 0;
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
    
    // Determine extended style (RTL support)
    DWORD exStyle = WS_EX_CLIENTEDGE;
    if (m_rtl) {
        exStyle |= WS_EX_RTLREADING | WS_EX_RIGHT;
    }
    
    m_hwndEdit = CreateWindowExW(
        exStyle,
        L"EDIT",
        L"",
        style,
        rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
        m_hwndParent,
        nullptr,
        m_hInstance,
        nullptr
    );
    
    if (m_hwndEdit) {
        // Set unlimited text
        SendMessageW(m_hwndEdit, EM_SETLIMITTEXT, 0, 0);
        
        // Apply font
        if (m_font.get()) {
            SendMessageW(m_hwndEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_font.get()), TRUE);
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
// Create editor font with zoom applied
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
            break;
        }
        
        case WM_KEYDOWN: {
            // Additional key handling if needed
            break;
        }
    }
    
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

} // namespace QNote
