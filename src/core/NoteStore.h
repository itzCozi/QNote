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

namespace QNote {

//------------------------------------------------------------------------------
// Note structure - represents a single note
//------------------------------------------------------------------------------
struct Note {
    std::wstring id;           // Unique identifier (timestamp-based)
    std::wstring content;      // Note text content
    std::wstring title;        // Auto-generated from first line or explicit
    time_t createdAt = 0;      // Creation timestamp
    time_t updatedAt = 0;      // Last modification timestamp
    bool isPinned = false;     // Whether the note is starred/pinned
    
    // Generate a display title from content
    [[nodiscard]] std::wstring GetDisplayTitle() const;
    
    // Get formatted date strings
    [[nodiscard]] std::wstring GetCreatedDateString() const;
    [[nodiscard]] std::wstring GetUpdatedDateString() const;
};

//------------------------------------------------------------------------------
// Search result structure
//------------------------------------------------------------------------------
struct NoteSearchResult {
    Note note;
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
    
    // Get a note by ID
    [[nodiscard]] std::optional<Note> GetNote(const std::wstring& id) const;
    
    // Update an existing note (autosaves)
    [[nodiscard]] bool UpdateNote(const Note& note);
    
    // Delete a note
    [[nodiscard]] bool DeleteNote(const std::wstring& id);
    
    // Toggle pin status
    [[nodiscard]] bool TogglePin(const std::wstring& id);
    
    // Get all notes with optional filtering
    [[nodiscard]] std::vector<Note> GetNotes(const NoteFilter& filter = NoteFilter()) const;
    
    // Get pinned notes
    [[nodiscard]] std::vector<Note> GetPinnedNotes() const;
    
    // Search notes
    [[nodiscard]] std::vector<NoteSearchResult> SearchNotes(const std::wstring& query, 
                                                            bool matchCase = false) const;
    
    // Get notes for a specific date (timeline view)
    [[nodiscard]] std::vector<Note> GetNotesForDate(int year, int month, int day) const;
    
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
    
    // Get the store directory path
    [[nodiscard]] const std::wstring& GetStorePath() const { return m_storePath; }
    
private:
    // Load notes from JSON file
    [[nodiscard]] bool LoadFromFile();
    
    // Save notes to JSON file
    [[nodiscard]] bool SaveToFile();
    
    // Generate a unique ID for a new note
    [[nodiscard]] std::wstring GenerateNoteId() const;
    
    // Parse JSON content
    [[nodiscard]] bool ParseJson(const std::wstring& json);
    
    // Serialize notes to JSON
    [[nodiscard]] std::wstring ToJson() const;
    
    // Escape string for JSON
    [[nodiscard]] static std::wstring JsonEscape(const std::wstring& str);
    
    // Unescape JSON string
    [[nodiscard]] static std::wstring JsonUnescape(const std::wstring& str);
    
    // Apply filter and sort to notes
    [[nodiscard]] std::vector<Note> FilterAndSort(const NoteFilter& filter) const;
    
    // Case-insensitive string search
    [[nodiscard]] static size_t FindCaseInsensitive(const std::wstring& haystack, 
                                                    const std::wstring& needle,
                                                    size_t startPos = 0);
    
private:
    std::vector<Note> m_notes;
    std::wstring m_storeDir;
    std::wstring m_storePath;
    std::wstring m_activeNoteId;
    bool m_dirty = false;
    
    // Auto-save timer related
    static constexpr DWORD AUTOSAVE_INTERVAL_MS = 3000;  // 3 seconds
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
