//==============================================================================
// QNote - A Lightweight Notepad Clone
// FileIO.h - File input/output with encoding detection
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
#include <memory>
#include "Settings.h"

namespace QNote {

//------------------------------------------------------------------------------
// File read result structure
//------------------------------------------------------------------------------
struct FileReadResult {
    bool success = false;
    std::wstring content;
    std::wstring errorMessage;
    TextEncoding detectedEncoding = TextEncoding::UTF8;
    LineEnding detectedLineEnding = LineEnding::CRLF;
};

//------------------------------------------------------------------------------
// File write result structure
//------------------------------------------------------------------------------
struct FileWriteResult {
    bool success = false;
    std::wstring errorMessage;
};

//------------------------------------------------------------------------------
// File I/O class
//------------------------------------------------------------------------------
class FileIO {
public:
    // Read a file with automatic encoding detection
    [[nodiscard]] static FileReadResult ReadFile(const std::wstring& filePath);
    
    // Read a file forcing a specific encoding (for "Reopen with Encoding")
    [[nodiscard]] static FileReadResult ReadFileWithEncoding(const std::wstring& filePath, TextEncoding encoding);
    
    // Write a file with specified encoding and line endings
    [[nodiscard]] static FileWriteResult WriteFile(const std::wstring& filePath,
                                     const std::wstring& content,
                                     TextEncoding encoding,
                                     LineEnding lineEnding);
    
    // Detect encoding from raw bytes
    [[nodiscard]] static TextEncoding DetectEncoding(const std::vector<uint8_t>& data);
    
    // Detect line ending type from text
    [[nodiscard]] static LineEnding DetectLineEnding(const std::wstring& text);
    
    // Convert line endings in text
    [[nodiscard]] static std::wstring ConvertLineEndings(const std::wstring& text, LineEnding targetEnding);
    
    // Normalize line endings to LF for internal use
    [[nodiscard]] static std::wstring NormalizeToLF(const std::wstring& text);
    
    // Get file name from path
    [[nodiscard]] static std::wstring GetFileName(const std::wstring& filePath);
    
    // Show Open File dialog
    [[nodiscard]] static bool ShowOpenDialog(HWND parent, std::wstring& outPath);
    
    // Show Save File dialog
    [[nodiscard]] static bool ShowSaveDialog(HWND parent, std::wstring& outPath, TextEncoding& outEncoding,
                               const std::wstring& currentPath = L"");
    
private:
    // Decode bytes to wstring based on encoding
    static std::wstring DecodeToWString(const std::vector<uint8_t>& data, TextEncoding encoding);
    
    // Encode wstring to bytes based on encoding
    static std::vector<uint8_t> EncodeFromWString(const std::wstring& text, TextEncoding encoding);
    
    // Format error message from GetLastError
    static std::wstring FormatLastError(DWORD errorCode);
};

//------------------------------------------------------------------------------
// RAII wrapper for HANDLE
//------------------------------------------------------------------------------
class HandleGuard {
public:
    explicit HandleGuard(HANDLE h = INVALID_HANDLE_VALUE) noexcept : m_handle(h) {}
    ~HandleGuard() noexcept {
        if (m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr) {
            CloseHandle(m_handle);
        }
    }
    
    HandleGuard(const HandleGuard&) = delete;
    HandleGuard& operator=(const HandleGuard&) = delete;
    
    HandleGuard(HandleGuard&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = INVALID_HANDLE_VALUE;
    }
    
    HandleGuard& operator=(HandleGuard&& other) noexcept {
        if (this != &other) {
            if (m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr) {
                CloseHandle(m_handle);
            }
            m_handle = other.m_handle;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
    
    [[nodiscard]] HANDLE get() const noexcept { return m_handle; }
    [[nodiscard]] bool valid() const noexcept { return m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr; }
    HANDLE release() noexcept {
        HANDLE h = m_handle;
        m_handle = INVALID_HANDLE_VALUE;
        return h;
    }
    
private:
    HANDLE m_handle;
};

} // namespace QNote
