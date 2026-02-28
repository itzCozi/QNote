//==============================================================================
// QNote - A Lightweight Notepad Clone
// NoteStore.cpp - Note storage and management implementation
//==============================================================================

#include "NoteStore.h"
#include "FileIO.h"
#include <ShlObj.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <unordered_set>

namespace QNote {

//------------------------------------------------------------------------------
// NoteSummary methods
//------------------------------------------------------------------------------

std::wstring NoteSummary::GetDisplayTitle() const {
    if (!title.empty()) {
        return title;
    }
    return L"Untitled Note";
}

std::wstring NoteSummary::GetCreatedDateString() const {
    return FormatTimestamp(createdAt);
}

std::wstring NoteSummary::GetUpdatedDateString() const {
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
        m_notesDir = m_storeDir + L"\\notes";
        m_storePath = m_storeDir + L"\\notes_index.json";
        m_legacyStorePath = m_storeDir + L"\\notes.json";
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
    
    // Ensure base directory exists
    DWORD attrs = GetFileAttributesW(m_storeDir.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        if (!CreateDirectoryW(m_storeDir.c_str(), nullptr)) {
            return false;
        }
    }
    
    // Ensure notes content directory exists
    attrs = GetFileAttributesW(m_notesDir.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        if (!CreateDirectoryW(m_notesDir.c_str(), nullptr)) {
            return false;
        }
    }
    
    // Try loading new index format first
    if (GetFileAttributesW(m_storePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        if (!LoadFromFile()) {
            return false;
        }
        // Backfill content previews for notes saved before preview support
        bool needsSave = false;
        for (auto& summary : m_notes) {
            if (summary.contentPreview.empty()) {
                std::wstring content = LoadNoteContentFromDisk(summary.id);
                if (!content.empty()) {
                    summary.contentPreview = MakeContentPreview(content);
                    needsSave = true;
                }
            }
        }
        if (needsSave) {
            m_dirty = true;
            (void)Save();
        }
        return true;
    }
    
    // Check for legacy format and migrate
    if (GetFileAttributesW(m_legacyStorePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return MigrateFromLegacyFormat();
    }
    
    // No notes file yet, start fresh
    return true;
}

Note NoteStore::CreateNote(const std::wstring& content) {
    Note note;
    note.id = GenerateNoteId();
    note.content = content;
    note.createdAt = std::time(nullptr);
    note.updatedAt = note.createdAt;
    note.isPinned = false;
    
    // Generate title from first line of content
    if (!content.empty()) {
        size_t newlinePos = content.find_first_of(L"\r\n");
        std::wstring firstLine = (newlinePos != std::wstring::npos) 
            ? content.substr(0, newlinePos) 
            : content;
        size_t start = firstLine.find_first_not_of(L" \t");
        if (start != std::wstring::npos) {
            size_t end = firstLine.find_last_not_of(L" \t");
            firstLine = firstLine.substr(start, end - start + 1);
            if (firstLine.length() > 50) {
                firstLine = firstLine.substr(0, 47) + L"...";
            }
            note.title = firstLine;
        }
    }
    
    // Save content to individual file
    (void)SaveNoteContent(note.id, content);
    
    // Add only metadata to in-memory index
    NoteSummary summary;
    summary.id = note.id;
    summary.title = note.title;
    summary.contentPreview = MakeContentPreview(content);
    summary.createdAt = note.createdAt;
    summary.updatedAt = note.updatedAt;
    summary.isPinned = note.isPinned;
    m_notes.push_back(summary);
    
    m_dirty = true;
    (void)Save();  // Autosave index
    
    return note;
}

std::optional<Note> NoteStore::GetNote(const std::wstring& id) const {
    auto it = std::find_if(m_notes.begin(), m_notes.end(),
        [&id](const NoteSummary& n) { return n.id == id; });
    
    if (it != m_notes.end()) {
        Note note;
        // Copy metadata from summary
        static_cast<NoteSummary&>(note) = *it;
        // Load content from individual file
        note.content = LoadNoteContent(id);
        return note;
    }
    return std::nullopt;
}

std::optional<NoteSummary> NoteStore::GetNoteSummary(const std::wstring& id) const {
    auto it = std::find_if(m_notes.begin(), m_notes.end(),
        [&id](const NoteSummary& n) { return n.id == id; });
    
    if (it != m_notes.end()) {
        return *it;
    }
    return std::nullopt;
}

bool NoteStore::UpdateNote(const Note& note) {
    auto it = std::find_if(m_notes.begin(), m_notes.end(),
        [&note](const NoteSummary& n) { return n.id == note.id; });
    
    if (it != m_notes.end()) {
        // Update title from content if not explicitly set
        std::wstring newTitle = note.title;
        if (newTitle.empty() && !note.content.empty()) {
            size_t newlinePos = note.content.find_first_of(L"\r\n");
            std::wstring firstLine = (newlinePos != std::wstring::npos) 
                ? note.content.substr(0, newlinePos) 
                : note.content;
            size_t start = firstLine.find_first_not_of(L" \t");
            if (start != std::wstring::npos) {
                size_t end = firstLine.find_last_not_of(L" \t");
                firstLine = firstLine.substr(start, end - start + 1);
                if (firstLine.length() > 50) {
                    firstLine = firstLine.substr(0, 47) + L"...";
                }
                newTitle = firstLine;
            }
        }
        
        it->title = newTitle;
        it->updatedAt = std::time(nullptr);
        it->isPinned = note.isPinned;
        
        // Save content to individual file
        (void)SaveNoteContent(note.id, note.content);
        
        m_dirty = true;
        (void)Save();  // Autosave index
        return true;
    }
    return false;
}

bool NoteStore::DeleteNote(const std::wstring& id) {
    auto it = std::find_if(m_notes.begin(), m_notes.end(),
        [&id](const NoteSummary& n) { return n.id == id; });
    
    if (it != m_notes.end()) {
        m_notes.erase(it);
        DeleteNoteContent(id);  // Remove content file
        m_dirty = true;
        (void)Save();  // Autosave index
        return true;
    }
    return false;
}

bool NoteStore::TogglePin(const std::wstring& id) {
    auto it = std::find_if(m_notes.begin(), m_notes.end(),
        [&id](const NoteSummary& n) { return n.id == id; });
    
    if (it != m_notes.end()) {
        it->isPinned = !it->isPinned;
        it->updatedAt = std::time(nullptr);
        m_dirty = true;
        (void)Save();  // Autosave index
        return true;
    }
    return false;
}

std::vector<NoteSummary> NoteStore::GetNotes(const NoteFilter& filter) const {
    return FilterAndSort(filter);
}

std::vector<NoteSummary> NoteStore::GetPinnedNotes() const {
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
    
    for (const auto& summary : m_notes) {
        // Quick pre-filter: check title and preview before loading full content
        std::wstring titleText = summary.title;
        std::wstring previewText = summary.contentPreview;
        if (!matchCase) {
            std::transform(titleText.begin(), titleText.end(), titleText.begin(), ::towlower);
            std::transform(previewText.begin(), previewText.end(), previewText.begin(), ::towlower);
        }
        
        bool mightMatch = (titleText.find(searchQuery) != std::wstring::npos) ||
                          (previewText.find(searchQuery) != std::wstring::npos);
        
        // If preview didn't match and the note is longer than the preview, still check full content
        if (!mightMatch && summary.contentPreview.length() < PREVIEW_LENGTH) {
            continue;  // Short note — preview IS the full content, no match
        }
        
        // Load full content (needed for match position and snippet, uses LRU cache)
        std::wstring content = LoadNoteContent(summary.id);
        
        std::wstring searchText = content;
        if (!matchCase) {
            std::transform(searchText.begin(), searchText.end(), searchText.begin(), ::towlower);
        }
        
        size_t pos = searchText.find(searchQuery);
        if (pos != std::wstring::npos) {
            NoteSearchResult result;
            result.summary = summary;
            result.matchStart = pos;
            result.matchLength = query.length();
            
            // Create context snippet
            size_t snippetStart = (pos > 30) ? pos - 30 : 0;
            size_t snippetEnd = std::min(pos + query.length() + 50, content.length());
            result.contextSnippet = L"";
            if (snippetStart > 0) result.contextSnippet = L"...";
            result.contextSnippet += content.substr(snippetStart, snippetEnd - snippetStart);
            if (snippetEnd < content.length()) result.contextSnippet += L"...";
            
            results.push_back(result);
        }
    }
    
    return results;
}

std::vector<NoteSummary> NoteStore::GetNotesForDate(int year, int month, int day) const {
    std::vector<NoteSummary> result;
    
    struct tm targetDate = {};
    targetDate.tm_year = year - 1900;
    targetDate.tm_mon = month - 1;
    targetDate.tm_mday = day;
    time_t targetStart = mktime(&targetDate);
    time_t targetEnd = targetStart + 24 * 60 * 60;  // Next day
    
    for (const auto& summary : m_notes) {
        if (summary.createdAt >= targetStart && summary.createdAt < targetEnd) {
            result.push_back(summary);
        }
    }
    
    // Sort by creation time descending
    std::sort(result.begin(), result.end(),
        [](const NoteSummary& a, const NoteSummary& b) { return a.createdAt > b.createdAt; });
    
    return result;
}

std::vector<time_t> NoteStore::GetNoteDates() const {
    std::unordered_set<time_t> uniqueDates;
    uniqueDates.reserve(m_notes.size());
    
    for (const auto& note : m_notes) {
        time_t midnight = GetMidnight(note.createdAt);
        uniqueDates.insert(midnight);
    }
    
    // Convert to vector and sort descending (most recent first)
    std::vector<time_t> dates(uniqueDates.begin(), uniqueDates.end());
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
    HandleGuard hFile(CreateFileW(m_storePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!hFile.valid()) {
        return false;
    }
    
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile.get(), &fileSize)) {
        return false;
    }
    
    // Read as UTF-8
    std::vector<char> buffer(static_cast<size_t>(fileSize.QuadPart) + 1, 0);
    DWORD bytesRead;
    if (!ReadFile(hFile.get(), buffer.data(), static_cast<DWORD>(fileSize.QuadPart), &bytesRead, nullptr)) {
        return false;
    }
    hFile = HandleGuard();
    
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
    if (utf8Len <= 0) return false;
    std::vector<char> utf8Data(utf8Len);
    WideCharToMultiByte(CP_UTF8, 0, json.c_str(), -1, utf8Data.data(), utf8Len, nullptr, nullptr);
    
    // Atomic write: write to temp file, then rename over the real file
    std::wstring tempPath = m_storePath + L".tmp";
    
    HandleGuard hFile(CreateFileW(tempPath.c_str(), GENERIC_WRITE, 0,
                               nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!hFile.valid()) {
        return false;
    }
    
    DWORD bytesWritten;
    BOOL result = WriteFile(hFile.get(), utf8Data.data(), static_cast<DWORD>(utf8Len - 1), &bytesWritten, nullptr);
    
    // Flush to disk before renaming
    FlushFileBuffers(hFile.get());
    hFile = HandleGuard();  // Close before rename
    
    if (!result) {
        DeleteFileW(tempPath.c_str());
        return false;
    }
    
    // Atomic replace: MoveFileEx with MOVEFILE_REPLACE_EXISTING
    if (!MoveFileExW(tempPath.c_str(), m_storePath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(tempPath.c_str());
        return false;
    }
    
    return true;
}

std::wstring NoteStore::GenerateNoteId() const {
    // Generate ID: timestamp + high-resolution counter for uniqueness
    time_t now = std::time(nullptr);
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    
    std::wstringstream ss;
    ss << std::hex << now << L"-" << (counter.QuadPart & 0xFFFFFFFF);
    return ss.str();
}

bool NoteStore::ParseJson(const std::wstring& json) {
    // Simple JSON parser for note index (metadata only, no content)
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
        
        NoteSummary summary;
        
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
        
        // Parse id
        size_t idPos = noteJson.find(L"\"id\"");
        if (idPos != std::wstring::npos) {
            size_t valStart = noteJson.find(L"\"", idPos + 4) + 1;
            size_t valEnd = findStringEnd(valStart);
            if (valEnd != std::wstring::npos) {
                summary.id = JsonUnescape(noteJson.substr(valStart, valEnd - valStart));
            }
        }
        
        // Parse title
        size_t titlePos = noteJson.find(L"\"title\"");
        if (titlePos != std::wstring::npos) {
            size_t valStart = noteJson.find(L"\"", titlePos + 7) + 1;
            size_t valEnd = findStringEnd(valStart);
            if (valEnd != std::wstring::npos) {
                summary.title = JsonUnescape(noteJson.substr(valStart, valEnd - valStart));
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
                summary.createdAt = static_cast<time_t>(_wtoll(numStr.c_str()));
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
                summary.updatedAt = static_cast<time_t>(_wtoll(numStr.c_str()));
            }
        }
        
        // Parse isPinned
        size_t pinnedPos = noteJson.find(L"\"isPinned\"");
        if (pinnedPos != std::wstring::npos) {
            size_t valStart = noteJson.find(L":", pinnedPos) + 1;
            while (valStart < noteJson.length() && (noteJson[valStart] == L' ' || noteJson[valStart] == L'\t')) valStart++;
            summary.isPinned = (valStart < noteJson.length() && noteJson[valStart] == L't');
        }
        
        // Parse contentPreview
        size_t previewPos = noteJson.find(L"\"contentPreview\"");
        if (previewPos != std::wstring::npos) {
            size_t valStart = noteJson.find(L"\"", previewPos + 16) + 1;
            size_t valEnd = findStringEnd(valStart);
            if (valEnd != std::wstring::npos) {
                summary.contentPreview = JsonUnescape(noteJson.substr(valStart, valEnd - valStart));
            }
        }
        
        if (!summary.id.empty()) {
            m_notes.push_back(summary);
        }
        
        pos = objEnd;
    }
    
    return true;
}

std::wstring NoteStore::ToJson() const {
    // Serialize index only (metadata, no content)
    std::wstringstream ss;
    ss << L"[\n";
    
    for (size_t i = 0; i < m_notes.size(); ++i) {
        const NoteSummary& summary = m_notes[i];
        ss << L"  {\n";
        ss << L"    \"id\": \"" << JsonEscape(summary.id) << L"\",\n";
        ss << L"    \"title\": \"" << JsonEscape(summary.title) << L"\",\n";
        ss << L"    \"contentPreview\": \"" << JsonEscape(summary.contentPreview) << L"\",\n";
        ss << L"    \"createdAt\": " << summary.createdAt << L",\n";
        ss << L"    \"updatedAt\": " << summary.updatedAt << L",\n";
        ss << L"    \"isPinned\": " << (summary.isPinned ? L"true" : L"false") << L"\n";
        ss << L"  }";
        if (i < m_notes.size() - 1) {
            ss << L",";
        }
        ss << L"\n";
    }
    
    ss << L"]\n";
    return ss.str();
}

bool NoteStore::ParseLegacyJson(const std::wstring& json,
                                 std::vector<Note>& outNotes) const {
    // Parse the old format that has embedded content
    outNotes.clear();
    
    if (json.empty() || json.find(L"[") == std::wstring::npos) {
        return true;
    }
    
    size_t arrStart = json.find(L"[");
    size_t arrEnd = json.rfind(L"]");
    if (arrStart == std::wstring::npos || arrEnd == std::wstring::npos) {
        return true;
    }
    
    size_t pos = arrStart + 1;
    while (pos < arrEnd) {
        size_t objStart = json.find(L"{", pos);
        if (objStart == std::wstring::npos || objStart >= arrEnd) break;
        
        int braceCount = 1;
        size_t objEnd = objStart + 1;
        while (objEnd < arrEnd && braceCount > 0) {
            if (json[objEnd] == L'{') braceCount++;
            else if (json[objEnd] == L'}') braceCount--;
            objEnd++;
        }
        
        if (braceCount != 0) break;
        
        std::wstring noteJson = json.substr(objStart, objEnd - objStart);
        
        Note note;
        
        auto findStringEnd = [&noteJson](size_t start) -> size_t {
            size_t pos = start;
            while (pos < noteJson.length()) {
                if (noteJson[pos] == L'\\') {
                    pos += 2;
                } else if (noteJson[pos] == L'"') {
                    return pos;
                } else {
                    pos++;
                }
            }
            return std::wstring::npos;
        };
        
        // Parse id
        size_t idPos = noteJson.find(L"\"id\"");
        if (idPos != std::wstring::npos) {
            size_t valStart = noteJson.find(L"\"", idPos + 4) + 1;
            size_t valEnd = findStringEnd(valStart);
            if (valEnd != std::wstring::npos) {
                note.id = JsonUnescape(noteJson.substr(valStart, valEnd - valStart));
            }
        }
        
        // Parse content
        size_t contentPos = noteJson.find(L"\"content\"");
        if (contentPos != std::wstring::npos) {
            size_t valStart = noteJson.find(L"\"", contentPos + 9) + 1;
            size_t valEnd = valStart;
            while (valEnd < noteJson.length()) {
                if (noteJson[valEnd] == L'\\') {
                    valEnd += 2;
                } else if (noteJson[valEnd] == L'"') {
                    break;
                } else {
                    valEnd++;
                }
            }
            note.content = JsonUnescape(noteJson.substr(valStart, valEnd - valStart));
        }
        
        // Parse title
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
            while (valStart < noteJson.length() && (noteJson[valStart] == L' ' || noteJson[valStart] == L'\t')) valStart++;
            note.isPinned = (valStart < noteJson.length() && noteJson[valStart] == L't');
        }
        
        if (!note.id.empty()) {
            outNotes.push_back(note);
        }
        
        pos = objEnd;
    }
    
    return true;
}

std::wstring NoteStore::ToFullJson(const std::vector<Note>& notes) const {
    // Serialize with content (for export)
    std::wstringstream ss;
    ss << L"[\n";
    
    for (size_t i = 0; i < notes.size(); ++i) {
        const Note& note = notes[i];
        ss << L"  {\n";
        ss << L"    \"id\": \"" << JsonEscape(note.id) << L"\",\n";
        ss << L"    \"title\": \"" << JsonEscape(note.title) << L"\",\n";
        ss << L"    \"content\": \"" << JsonEscape(note.content) << L"\",\n";
        ss << L"    \"createdAt\": " << note.createdAt << L",\n";
        ss << L"    \"updatedAt\": " << note.updatedAt << L",\n";
        ss << L"    \"isPinned\": " << (note.isPinned ? L"true" : L"false") << L"\n";
        ss << L"  }";
        if (i < notes.size() - 1) {
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
                    } else {
                        // Truncated \u escape — preserve literally
                        result += L'\\';
                        result += L'u';
                        i++;
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

std::vector<NoteSummary> NoteStore::FilterAndSort(const NoteFilter& filter) const {
    std::vector<NoteSummary> result;
    result.reserve(m_notes.size());
    
    for (const auto& summary : m_notes) {
        // Apply filters
        if (filter.pinnedOnly && !summary.isPinned) {
            continue;
        }
        
        if (filter.fromDate != 0 && summary.createdAt < filter.fromDate) {
            continue;
        }
        
        if (filter.toDate != 0 && summary.createdAt > filter.toDate) {
            continue;
        }
        
        if (!filter.searchQuery.empty()) {
            std::wstring lowerQuery = filter.searchQuery;
            std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::towlower);
            
            // 1) Quick check: title match (no disk I/O)
            std::wstring lowerTitle = summary.title;
            std::transform(lowerTitle.begin(), lowerTitle.end(), lowerTitle.begin(), ::towlower);
            if (lowerTitle.find(lowerQuery) != std::wstring::npos) {
                // Title matched — include without loading full content
                result.push_back(summary);
                continue;
            }
            
            // 2) Check content preview (still no disk I/O)
            std::wstring lowerPreview = summary.contentPreview;
            std::transform(lowerPreview.begin(), lowerPreview.end(), lowerPreview.begin(), ::towlower);
            if (lowerPreview.find(lowerQuery) != std::wstring::npos) {
                // Preview matched — include without loading full content
                result.push_back(summary);
                continue;
            }
            
            // 3) Preview didn't match but note may be longer — load full content
            //    (skip if preview IS the full content, i.e. note is short)
            if (summary.contentPreview.length() >= PREVIEW_LENGTH) {
                std::wstring content = LoadNoteContent(summary.id);
                std::wstring lowerContent = content;
                std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(), ::towlower);
                if (lowerContent.find(lowerQuery) != std::wstring::npos) {
                    result.push_back(summary);
                    continue;
                }
            }
            
            // No match in title, preview, or full content
            continue;
        }
        
        result.push_back(summary);
    }
    
    // Apply sorting
    switch (filter.sortBy) {
        case NoteFilter::SortBy::UpdatedDesc:
            std::sort(result.begin(), result.end(),
                [](const NoteSummary& a, const NoteSummary& b) { return a.updatedAt > b.updatedAt; });
            break;
        case NoteFilter::SortBy::UpdatedAsc:
            std::sort(result.begin(), result.end(),
                [](const NoteSummary& a, const NoteSummary& b) { return a.updatedAt < b.updatedAt; });
            break;
        case NoteFilter::SortBy::CreatedDesc:
            std::sort(result.begin(), result.end(),
                [](const NoteSummary& a, const NoteSummary& b) { return a.createdAt > b.createdAt; });
            break;
        case NoteFilter::SortBy::CreatedAsc:
            std::sort(result.begin(), result.end(),
                [](const NoteSummary& a, const NoteSummary& b) { return a.createdAt < b.createdAt; });
            break;
        case NoteFilter::SortBy::TitleAsc:
            std::sort(result.begin(), result.end(),
                [](const NoteSummary& a, const NoteSummary& b) { 
                    return _wcsicmp(a.GetDisplayTitle().c_str(), b.GetDisplayTitle().c_str()) < 0; 
                });
            break;
        case NoteFilter::SortBy::TitleDesc:
            std::sort(result.begin(), result.end(),
                [](const NoteSummary& a, const NoteSummary& b) { 
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

//------------------------------------------------------------------------------
// Export all notes to a JSON file (loads content from individual files)
//------------------------------------------------------------------------------
bool NoteStore::ExportNotes(const std::wstring& filePath) {
    // Build full notes with content for export
    std::vector<Note> fullNotes;
    fullNotes.reserve(m_notes.size());
    for (const auto& summary : m_notes) {
        Note note;
        static_cast<NoteSummary&>(note) = summary;
        note.content = LoadNoteContent(summary.id);
        fullNotes.push_back(std::move(note));
    }
    
    std::wstring json = ToFullJson(fullNotes);
    
    // Convert to UTF-8
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, json.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 0) return false;
    
    std::vector<char> utf8Data(utf8Len);
    WideCharToMultiByte(CP_UTF8, 0, json.c_str(), -1, utf8Data.data(), utf8Len, nullptr, nullptr);
    
    HandleGuard hFile(CreateFileW(filePath.c_str(), GENERIC_WRITE, 0,
                               nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!hFile.valid()) return false;
    
    DWORD bytesWritten;
    BOOL result = WriteFile(hFile.get(), utf8Data.data(), static_cast<DWORD>(utf8Len - 1), &bytesWritten, nullptr);
    
    return result != FALSE;
}

//------------------------------------------------------------------------------
// Import notes from a JSON file (merges with existing)
//------------------------------------------------------------------------------
bool NoteStore::ImportNotes(const std::wstring& filePath) {
    // Read file
    HandleGuard hFile(CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!hFile.valid()) return false;
    
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile.get(), &fileSize)) {
        return false;
    }
    
    std::vector<char> buffer(static_cast<size_t>(fileSize.QuadPart) + 1, 0);
    DWORD bytesRead;
    if (!ReadFile(hFile.get(), buffer.data(), static_cast<DWORD>(fileSize.QuadPart), &bytesRead, nullptr)) {
        return false;
    }
    hFile = HandleGuard();
    
    // Convert UTF-8 to wstring
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, buffer.data(), bytesRead, nullptr, 0);
    std::wstring json(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, buffer.data(), bytesRead, &json[0], wideLen);
    
    // Parse imported notes (with content)
    std::vector<Note> importedNotes;
    if (!ParseLegacyJson(json, importedNotes)) {
        return false;
    }
    
    // Merge: add imported notes whose IDs don't already exist
    int addedCount = 0;
    for (const auto& imported : importedNotes) {
        bool exists = false;
        for (const auto& existing : m_notes) {
            if (existing.id == imported.id) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            // Save content to individual file
            (void)SaveNoteContent(imported.id, imported.content);
            
            // Generate title if empty
            std::wstring title = imported.title;
            if (title.empty() && !imported.content.empty()) {
                size_t newlinePos = imported.content.find_first_of(L"\r\n");
                std::wstring firstLine = (newlinePos != std::wstring::npos) 
                    ? imported.content.substr(0, newlinePos) 
                    : imported.content;
                size_t start = firstLine.find_first_not_of(L" \t");
                if (start != std::wstring::npos) {
                    size_t end = firstLine.find_last_not_of(L" \t");
                    firstLine = firstLine.substr(start, end - start + 1);
                    if (firstLine.length() > 50) {
                        firstLine = firstLine.substr(0, 47) + L"...";
                    }
                    title = firstLine;
                }
            }
            
            // Add summary to index
            NoteSummary summary;
            summary.id = imported.id;
            summary.title = title;
            summary.contentPreview = MakeContentPreview(imported.content);
            summary.createdAt = imported.createdAt;
            summary.updatedAt = imported.updatedAt;
            summary.isPinned = imported.isPinned;
            m_notes.push_back(summary);
            addedCount++;
        }
    }
    
    if (addedCount > 0) {
        m_dirty = true;
        (void)Save();
    }
    
    return true;
}

//------------------------------------------------------------------------------
// Per-note content file I/O
//------------------------------------------------------------------------------

std::wstring NoteStore::GetNoteContentPath(const std::wstring& id) const {
    return m_notesDir + L"\\" + id + L".txt";
}

//------------------------------------------------------------------------------
// LRU content cache helpers
//------------------------------------------------------------------------------

void NoteStore::CacheContent(const std::wstring& id, const std::wstring& content) const {
    auto it = m_contentCache.find(id);
    if (it != m_contentCache.end()) {
        // Move to front (most recently used)
        m_cacheOrder.erase(it->second.first);
        m_cacheOrder.push_front(id);
        it->second.first = m_cacheOrder.begin();
        it->second.second = content;
        return;
    }
    
    // Evict LRU entry if at capacity
    if (m_contentCache.size() >= MAX_CACHE_ENTRIES) {
        const std::wstring& evictId = m_cacheOrder.back();
        m_contentCache.erase(evictId);
        m_cacheOrder.pop_back();
    }
    
    m_cacheOrder.push_front(id);
    m_contentCache[id] = { m_cacheOrder.begin(), content };
}

void NoteStore::InvalidateCache(const std::wstring& id) const {
    auto it = m_contentCache.find(id);
    if (it != m_contentCache.end()) {
        m_cacheOrder.erase(it->second.first);
        m_contentCache.erase(it);
    }
}

std::wstring NoteStore::MakeContentPreview(const std::wstring& content, size_t maxLen) {
    if (content.length() <= maxLen) {
        return content;
    }
    // Cut at a word boundary if possible
    size_t cutPos = content.rfind(L' ', maxLen);
    if (cutPos == std::wstring::npos || cutPos < maxLen / 2) {
        cutPos = maxLen;
    }
    return content.substr(0, cutPos);
}

//------------------------------------------------------------------------------
// Content loading with LRU cache
//------------------------------------------------------------------------------

std::wstring NoteStore::LoadNoteContent(const std::wstring& id) const {
    // Check cache first
    auto cacheIt = m_contentCache.find(id);
    if (cacheIt != m_contentCache.end()) {
        // Move to front (most recently used)
        m_cacheOrder.erase(cacheIt->second.first);
        m_cacheOrder.push_front(id);
        cacheIt->second.first = m_cacheOrder.begin();
        return cacheIt->second.second;
    }
    
    // Cache miss — load from disk
    std::wstring content = LoadNoteContentFromDisk(id);
    
    // Cache the result
    CacheContent(id, content);
    
    return content;
}

std::wstring NoteStore::LoadNoteContentFromDisk(const std::wstring& id) const {
    std::wstring path = GetNoteContentPath(id);
    
    HandleGuard hFile(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!hFile.valid()) {
        return L"";
    }
    
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile.get(), &fileSize)) {
        return L"";
    }
    
    if (fileSize.QuadPart == 0) {
        return L"";
    }
    
    // Read as UTF-8
    std::vector<char> buffer(static_cast<size_t>(fileSize.QuadPart) + 1, 0);
    DWORD bytesRead;
    if (!ReadFile(hFile.get(), buffer.data(), static_cast<DWORD>(fileSize.QuadPart), &bytesRead, nullptr)) {
        return L"";
    }
    
    // Convert UTF-8 to wstring
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, buffer.data(), bytesRead, nullptr, 0);
    if (wideLen <= 0) return L"";
    
    std::wstring content(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, buffer.data(), bytesRead, &content[0], wideLen);
    
    return content;
}

bool NoteStore::SaveNoteContent(const std::wstring& id, const std::wstring& content) {
    std::wstring path = GetNoteContentPath(id);
    
    // Convert to UTF-8
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, content.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 0) return false;
    
    std::vector<char> utf8Data(utf8Len);
    WideCharToMultiByte(CP_UTF8, 0, content.c_str(), -1, utf8Data.data(), utf8Len, nullptr, nullptr);
    
    // Atomic write: write to temp file, then rename
    std::wstring tempPath = path + L".tmp";
    
    HandleGuard hFile(CreateFileW(tempPath.c_str(), GENERIC_WRITE, 0,
                               nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!hFile.valid()) {
        return false;
    }
    
    DWORD bytesWritten;
    BOOL result = WriteFile(hFile.get(), utf8Data.data(), static_cast<DWORD>(utf8Len - 1), &bytesWritten, nullptr);
    FlushFileBuffers(hFile.get());
    hFile = HandleGuard();  // Close before rename
    
    if (!result) {
        DeleteFileW(tempPath.c_str());
        return false;
    }
    
    if (!MoveFileExW(tempPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(tempPath.c_str());
        return false;
    }
    
    result = TRUE;  // Rename succeeded
    
    if (result) {
        // Update cache with new content
        CacheContent(id, content);
        
        // Update content preview in the index
        auto it = std::find_if(m_notes.begin(), m_notes.end(),
            [&id](const NoteSummary& n) { return n.id == id; });
        if (it != m_notes.end()) {
            it->contentPreview = MakeContentPreview(content);
        }
    }
    
    return result != FALSE;
}

void NoteStore::DeleteNoteContent(const std::wstring& id) {
    std::wstring path = GetNoteContentPath(id);
    DeleteFileW(path.c_str());
    InvalidateCache(id);
}

//------------------------------------------------------------------------------
// Migration from legacy single-file format
//------------------------------------------------------------------------------

bool NoteStore::MigrateFromLegacyFormat() {
    // Read legacy file
    HandleGuard hFile(CreateFileW(m_legacyStorePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!hFile.valid()) {
        return false;
    }
    
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile.get(), &fileSize)) {
        return false;
    }
    
    std::vector<char> buffer(static_cast<size_t>(fileSize.QuadPart) + 1, 0);
    DWORD bytesRead;
    if (!ReadFile(hFile.get(), buffer.data(), static_cast<DWORD>(fileSize.QuadPart), &bytesRead, nullptr)) {
        return false;
    }
    hFile = HandleGuard();
    
    // Convert UTF-8 to wstring
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, buffer.data(), bytesRead, nullptr, 0);
    std::wstring json(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, buffer.data(), bytesRead, &json[0], wideLen);
    
    // Parse legacy format (with embedded content)
    std::vector<Note> legacyNotes;
    if (!ParseLegacyJson(json, legacyNotes)) {
        return false;
    }
    
    // Ensure notes directory exists
    DWORD attrs = GetFileAttributesW(m_notesDir.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        if (!CreateDirectoryW(m_notesDir.c_str(), nullptr)) {
            return false;
        }
    }
    
    // Migrate each note: save content to individual file, keep summary in index
    m_notes.clear();
    for (const auto& note : legacyNotes) {
        // Save content to individual file
        (void)SaveNoteContent(note.id, note.content);
        
        // Generate title from content if not set
        std::wstring title = note.title;
        if (title.empty() && !note.content.empty()) {
            size_t newlinePos = note.content.find_first_of(L"\r\n");
            std::wstring firstLine = (newlinePos != std::wstring::npos) 
                ? note.content.substr(0, newlinePos) 
                : note.content;
            size_t start = firstLine.find_first_not_of(L" \t");
            if (start != std::wstring::npos) {
                size_t end = firstLine.find_last_not_of(L" \t");
                firstLine = firstLine.substr(start, end - start + 1);
                if (firstLine.length() > 50) {
                    firstLine = firstLine.substr(0, 47) + L"...";
                }
                title = firstLine;
            }
        }
        
        // Add summary to index
        NoteSummary summary;
        summary.id = note.id;
        summary.title = title;
        summary.contentPreview = MakeContentPreview(note.content);
        summary.createdAt = note.createdAt;
        summary.updatedAt = note.updatedAt;
        summary.isPinned = note.isPinned;
        m_notes.push_back(summary);
    }
    
    // Save the new index
    m_dirty = true;
    if (!Save()) {
        return false;
    }
    
    // Rename legacy file as backup (don't delete it)
    std::wstring backupPath = m_legacyStorePath + L".bak";
    MoveFileW(m_legacyStorePath.c_str(), backupPath.c_str());
    
    return true;
}

} // namespace QNote
