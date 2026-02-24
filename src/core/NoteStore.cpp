//==============================================================================
// QNote - A Lightweight Notepad Clone
// NoteStore.cpp - Note storage and management implementation
//==============================================================================

#include "NoteStore.h"
#include <ShlObj.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <locale>
#include <codecvt>

namespace QNote {

//------------------------------------------------------------------------------
// Note methods
//------------------------------------------------------------------------------

std::wstring Note::GetDisplayTitle() const {
    if (!title.empty()) {
        return title;
    }
    
    // Generate from first line of content
    if (content.empty()) {
        return L"Untitled Note";
    }
    
    // Find first newline
    size_t newlinePos = content.find_first_of(L"\r\n");
    std::wstring firstLine = (newlinePos != std::wstring::npos) 
        ? content.substr(0, newlinePos) 
        : content;
    
    // Trim whitespace
    size_t start = firstLine.find_first_not_of(L" \t");
    if (start == std::wstring::npos) {
        return L"Untitled Note";
    }
    size_t end = firstLine.find_last_not_of(L" \t");
    firstLine = firstLine.substr(start, end - start + 1);
    
    // Truncate if too long
    if (firstLine.length() > 50) {
        firstLine = firstLine.substr(0, 47) + L"...";
    }
    
    return firstLine.empty() ? L"Untitled Note" : firstLine;
}

std::wstring Note::GetCreatedDateString() const {
    return FormatTimestamp(createdAt);
}

std::wstring Note::GetUpdatedDateString() const {
    return FormatTimestamp(updatedAt);
}

//------------------------------------------------------------------------------
// NoteStore methods
//------------------------------------------------------------------------------

NoteStore::NoteStore() {
    // Get AppData path for note storage
    wchar_t appDataPath[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appDataPath))) {
        m_storeDir = appDataPath;
        m_storeDir += L"\\QNote";
        m_storePath = m_storeDir + L"\\notes.json";
    }
}

NoteStore::~NoteStore() {
    // Auto-save on destruction if dirty
    if (m_dirty) {
        (void)Save();
    }
}

bool NoteStore::Initialize() {
    if (m_storeDir.empty()) {
        return false;
    }
    
    // Ensure directory exists
    DWORD attrs = GetFileAttributesW(m_storeDir.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        if (!CreateDirectoryW(m_storeDir.c_str(), nullptr)) {
            return false;
        }
    }
    
    // Load existing notes
    return LoadFromFile();
}

Note NoteStore::CreateNote(const std::wstring& content) {
    Note note;
    note.id = GenerateNoteId();
    note.content = content;
    note.createdAt = std::time(nullptr);
    note.updatedAt = note.createdAt;
    note.isPinned = false;
    
    m_notes.push_back(note);
    m_dirty = true;
    (void)Save();  // Autosave
    
    return note;
}

std::optional<Note> NoteStore::GetNote(const std::wstring& id) const {
    auto it = std::find_if(m_notes.begin(), m_notes.end(),
        [&id](const Note& n) { return n.id == id; });
    
    if (it != m_notes.end()) {
        return *it;
    }
    return std::nullopt;
}

bool NoteStore::UpdateNote(const Note& note) {
    auto it = std::find_if(m_notes.begin(), m_notes.end(),
        [&note](const Note& n) { return n.id == note.id; });
    
    if (it != m_notes.end()) {
        it->content = note.content;
        it->title = note.title;
        it->updatedAt = std::time(nullptr);
        it->isPinned = note.isPinned;
        m_dirty = true;
        (void)Save();  // Autosave
        return true;
    }
    return false;
}

bool NoteStore::DeleteNote(const std::wstring& id) {
    auto it = std::find_if(m_notes.begin(), m_notes.end(),
        [&id](const Note& n) { return n.id == id; });
    
    if (it != m_notes.end()) {
        m_notes.erase(it);
        m_dirty = true;
        (void)Save();  // Autosave
        return true;
    }
    return false;
}

bool NoteStore::TogglePin(const std::wstring& id) {
    auto it = std::find_if(m_notes.begin(), m_notes.end(),
        [&id](const Note& n) { return n.id == id; });
    
    if (it != m_notes.end()) {
        it->isPinned = !it->isPinned;
        it->updatedAt = std::time(nullptr);
        m_dirty = true;
        (void)Save();  // Autosave
        return true;
    }
    return false;
}

std::vector<Note> NoteStore::GetNotes(const NoteFilter& filter) const {
    return FilterAndSort(filter);
}

std::vector<Note> NoteStore::GetPinnedNotes() const {
    NoteFilter filter;
    filter.pinnedOnly = true;
    return FilterAndSort(filter);
}

std::vector<NoteSearchResult> NoteStore::SearchNotes(const std::wstring& query, bool matchCase) const {
    std::vector<NoteSearchResult> results;
    
    if (query.empty()) {
        return results;
    }
    
    std::wstring searchQuery = query;
    if (!matchCase) {
        std::transform(searchQuery.begin(), searchQuery.end(), searchQuery.begin(), ::towlower);
    }
    
    for (const auto& note : m_notes) {
        std::wstring searchText = note.content;
        if (!matchCase) {
            std::transform(searchText.begin(), searchText.end(), searchText.begin(), ::towlower);
        }
        
        size_t pos = searchText.find(searchQuery);
        if (pos != std::wstring::npos) {
            NoteSearchResult result;
            result.note = note;
            result.matchStart = pos;
            result.matchLength = query.length();
            
            // Create context snippet
            size_t snippetStart = (pos > 30) ? pos - 30 : 0;
            size_t snippetEnd = std::min(pos + query.length() + 50, note.content.length());
            result.contextSnippet = L"";
            if (snippetStart > 0) result.contextSnippet = L"...";
            result.contextSnippet += note.content.substr(snippetStart, snippetEnd - snippetStart);
            if (snippetEnd < note.content.length()) result.contextSnippet += L"...";
            
            results.push_back(result);
        }
    }
    
    return results;
}

std::vector<Note> NoteStore::GetNotesForDate(int year, int month, int day) const {
    std::vector<Note> result;
    
    struct tm targetDate = {};
    targetDate.tm_year = year - 1900;
    targetDate.tm_mon = month - 1;
    targetDate.tm_mday = day;
    time_t targetStart = mktime(&targetDate);
    time_t targetEnd = targetStart + 24 * 60 * 60;  // Next day
    
    for (const auto& note : m_notes) {
        if (note.createdAt >= targetStart && note.createdAt < targetEnd) {
            result.push_back(note);
        }
    }
    
    // Sort by creation time descending
    std::sort(result.begin(), result.end(),
        [](const Note& a, const Note& b) { return a.createdAt > b.createdAt; });
    
    return result;
}

std::vector<time_t> NoteStore::GetNoteDates() const {
    std::vector<time_t> dates;
    
    for (const auto& note : m_notes) {
        time_t midnight = GetMidnight(note.createdAt);
        if (std::find(dates.begin(), dates.end(), midnight) == dates.end()) {
            dates.push_back(midnight);
        }
    }
    
    // Sort dates descending (most recent first)
    std::sort(dates.begin(), dates.end(), std::greater<time_t>());
    
    return dates;
}

std::optional<Note> NoteStore::GetActiveNote() const {
    if (m_activeNoteId.empty()) {
        return std::nullopt;
    }
    return GetNote(m_activeNoteId);
}

void NoteStore::SetActiveNote(const std::wstring& id) {
    m_activeNoteId = id;
}

bool NoteStore::Save() {
    if (!m_dirty) {
        return true;
    }
    bool result = SaveToFile();
    if (result) {
        m_dirty = false;
    }
    return result;
}

bool NoteStore::LoadFromFile() {
    // Check if file exists
    if (GetFileAttributesW(m_storePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        // No notes file yet, start fresh
        return true;
    }
    
    // Read file
    HANDLE hFile = CreateFileW(m_storePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return false;
    }
    
    // Read as UTF-8
    std::vector<char> buffer(static_cast<size_t>(fileSize.QuadPart) + 1, 0);
    DWORD bytesRead;
    if (!ReadFile(hFile, buffer.data(), static_cast<DWORD>(fileSize.QuadPart), &bytesRead, nullptr)) {
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);
    
    // Convert UTF-8 to wstring
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, buffer.data(), bytesRead, nullptr, 0);
    std::wstring json(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, buffer.data(), bytesRead, &json[0], wideLen);
    
    return ParseJson(json);
}

bool NoteStore::SaveToFile() {
    std::wstring json = ToJson();
    
    // Convert to UTF-8
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, json.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::vector<char> utf8Data(utf8Len);
    WideCharToMultiByte(CP_UTF8, 0, json.c_str(), -1, utf8Data.data(), utf8Len, nullptr, nullptr);
    
    // Write to file
    HANDLE hFile = CreateFileW(m_storePath.c_str(), GENERIC_WRITE, 0,
                               nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    DWORD bytesWritten;
    BOOL result = WriteFile(hFile, utf8Data.data(), static_cast<DWORD>(utf8Len - 1), &bytesWritten, nullptr);
    CloseHandle(hFile);
    
    return result != FALSE;
}

std::wstring NoteStore::GenerateNoteId() const {
    // Generate ID based on timestamp + random component
    time_t now = std::time(nullptr);
    DWORD tick = GetTickCount();
    
    std::wstringstream ss;
    ss << std::hex << now << L"-" << tick;
    return ss.str();
}

bool NoteStore::ParseJson(const std::wstring& json) {
    // Simple JSON parser for our notes format
    m_notes.clear();
    
    if (json.empty() || json.find(L"[") == std::wstring::npos) {
        return true;  // Empty or invalid, start fresh
    }
    
    // Find array start
    size_t arrStart = json.find(L"[");
    size_t arrEnd = json.rfind(L"]");
    if (arrStart == std::wstring::npos || arrEnd == std::wstring::npos) {
        return true;
    }
    
    // Parse each note object
    size_t pos = arrStart + 1;
    while (pos < arrEnd) {
        // Find object start
        size_t objStart = json.find(L"{", pos);
        if (objStart == std::wstring::npos || objStart >= arrEnd) break;
        
        // Find matching object end (handle nested braces)
        int braceCount = 1;
        size_t objEnd = objStart + 1;
        while (objEnd < arrEnd && braceCount > 0) {
            if (json[objEnd] == L'{') braceCount++;
            else if (json[objEnd] == L'}') braceCount--;
            objEnd++;
        }
        
        if (braceCount != 0) break;
        
        std::wstring noteJson = json.substr(objStart, objEnd - objStart);
        
        // Parse note fields
        Note note;
        
        // Helper: find end of a JSON string value, handling escape sequences
        auto findStringEnd = [&noteJson](size_t start) -> size_t {
            size_t pos = start;
            while (pos < noteJson.length()) {
                if (noteJson[pos] == L'\\') {
                    pos += 2;  // Skip escaped char
                } else if (noteJson[pos] == L'"') {
                    return pos;
                } else {
                    pos++;
                }
            }
            return std::wstring::npos;
        };
        
        // Parse id (with escape-aware string end)
        size_t idPos = noteJson.find(L"\"id\"");
        if (idPos != std::wstring::npos) {
            size_t valStart = noteJson.find(L"\"", idPos + 4) + 1;
            size_t valEnd = findStringEnd(valStart);
            if (valEnd != std::wstring::npos) {
                note.id = JsonUnescape(noteJson.substr(valStart, valEnd - valStart));
            }
        }
        
        // Parse content (already escape-aware)
        size_t contentPos = noteJson.find(L"\"content\"");
        if (contentPos != std::wstring::npos) {
            size_t valStart = noteJson.find(L"\"", contentPos + 9) + 1;
            // Find end quote (handling escapes)
            size_t valEnd = valStart;
            while (valEnd < noteJson.length()) {
                if (noteJson[valEnd] == L'\\') {
                    valEnd += 2;  // Skip escaped char
                } else if (noteJson[valEnd] == L'"') {
                    break;
                } else {
                    valEnd++;
                }
            }
            note.content = JsonUnescape(noteJson.substr(valStart, valEnd - valStart));
        }
        
        // Parse title (with escape-aware string end)
        size_t titlePos = noteJson.find(L"\"title\"");
        if (titlePos != std::wstring::npos) {
            size_t valStart = noteJson.find(L"\"", titlePos + 7) + 1;
            size_t valEnd = findStringEnd(valStart);
            if (valEnd != std::wstring::npos) {
                note.title = JsonUnescape(noteJson.substr(valStart, valEnd - valStart));
            }
        }
        
        // Parse createdAt
        size_t createdPos = noteJson.find(L"\"createdAt\"");
        if (createdPos != std::wstring::npos) {
            size_t valStart = noteJson.find(L":", createdPos) + 1;
            while (valStart < noteJson.length() && (noteJson[valStart] == L' ' || noteJson[valStart] == L'\t')) valStart++;
            std::wstring numStr;
            while (valStart < noteJson.length() && noteJson[valStart] >= L'0' && noteJson[valStart] <= L'9') {
                numStr += noteJson[valStart++];
            }
            if (!numStr.empty()) {
                note.createdAt = static_cast<time_t>(_wtoll(numStr.c_str()));
            }
        }
        
        // Parse updatedAt
        size_t updatedPos = noteJson.find(L"\"updatedAt\"");
        if (updatedPos != std::wstring::npos) {
            size_t valStart = noteJson.find(L":", updatedPos) + 1;
            while (valStart < noteJson.length() && (noteJson[valStart] == L' ' || noteJson[valStart] == L'\t')) valStart++;
            std::wstring numStr;
            while (valStart < noteJson.length() && noteJson[valStart] >= L'0' && noteJson[valStart] <= L'9') {
                numStr += noteJson[valStart++];
            }
            if (!numStr.empty()) {
                note.updatedAt = static_cast<time_t>(_wtoll(numStr.c_str()));
            }
        }
        
        // Parse isPinned
        size_t pinnedPos = noteJson.find(L"\"isPinned\"");
        if (pinnedPos != std::wstring::npos) {
            size_t valStart = noteJson.find(L":", pinnedPos) + 1;
            // Skip whitespace
            while (valStart < noteJson.length() && (noteJson[valStart] == L' ' || noteJson[valStart] == L'\t')) valStart++;
            // Check if value starts with 't' (true)
            note.isPinned = (valStart < noteJson.length() && noteJson[valStart] == L't');
        }
        
        if (!note.id.empty()) {
            m_notes.push_back(note);
        }
        
        pos = objEnd;
    }
    
    return true;
}

std::wstring NoteStore::ToJson() const {
    std::wstringstream ss;
    ss << L"[\n";
    
    for (size_t i = 0; i < m_notes.size(); ++i) {
        const Note& note = m_notes[i];
        ss << L"  {\n";
        ss << L"    \"id\": \"" << JsonEscape(note.id) << L"\",\n";
        ss << L"    \"title\": \"" << JsonEscape(note.title) << L"\",\n";
        ss << L"    \"content\": \"" << JsonEscape(note.content) << L"\",\n";
        ss << L"    \"createdAt\": " << note.createdAt << L",\n";
        ss << L"    \"updatedAt\": " << note.updatedAt << L",\n";
        ss << L"    \"isPinned\": " << (note.isPinned ? L"true" : L"false") << L"\n";
        ss << L"  }";
        if (i < m_notes.size() - 1) {
            ss << L",";
        }
        ss << L"\n";
    }
    
    ss << L"]\n";
    return ss.str();
}

std::wstring NoteStore::JsonEscape(const std::wstring& str) {
    std::wstring result;
    result.reserve(str.length() * 2);
    
    for (wchar_t c : str) {
        switch (c) {
            case L'\\': result += L"\\\\"; break;
            case L'"':  result += L"\\\""; break;
            case L'\n': result += L"\\n"; break;
            case L'\r': result += L"\\r"; break;
            case L'\t': result += L"\\t"; break;
            case L'\b': result += L"\\b"; break;
            case L'\f': result += L"\\f"; break;
            default:
                if (c < 0x20) {
                    // Control character - use \uXXXX format
                    wchar_t buf[8];
                    swprintf_s(buf, L"\\u%04x", static_cast<unsigned int>(c));
                    result += buf;
                } else {
                    result += c;
                }
                break;
        }
    }
    
    return result;
}

std::wstring NoteStore::JsonUnescape(const std::wstring& str) {
    std::wstring result;
    result.reserve(str.length());
    
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == L'\\' && i + 1 < str.length()) {
            switch (str[i + 1]) {
                case L'\\': result += L'\\'; i++; break;
                case L'"':  result += L'"'; i++; break;
                case L'n':  result += L'\n'; i++; break;
                case L'r':  result += L'\r'; i++; break;
                case L't':  result += L'\t'; i++; break;
                case L'b':  result += L'\b'; i++; break;
                case L'f':  result += L'\f'; i++; break;
                case L'u':
                    if (i + 5 < str.length()) {
                        std::wstring hex = str.substr(i + 2, 4);
                        wchar_t c = static_cast<wchar_t>(wcstoul(hex.c_str(), nullptr, 16));
                        result += c;
                        i += 5;
                    }
                    break;
                default:
                    result += str[i];
                    break;
            }
        } else {
            result += str[i];
        }
    }
    
    return result;
}

std::vector<Note> NoteStore::FilterAndSort(const NoteFilter& filter) const {
    std::vector<Note> result;
    
    for (const auto& note : m_notes) {
        // Apply filters
        if (filter.pinnedOnly && !note.isPinned) {
            continue;
        }
        
        if (filter.fromDate != 0 && note.createdAt < filter.fromDate) {
            continue;
        }
        
        if (filter.toDate != 0 && note.createdAt > filter.toDate) {
            continue;
        }
        
        if (!filter.searchQuery.empty()) {
            std::wstring lowerContent = note.content;
            std::wstring lowerTitle = note.title;
            std::wstring lowerQuery = filter.searchQuery;
            std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(), ::towlower);
            std::transform(lowerTitle.begin(), lowerTitle.end(), lowerTitle.begin(), ::towlower);
            std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::towlower);
            
            if (lowerContent.find(lowerQuery) == std::wstring::npos &&
                lowerTitle.find(lowerQuery) == std::wstring::npos) {
                continue;
            }
        }
        
        result.push_back(note);
    }
    
    // Apply sorting
    switch (filter.sortBy) {
        case NoteFilter::SortBy::UpdatedDesc:
            std::sort(result.begin(), result.end(),
                [](const Note& a, const Note& b) { return a.updatedAt > b.updatedAt; });
            break;
        case NoteFilter::SortBy::UpdatedAsc:
            std::sort(result.begin(), result.end(),
                [](const Note& a, const Note& b) { return a.updatedAt < b.updatedAt; });
            break;
        case NoteFilter::SortBy::CreatedDesc:
            std::sort(result.begin(), result.end(),
                [](const Note& a, const Note& b) { return a.createdAt > b.createdAt; });
            break;
        case NoteFilter::SortBy::CreatedAsc:
            std::sort(result.begin(), result.end(),
                [](const Note& a, const Note& b) { return a.createdAt < b.createdAt; });
            break;
        case NoteFilter::SortBy::TitleAsc:
            std::sort(result.begin(), result.end(),
                [](const Note& a, const Note& b) { 
                    return _wcsicmp(a.GetDisplayTitle().c_str(), b.GetDisplayTitle().c_str()) < 0; 
                });
            break;
        case NoteFilter::SortBy::TitleDesc:
            std::sort(result.begin(), result.end(),
                [](const Note& a, const Note& b) { 
                    return _wcsicmp(a.GetDisplayTitle().c_str(), b.GetDisplayTitle().c_str()) > 0; 
                });
            break;
    }
    
    return result;
}

size_t NoteStore::FindCaseInsensitive(const std::wstring& haystack, 
                                       const std::wstring& needle,
                                       size_t startPos) {
    if (needle.empty()) return std::wstring::npos;
    
    std::wstring lowerHaystack = haystack;
    std::wstring lowerNeedle = needle;
    std::transform(lowerHaystack.begin(), lowerHaystack.end(), lowerHaystack.begin(), ::towlower);
    std::transform(lowerNeedle.begin(), lowerNeedle.end(), lowerNeedle.begin(), ::towlower);
    
    return lowerHaystack.find(lowerNeedle, startPos);
}

//------------------------------------------------------------------------------
// Utility functions
//------------------------------------------------------------------------------

std::wstring FormatTimestamp(time_t timestamp, bool includeTime) {
    struct tm localTime;
    localtime_s(&localTime, &timestamp);
    
    wchar_t buffer[64];
    if (includeTime) {
        wcsftime(buffer, 64, L"%Y-%m-%d %H:%M", &localTime);
    } else {
        wcsftime(buffer, 64, L"%Y-%m-%d", &localTime);
    }
    
    return buffer;
}

time_t GetMidnight(time_t timestamp) {
    struct tm localTime;
    localtime_s(&localTime, &timestamp);
    
    localTime.tm_hour = 0;
    localTime.tm_min = 0;
    localTime.tm_sec = 0;
    
    return mktime(&localTime);
}

time_t ParseDate(const std::wstring& dateStr) {
    int year, month, day;
    if (swscanf_s(dateStr.c_str(), L"%d-%d-%d", &year, &month, &day) == 3) {
        struct tm date = {};
        date.tm_year = year - 1900;
        date.tm_mon = month - 1;
        date.tm_mday = day;
        return mktime(&date);
    }
    return 0;
}

} // namespace QNote
