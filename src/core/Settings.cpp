//==============================================================================
// QNote - A Lightweight Notepad Clone
// Settings.cpp - Application settings persistence implementation
//==============================================================================

#include "Settings.h"
#include <ShlObj.h>
#include <algorithm>
#include <sstream>

namespace QNote {

//------------------------------------------------------------------------------
// SettingsManager Implementation
//------------------------------------------------------------------------------

SettingsManager::SettingsManager() {
    // Get AppData path
    wchar_t appDataPath[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appDataPath))) {
        m_settingsDir = appDataPath;
        m_settingsDir += L"\\QNote";
        m_settingsPath = m_settingsDir + L"\\config.ini";
    }
}

SettingsManager::~SettingsManager() {
    // Settings are saved explicitly by the application
}

bool SettingsManager::EnsureSettingsDirectory() {
    if (m_settingsDir.empty()) {
        return false;
    }
    
    // Check if directory exists
    DWORD attrs = GetFileAttributesW(m_settingsDir.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }
    
    // Create directory
    return CreateDirectoryW(m_settingsDir.c_str(), nullptr) != 0;
}

int SettingsManager::ParseInt(const std::wstring& section, const std::wstring& key, int defaultValue) {
    return GetPrivateProfileIntW(section.c_str(), key.c_str(), defaultValue, m_settingsPath.c_str());
}

bool SettingsManager::ParseBool(const std::wstring& section, const std::wstring& key, bool defaultValue) {
    return ParseInt(section, key, defaultValue ? 1 : 0) != 0;
}

std::wstring SettingsManager::ParseString(const std::wstring& section, const std::wstring& key, 
                                           const std::wstring& defaultValue) {
    wchar_t buffer[1024] = {};
    GetPrivateProfileStringW(section.c_str(), key.c_str(), defaultValue.c_str(),
                             buffer, _countof(buffer), m_settingsPath.c_str());
    return buffer;
}

void SettingsManager::WriteInt(const std::wstring& section, const std::wstring& key, int value) {
    WritePrivateProfileStringW(section.c_str(), key.c_str(), 
                               std::to_wstring(value).c_str(), m_settingsPath.c_str());
}

void SettingsManager::WriteBool(const std::wstring& section, const std::wstring& key, bool value) {
    WriteInt(section, key, value ? 1 : 0);
}

void SettingsManager::WriteString(const std::wstring& section, const std::wstring& key, 
                                   const std::wstring& value) {
    WritePrivateProfileStringW(section.c_str(), key.c_str(), value.c_str(), m_settingsPath.c_str());
}

bool SettingsManager::Load() {
    if (m_settingsPath.empty()) {
        return false;
    }
    
    // Check if config file exists
    if (GetFileAttributesW(m_settingsPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        // Use defaults
        return true;
    }
    
    // Window section
    m_settings.windowX = ParseInt(L"Window", L"X", CW_USEDEFAULT);
    m_settings.windowY = ParseInt(L"Window", L"Y", CW_USEDEFAULT);
    m_settings.windowWidth = ParseInt(L"Window", L"Width", 800);
    m_settings.windowHeight = ParseInt(L"Window", L"Height", 600);
    m_settings.windowMaximized = ParseBool(L"Window", L"Maximized", false);
    
    // Font section
    m_settings.fontName = ParseString(L"Font", L"Name", L"Consolas");
    m_settings.fontSize = ParseInt(L"Font", L"Size", 11);
    m_settings.fontWeight = ParseInt(L"Font", L"Weight", FW_NORMAL);
    m_settings.fontItalic = ParseBool(L"Font", L"Italic", false);
    
    // Editor section
    m_settings.wordWrap = ParseBool(L"Editor", L"WordWrap", false);
    m_settings.tabSize = ParseInt(L"Editor", L"TabSize", 4);
    m_settings.zoomLevel = ParseInt(L"Editor", L"ZoomLevel", 100);
    m_settings.showStatusBar = ParseBool(L"Editor", L"StatusBar", true);
    m_settings.showLineNumbers = ParseBool(L"Editor", L"LineNumbers", false);
    m_settings.showWhitespace = ParseBool(L"Editor", L"ShowWhitespace", false);
    m_settings.fileAutoSave = ParseBool(L"Editor", L"FileAutoSave", true);
    m_settings.rightToLeft = ParseBool(L"Editor", L"RightToLeft", false);
    
    // Validate zoom level (25-500%)
    if (m_settings.zoomLevel < 25) m_settings.zoomLevel = 25;
    if (m_settings.zoomLevel > 500) m_settings.zoomLevel = 500;
    
    // Validate tab size (1-16)
    if (m_settings.tabSize < 1) m_settings.tabSize = 1;
    if (m_settings.tabSize > 16) m_settings.tabSize = 16;
    
    // Encoding section
    std::wstring encodingStr = ParseString(L"Encoding", L"Default", L"UTF8");
    m_settings.defaultEncoding = StringToEncoding(encodingStr);
    
    std::wstring lineEndingStr = ParseString(L"Encoding", L"LineEnding", L"CRLF");
    m_settings.defaultLineEnding = StringToLineEnding(lineEndingStr);
    
    // Search section
    m_settings.searchMatchCase = ParseBool(L"Search", L"MatchCase", false);
    m_settings.searchWrapAround = ParseBool(L"Search", L"WrapAround", true);
    m_settings.searchUseRegex = ParseBool(L"Search", L"UseRegex", false);
    
    // Recent files
    m_settings.recentFiles.clear();
    for (size_t i = 0; i < AppSettings::MAX_RECENT_FILES; ++i) {
        std::wstring key = L"File" + std::to_wstring(i);
        std::wstring path = ParseString(L"RecentFiles", key, L"");
        if (!path.empty()) {
            // Verify the file still exists
            if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                m_settings.recentFiles.push_back(path);
            }
        }
    }
    
    return true;
}

bool SettingsManager::Save() {
    if (m_settingsPath.empty()) {
        return false;
    }
    
    // Ensure directory exists
    if (!EnsureSettingsDirectory()) {
        return false;
    }
    
    // Window section
    WriteInt(L"Window", L"X", m_settings.windowX);
    WriteInt(L"Window", L"Y", m_settings.windowY);
    WriteInt(L"Window", L"Width", m_settings.windowWidth);
    WriteInt(L"Window", L"Height", m_settings.windowHeight);
    WriteBool(L"Window", L"Maximized", m_settings.windowMaximized);
    
    // Font section
    WriteString(L"Font", L"Name", m_settings.fontName);
    WriteInt(L"Font", L"Size", m_settings.fontSize);
    WriteInt(L"Font", L"Weight", m_settings.fontWeight);
    WriteBool(L"Font", L"Italic", m_settings.fontItalic);
    
    // Editor section
    WriteBool(L"Editor", L"WordWrap", m_settings.wordWrap);
    WriteInt(L"Editor", L"TabSize", m_settings.tabSize);
    WriteInt(L"Editor", L"ZoomLevel", m_settings.zoomLevel);
    WriteBool(L"Editor", L"StatusBar", m_settings.showStatusBar);
    WriteBool(L"Editor", L"LineNumbers", m_settings.showLineNumbers);
    WriteBool(L"Editor", L"ShowWhitespace", m_settings.showWhitespace);
    WriteBool(L"Editor", L"FileAutoSave", m_settings.fileAutoSave);
    WriteBool(L"Editor", L"RightToLeft", m_settings.rightToLeft);
    
    // Encoding section
    WriteString(L"Encoding", L"Default", EncodingToString(m_settings.defaultEncoding));
    WriteString(L"Encoding", L"LineEnding", LineEndingToString(m_settings.defaultLineEnding));
    
    // Search section
    WriteBool(L"Search", L"MatchCase", m_settings.searchMatchCase);
    WriteBool(L"Search", L"WrapAround", m_settings.searchWrapAround);
    WriteBool(L"Search", L"UseRegex", m_settings.searchUseRegex);
    
    // Recent files - clear old entries first
    for (size_t i = 0; i < AppSettings::MAX_RECENT_FILES; ++i) {
        std::wstring key = L"File" + std::to_wstring(i);
        WritePrivateProfileStringW(L"RecentFiles", key.c_str(), nullptr, m_settingsPath.c_str());
    }
    
    // Write current recent files
    for (size_t i = 0; i < m_settings.recentFiles.size(); ++i) {
        std::wstring key = L"File" + std::to_wstring(i);
        WriteString(L"RecentFiles", key, m_settings.recentFiles[i]);
    }
    
    return true;
}

void SettingsManager::AddRecentFile(const std::wstring& filePath) {
    if (filePath.empty()) return;
    
    // Remove if already exists (case-insensitive)
    auto it = std::find_if(m_settings.recentFiles.begin(), m_settings.recentFiles.end(),
        [&filePath](const std::wstring& existing) {
            return _wcsicmp(existing.c_str(), filePath.c_str()) == 0;
        });
    
    if (it != m_settings.recentFiles.end()) {
        m_settings.recentFiles.erase(it);
    }
    
    // Add to front
    m_settings.recentFiles.insert(m_settings.recentFiles.begin(), filePath);
    
    // Trim to max size
    if (m_settings.recentFiles.size() > AppSettings::MAX_RECENT_FILES) {
        m_settings.recentFiles.resize(AppSettings::MAX_RECENT_FILES);
    }
}

void SettingsManager::RemoveRecentFile(const std::wstring& filePath) {
    auto it = std::find_if(m_settings.recentFiles.begin(), m_settings.recentFiles.end(),
        [&filePath](const std::wstring& existing) {
            return _wcsicmp(existing.c_str(), filePath.c_str()) == 0;
        });
    
    if (it != m_settings.recentFiles.end()) {
        m_settings.recentFiles.erase(it);
    }
}

void SettingsManager::ClearRecentFiles() {
    m_settings.recentFiles.clear();
}

//------------------------------------------------------------------------------
// Utility Functions
//------------------------------------------------------------------------------

const wchar_t* EncodingToString(TextEncoding encoding) {
    switch (encoding) {
        case TextEncoding::ANSI:      return L"ANSI";
        case TextEncoding::UTF8:      return L"UTF-8";
        case TextEncoding::UTF8_BOM:  return L"UTF-8 BOM";
        case TextEncoding::UTF16_LE:  return L"UTF-16 LE";
        case TextEncoding::UTF16_BE:  return L"UTF-16 BE";
        default:                      return L"UTF-8";
    }
}

const wchar_t* LineEndingToString(LineEnding lineEnding) {
    switch (lineEnding) {
        case LineEnding::CRLF: return L"CRLF";
        case LineEnding::LF:   return L"LF";
        case LineEnding::CR:   return L"CR";
        default:               return L"CRLF";
    }
}

TextEncoding StringToEncoding(const std::wstring& str) {
    if (_wcsicmp(str.c_str(), L"ANSI") == 0) return TextEncoding::ANSI;
    if (_wcsicmp(str.c_str(), L"UTF8") == 0) return TextEncoding::UTF8;
    if (_wcsicmp(str.c_str(), L"UTF-8") == 0) return TextEncoding::UTF8;
    if (_wcsicmp(str.c_str(), L"UTF8BOM") == 0) return TextEncoding::UTF8_BOM;
    if (_wcsicmp(str.c_str(), L"UTF-8 BOM") == 0) return TextEncoding::UTF8_BOM;
    if (_wcsicmp(str.c_str(), L"UTF16LE") == 0) return TextEncoding::UTF16_LE;
    if (_wcsicmp(str.c_str(), L"UTF-16 LE") == 0) return TextEncoding::UTF16_LE;
    if (_wcsicmp(str.c_str(), L"UTF16BE") == 0) return TextEncoding::UTF16_BE;
    if (_wcsicmp(str.c_str(), L"UTF-16 BE") == 0) return TextEncoding::UTF16_BE;
    return TextEncoding::UTF8;
}

LineEnding StringToLineEnding(const std::wstring& str) {
    if (_wcsicmp(str.c_str(), L"CRLF") == 0) return LineEnding::CRLF;
    if (_wcsicmp(str.c_str(), L"LF") == 0) return LineEnding::LF;
    if (_wcsicmp(str.c_str(), L"CR") == 0) return LineEnding::CR;
    return LineEnding::CRLF;
}

} // namespace QNote
