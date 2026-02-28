//==============================================================================
// QNote - A Lightweight Notepad Clone
// Settings.h - Application settings persistence
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
#include <array>

namespace QNote {

//------------------------------------------------------------------------------
// Line ending types
//------------------------------------------------------------------------------
enum class LineEnding {
    CRLF,   // Windows (0x0D 0x0A)
    LF,     // Unix (0x0A)
    CR      // Classic Mac (0x0D)
};

//------------------------------------------------------------------------------
// Save style modes
//------------------------------------------------------------------------------
enum class SaveStyle {
    Manual,     // User must save manually (Ctrl+S)
    AutoSave    // Automatically save after a delay
};

//------------------------------------------------------------------------------
// Text encoding types
//------------------------------------------------------------------------------
enum class TextEncoding {
    ANSI,
    UTF8,
    UTF8_BOM,
    UTF16_LE,
    UTF16_BE
};

//------------------------------------------------------------------------------
// Application settings structure
//------------------------------------------------------------------------------
struct AppSettings {
    // Window state
    int windowX = CW_USEDEFAULT;
    int windowY = CW_USEDEFAULT;
    int windowWidth = 800;
    int windowHeight = 600;
    bool windowMaximized = false;
    
    // Font settings
    std::wstring fontName = L"Consolas";
    int fontSize = 11;
    int fontWeight = FW_NORMAL;
    bool fontItalic = false;
    
    // Editor settings
    bool wordWrap = false;
    int tabSize = 4;
    int zoomLevel = 100;  // Percentage (25-500)
    bool showStatusBar = true;
    bool showLineNumbers = false;  // Line numbers gutter
    bool showWhitespace = false;     // Show whitespace characters
    bool spellCheckEnabled = false;  // Spell check with wavy underlines
    bool fileAutoSave = true;      // Auto-save backup files (.autosave)
    bool rightToLeft = false;  // Right-to-left reading order
    int scrollLines = 0;          // Lines per scroll wheel notch (0 = system default)
    bool autoCompleteBraces = true;  // Auto-complete braces, brackets, and quotes
    bool syntaxHighlightEnabled = false; // Syntax highlighting for code files
    
    // Default encoding for new files
    TextEncoding defaultEncoding = TextEncoding::UTF8;
    LineEnding defaultLineEnding = LineEnding::CRLF;
    
    // Behavior settings
    int minimizeMode = 1;              // 0 = taskbar, 1 = system tray
    int closeMode = 0;                 // 0 = quit the app, 1 = minimize to system tray
    bool autoUpdate = false;           // Check for updates on startup
    bool portableMode = false;         // Use config next to exe
    bool promptSaveOnClose = true;     // Show "Do you want to save?" dialog
    SaveStyle saveStyle = SaveStyle::Manual;  // Manual or AutoSave
    int autoSaveDelayMs = 5000;        // Auto-save delay in milliseconds (1000-60000)
    
    // Search settings (persistent across sessions)
    bool searchMatchCase = false;
    bool searchWrapAround = true;
    bool searchUseRegex = false;
    
    // View state
    bool alwaysOnTop = false;
    bool fullScreen = false;
    bool menuBarVisible = true;
    
    // Print settings (persistent)
    int printQuality = 0;    // 0=Normal, 1=Draft, 2=High, 3=Letter Quality
    int paperSource  = 0;    // 0=Auto, 1=Upper, 2=Lower, 3=Manual, 4=Tractor, 5=Continuous
    int paperSize    = 0;    // 0=Default, 1-11 = specific sizes
    int duplex       = 0;    // 0=None, 1=Long Edge, 2=Short Edge
    int pageFilter   = 0;    // 0=All, 1=Odd Only, 2=Even Only
    bool condensed   = false; // Condensed mode for dot matrix
    bool formFeed    = false; // Send form feed after print job
    
    // Recent files list (max 10)
    static constexpr size_t MAX_RECENT_FILES = 10;
    std::vector<std::wstring> recentFiles;
};

//------------------------------------------------------------------------------
// Settings manager class
//------------------------------------------------------------------------------
class SettingsManager {
public:
    SettingsManager();
    ~SettingsManager();
    
    // Prevent copying
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;
    
    // Load settings from INI file
    [[nodiscard]] bool Load();
    
    // Save settings to INI file
    [[nodiscard]] bool Save();
    
    // Get the settings path
    [[nodiscard]] const std::wstring& GetSettingsPath() const noexcept { return m_settingsPath; }
    
    // Get the settings directory
    [[nodiscard]] const std::wstring& GetSettingsDir() const noexcept { return m_settingsDir; }
    
    // Check if running in portable mode (config.ini next to exe)
    [[nodiscard]] static bool DetectPortableMode();
    
    // Access settings
    [[nodiscard]] AppSettings& GetSettings() noexcept { return m_settings; }
    [[nodiscard]] const AppSettings& GetSettings() const noexcept { return m_settings; }
    
    // Recent files management
    void AddRecentFile(const std::wstring& filePath);
    void RemoveRecentFile(const std::wstring& filePath);
    void ClearRecentFiles();
    
private:
    // Create the settings directory if it doesn't exist
    bool EnsureSettingsDirectory();
    
    // Parse helpers
    int ParseInt(const std::wstring& section, const std::wstring& key, int defaultValue);
    bool ParseBool(const std::wstring& section, const std::wstring& key, bool defaultValue);
    std::wstring ParseString(const std::wstring& section, const std::wstring& key, const std::wstring& defaultValue);
    
    // Write helpers
    void WriteInt(const std::wstring& section, const std::wstring& key, int value);
    void WriteBool(const std::wstring& section, const std::wstring& key, bool value);
    void WriteString(const std::wstring& section, const std::wstring& key, const std::wstring& value);
    
private:
    std::wstring m_settingsDir;
    std::wstring m_settingsPath;
    AppSettings m_settings;
};

//------------------------------------------------------------------------------
// Utility functions for encoding/line ending conversion
//------------------------------------------------------------------------------

// Get display name for encoding
const wchar_t* EncodingToString(TextEncoding encoding);

// Get display name for line ending
const wchar_t* LineEndingToString(LineEnding lineEnding);

// Parse encoding from string
TextEncoding StringToEncoding(const std::wstring& str);

// Parse line ending from string
LineEnding StringToLineEnding(const std::wstring& str);

} // namespace QNote
