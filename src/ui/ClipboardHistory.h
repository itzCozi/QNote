//==============================================================================
// QNote - A Lightweight Notepad Clone
// ClipboardHistory.h - Clipboard ring buffer with popup picker
//==============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <string>
#include <vector>
#include <functional>

namespace QNote {

//------------------------------------------------------------------------------
// A single clipboard history entry
//------------------------------------------------------------------------------
struct ClipEntry {
    std::wstring text;
    SYSTEMTIME   timestamp;
};

//------------------------------------------------------------------------------
// Clipboard history manager & popup picker
//
// Monitors the clipboard via WM_CLIPBOARDUPDATE and stores the last N text
// entries.  Provides a popup list (Ctrl+Shift+V) for the user to pick one.
//------------------------------------------------------------------------------
class ClipboardHistory {
public:
    ClipboardHistory() = default;
    ~ClipboardHistory();

    ClipboardHistory(const ClipboardHistory&) = delete;
    ClipboardHistory& operator=(const ClipboardHistory&) = delete;

    // Register as clipboard listener (call once after main window creation).
    void Initialize(HWND mainWnd) noexcept;

    // Unregister clipboard listener.
    void Shutdown() noexcept;

    // Call this from the main window's WM_CLIPBOARDUPDATE handler.
    void OnClipboardUpdate();

    // Show the picker popup near the caret in the given window.
    // insertCallback receives the chosen text.
    void ShowPicker(HWND parent, HINSTANCE hInstance,
                    std::function<void(const std::wstring&)> insertCallback);

    // Close picker if open.
    void ClosePicker() noexcept;

    // Forward messages for the picker (keyboard navigation).
    [[nodiscard]] bool IsPickerMessage(MSG* pMsg) noexcept;

    // Is the picker visible?
    [[nodiscard]] bool IsPickerVisible() const noexcept;

    // Clear all history.
    void Clear() noexcept;

    // Access history.
    [[nodiscard]] const std::vector<ClipEntry>& GetEntries() const noexcept { return m_entries; }

    // Maximum number of stored entries (public so MainWindow could make it configurable)
    static constexpr int MAX_ENTRIES = 25;

private:
    // Internal: capture current clipboard text into history.
    void CaptureClipboard();

    // Popup window
    static LRESULT CALLBACK PickerProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandlePickerMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void PaintPicker(HDC hdc);
    void PickerSelect(int index);
    int  PickerHitTest(int y) const;
    void PickerScrollTo(int pos);
    void PickerUpdateScrollBar();
    void PickerEnsureVisible(int index);

    // Picker layout constants
    static constexpr int ITEM_HEIGHT   = 48;
    static constexpr int PICKER_WIDTH  = 420;
    static constexpr int PICKER_MAX_H  = 400;
    static constexpr int TIMESTAMP_W   = 60;

    HWND m_mainWnd = nullptr;
    bool m_listening = false;

    std::vector<ClipEntry> m_entries;

    // Picker state
    HWND m_pickerWnd = nullptr;
    HFONT m_pickerFont = nullptr;
    HFONT m_pickerFontSmall = nullptr;
    int m_pickerSelected = 0;
    int m_pickerScroll = 0;
    std::function<void(const std::wstring&)> m_insertCallback;

    // Suppress capturing our own paste operations
    bool m_suppressCapture = false;

    static constexpr wchar_t PICKER_CLASS[] = L"QNoteClipboardPicker";
    static bool s_pickerClassRegistered;
};

} // namespace QNote
