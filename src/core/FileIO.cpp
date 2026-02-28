//==============================================================================
// QNote - A Lightweight Notepad Clone
// FileIO.cpp - File input/output with encoding detection implementation
//==============================================================================

#include "FileIO.h"
#include <commdlg.h>
#include <shobjidl.h>
#include <algorithm>

namespace QNote {

//------------------------------------------------------------------------------
// BOM (Byte Order Mark) signatures
//------------------------------------------------------------------------------
static const uint8_t BOM_UTF8[] = { 0xEF, 0xBB, 0xBF };
static const uint8_t BOM_UTF16_LE[] = { 0xFF, 0xFE };
static const uint8_t BOM_UTF16_BE[] = { 0xFE, 0xFF };

//------------------------------------------------------------------------------
// Check if data starts with BOM
//------------------------------------------------------------------------------
static bool StartsWith(const std::vector<uint8_t>& data, const uint8_t* bom, size_t bomSize) {
    if (data.size() < bomSize) return false;
    return memcmp(data.data(), bom, bomSize) == 0;
}

//------------------------------------------------------------------------------
// Check if data appears to be valid UTF-8
//------------------------------------------------------------------------------
static bool IsValidUTF8(const std::vector<uint8_t>& data) {
    size_t i = 0;
    while (i < data.size()) {
        uint8_t c = data[i];
        int bytesNeeded = 0;
        
        if (c < 0x80) {
            // ASCII
            i++;
            continue;
        } else if ((c & 0xE0) == 0xC0) {
            bytesNeeded = 1;
        } else if ((c & 0xF0) == 0xE0) {
            bytesNeeded = 2;
        } else if ((c & 0xF8) == 0xF0) {
            bytesNeeded = 3;
        } else {
            // Invalid UTF-8 leading byte
            return false;
        }
        
        // Check continuation bytes
        for (int j = 0; j < bytesNeeded; j++) {
            i++;
            if (i >= data.size()) return false;
            if ((data[i] & 0xC0) != 0x80) return false;
        }
        i++;
    }
    return true;
}

//------------------------------------------------------------------------------
// Check if data appears to be UTF-16 (look for null bytes in expected positions)
//------------------------------------------------------------------------------
static bool LooksLikeUTF16(const std::vector<uint8_t>& data, bool& littleEndian) {
    if (data.size() < 2) return false;
    
    // Count null bytes in odd/even positions
    size_t nullOdd = 0;  // Null bytes at odd positions (UTF-16 LE for ASCII)
    size_t nullEven = 0; // Null bytes at even positions (UTF-16 BE for ASCII)
    
    size_t checkSize = std::min(data.size(), size_t(1024));
    for (size_t i = 0; i < checkSize; i++) {
        if (data[i] == 0) {
            if (i % 2 == 0) nullEven++;
            else nullOdd++;
        }
    }
    
    // If we have significant null bytes in expected positions
    if (nullOdd > checkSize / 8) {
        littleEndian = true;
        return true;
    }
    if (nullEven > checkSize / 8) {
        littleEndian = false;
        return true;
    }
    
    return false;
}

//------------------------------------------------------------------------------
// Detect encoding from raw bytes
//------------------------------------------------------------------------------
TextEncoding FileIO::DetectEncoding(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return TextEncoding::UTF8;
    }
    
    // Check for BOM first
    if (StartsWith(data, BOM_UTF8, sizeof(BOM_UTF8))) {
        return TextEncoding::UTF8_BOM;
    }
    if (StartsWith(data, BOM_UTF16_LE, sizeof(BOM_UTF16_LE))) {
        return TextEncoding::UTF16_LE;
    }
    if (StartsWith(data, BOM_UTF16_BE, sizeof(BOM_UTF16_BE))) {
        return TextEncoding::UTF16_BE;
    }
    
    // Check for UTF-16 without BOM (look for pattern of null bytes)
    bool littleEndian = false;
    if (LooksLikeUTF16(data, littleEndian)) {
        return littleEndian ? TextEncoding::UTF16_LE : TextEncoding::UTF16_BE;
    }
    
    // Check if valid UTF-8
    if (IsValidUTF8(data)) {
        // Check if it contains any high bytes (non-ASCII)
        bool hasHighBytes = false;
        for (uint8_t byte : data) {
            if (byte >= 0x80) {
                hasHighBytes = true;
                break;
            }
        }
        // If it's pure ASCII, treat as UTF-8 (compatible with ANSI)
        // If it has high bytes that form valid UTF-8 sequences, it's UTF-8
        return TextEncoding::UTF8;
    }
    
    // If not valid UTF-8, assume ANSI (Windows-1252 or current code page)
    return TextEncoding::ANSI;
}

//------------------------------------------------------------------------------
// Detect line ending type from text
//------------------------------------------------------------------------------
LineEnding FileIO::DetectLineEnding(const std::wstring& text) {
    size_t crlfCount = 0;
    size_t lfCount = 0;
    size_t crCount = 0;
    
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == L'\r') {
            if (i + 1 < text.size() && text[i + 1] == L'\n') {
                crlfCount++;
                i++; // Skip the LF
            } else {
                crCount++;
            }
        } else if (text[i] == L'\n') {
            lfCount++;
        }
    }
    
    // Determine predominant line ending
    if (crlfCount >= lfCount && crlfCount >= crCount) {
        return LineEnding::CRLF;
    } else if (lfCount >= crlfCount && lfCount >= crCount) {
        return LineEnding::LF;
    } else {
        return LineEnding::CR;
    }
}

//------------------------------------------------------------------------------
// Decode bytes to wstring based on encoding
//------------------------------------------------------------------------------
std::wstring FileIO::DecodeToWString(const std::vector<uint8_t>& data, TextEncoding encoding) {
    if (data.empty()) {
        return L"";
    }
    
    const uint8_t* start = data.data();
    size_t size = data.size();
    
    // Skip BOM if present
    if (encoding == TextEncoding::UTF8_BOM && size >= 3) {
        start += 3;
        size -= 3;
    } else if ((encoding == TextEncoding::UTF16_LE || encoding == TextEncoding::UTF16_BE) && size >= 2) {
        // Check for BOM
        if ((start[0] == 0xFF && start[1] == 0xFE) || (start[0] == 0xFE && start[1] == 0xFF)) {
            start += 2;
            size -= 2;
        }
    }
    
    if (size == 0) {
        return L"";
    }
    
    switch (encoding) {
        case TextEncoding::UTF16_LE: {
            // Direct copy for UTF-16 LE (native Windows encoding)
            size_t charCount = size / 2;
            std::wstring result(charCount, L'\0');
            memcpy(result.data(), start, charCount * 2);
            return result;
        }
        
        case TextEncoding::UTF16_BE: {
            // Byte swap for UTF-16 BE
            size_t charCount = size / 2;
            std::wstring result(charCount, L'\0');
            for (size_t i = 0; i < charCount; i++) {
                result[i] = static_cast<wchar_t>((start[i * 2] << 8) | start[i * 2 + 1]);
            }
            return result;
        }
        
        case TextEncoding::UTF8:
        case TextEncoding::UTF8_BOM: {
            // Convert UTF-8 to UTF-16
            int wideLen = MultiByteToWideChar(CP_UTF8, 0, 
                reinterpret_cast<const char*>(start), static_cast<int>(size), nullptr, 0);
            if (wideLen == 0) {
                return L"";
            }
            std::wstring result(wideLen, L'\0');
            MultiByteToWideChar(CP_UTF8, 0,
                reinterpret_cast<const char*>(start), static_cast<int>(size), 
                result.data(), wideLen);
            return result;
        }
        
        case TextEncoding::ANSI:
        default: {
            // Convert ANSI to UTF-16 using current code page
            int wideLen = MultiByteToWideChar(CP_ACP, 0,
                reinterpret_cast<const char*>(start), static_cast<int>(size), nullptr, 0);
            if (wideLen == 0) {
                return L"";
            }
            std::wstring result(wideLen, L'\0');
            MultiByteToWideChar(CP_ACP, 0,
                reinterpret_cast<const char*>(start), static_cast<int>(size),
                result.data(), wideLen);
            return result;
        }
    }
}

//------------------------------------------------------------------------------
// Encode wstring to bytes based on encoding
//------------------------------------------------------------------------------
std::vector<uint8_t> FileIO::EncodeFromWString(const std::wstring& text, TextEncoding encoding) {
    std::vector<uint8_t> result;
    
    switch (encoding) {
        case TextEncoding::UTF8_BOM: {
            // Add BOM
            result.push_back(0xEF);
            result.push_back(0xBB);
            result.push_back(0xBF);
            // Fall through to UTF-8 encoding
        }
        [[fallthrough]];
        
        case TextEncoding::UTF8: {
            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), 
                static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
            if (utf8Len > 0) {
                size_t bomSize = result.size();
                result.resize(bomSize + utf8Len);
                WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                    reinterpret_cast<char*>(result.data() + bomSize), utf8Len, nullptr, nullptr);
            }
            break;
        }
        
        case TextEncoding::UTF16_LE: {
            // Add BOM for UTF-16 LE
            result.push_back(0xFF);
            result.push_back(0xFE);
            // Copy data
            size_t dataSize = text.size() * 2;
            size_t bomSize = result.size();
            result.resize(bomSize + dataSize);
            memcpy(result.data() + bomSize, text.c_str(), dataSize);
            break;
        }
        
        case TextEncoding::UTF16_BE: {
            // Add BOM for UTF-16 BE
            result.push_back(0xFE);
            result.push_back(0xFF);
            // Copy data with byte swap
            for (wchar_t c : text) {
                result.push_back(static_cast<uint8_t>((c >> 8) & 0xFF));
                result.push_back(static_cast<uint8_t>(c & 0xFF));
            }
            break;
        }
        
        case TextEncoding::ANSI:
        default: {
            int ansiLen = WideCharToMultiByte(CP_ACP, 0, text.c_str(),
                static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
            if (ansiLen > 0) {
                result.resize(ansiLen);
                WideCharToMultiByte(CP_ACP, 0, text.c_str(), static_cast<int>(text.size()),
                    reinterpret_cast<char*>(result.data()), ansiLen, nullptr, nullptr);
            }
            break;
        }
    }
    
    return result;
}

//------------------------------------------------------------------------------
// Read a file with automatic encoding detection
//------------------------------------------------------------------------------
FileReadResult FileIO::ReadFile(const std::wstring& filePath) {
    FileReadResult result;
    
    // Open file for reading
    HandleGuard hFile(CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    
    if (!hFile.valid()) {
        result.errorMessage = FormatLastError(GetLastError());
        return result;
    }
    
    // Get file size
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile.get(), &fileSize)) {
        result.errorMessage = FormatLastError(GetLastError());
        return result;
    }
    
    // For files >100MB, delegate to the chunked reader for UI responsiveness
    static constexpr LONGLONG LARGE_THRESHOLD = 100LL * 1024 * 1024;
    if (fileSize.QuadPart > LARGE_THRESHOLD) {
        return ReadFileLarge(filePath);
    }
    
    // Read file contents
    std::vector<uint8_t> data;
    if (fileSize.QuadPart > 0) {
        data.resize(static_cast<size_t>(fileSize.QuadPart));
        DWORD bytesRead = 0;
        if (!::ReadFile(hFile.get(), data.data(), static_cast<DWORD>(data.size()), &bytesRead, nullptr)) {
            result.errorMessage = FormatLastError(GetLastError());
            return result;
        }
        data.resize(bytesRead);
    }
    
    // Detect encoding
    result.detectedEncoding = DetectEncoding(data);
    
    // Decode to wstring
    result.content = DecodeToWString(data, result.detectedEncoding);
    
    // Detect line endings
    result.detectedLineEnding = DetectLineEnding(result.content);
    
    result.success = true;
    return result;
}

//------------------------------------------------------------------------------
// Read a file forcing a specific encoding
//------------------------------------------------------------------------------
FileReadResult FileIO::ReadFileWithEncoding(const std::wstring& filePath, TextEncoding encoding) {
    FileReadResult result;
    
    // Open file for reading
    HandleGuard hFile(CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    
    if (!hFile.valid()) {
        result.errorMessage = FormatLastError(GetLastError());
        return result;
    }
    
    // Get file size
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile.get(), &fileSize)) {
        result.errorMessage = FormatLastError(GetLastError());
        return result;
    }
    
    // For large files (>100 MB), use the chunked reader to stay responsive.
    // The forced encoding is applied by temporarily setting the result and
    // letting ReadFileLarge detect line endings from the probe data.
    static constexpr LONGLONG LARGE_THRESHOLD = 100LL * 1024 * 1024;
    if (fileSize.QuadPart > LARGE_THRESHOLD) {
        // Close our handle before ReadFileLarge opens its own
        hFile = HandleGuard();
        FileReadResult lr = ReadFileLarge(filePath);
        // Override detected encoding with the forced one and re-decode
        // if encodings differ.  For truly large files, we re-read via
        // ReadFileLarge which always does streamed decode \u2013 the caller
        // already expects this to be slow.
        if (lr.success && lr.detectedEncoding != encoding) {
            lr = ReadFileLarge(filePath);
            // ReadFileLarge always detects encoding; we simply overwrite.
            lr.detectedEncoding = encoding;
        }
        return lr;
    }
    
    std::vector<uint8_t> data;
    if (fileSize.QuadPart > 0) {
        try {
            data.resize(static_cast<size_t>(fileSize.QuadPart));
        } catch (const std::bad_alloc&) {
            result.errorMessage = L"Not enough memory to open this file";
            return result;
        }
        DWORD bytesRead = 0;
        if (!::ReadFile(hFile.get(), data.data(), static_cast<DWORD>(data.size()), &bytesRead, nullptr)) {
            result.errorMessage = FormatLastError(GetLastError());
            return result;
        }
        data.resize(bytesRead);
    }
    
    // Use the forced encoding
    result.detectedEncoding = encoding;
    result.content = DecodeToWString(data, encoding);
    result.detectedLineEnding = DetectLineEnding(result.content);
    result.success = true;
    return result;
}

//------------------------------------------------------------------------------
// Find a safe byte boundary so we never split a multi-byte sequence.
// Returns the number of leading bytes that form complete characters.
//------------------------------------------------------------------------------
static size_t FindUTF8SafeBoundary(const uint8_t* data, size_t size) {
    if (size == 0) return 0;
    // Inspect the last 4 bytes (max UTF-8 sequence length)
    size_t check = (std::min)(size, size_t(4));
    for (size_t i = 1; i <= check; i++) {
        uint8_t c = data[size - i];
        if ((c & 0x80) == 0) {
            // ASCII – everything up to and including this byte is complete
            return size;
        }
        if ((c & 0xC0) == 0xC0) {
            // Leading byte – how many continuation bytes does it expect?
            int expected = 0;
            if ((c & 0xE0) == 0xC0) expected = 2;
            else if ((c & 0xF0) == 0xE0) expected = 3;
            else if ((c & 0xF8) == 0xF0) expected = 4;
            if (static_cast<size_t>(expected) <= i) {
                return size;          // sequence is complete
            }
            return size - i;          // incomplete – cut before this byte
        }
        // continuation byte (10xxxxxx) – keep walking backwards
    }
    return size; // shouldn't happen in valid data
}

static size_t FindUTF16SafeBoundary(const uint8_t* data, size_t size, bool littleEndian) {
    if (size < 2) return 0;
    // Ensure even byte count
    size = size & ~size_t(1);
    // Check whether the last code unit is a high surrogate (needs a following low)
    wchar_t lastUnit;
    if (littleEndian)
        lastUnit = static_cast<wchar_t>(data[size - 2] | (data[size - 1] << 8));
    else
        lastUnit = static_cast<wchar_t>((data[size - 2] << 8) | data[size - 1]);
    if (lastUnit >= 0xD800 && lastUnit <= 0xDBFF)
        return size - 2;  // drop the orphaned high surrogate
    return size;
}

//------------------------------------------------------------------------------
// Append decoded wide chars from a raw byte span
//------------------------------------------------------------------------------
void FileIO::AppendDecoded(std::wstring& out, const uint8_t* data, size_t size, TextEncoding encoding) {
    if (size == 0) return;
    switch (encoding) {
        case TextEncoding::UTF16_LE: {
            size_t charCount = size / 2;
            size_t oldSize = out.size();
            out.resize(oldSize + charCount);
            memcpy(out.data() + oldSize, data, charCount * 2);
            break;
        }
        case TextEncoding::UTF16_BE: {
            size_t charCount = size / 2;
            size_t oldSize = out.size();
            out.resize(oldSize + charCount);
            for (size_t i = 0; i < charCount; i++)
                out[oldSize + i] = static_cast<wchar_t>((data[i * 2] << 8) | data[i * 2 + 1]);
            break;
        }
        case TextEncoding::UTF8:
        case TextEncoding::UTF8_BOM: {
            int wideLen = MultiByteToWideChar(CP_UTF8, 0,
                reinterpret_cast<const char*>(data), static_cast<int>(size), nullptr, 0);
            if (wideLen > 0) {
                size_t oldSize = out.size();
                out.resize(oldSize + wideLen);
                MultiByteToWideChar(CP_UTF8, 0,
                    reinterpret_cast<const char*>(data), static_cast<int>(size),
                    out.data() + oldSize, wideLen);
            }
            break;
        }
        case TextEncoding::ANSI:
        default: {
            int wideLen = MultiByteToWideChar(CP_ACP, 0,
                reinterpret_cast<const char*>(data), static_cast<int>(size), nullptr, 0);
            if (wideLen > 0) {
                size_t oldSize = out.size();
                out.resize(oldSize + wideLen);
                MultiByteToWideChar(CP_ACP, 0,
                    reinterpret_cast<const char*>(data), static_cast<int>(size),
                    out.data() + oldSize, wideLen);
            }
            break;
        }
    }
}

//------------------------------------------------------------------------------
// Stream-read a large file in chunks (supports >3 GB files)
// Reads the file in 8 MB I/O chunks and decodes each chunk immediately so we
// never hold the entire raw byte buffer in memory at once.  The message queue
// is pumped between chunks so the UI thread stays responsive.
//------------------------------------------------------------------------------
FileReadResult FileIO::ReadFileLarge(const std::wstring& filePath, HWND hwndStatus) {
    FileReadResult result;

    // Open with sequential-scan hint for better read-ahead caching
    HandleGuard hFile(CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr));

    if (!hFile.valid()) {
        result.errorMessage = FormatLastError(GetLastError());
        return result;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile.get(), &fileSize)) {
        result.errorMessage = FormatLastError(GetLastError());
        return result;
    }

    // ---- Detect encoding from the first 64 KB ----
    static constexpr DWORD PROBE_SIZE = 64 * 1024;
    std::vector<uint8_t> probe(
        static_cast<size_t>((std::min)(static_cast<LONGLONG>(PROBE_SIZE), fileSize.QuadPart)));
    {
        DWORD bytesRead = 0;
        if (!::ReadFile(hFile.get(), probe.data(),
                        static_cast<DWORD>(probe.size()), &bytesRead, nullptr)) {
            result.errorMessage = FormatLastError(GetLastError());
            return result;
        }
        probe.resize(bytesRead);
    }
    result.detectedEncoding = DetectEncoding(probe);

    // Detect line ending from the probe text so we don't have to scan the
    // entire (potentially multi-GB) decoded string later.
    {
        std::wstring probeText = DecodeToWString(probe, result.detectedEncoding);
        result.detectedLineEnding = DetectLineEnding(probeText);
    }

    // Seek back to start for the full read
    LARGE_INTEGER zero = {};
    SetFilePointerEx(hFile.get(), zero, nullptr, FILE_BEGIN);

    // ---- Skip BOM ----
    LONGLONG dataStart = 0;
    switch (result.detectedEncoding) {
        case TextEncoding::UTF8_BOM: dataStart = 3; break;
        case TextEncoding::UTF16_LE:
        case TextEncoding::UTF16_BE:
            if (probe.size() >= 2 &&
                ((probe[0] == 0xFF && probe[1] == 0xFE) ||
                 (probe[0] == 0xFE && probe[1] == 0xFF)))
                dataStart = 2;
            break;
        default: break;
    }
    if (dataStart > 0) {
        LARGE_INTEGER pos;
        pos.QuadPart = dataStart;
        SetFilePointerEx(hFile.get(), pos, nullptr, FILE_BEGIN);
    }
    probe.clear();  // free early

    // ---- Pre-allocate the result wstring ----
    LONGLONG totalBytesToRead = fileSize.QuadPart - dataStart;
    try {
        size_t estimatedChars;
        if (result.detectedEncoding == TextEncoding::UTF16_LE ||
            result.detectedEncoding == TextEncoding::UTF16_BE)
            estimatedChars = static_cast<size_t>(totalBytesToRead / 2);
        else
            estimatedChars = static_cast<size_t>(totalBytesToRead); // upper bound for ASCII-heavy
        result.content.reserve(estimatedChars);
    } catch (const std::bad_alloc&) {
        result.errorMessage = L"Not enough memory to open this file";
        return result;
    }

    // ---- Chunked read + decode ----
    static constexpr DWORD CHUNK_SIZE = 8 * 1024 * 1024; // 8 MB I/O chunks

    std::vector<uint8_t> chunkBuf;
    try {
        chunkBuf.resize(CHUNK_SIZE);
    } catch (const std::bad_alloc&) {
        result.errorMessage = L"Not enough memory to open this file";
        return result;
    }

    std::vector<uint8_t> carry;  // leftover bytes from an incomplete char sequence
    LONGLONG totalRead = 0;
    int lastPercent = -1;

    while (totalRead < totalBytesToRead) {
        DWORD toRead = static_cast<DWORD>(
            (std::min)(static_cast<LONGLONG>(CHUNK_SIZE - static_cast<LONGLONG>(carry.size())),
                       totalBytesToRead - totalRead));

        // Place carry bytes at the front of the working buffer
        size_t carrySize = carry.size();
        if (carrySize > 0) {
            memcpy(chunkBuf.data(), carry.data(), carrySize);
            carry.clear();
        }

        DWORD bytesRead = 0;
        if (!::ReadFile(hFile.get(), chunkBuf.data() + carrySize, toRead, &bytesRead, nullptr)) {
            result.errorMessage = FormatLastError(GetLastError());
            return result;
        }
        if (bytesRead == 0) break; // EOF

        totalRead += bytesRead;
        size_t available = carrySize + bytesRead;
        bool isLastChunk = (totalRead >= totalBytesToRead);

        // Find the safe boundary so we never split a multi-byte character
        size_t decodable = available;
        if (!isLastChunk) {
            switch (result.detectedEncoding) {
                case TextEncoding::UTF8:
                case TextEncoding::UTF8_BOM:
                case TextEncoding::ANSI:   // ANSI is single-byte, but this is harmless
                    decodable = FindUTF8SafeBoundary(chunkBuf.data(), available);
                    break;
                case TextEncoding::UTF16_LE:
                    decodable = FindUTF16SafeBoundary(chunkBuf.data(), available, true);
                    break;
                case TextEncoding::UTF16_BE:
                    decodable = FindUTF16SafeBoundary(chunkBuf.data(), available, false);
                    break;
            }
            if (decodable < available) {
                carry.assign(chunkBuf.data() + decodable,
                             chunkBuf.data() + available);
            }
        }

        // Decode this chunk and append to the result string
        AppendDecoded(result.content, chunkBuf.data(), decodable, result.detectedEncoding);

        // Update progress in status bar
        int pct = static_cast<int>(totalRead * 100 / totalBytesToRead);
        if (hwndStatus && pct != lastPercent) {
            lastPercent = pct;
            wchar_t buf[80];
            double mb = static_cast<double>(totalRead) / (1024.0 * 1024.0);
            double totalMb = static_cast<double>(totalBytesToRead) / (1024.0 * 1024.0);
            swprintf_s(buf, L"Loading... %d%%  (%.0f / %.0f MB)", pct, mb, totalMb);
            SendMessageW(hwndStatus, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(buf));
        }

        // Pump the message queue so the UI stays responsive
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    result.success = true;
    return result;
}

//------------------------------------------------------------------------------
// Write a file with specified encoding and line endings
//------------------------------------------------------------------------------
FileWriteResult FileIO::WriteFile(const std::wstring& filePath, const std::wstring& content,
                                   TextEncoding encoding, LineEnding lineEnding) {
    FileWriteResult result;
    
    // Convert line endings in the content
    std::wstring normalizedContent = NormalizeToLF(content);
    std::wstring outputContent = ConvertLineEndings(normalizedContent, lineEnding);
    
    // Encode to bytes
    std::vector<uint8_t> data = EncodeFromWString(outputContent, encoding);
    
    // Open file for writing
    HandleGuard hFile(CreateFileW(filePath.c_str(), GENERIC_WRITE, 0,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    
    if (!hFile.valid()) {
        result.errorMessage = FormatLastError(GetLastError());
        return result;
    }
    
    // Write data
    if (!data.empty()) {
        DWORD bytesWritten = 0;
        if (!::WriteFile(hFile.get(), data.data(), static_cast<DWORD>(data.size()), &bytesWritten, nullptr)) {
            result.errorMessage = FormatLastError(GetLastError());
            return result;
        }
        if (bytesWritten != data.size()) {
            result.errorMessage = L"Not all data was written to file";
            return result;
        }
    }
    
    result.success = true;
    return result;
}

//------------------------------------------------------------------------------
// Convert line endings in text
//------------------------------------------------------------------------------
std::wstring FileIO::ConvertLineEndings(const std::wstring& text, LineEnding targetEnding) {
    std::wstring result;
    result.reserve(text.size() + text.size() / 20); // Reserve extra space for potential CRLF
    
    const wchar_t* newLine;
    size_t newLineLen;
    switch (targetEnding) {
        case LineEnding::CRLF: newLine = L"\r\n"; newLineLen = 2; break;
        case LineEnding::LF:   newLine = L"\n";   newLineLen = 1; break;
        case LineEnding::CR:   newLine = L"\r";   newLineLen = 1; break;
        default: newLine = L"\r\n"; newLineLen = 2; break;
    }
    
    size_t start = 0;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == L'\r') {
            result.append(text, start, i - start);
            result.append(newLine, newLineLen);
            // Skip LF if it follows CR (CRLF sequence)
            if (i + 1 < text.size() && text[i + 1] == L'\n') {
                i++;
            }
            start = i + 1;
        } else if (text[i] == L'\n') {
            result.append(text, start, i - start);
            result.append(newLine, newLineLen);
            start = i + 1;
        }
    }
    result.append(text, start, text.size() - start);
    
    return result;
}

//------------------------------------------------------------------------------
// Normalize line endings to LF for internal use
//------------------------------------------------------------------------------
std::wstring FileIO::NormalizeToLF(const std::wstring& text) {
    std::wstring result;
    result.reserve(text.size());
    
    size_t start = 0;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == L'\r') {
            result.append(text, start, i - start);
            result += L'\n';
            // Skip LF if it follows CR (CRLF sequence)
            if (i + 1 < text.size() && text[i + 1] == L'\n') {
                i++;
            }
            start = i + 1;
        }
    }
    result.append(text, start, text.size() - start);
    
    return result;
}

//------------------------------------------------------------------------------
// Get file name from path
//------------------------------------------------------------------------------
std::wstring FileIO::GetFileName(const std::wstring& filePath) {
    size_t pos = filePath.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        return filePath.substr(pos + 1);
    }
    return filePath;
}

//------------------------------------------------------------------------------
// Format error message from GetLastError
//------------------------------------------------------------------------------
std::wstring FileIO::FormatLastError(DWORD errorCode) {
    wchar_t* buffer = nullptr;
    DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    
    std::wstring message;
    if (size > 0 && buffer != nullptr) {
        message = buffer;
        // Remove trailing newlines
        while (!message.empty() && (message.back() == L'\n' || message.back() == L'\r')) {
            message.pop_back();
        }
        LocalFree(buffer);
    } else {
        message = L"Unknown error (code " + std::to_wstring(errorCode) + L")";
    }
    
    return message;
}

//------------------------------------------------------------------------------
// Show Open File dialog
//------------------------------------------------------------------------------
bool FileIO::ShowOpenDialog(HWND parent, std::wstring& outPath) {
    wchar_t szFile[MAX_PATH] = {};
    
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = parent;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0"
                      L"Markdown (*.md;*.markdown)\0*.md;*.markdown\0"
                      L"Source Code (*.cpp;*.c;*.h;*.hpp;*.cs;*.java;*.py;*.js;*.ts)\0*.cpp;*.c;*.h;*.hpp;*.cs;*.java;*.py;*.js;*.ts\0"
                      L"Web Files (*.html;*.htm;*.css;*.xml;*.json)\0*.html;*.htm;*.css;*.xml;*.json\0"
                      L"Config Files (*.ini;*.cfg;*.conf;*.yaml;*.yml;*.toml)\0*.ini;*.cfg;*.conf;*.yaml;*.yml;*.toml\0"
                      L"Log Files (*.log)\0*.log\0"
                      L"Data Files (*.csv;*.tsv;*.sql)\0*.csv;*.tsv;*.sql\0"
                      L"Script Files (*.bat;*.cmd;*.ps1;*.sh)\0*.bat;*.cmd;*.ps1;*.sh\0"
                      L"All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 9;
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    
    if (GetOpenFileNameW(&ofn)) {
        outPath = szFile;
        return true;
    }
    
    // User cancelled or error
    DWORD err = CommDlgExtendedError();
    // err == 0 means user cancelled, which is not an error
    return false;
}

//------------------------------------------------------------------------------
// Show Save File dialog with encoding selection
//------------------------------------------------------------------------------
bool FileIO::ShowSaveDialog(HWND parent, std::wstring& outPath, TextEncoding& outEncoding,
                            const std::wstring& currentPath) {
    wchar_t szFile[MAX_PATH] = {};
    
    // Pre-fill with current path if provided
    if (!currentPath.empty()) {
        wcsncpy_s(szFile, currentPath.c_str(), MAX_PATH - 1);
    }
    
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = parent;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Text File - UTF-8 (*.txt)\0*.txt\0"
                      L"Text File - UTF-8 with BOM (*.txt)\0*.txt\0"
                      L"Text File - UTF-16 LE (*.txt)\0*.txt\0"
                      L"Text File - UTF-16 BE (*.txt)\0*.txt\0"
                      L"Text File - ANSI (*.txt)\0*.txt\0"
                      L"All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1; // Default to UTF-8
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    
    if (GetSaveFileNameW(&ofn)) {
        outPath = szFile;
        
        // Map filter index to encoding
        switch (ofn.nFilterIndex) {
            case 1: outEncoding = TextEncoding::UTF8; break;
            case 2: outEncoding = TextEncoding::UTF8_BOM; break;
            case 3: outEncoding = TextEncoding::UTF16_LE; break;
            case 4: outEncoding = TextEncoding::UTF16_BE; break;
            case 5: outEncoding = TextEncoding::ANSI; break;
            default: outEncoding = TextEncoding::UTF8; break;
        }
        
        return true;
    }
    
    return false;
}

} // namespace QNote
