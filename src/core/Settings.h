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
    bool fileAutoSave = true;      // Auto-save backup files (.autosave)
    bool rightToLeft = false;  // Right-to-left reading order
    
    // Default encoding for new files
    TextEncoding defaultEncoding = TextEncoding::UTF8;
    LineEnding defaultLineEnding = LineEnding::CRLF;
    
    // Search settings (persistent across sessions)
    bool searchMatchCase = false;
    bool searchWrapAround = true;
    bool searchUseRegex = false;
    
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
