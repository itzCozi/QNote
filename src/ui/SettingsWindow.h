//==============================================================================
// QNote - A Lightweight Notepad Clone
// SettingsWindow.h - Application settings dialog
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
#include "Settings.h"

namespace QNote {

//------------------------------------------------------------------------------
// Settings window class - modal tabbed dialog for all application settings
//------------------------------------------------------------------------------
class SettingsWindow {
public:
    SettingsWindow() = default;
    ~SettingsWindow() = default;
    
    // Prevent copying
    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;
    
    // Show the settings dialog (modal).
    // Returns true if user clicked OK; settings are modified in-place.
    static bool Show(HWND parent, HINSTANCE hInstance, AppSettings& settings);

private:
    // Dialog procedure
    static INT_PTR CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Message handlers
    void OnInit(HWND hwnd);
    void OnTabChanged();
    void OnOK();
    void OnFontButton();
    void OnApplyAssociations();
    
    // Page management
    void ShowPage(int pageIndex);
    
    // Transfer data between controls and settings
    void InitControlsFromSettings();
    void ReadControlsToSettings();
    
    // Build and display font preview string
    void UpdateFontPreview();

private:
    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;
    AppSettings* m_settings = nullptr;
    
    // Working copy of settings (committed on OK)
    AppSettings m_editSettings;
    
    // Font state (tracks font chooser selections before OK)
    std::wstring m_fontName;
    int m_fontSize = 11;
    int m_fontWeight = FW_NORMAL;
    bool m_fontItalic = false;
    
    // Currently visible tab page
    int m_currentPage = 0;
    
    // Number of tab pages
    static constexpr int PAGE_COUNT = 5;
    
    // Static instance for dialog proc callback
    static SettingsWindow* s_instance;
};

} // namespace QNote
