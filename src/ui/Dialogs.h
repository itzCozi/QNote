//==============================================================================
// QNote - A Lightweight Notepad Clone
// Dialogs.h - Find/Replace, GoTo, and other dialog boxes
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

namespace QNote {

// Forward declarations to avoid including full headers
class Editor;
struct AppSettings;

//------------------------------------------------------------------------------
// Find dialog parameters
//------------------------------------------------------------------------------
struct FindParams {
    std::wstring searchText;
    bool matchCase = false;
    bool wrapAround = true;
    bool useRegex = false;
    bool searchUp = false;
};

//------------------------------------------------------------------------------
// Replace dialog parameters
//------------------------------------------------------------------------------
struct ReplaceParams {
    std::wstring searchText;
    std::wstring replaceText;
    bool matchCase = false;
    bool wrapAround = true;
    bool useRegex = false;
};

//------------------------------------------------------------------------------
// Dialog manager class
//------------------------------------------------------------------------------
class DialogManager {
public:
    DialogManager();
    ~DialogManager();
    
    // Prevent copying
    DialogManager(const DialogManager&) = delete;
    DialogManager& operator=(const DialogManager&) = delete;
    
    // Initialize with parent window and editor reference
    void Initialize(HWND parent, HINSTANCE hInstance, Editor* editor, AppSettings* settings) noexcept;
    
    // Show Find dialog (modeless)
    void ShowFindDialog();
    
    // Show Replace dialog (modeless)
    void ShowReplaceDialog();
    
    // Show Go To Line dialog (modal)
    [[nodiscard]] bool ShowGoToDialog(int& lineNumber);
    
    // Show Tab Size dialog (modal)
    [[nodiscard]] bool ShowTabSizeDialog(int& tabSize);
    
    // Show Font chooser dialog (modal)
    [[nodiscard]] bool ShowFontDialog(std::wstring& fontName, int& fontSize, int& fontWeight, bool& italic);
    
    // Show About dialog (modal)
    void ShowAboutDialog();
    
    // Close any open modeless dialogs
    void CloseFindDialog() noexcept;
    void CloseReplaceDialog() noexcept;
    
    // Check if a dialog is currently open
    [[nodiscard]] bool IsFindDialogOpen() const noexcept { return m_hwndFind != nullptr; }
    [[nodiscard]] bool IsReplaceDialogOpen() const noexcept { return m_hwndReplace != nullptr; }
    
    // Execute Find Next (for F3 key)
    void FindNext();
    
    // Get find parameters (for settings persistence)
    [[nodiscard]] const FindParams& GetFindParams() const noexcept { return m_findParams; }
    
    // Handle dialog messages in the main message loop
    [[nodiscard]] bool IsDialogMessage(MSG* pMsg) noexcept;
    
private:
    // Dialog procedures
    static INT_PTR CALLBACK FindDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK ReplaceDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK GoToDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK TabSizeDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK AboutDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Internal handlers
    void OnFindInit(HWND hwnd);
    void OnFindNext(HWND hwnd);
    void OnFindClose(HWND hwnd);
    
    void OnReplaceInit(HWND hwnd);
    void OnReplace(HWND hwnd);
    void OnReplaceAll(HWND hwnd);
    void OnReplaceClose(HWND hwnd);
    
private:
    HWND m_hwndParent = nullptr;
    HINSTANCE m_hInstance = nullptr;
    Editor* m_editor = nullptr;
    AppSettings* m_settings = nullptr;
    
    // Modeless dialog handles
    HWND m_hwndFind = nullptr;
    HWND m_hwndReplace = nullptr;
    
    // Search parameters
    FindParams m_findParams;
    ReplaceParams m_replaceParams;
    
    // Static pointer for dialog procedures
    static DialogManager* s_instance;
};

} // namespace QNote
