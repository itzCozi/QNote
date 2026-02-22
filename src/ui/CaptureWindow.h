//==============================================================================
// QNote - A Lightweight Notepad Clone
// CaptureWindow.h - Quick capture popup window (global hotkey)
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
#include <functional>
#include "NoteStore.h"

namespace QNote {

//------------------------------------------------------------------------------
// Capture window callback types
//------------------------------------------------------------------------------
using CaptureCallback = std::function<void(const std::wstring& text)>;
using CaptureEditCallback = std::function<void(const std::wstring& noteId)>;

//------------------------------------------------------------------------------
// Quick capture window class - always on top popup
//------------------------------------------------------------------------------
class CaptureWindow {
public:
    CaptureWindow();
    ~CaptureWindow();
    
    // Prevent copying
    CaptureWindow(const CaptureWindow&) = delete;
    CaptureWindow& operator=(const CaptureWindow&) = delete;
    
    // Initialize the capture window (create hidden)
    [[nodiscard]] bool Create(HINSTANCE hInstance, NoteStore* noteStore);
    
    // Show/hide the capture window
    void Show();
    void Hide();
    void Toggle();
    [[nodiscard]] bool IsVisible() const;
    
    // Set callback for when a note is captured
    void SetCaptureCallback(CaptureCallback callback) { m_captureCallback = callback; }
    
    // Set callback for when user wants to edit in main window
    void SetEditCallback(CaptureEditCallback callback) { m_editCallback = callback; }
    
    // Get window handle
    [[nodiscard]] HWND GetHandle() const { return m_hwnd; }
    
private:
    // Window procedure
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Message handlers
    void OnCreate();
    void OnDestroy();
    void OnSize(int width, int height);
    void OnCommand(WORD id, WORD code, HWND hwndCtl);
    void OnKeyDown(WPARAM key);
    LRESULT OnCtlColorEdit(HDC hdc, HWND hwndEdit);
    LRESULT OnCtlColorStatic(HDC hdc, HWND hwndStatic);
    
    // Save the current note
    void SaveNote();
    
    // Clear and focus the edit
    void ClearAndFocus();
    
    // Pin/star the current note
    void TogglePinNote();
    
    // Position window near cursor or center of primary monitor
    void PositionWindow();
    
    // Create child controls
    void CreateControls();
    
private:
    HWND m_hwnd = nullptr;
    HWND m_hwndEdit = nullptr;
    HWND m_hwndSaveBtn = nullptr;
    HWND m_hwndPinBtn = nullptr;
    HWND m_hwndEditBtn = nullptr;
    HWND m_hwndHintLabel = nullptr;
    HINSTANCE m_hInstance = nullptr;
    HFONT m_hFont = nullptr;
    HFONT m_hSmallFont = nullptr;
    
    NoteStore* m_noteStore = nullptr;
    std::wstring m_currentNoteId;
    bool m_isPinned = false;
    
    CaptureCallback m_captureCallback;
    CaptureEditCallback m_editCallback;
    
    // Window dimensions
    static constexpr int WINDOW_WIDTH = 400;
    static constexpr int WINDOW_HEIGHT = 200;
    static constexpr int PADDING = 10;
    static constexpr int BUTTON_HEIGHT = 28;
    static constexpr int BUTTON_WIDTH = 70;
    
    // Window class name
    static constexpr wchar_t WINDOW_CLASS[] = L"QNoteCaptureWindow";
    
    // Control IDs
    static constexpr int IDC_CAPTURE_EDIT = 1001;
    static constexpr int IDC_SAVE_BTN = 1002;
    static constexpr int IDC_PIN_BTN = 1003;
    static constexpr int IDC_EDIT_BTN = 1004;
};

//------------------------------------------------------------------------------
// Global hotkey manager
//------------------------------------------------------------------------------
class GlobalHotkeyManager {
public:
    GlobalHotkeyManager();
    ~GlobalHotkeyManager();
    
    // Register the global hotkey (Ctrl+Shift+Q by default)
    [[nodiscard]] bool Register(HWND targetWindow, int hotkeyId = 1);
    
    // Unregister the hotkey
    void Unregister();
    
    // Check if registered
    [[nodiscard]] bool IsRegistered() const { return m_registered; }
    
    // Get the hotkey ID
    [[nodiscard]] int GetHotkeyId() const { return m_hotkeyId; }
    
    // Get hotkey description for display
    [[nodiscard]] std::wstring GetHotkeyDescription() const;
    
private:
    HWND m_targetWindow = nullptr;
    int m_hotkeyId = 0;
    bool m_registered = false;
    
    // Default: Ctrl+Shift+Q
    UINT m_modifiers = MOD_CONTROL | MOD_SHIFT;
    UINT m_virtualKey = 'Q';
};

} // namespace QNote
