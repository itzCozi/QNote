//==============================================================================
// QNote - A Lightweight Notepad Clone
// DocumentManager.cpp - Multi-document management implementation
//==============================================================================

#include "DocumentManager.h"
#include <algorithm>

namespace QNote {

//------------------------------------------------------------------------------
// Initialize with editor and tab bar references
//------------------------------------------------------------------------------
void DocumentManager::Initialize(Editor* editor, TabBar* tabBar, SettingsManager* settings) {
    m_editor = editor;
    m_tabBar = tabBar;
    m_settings = settings;
}

//------------------------------------------------------------------------------
// Create a new untitled document
//------------------------------------------------------------------------------
int DocumentManager::NewDocument() {
    if (!m_tabBar || !m_editor) return -1;

    // Save current document state first
    if (m_activeTabId >= 0) {
        SaveCurrentState();
    }

    // Create the document state
    DocumentState doc;
    ApplyDefaults(doc);

    // Add a tab
    int tabId = m_tabBar->AddTab(doc.GetDisplayTitle());
    doc.tabId = tabId;

    m_documents.push_back(std::move(doc));

    // Switch to the new document
    m_activeTabId = tabId;
    m_tabBar->SetActiveTab(tabId);

    // Clear editor for new document
    m_editor->Clear();
    m_editor->SetModified(false);
    m_editor->SetEncoding(m_documents.back().encoding);
    m_editor->SetLineEnding(m_documents.back().lineEnding);
    m_editor->SetSelection(0, 0);
    m_editor->SetFocus();

    return tabId;
}

//------------------------------------------------------------------------------
// Open a file in a new tab
//------------------------------------------------------------------------------
int DocumentManager::OpenDocument(const std::wstring& filePath,
                                   const std::wstring& content,
                                   TextEncoding encoding,
                                   LineEnding lineEnding) {
    if (!m_tabBar || !m_editor) return -1;

    // Check if file is already open
    int existingTab = FindDocumentByPath(filePath);
    if (existingTab >= 0) {
        SwitchToDocument(existingTab);
        return existingTab;
    }

    // Save current document state first
    if (m_activeTabId >= 0) {
        SaveCurrentState();
    }

    // Create document
    DocumentState doc;
    doc.filePath = filePath;
    doc.text = content;
    doc.isNewFile = false;
    doc.isModified = false;
    doc.encoding = encoding;
    doc.lineEnding = lineEnding;

    // Extract filename for tab title
    std::wstring title = doc.GetDisplayTitle();

    int tabId = m_tabBar->AddTab(title, filePath);
    doc.tabId = tabId;

    m_documents.push_back(std::move(doc));

    // Switch to the new document
    m_activeTabId = tabId;
    m_tabBar->SetActiveTab(tabId);

    // Load into editor
    m_editor->SetText(content);
    m_editor->SetEncoding(encoding);
    m_editor->SetLineEnding(lineEnding);
    m_editor->SetModified(false);
    m_editor->SetSelection(0, 0);
    m_editor->SetFocus();

    return tabId;
}

//------------------------------------------------------------------------------
// Close a document/tab
//------------------------------------------------------------------------------
void DocumentManager::CloseDocument(int tabId) {
    int idx = FindDocumentIndex(tabId);
    if (idx < 0) return;

    bool wasActive = (tabId == m_activeTabId);

    // Remove document
    m_documents.erase(m_documents.begin() + idx);

    // Remove from tab bar
    m_tabBar->RemoveTab(tabId);

    if (wasActive) {
        // The TabBar::RemoveTab will select a new tab and fire TabSelected
        m_activeTabId = m_tabBar->GetActiveTabId();
        if (m_activeTabId >= 0) {
            RestoreState(m_activeTabId);
        }
    }
}

//------------------------------------------------------------------------------
// Switch to a different document
//------------------------------------------------------------------------------
bool DocumentManager::SwitchToDocument(int tabId) {
    if (tabId == m_activeTabId) return true;

    int idx = FindDocumentIndex(tabId);
    if (idx < 0) return false;

    // Save current state
    if (m_activeTabId >= 0) {
        SaveCurrentState();
    }

    // Switch
    m_activeTabId = tabId;
    m_tabBar->SetActiveTab(tabId);

    // Restore new document's state
    RestoreState(tabId);

    return true;
}

//------------------------------------------------------------------------------
// Save current editor state to the active document
//------------------------------------------------------------------------------
void DocumentManager::SaveCurrentState() {
    if (!m_editor) return;

    DocumentState* doc = GetActiveDocument();
    if (!doc) return;

    doc->text = m_editor->GetText();
    doc->isModified = m_editor->IsModified();
    doc->encoding = m_editor->GetEncoding();
    doc->lineEnding = m_editor->GetLineEnding();

    // Save cursor position
    DWORD start = 0, end = 0;
    m_editor->GetSelection(start, end);
    doc->cursorStart = start;
    doc->cursorEnd = end;
    doc->firstVisibleLine = m_editor->GetFirstVisibleLine();

    // Save bookmarks
    doc->bookmarks = m_editor->GetBookmarks();

    // Update tab bar state
    if (m_tabBar) {
        m_tabBar->SetTabModified(doc->tabId, doc->isModified);
    }
}

//------------------------------------------------------------------------------
// Restore a document's state into the editor
//------------------------------------------------------------------------------
void DocumentManager::RestoreState(int tabId) {
    if (!m_editor) return;

    DocumentState* doc = GetDocument(tabId);
    if (!doc) return;

    // Restore content
    m_editor->SetText(doc->text);
    m_editor->SetEncoding(doc->encoding);
    m_editor->SetLineEnding(doc->lineEnding);
    m_editor->SetModified(doc->isModified);

    // Restore cursor position
    m_editor->SetSelection(doc->cursorStart, doc->cursorEnd);

    // Restore scroll position
    if (doc->firstVisibleLine > 0) {
        m_editor->GoToLine(doc->firstVisibleLine);
        // Re-set the cursor to its saved position (GoToLine changes it)
        m_editor->SetSelection(doc->cursorStart, doc->cursorEnd);
    }

    // Restore bookmarks
    m_editor->SetBookmarks(doc->bookmarks);

    m_editor->SetFocus();
}

//------------------------------------------------------------------------------
// Query methods
//------------------------------------------------------------------------------
DocumentState* DocumentManager::GetDocument(int tabId) {
    int idx = FindDocumentIndex(tabId);
    if (idx >= 0) return &m_documents[idx];
    return nullptr;
}

const DocumentState* DocumentManager::GetDocument(int tabId) const {
    int idx = FindDocumentIndex(tabId);
    if (idx >= 0) return &m_documents[idx];
    return nullptr;
}

DocumentState* DocumentManager::GetActiveDocument() {
    return GetDocument(m_activeTabId);
}

const DocumentState* DocumentManager::GetActiveDocument() const {
    return GetDocument(m_activeTabId);
}

std::vector<int> DocumentManager::GetAllTabIds() const {
    std::vector<int> ids;
    ids.reserve(m_documents.size());
    for (const auto& doc : m_documents) {
        ids.push_back(doc.tabId);
    }
    return ids;
}

//------------------------------------------------------------------------------
// Find document by file path
//------------------------------------------------------------------------------
int DocumentManager::FindDocumentByPath(const std::wstring& filePath) const {
    if (filePath.empty()) return -1;

    // Case-insensitive comparison for Windows paths
    for (const auto& doc : m_documents) {
        if (!doc.filePath.empty() && _wcsicmp(doc.filePath.c_str(), filePath.c_str()) == 0) {
            return doc.tabId;
        }
    }
    return -1;
}

//------------------------------------------------------------------------------
// Update document properties
//------------------------------------------------------------------------------
void DocumentManager::SetDocumentModified(int tabId, bool modified) {
    DocumentState* doc = GetDocument(tabId);
    if (doc) {
        doc->isModified = modified;
        if (m_tabBar) {
            m_tabBar->SetTabModified(tabId, modified);
        }
    }
}

void DocumentManager::SetDocumentFilePath(int tabId, const std::wstring& filePath) {
    DocumentState* doc = GetDocument(tabId);
    if (doc) {
        doc->filePath = filePath;
        doc->isNewFile = filePath.empty();
        if (m_tabBar) {
            m_tabBar->SetTabFilePath(tabId, filePath);
            // Update title if no custom title is set
            if (doc->customTitle.empty()) {
                m_tabBar->SetTabTitle(tabId, doc->GetDisplayTitle());
            }
        }
    }
}

void DocumentManager::SetDocumentTitle(int tabId, const std::wstring& title) {
    DocumentState* doc = GetDocument(tabId);
    if (doc) {
        doc->customTitle = title;
        if (m_tabBar) {
            m_tabBar->SetTabTitle(tabId, title);
        }
    }
}

void DocumentManager::SetDocumentPinned(int tabId, bool pinned) {
    DocumentState* doc = GetDocument(tabId);
    if (doc) {
        doc->isPinned = pinned;
        if (m_tabBar) {
            m_tabBar->SetTabPinned(tabId, pinned);
        }
    }
}

void DocumentManager::SetDocumentNoteMode(int tabId, bool isNoteMode, const std::wstring& noteId) {
    DocumentState* doc = GetDocument(tabId);
    if (doc) {
        doc->isNoteMode = isNoteMode;
        doc->noteId = noteId;
    }
}

//------------------------------------------------------------------------------
// Sync modified state from editor to active document
//------------------------------------------------------------------------------
void DocumentManager::SyncModifiedState() {
    if (!m_editor) return;

    DocumentState* doc = GetActiveDocument();
    if (!doc) return;

    bool modified = m_editor->IsModified();
    if (doc->isModified != modified) {
        doc->isModified = modified;
        if (m_tabBar) {
            m_tabBar->SetTabModified(doc->tabId, modified);
        }
    }
}

//------------------------------------------------------------------------------
// Reset the active document to a fresh untitled state
//------------------------------------------------------------------------------
void DocumentManager::ResetActiveDocument() {
    if (!m_editor || !m_tabBar) return;

    DocumentState* doc = GetActiveDocument();
    if (!doc) return;

    int tabId = doc->tabId;

    // Reset all document state
    doc->text.clear();
    doc->filePath.clear();
    doc->customTitle.clear();
    doc->isNewFile = true;
    doc->isModified = false;
    doc->isPinned = false;
    doc->encoding = TextEncoding::UTF8;
    doc->lineEnding = LineEnding::CRLF;
    doc->cursorStart = 0;
    doc->cursorEnd = 0;
    doc->firstVisibleLine = 0;
    doc->isNoteMode = false;
    doc->noteId.clear();
    doc->bookmarks.clear();
    ApplyDefaults(*doc);

    // Update tab bar
    m_tabBar->SetTabTitle(tabId, doc->GetDisplayTitle());
    m_tabBar->SetTabFilePath(tabId, L"");
    m_tabBar->SetTabModified(tabId, false);
    m_tabBar->SetTabPinned(tabId, false);

    // Clear editor
    m_editor->Clear();
    m_editor->SetEncoding(doc->encoding);
    m_editor->SetLineEnding(doc->lineEnding);
    m_editor->SetSelection(0, 0);
    m_editor->SetBookmarks({});
    m_editor->SetFocus();
}

//------------------------------------------------------------------------------
// Close all documents except the specified one
//------------------------------------------------------------------------------
void DocumentManager::CloseOtherDocuments(int keepTabId) {
    std::vector<int> toClose;
    for (const auto& doc : m_documents) {
        if (doc.tabId != keepTabId) {
            toClose.push_back(doc.tabId);
        }
    }
    for (int id : toClose) {
        CloseDocument(id);
    }
}

//------------------------------------------------------------------------------
// Close all documents to the right of the specified tab
//------------------------------------------------------------------------------
void DocumentManager::CloseDocumentsToRight(int tabId) {
    int idx = FindDocumentIndex(tabId);
    if (idx < 0) return;

    std::vector<int> toClose;
    for (int i = idx + 1; i < static_cast<int>(m_documents.size()); i++) {
        toClose.push_back(m_documents[i].tabId);
    }
    for (int id : toClose) {
        CloseDocument(id);
    }
}

//------------------------------------------------------------------------------
// Close all documents
//------------------------------------------------------------------------------
void DocumentManager::CloseAllDocuments() {
    std::vector<int> toClose;
    for (const auto& doc : m_documents) {
        toClose.push_back(doc.tabId);
    }
    for (int id : toClose) {
        CloseDocument(id);
    }
}

//------------------------------------------------------------------------------
// Check if any documents have unsaved changes
//------------------------------------------------------------------------------
bool DocumentManager::HasUnsavedDocuments() const {
    for (const auto& doc : m_documents) {
        if (doc.isModified) return true;
    }
    return false;
}

//------------------------------------------------------------------------------
// Apply default settings to a new document
//------------------------------------------------------------------------------
void DocumentManager::ApplyDefaults(DocumentState& doc) {
    if (m_settings) {
        const AppSettings& settings = m_settings->GetSettings();
        doc.encoding = settings.defaultEncoding;
        doc.lineEnding = settings.defaultLineEnding;
    }
}

//------------------------------------------------------------------------------
// Find document index by tab id
//------------------------------------------------------------------------------
int DocumentManager::FindDocumentIndex(int tabId) const {
    for (int i = 0; i < static_cast<int>(m_documents.size()); i++) {
        if (m_documents[i].tabId == tabId) return i;
    }
    return -1;
}

} // namespace QNote
