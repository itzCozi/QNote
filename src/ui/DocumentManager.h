//==============================================================================
// QNote - A Lightweight Notepad Clone
// DocumentManager.h - Multi-document management for tabbed editing
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
#include <unordered_map>
#include <memory>
#include <optional>
#include <set>

#include "Settings.h"
#include "Editor.h"
#include "TabBar.h"

namespace QNote {

//------------------------------------------------------------------------------
// Document state - everything needed to save/restore a document in the editor
//------------------------------------------------------------------------------
struct DocumentState {
    int tabId = -1;                        // Associated TabBar tab id

    // Content
    std::wstring text;                     // Document text content

    // File info
    std::wstring filePath;                 // File path (empty for untitled)
    std::wstring customTitle;              // User-set custom title (from rename)
    bool isNewFile = true;                 // Whether the file has never been saved
    bool isModified = false;               // Has unsaved changes
    bool isPinned = false;                 // Tab is pinned

    // Encoding/format
    TextEncoding encoding = TextEncoding::UTF8;
    LineEnding lineEnding = LineEnding::CRLF;

    // Cursor/scroll state for restoration
    DWORD cursorStart = 0;
    DWORD cursorEnd = 0;
    int firstVisibleLine = 0;

    // Note mode state
    bool isNoteMode = false;
    std::wstring noteId;
    
    // Bookmarks
    std::set<int> bookmarks;

    // Get the display title for this document
    [[nodiscard]] std::wstring GetDisplayTitle() const {
        if (!customTitle.empty()) {
            return customTitle;
        }
        if (!filePath.empty()) {
            // Extract filename from path
            size_t pos = filePath.find_last_of(L"\\/");
            if (pos != std::wstring::npos) {
                return filePath.substr(pos + 1);
            }
            return filePath;
        }
        return L"Untitled";
    }
};

//------------------------------------------------------------------------------
// DocumentManager - manages multiple documents and tab bar synchronization
//------------------------------------------------------------------------------
class DocumentManager {
public:
    DocumentManager() noexcept = default;
    ~DocumentManager() = default;

    // Prevent copying
    DocumentManager(const DocumentManager&) = delete;
    DocumentManager& operator=(const DocumentManager&) = delete;

    // Initialize with editor and tab bar references
    void Initialize(Editor* editor, TabBar* tabBar, SettingsManager* settings);

    // Document operations
    int NewDocument();                                     // Create a new untitled document, returns tab id
    int OpenDocument(const std::wstring& filePath,         // Open file in new tab
                     const std::wstring& content,
                     TextEncoding encoding,
                     LineEnding lineEnding);
    void CloseDocument(int tabId);                         // Close a document/tab
    bool SwitchToDocument(int tabId);                      // Switch active document
    
    // Save/restore editor state for current document
    void SaveCurrentState();
    void RestoreState(int tabId);

    // Query
    [[nodiscard]] DocumentState* GetDocument(int tabId);
    [[nodiscard]] const DocumentState* GetDocument(int tabId) const;
    [[nodiscard]] DocumentState* GetActiveDocument();
    [[nodiscard]] const DocumentState* GetActiveDocument() const;
    [[nodiscard]] int GetActiveTabId() const noexcept { return m_activeTabId; }
    [[nodiscard]] int GetDocumentCount() const noexcept { return static_cast<int>(m_documents.size()); }
    [[nodiscard]] std::vector<int> GetAllTabIds() const;

    // Check if a file is already open; returns tab id or -1
    [[nodiscard]] int FindDocumentByPath(const std::wstring& filePath) const;

    // Update document properties
    void SetDocumentModified(int tabId, bool modified);
    void SetDocumentFilePath(int tabId, const std::wstring& filePath);
    void SetDocumentTitle(int tabId, const std::wstring& title);
    void SetDocumentPinned(int tabId, bool pinned);
    void SetDocumentNoteMode(int tabId, bool isNoteMode, const std::wstring& noteId = L"");

    // Update the active document's modified state from the editor
    void SyncModifiedState();

    // Reset the active document to a fresh untitled state
    void ResetActiveDocument();

    // Close helpers for context menu actions
    void CloseOtherDocuments(int keepTabId);
    void CloseDocumentsToRight(int tabId);
    void CloseAllDocuments();

    // Check if any documents have unsaved changes
    [[nodiscard]] bool HasUnsavedDocuments() const;

private:
    // Apply default settings to a new document
    void ApplyDefaults(DocumentState& doc);

    // Find document by tab id (internal)
    int FindDocumentIndex(int tabId) const;

private:
    Editor* m_editor = nullptr;
    TabBar* m_tabBar = nullptr;
    SettingsManager* m_settings = nullptr;

    std::vector<DocumentState> m_documents;
    int m_activeTabId = -1;
};

} // namespace QNote
