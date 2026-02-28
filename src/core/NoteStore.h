//==============================================================================
// QNote - A Lightweight Notepad Clone
// NoteStore.h - Note storage and management for quick capture
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
#include <ctime>
#include <optional>
#include <list>
#include <unordered_map>

namespace QNote {

//------------------------------------------------------------------------------
// NoteSummary - lightweight metadata kept in RAM (no content)
//------------------------------------------------------------------------------
struct NoteSummary {
    std::wstring id;           // Unique identifier (timestamp-based)
    std::wstring title;        // Auto-generated from first line or explicit
    std::wstring contentPreview; // First ~200 chars for search/display (avoids full content load)
    time_t createdAt = 0;      // Creation timestamp
    time_t updatedAt = 0;      // Last modification timestamp
    bool isPinned = false;     // Whether the note is starred/pinned

    // Generate a display title
    [[nodiscard]] std::wstring GetDisplayTitle() const;

    // Get formatted date strings
    [[nodiscard]] std::wstring GetCreatedDateString() const;
    [[nodiscard]] std::wstring GetUpdatedDateString() const;
};

//------------------------------------------------------------------------------
// Note structure - full note with content (loaded on demand)
//------------------------------------------------------------------------------
struct Note : NoteSummary {
    std::wstring content;      // Note text content (loaded lazily from file)
};

//------------------------------------------------------------------------------
// Search result structure
//------------------------------------------------------------------------------
struct NoteSearchResult {
    NoteSummary summary;
    size_t matchStart = 0;     // Start position of match in content
    size_t matchLength = 0;    // Length of match
    std::wstring contextSnippet; // Text snippet around the match
};

//------------------------------------------------------------------------------
// Note filter options for querying
//------------------------------------------------------------------------------
struct NoteFilter {
    bool pinnedOnly = false;
    std::wstring searchQuery;
    time_t fromDate = 0;       // 0 means no filter
    time_t toDate = 0;         // 0 means no filter
    
    // Sort options
    enum class SortBy {
        UpdatedDesc,           // Most recently updated first
        UpdatedAsc,
        CreatedDesc,
        CreatedAsc,
        TitleAsc,
        TitleDesc
    };
    SortBy sortBy = SortBy::UpdatedDesc;
};

//------------------------------------------------------------------------------
// Note store class - manages all notes with autosave
//------------------------------------------------------------------------------
class NoteStore {
public:
    NoteStore();
    ~NoteStore();
    
    // Prevent copying
    NoteStore(const NoteStore&) = delete;
    NoteStore& operator=(const NoteStore&) = delete;
    
    // Initialize the store (load from disk)
    [[nodiscard]] bool Initialize();
    
    // Create a new note and return it
    [[nodiscard]] Note CreateNote(const std::wstring& content = L"");
    
    // Get a note by ID (loads content from disk on demand)
    [[nodiscard]] std::optional<Note> GetNote(const std::wstring& id) const;
    
    // Get a note summary by ID (metadata only, no disk I/O)
    [[nodiscard]] std::optional<NoteSummary> GetNoteSummary(const std::wstring& id) const;
    
    // Update an existing note (autosaves)
    [[nodiscard]] bool UpdateNote(const Note& note);
    
    // Delete a note
    [[nodiscard]] bool DeleteNote(const std::wstring& id);
    
    // Toggle pin status
    [[nodiscard]] bool TogglePin(const std::wstring& id);
    
    // Get note summaries with optional filtering (no content loaded)
    [[nodiscard]] std::vector<NoteSummary> GetNotes(const NoteFilter& filter = NoteFilter()) const;
    
    // Get pinned note summaries
    [[nodiscard]] std::vector<NoteSummary> GetPinnedNotes() const;
    
    // Search notes (loads content on demand during search)
    [[nodiscard]] std::vector<NoteSearchResult> SearchNotes(const std::wstring& query, 
                                                            bool matchCase = false) const;
    
    // Get notes for a specific date (timeline view)
    [[nodiscard]] std::vector<NoteSummary> GetNotesForDate(int year, int month, int day) const;
    
    // Get unique dates that have notes (for timeline)
    [[nodiscard]] std::vector<time_t> GetNoteDates() const;
    
    // Get the current "active" note (the one being edited in main window)
    [[nodiscard]] std::optional<Note> GetActiveNote() const;
    
    // Set the active note
    void SetActiveNote(const std::wstring& id);
    
    // Get note count
    [[nodiscard]] size_t GetNoteCount() const { return m_notes.size(); }
    
    // Force save (normally auto-saved)
    [[nodiscard]] bool Save();
    
    // Export all notes to a file
    [[nodiscard]] bool ExportNotes(const std::wstring& filePath);
    
    // Import notes from a file (merges with existing)
    [[nodiscard]] bool ImportNotes(const std::wstring& filePath);
    
    // Get the store directory path
    [[nodiscard]] const std::wstring& GetStorePath() const { return m_storePath; }
    
private:
    // Load note index from JSON file (metadata only)
    [[nodiscard]] bool LoadFromFile();
    
    // Save note index to JSON file (metadata only)
    [[nodiscard]] bool SaveToFile();
    
    // Load note content from individual file (uses cache)
    [[nodiscard]] std::wstring LoadNoteContent(const std::wstring& id) const;
    
    // Load note content bypassing cache (direct disk read)
    [[nodiscard]] std::wstring LoadNoteContentFromDisk(const std::wstring& id) const;
    
    // Save note content to individual file
    [[nodiscard]] bool SaveNoteContent(const std::wstring& id, const std::wstring& content);
    
    // Delete note content file
    void DeleteNoteContent(const std::wstring& id);
    
    // Get the file path for a note's content
    [[nodiscard]] std::wstring GetNoteContentPath(const std::wstring& id) const;
    
    // Migrate from legacy single-file format (notes.json with embedded content)
    [[nodiscard]] bool MigrateFromLegacyFormat();
    
    // Generate a unique ID for a new note
    [[nodiscard]] std::wstring GenerateNoteId() const;
    
    // Parse JSON index (metadata only)
    [[nodiscard]] bool ParseJson(const std::wstring& json);
    
    // Serialize note index to JSON (metadata only)
    [[nodiscard]] std::wstring ToJson() const;
    
    // Parse legacy JSON format (with embedded content) for migration
    [[nodiscard]] bool ParseLegacyJson(const std::wstring& json,
                                        std::vector<Note>& outNotes) const;
    
    // Serialize notes with content to JSON (for export)
    [[nodiscard]] std::wstring ToFullJson(const std::vector<Note>& notes) const;
    
    // Escape string for JSON
    [[nodiscard]] static std::wstring JsonEscape(const std::wstring& str);
    
    // Unescape JSON string
    [[nodiscard]] static std::wstring JsonUnescape(const std::wstring& str);
    
    // Apply filter and sort to note summaries
    [[nodiscard]] std::vector<NoteSummary> FilterAndSort(const NoteFilter& filter) const;
    
    // Case-insensitive string search
    [[nodiscard]] static size_t FindCaseInsensitive(const std::wstring& haystack, 
                                                    const std::wstring& needle,
                                                    size_t startPos = 0);
    
    // Generate a content preview from full content
    [[nodiscard]] static std::wstring MakeContentPreview(const std::wstring& content,
                                                         size_t maxLen = PREVIEW_LENGTH);
    
    // Update cached content and invalidate stale entry
    void CacheContent(const std::wstring& id, const std::wstring& content) const;
    void InvalidateCache(const std::wstring& id) const;
    
private:
    std::vector<NoteSummary> m_notes;      // Only metadata in RAM
    std::wstring m_storeDir;               // Base directory (AppData/QNote)
    std::wstring m_notesDir;               // Per-note content directory (AppData/QNote/notes)
    std::wstring m_storePath;              // Index file path (notes_index.json)
    std::wstring m_legacyStorePath;        // Legacy file path (notes.json)
    std::wstring m_activeNoteId;
    bool m_dirty = false;
    
    // LRU content cache: avoids redundant disk reads for recently-accessed notes
    // Key = note id, Value = full content string
    // m_cacheOrder front = most recently used, back = least recently used
    mutable std::list<std::wstring> m_cacheOrder;  // LRU order (note ids)
    mutable std::unordered_map<std::wstring, 
        std::pair<std::list<std::wstring>::iterator, std::wstring>> m_contentCache;
    
    // Auto-save timer related
    static constexpr DWORD AUTOSAVE_INTERVAL_MS = 3000;  // 3 seconds
    static constexpr size_t MAX_CACHE_ENTRIES = 32;       // Max cached note contents
    static constexpr size_t PREVIEW_LENGTH = 200;         // Chars stored in contentPreview
};

//------------------------------------------------------------------------------
// Utility functions
//------------------------------------------------------------------------------

// Format timestamp as readable date
std::wstring FormatTimestamp(time_t timestamp, bool includeTime = true);

// Get midnight timestamp for a given date
time_t GetMidnight(time_t timestamp);

// Parse a date from "YYYY-MM-DD" format
time_t ParseDate(const std::wstring& dateStr);

} // namespace QNote
