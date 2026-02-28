//==============================================================================
// QNote - A Lightweight Notepad Clone
// ClipboardHistory.cpp - Clipboard ring buffer with popup picker
//==============================================================================

#include "ClipboardHistory.h"
#include <windowsx.h>
#include <algorithm>
#include <ctime>
#include <sstream>

namespace QNote {

bool ClipboardHistory::s_pickerClassRegistered = false;

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
ClipboardHistory::~ClipboardHistory() {
    Shutdown();
    ClosePicker();
}

//------------------------------------------------------------------------------
// Initialize - register as clipboard format listener
//------------------------------------------------------------------------------
void ClipboardHistory::Initialize(HWND mainWnd) noexcept {
    m_mainWnd = mainWnd;
    if (!m_listening && mainWnd) {
        AddClipboardFormatListener(mainWnd);
        m_listening = true;
    }
}

//------------------------------------------------------------------------------
// Shutdown - unregister listener
//------------------------------------------------------------------------------
void ClipboardHistory::Shutdown() noexcept {
    if (m_listening && m_mainWnd) {
        RemoveClipboardFormatListener(m_mainWnd);
        m_listening = false;
    }
}

//------------------------------------------------------------------------------
// OnClipboardUpdate - called from main WndProc on WM_CLIPBOARDUPDATE
//------------------------------------------------------------------------------
void ClipboardHistory::OnClipboardUpdate() {
    if (m_suppressCapture) return;
    CaptureClipboard();
}

//------------------------------------------------------------------------------
// CaptureClipboard - read current clipboard text and push to front
//------------------------------------------------------------------------------
void ClipboardHistory::CaptureClipboard() {
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return;
    if (!OpenClipboard(m_mainWnd)) return;

    HGLOBAL hMem = GetClipboardData(CF_UNICODETEXT);
    if (hMem) {
        const wchar_t* pText = static_cast<const wchar_t*>(GlobalLock(hMem));
        if (pText) {
            std::wstring text = pText;
            GlobalUnlock(hMem);

            // Don't add empty or whitespace-only strings
            if (text.find_first_not_of(L" \t\r\n") != std::wstring::npos) {
                // Remove duplicate if already in history
                for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
                    if (it->text == text) {
                        m_entries.erase(it);
                        break;
                    }
                }

                // Push to front
                ClipEntry entry;
                entry.text = std::move(text);
                GetLocalTime(&entry.timestamp);
                m_entries.insert(m_entries.begin(), std::move(entry));

                // Trim to max
                while (static_cast<int>(m_entries.size()) > MAX_ENTRIES) {
                    m_entries.pop_back();
                }
            }
        }
    }
    CloseClipboard();
}

//------------------------------------------------------------------------------
// Clear
//------------------------------------------------------------------------------
void ClipboardHistory::Clear() noexcept {
    m_entries.clear();
}

//------------------------------------------------------------------------------
// ShowPicker - popup the clipboard history list
//------------------------------------------------------------------------------
void ClipboardHistory::ShowPicker(HWND parent, HINSTANCE hInstance,
                                  std::function<void(const std::wstring&)> insertCallback) {
    if (m_entries.empty()) {
        MessageBoxW(parent, L"Clipboard history is empty.", L"Clipboard History",
                    MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Close existing picker
    ClosePicker();

    m_insertCallback = std::move(insertCallback);
    m_pickerSelected = 0;
    m_pickerScroll = 0;

    // Register class once
    if (!s_pickerClassRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
        wc.lpfnWndProc = PickerProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = PICKER_CLASS;
        RegisterClassExW(&wc);
        s_pickerClassRegistered = true;
    }

    // Create fonts
    if (!m_pickerFont) {
        m_pickerFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Consolas");
    }
    if (!m_pickerFontSmall) {
        m_pickerFontSmall = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    }

    // Calculate window height
    int itemCount = static_cast<int>(m_entries.size());
    int totalH = itemCount * ITEM_HEIGHT;
    int winH = std::min(totalH + 2, PICKER_MAX_H);

    // Position: try to place near the caret, fallback to center of parent
    POINT pt = { 0, 0 };
    GetCaretPos(&pt);
    ClientToScreen(parent, &pt);

    // Offset below the caret
    pt.y += 20;

    // Ensure it stays on screen
    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    if (pt.x + PICKER_WIDTH > workArea.right) pt.x = workArea.right - PICKER_WIDTH;
    if (pt.y + winH > workArea.bottom) pt.y = pt.y - winH - 40;
    if (pt.x < workArea.left) pt.x = workArea.left;
    if (pt.y < workArea.top) pt.y = workArea.top;

    m_pickerWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        PICKER_CLASS,
        L"Clipboard History",
        WS_POPUP | WS_BORDER | WS_VSCROLL,
        pt.x, pt.y, PICKER_WIDTH, winH,
        parent,
        nullptr,
        hInstance,
        this);

    if (!m_pickerWnd) return;

    PickerUpdateScrollBar();
    ShowWindow(m_pickerWnd, SW_SHOW);
    SetForegroundWindow(m_pickerWnd);
    ::SetFocus(m_pickerWnd);
}

//------------------------------------------------------------------------------
// ClosePicker
//------------------------------------------------------------------------------
void ClipboardHistory::ClosePicker() noexcept {
    if (m_pickerWnd && IsWindow(m_pickerWnd)) {
        DestroyWindow(m_pickerWnd);
    }
    m_pickerWnd = nullptr;

    if (m_pickerFont)      { DeleteObject(m_pickerFont);      m_pickerFont = nullptr; }
    if (m_pickerFontSmall) { DeleteObject(m_pickerFontSmall); m_pickerFontSmall = nullptr; }
}

//------------------------------------------------------------------------------
// IsPickerVisible
//------------------------------------------------------------------------------
bool ClipboardHistory::IsPickerVisible() const noexcept {
    return m_pickerWnd && IsWindow(m_pickerWnd) && ::IsWindowVisible(m_pickerWnd);
}

//------------------------------------------------------------------------------
// IsPickerMessage
//------------------------------------------------------------------------------
bool ClipboardHistory::IsPickerMessage(MSG* pMsg) noexcept {
    if (!m_pickerWnd || !IsWindow(m_pickerWnd)) return false;
    if (pMsg->hwnd != m_pickerWnd) return false;

    if (pMsg->message == WM_KEYDOWN) {
        switch (pMsg->wParam) {
        case VK_UP:
            m_pickerSelected = std::max(0, m_pickerSelected - 1);
            PickerEnsureVisible(m_pickerSelected);
            InvalidateRect(m_pickerWnd, nullptr, FALSE);
            return true;
        case VK_DOWN:
            m_pickerSelected = std::min(static_cast<int>(m_entries.size()) - 1, m_pickerSelected + 1);
            PickerEnsureVisible(m_pickerSelected);
            InvalidateRect(m_pickerWnd, nullptr, FALSE);
            return true;
        case VK_RETURN:
            PickerSelect(m_pickerSelected);
            return true;
        case VK_ESCAPE:
            ClosePicker();
            return true;
        case VK_DELETE:
            // Delete entry from history
            if (m_pickerSelected >= 0 && m_pickerSelected < static_cast<int>(m_entries.size())) {
                m_entries.erase(m_entries.begin() + m_pickerSelected);
                if (m_entries.empty()) {
                    ClosePicker();
                } else {
                    m_pickerSelected = std::min(m_pickerSelected,
                                                static_cast<int>(m_entries.size()) - 1);
                    PickerUpdateScrollBar();
                    InvalidateRect(m_pickerWnd, nullptr, TRUE);
                }
            }
            return true;
        }
    }

    return false;
}

//------------------------------------------------------------------------------
// Picker window procedure
//------------------------------------------------------------------------------
LRESULT CALLBACK ClipboardHistory::PickerProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ClipboardHistory* pThis = nullptr;

    if (msg == WM_NCCREATE) {
        auto* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<ClipboardHistory*>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_pickerWnd = hwnd;
    } else {
        pThis = reinterpret_cast<ClipboardHistory*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandlePickerMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

//------------------------------------------------------------------------------
// Handle picker messages
//------------------------------------------------------------------------------
LRESULT ClipboardHistory::HandlePickerMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(m_pickerWnd, &ps);
        PaintPicker(hdc);
        EndPaint(m_pickerWnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int y = GET_Y_LPARAM(lParam);
        int idx = PickerHitTest(y);
        if (idx >= 0) {
            PickerSelect(idx);
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        int y = GET_Y_LPARAM(lParam);
        int idx = PickerHitTest(y);
        if (idx >= 0 && idx != m_pickerSelected) {
            m_pickerSelected = idx;
            InvalidateRect(m_pickerWnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        PickerScrollTo(m_pickerScroll - delta);
        return 0;
    }

    case WM_VSCROLL: {
        SCROLLINFO si = {};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(m_pickerWnd, SB_VERT, &si);

        int pos = si.nPos;
        switch (LOWORD(wParam)) {
            case SB_LINEUP:   pos -= ITEM_HEIGHT; break;
            case SB_LINEDOWN: pos += ITEM_HEIGHT; break;
            case SB_PAGEUP:   pos -= si.nPage; break;
            case SB_PAGEDOWN: pos += si.nPage; break;
            case SB_THUMBTRACK: pos = si.nTrackPos; break;
        }
        PickerScrollTo(pos);
        return 0;
    }

    case WM_ACTIVATE:
        // Close when the popup loses focus
        if (LOWORD(wParam) == WA_INACTIVE) {
            ClosePicker();
        }
        return 0;

    case WM_KILLFOCUS:
        // Close when focus moves elsewhere
        if (reinterpret_cast<HWND>(wParam) != m_pickerWnd) {
            ClosePicker();
        }
        return 0;

    case WM_DESTROY:
        m_pickerWnd = nullptr;
        return 0;

    default:
        break;
    }
    return DefWindowProcW(m_pickerWnd, msg, wParam, lParam);
}

//------------------------------------------------------------------------------
// PickerHitTest
//------------------------------------------------------------------------------
int ClipboardHistory::PickerHitTest(int y) const {
    int idx = (y + m_pickerScroll) / ITEM_HEIGHT;
    if (idx >= 0 && idx < static_cast<int>(m_entries.size())) return idx;
    return -1;
}

//------------------------------------------------------------------------------
// PickerSelect - insert the selected entry
//------------------------------------------------------------------------------
void ClipboardHistory::PickerSelect(int index) {
    if (index < 0 || index >= static_cast<int>(m_entries.size())) return;

    std::wstring text = m_entries[index].text;

    // Move selected entry to front of history
    if (index > 0) {
        ClipEntry entry = std::move(m_entries[index]);
        m_entries.erase(m_entries.begin() + index);
        m_entries.insert(m_entries.begin(), std::move(entry));
    }

    ClosePicker();

    // Also put it on the real clipboard (suppress self-capture)
    m_suppressCapture = true;
    if (OpenClipboard(m_mainWnd)) {
        EmptyClipboard();
        size_t len = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
        if (hMem) {
            wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
            if (pMem) {
                wcscpy_s(pMem, text.size() + 1, text.c_str());
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
        }
        CloseClipboard();
    }
    m_suppressCapture = false;

    if (m_insertCallback) {
        m_insertCallback(text);
    }
}

//------------------------------------------------------------------------------
// PickerScrollTo
//------------------------------------------------------------------------------
void ClipboardHistory::PickerScrollTo(int pos) {
    RECT rc;
    GetClientRect(m_pickerWnd, &rc);
    int totalH = static_cast<int>(m_entries.size()) * ITEM_HEIGHT;
    int visH = static_cast<int>(rc.bottom - rc.top);
    int maxScroll = std::max(0, totalH - visH);

    m_pickerScroll = std::clamp(pos, 0, maxScroll);
    PickerUpdateScrollBar();
    InvalidateRect(m_pickerWnd, nullptr, TRUE);
}

//------------------------------------------------------------------------------
// PickerUpdateScrollBar
//------------------------------------------------------------------------------
void ClipboardHistory::PickerUpdateScrollBar() {
    if (!m_pickerWnd) return;

    RECT rc;
    GetClientRect(m_pickerWnd, &rc);
    int visH = rc.bottom - rc.top;
    int totalH = static_cast<int>(m_entries.size()) * ITEM_HEIGHT;

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = totalH;
    si.nPage = visH;
    si.nPos = m_pickerScroll;
    SetScrollInfo(m_pickerWnd, SB_VERT, &si, TRUE);
}

//------------------------------------------------------------------------------
// PickerEnsureVisible
//------------------------------------------------------------------------------
void ClipboardHistory::PickerEnsureVisible(int index) {
    if (!m_pickerWnd) return;

    RECT rc;
    GetClientRect(m_pickerWnd, &rc);
    int visH = rc.bottom - rc.top;

    int top = index * ITEM_HEIGHT;
    int bot = top + ITEM_HEIGHT;

    if (top < m_pickerScroll) {
        PickerScrollTo(top);
    } else if (bot > m_pickerScroll + visH) {
        PickerScrollTo(bot - visH);
    }
}

//------------------------------------------------------------------------------
// Helper: create a single-line preview from text
//------------------------------------------------------------------------------
static std::wstring MakePreview(const std::wstring& text, size_t maxLen = 120) {
    std::wstring result;
    result.reserve(std::min(text.size(), maxLen + 3));

    for (size_t i = 0; i < text.size() && result.size() < maxLen; i++) {
        wchar_t ch = text[i];
        if (ch == L'\r' || ch == L'\n') {
            if (!result.empty() && result.back() != L' ') result += L' ';
            // Skip \r\n pair
            if (ch == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n') i++;
            continue;
        }
        if (ch == L'\t') {
            result += L' ';
            continue;
        }
        result += ch;
    }

    if (text.size() > maxLen) {
        result += L"\u2026";  // ellipsis
    }

    return result;
}

//------------------------------------------------------------------------------
// PaintPicker
//------------------------------------------------------------------------------
void ClipboardHistory::PaintPicker(HDC hdc) {
    RECT rc;
    GetClientRect(m_pickerWnd, &rc);
    int cw = rc.right;
    int ch = rc.bottom;

    // Double buffer
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, cw, ch);
    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDC, memBmp));

    // Background
    HBRUSH bgBrush = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
    FillRect(memDC, &rc, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(memDC, TRANSPARENT);

    HFONT oldFont = static_cast<HFONT>(SelectObject(memDC, m_pickerFont));

    for (int i = 0; i < static_cast<int>(m_entries.size()); i++) {
        int itemTop = i * ITEM_HEIGHT - m_pickerScroll;
        int itemBot = itemTop + ITEM_HEIGHT;

        // Skip items not visible
        if (itemBot < 0 || itemTop > ch) continue;

        RECT itemRect = { 0, itemTop, cw, itemBot };

        // Highlight selected item
        if (i == m_pickerSelected) {
            HBRUSH selBrush = CreateSolidBrush(GetSysColor(COLOR_HIGHLIGHT));
            FillRect(memDC, &itemRect, selBrush);
            DeleteObject(selBrush);
            SetTextColor(memDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
        } else {
            SetTextColor(memDC, GetSysColor(COLOR_WINDOWTEXT));
        }

        // Draw index number
        SelectObject(memDC, m_pickerFontSmall);
        wchar_t indexStr[16];
        swprintf_s(indexStr, L" %d", i + 1);
        RECT idxRect = { 0, itemTop, 28, itemTop + ITEM_HEIGHT };
        DrawTextW(memDC, indexStr, -1, &idxRect, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);

        // Draw text preview (first two lines)
        SelectObject(memDC, m_pickerFont);
        std::wstring preview = MakePreview(m_entries[i].text, 200);

        // Split into two visual lines for the preview
        RECT textRect = { 34, itemTop + 4, cw - TIMESTAMP_W - 4, itemTop + 24 };
        DrawTextW(memDC, preview.c_str(), static_cast<int>(preview.size()),
                  &textRect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // Show length info on second line
        SelectObject(memDC, m_pickerFontSmall);
        COLORREF prevColor = SetTextColor(memDC, GetSysColor(
            i == m_pickerSelected ? COLOR_HIGHLIGHTTEXT : COLOR_GRAYTEXT));

        wchar_t lengthStr[64];
        size_t textLen = m_entries[i].text.size();
        if (textLen > 1000) {
            swprintf_s(lengthStr, L"%zu chars", textLen);
        } else {
            // Count lines
            int lineCount = 1;
            for (wchar_t c : m_entries[i].text) {
                if (c == L'\n') lineCount++;
            }
            if (lineCount > 1) {
                swprintf_s(lengthStr, L"%zu chars, %d lines", textLen, lineCount);
            } else {
                swprintf_s(lengthStr, L"%zu chars", textLen);
            }
        }
        RECT lenRect = { 34, itemTop + 26, cw - TIMESTAMP_W - 4, itemBot - 2 };
        DrawTextW(memDC, lengthStr, -1, &lenRect, DT_SINGLELINE | DT_NOPREFIX);
        SetTextColor(memDC, prevColor);

        // Draw timestamp on the right
        COLORREF tsColor = SetTextColor(memDC, GetSysColor(
            i == m_pickerSelected ? COLOR_HIGHLIGHTTEXT : COLOR_GRAYTEXT));
        wchar_t timeStr[16];
        swprintf_s(timeStr, L"%02d:%02d",
                   m_entries[i].timestamp.wHour, m_entries[i].timestamp.wMinute);
        RECT tsRect = { cw - TIMESTAMP_W, itemTop, cw - 4, itemTop + ITEM_HEIGHT };
        DrawTextW(memDC, timeStr, -1, &tsRect, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
        SetTextColor(memDC, tsColor);

        // Separator line
        if (i < static_cast<int>(m_entries.size()) - 1) {
            HPEN pen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNFACE));
            HPEN oldPen = static_cast<HPEN>(SelectObject(memDC, pen));
            MoveToEx(memDC, 4, itemBot - 1, nullptr);
            LineTo(memDC, cw - 4, itemBot - 1);
            SelectObject(memDC, oldPen);
            DeleteObject(pen);
        }
    }

    SelectObject(memDC, oldFont);

    BitBlt(hdc, 0, 0, cw, ch, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

} // namespace QNote
