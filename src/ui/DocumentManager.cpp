//==============================================================================
// QNote - A Lightweight Notepad Clone
// DocumentManager.cpp - Multi-document management implementation
//==============================================================================

#include "DocumentManager.h"
#include "Editor.h"
#include "TabBar.h"
#include <algorithm>
#include <functional>

namespace {

// Normalize line endings to \r to match RichEdit's internal representation.
// RichEdit converts all line endings (\r\n, \n) to \r when text is loaded
// via SetWindowTextW.  Any text compared against editor->GetText() must use
// the same normalization, otherwise the modified-state check produces false
// positives.
std::wstring NormalizeForRichEdit(const std::wstring& text) {
    std::wstring result;
    result.reserve(text.size());
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == L'\r') {
            result += L'\r';
            // Skip \n in a \r\n pair
            if (i + 1 < text.size() && text[i + 1] == L'\n') {
                i++;
            }
        } else if (text[i] == L'\n') {
            result += L'\r';
        } else {
            result += text[i];
        }
    }
    return result;
}

} // anonymous namespace

namespace QNote {

//------------------------------------------------------------------------------
// Initialize with parent window info and tab bar reference
//------------------------------------------------------------------------------
void DocumentManager::Initialize(HWND parentHwnd, HINSTANCE hInstance, TabBar* tabBar, SettingsManager* settings) {
    m_parentHwnd = parentHwnd;
    m_hInstance = hInstance;
    m_tabBar = tabBar;
    m_settings = settings;
}

//------------------------------------------------------------------------------
// Create an Editor instance for a document
//------------------------------------------------------------------------------
std::unique_ptr<Editor> DocumentManager::CreateEditorForDocument() {
    auto editor = std::make_unique<Editor>();
    if (m_settings) {
        const AppSettings& settings = m_settings->GetSettings();
        if (!editor->Create(m_parentHwnd, m_hInstance, settings)) {
            return nullptr;
        }
        // Apply current global settings
        editor->SetShowWhitespace(settings.showWhitespace);
        editor->SetSpellCheck(settings.spellCheckEnabled);
    } else {
        AppSettings defaults;
        if (!editor->Create(m_parentHwnd, m_hInstance, defaults)) {
            return nullptr;
        }
    }
    // Start hidden; the caller will show the active editor
    ShowWindow(editor->GetHandle(), SW_HIDE);
    return editor;
}

//------------------------------------------------------------------------------
// Create a new untitled document
//------------------------------------------------------------------------------
int DocumentManager::NewDocument() {
    if (!m_tabBar) return -1;

    // Save current document state first
    if (m_activeTabId >= 0) {
        SaveCurrentState();
    }

    // Hide the current active editor
    if (auto* oldDoc = GetActiveDocument()) {
        if (oldDoc->editor) {
            ShowWindow(oldDoc->editor->GetHandle(), SW_HIDE);
        }
    }

    // Create the document state
    DocumentState doc;
    ApplyDefaults(doc);

    // Create a per-tab editor
    doc.editor = CreateEditorForDocument();
    if (!doc.editor) return -1;

    // Add a tab
    int tabId = m_tabBar->AddTab(doc.GetDisplayTitle());
    doc.tabId = tabId;

    m_documents.push_back(std::move(doc));

    // Switch to the new document
    m_activeTabId = tabId;
    m_tabBar->SetActiveTab(tabId);

    auto& newDoc = m_documents.back();
    Editor* editor = newDoc.editor.get();

    // Clear editor for new document
    editor->Clear();
    editor->SetModified(false);

    // Record clean hash for undo-to-clean detection
    newDoc.cleanTextHash = std::hash<std::wstring>{}(editor->GetText());
    editor->SetEncoding(newDoc.encoding);
    editor->SetLineEnding(newDoc.lineEnding);
    editor->SetSelection(0, 0);

    // Show and focus the new editor
    ShowWindow(editor->GetHandle(), SW_SHOW);
    editor->SetFocus();

    return tabId;
}

//------------------------------------------------------------------------------
// Open a file in a new tab
//------------------------------------------------------------------------------
int DocumentManager::OpenDocument(const std::wstring& filePath,
                                   const std::wstring& content,
                                   TextEncoding encoding,
                                   LineEnding lineEnding) {
    if (!m_tabBar) return -1;

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

    // Hide the current active editor
    if (auto* oldDoc = GetActiveDocument()) {
        if (oldDoc->editor) {
            ShowWindow(oldDoc->editor->GetHandle(), SW_HIDE);
        }
    }

    // Create document
    DocumentState doc;
    doc.filePath = filePath;
    doc.isNewFile = false;
    doc.isModified = false;
    doc.encoding = encoding;
    doc.lineEnding = lineEnding;

    // Create a per-tab editor
    doc.editor = CreateEditorForDocument();
    if (!doc.editor) return -1;

    // Extract filename for tab title
    std::wstring title = doc.GetDisplayTitle();

    int tabId = m_tabBar->AddTab(title, filePath);
    doc.tabId = tabId;

    m_documents.push_back(std::move(doc));

    // Switch to the new document
    m_activeTabId = tabId;
    m_tabBar->SetActiveTab(tabId);

    auto& newDoc = m_documents.back();
    Editor* editor = newDoc.editor.get();

    // Load into editor
    editor->SetText(content);
    editor->SetEncoding(encoding);
    editor->SetLineEnding(lineEnding);
    editor->SetModified(false);
    editor->SetSelection(0, 0);

    // Show and focus the new editor
    ShowWindow(editor->GetHandle(), SW_SHOW);
    editor->SetFocus();

    // Record clean hash for undo-to-clean detection
    newDoc.cleanTextHash = std::hash<std::wstring>{}(editor->GetText());

    return tabId;
}

//------------------------------------------------------------------------------
// Close a document/tab
//------------------------------------------------------------------------------
void DocumentManager::CloseDocument(int tabId) {
    int idx = FindDocumentIndex(tabId);
    if (idx < 0) return;

    bool wasActive = (tabId == m_activeTabId);

    // The document's unique_ptr<Editor> will be destroyed when erased
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

    // Save current state and hide current editor
    if (m_activeTabId >= 0) {
        SaveCurrentState();
        if (auto* oldDoc = GetActiveDocument()) {
            if (oldDoc->editor) {
                ShowWindow(oldDoc->editor->GetHandle(), SW_HIDE);
            }
        }
    }

    // Switch
    m_activeTabId = tabId;
    m_tabBar->SetActiveTab(tabId);

    // Show and restore the new document's editor
    RestoreState(tabId);

    return true;
}

//------------------------------------------------------------------------------
// Save current editor state to the active document
//------------------------------------------------------------------------------
void DocumentManager::SaveCurrentState() {
    DocumentState* doc = GetActiveDocument();
    if (!doc || !doc->editor) return;

    Editor* editor = doc->editor.get();

    const std::wstring currentText = editor->GetText();
    doc->isModified = (std::hash<std::wstring>{}(currentText) != doc->cleanTextHash);
    doc->encoding = editor->GetEncoding();
    doc->lineEnding = editor->GetLineEnding();

    // Save cursor position
    DWORD start = 0, end = 0;
    editor->GetSelection(start, end);
    doc->cursorStart = start;
    doc->cursorEnd = end;
    doc->firstVisibleLine = editor->GetFirstVisibleLine();

    // Save bookmarks
    doc->bookmarks = editor->GetBookmarks();

    // Update tab bar state
    if (m_tabBar) {
        m_tabBar->SetTabModified(doc->tabId, doc->isModified);
    }
}

//------------------------------------------------------------------------------
// Restore a document's state into the editor
// With per-tab editors, the RichEdit control already holds the content and
// undo/redo history.  We just need to show the HWND and focus it.
//------------------------------------------------------------------------------
void DocumentManager::RestoreState(int tabId) {
    DocumentState* doc = GetDocument(tabId);
    if (!doc || !doc->editor) return;

    Editor* editor = doc->editor.get();

    // Show the editor for this tab
    ShowWindow(editor->GetHandle(), SW_SHOW);
    editor->SetFocus();
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
        if (!modified && doc->editor) {
            doc->cleanTextHash = std::hash<std::wstring>{}(doc->editor->GetText());
        }
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
    DocumentState* doc = GetActiveDocument();
    if (!doc || !doc->editor) return;

    Editor* editor = doc->editor.get();

    // Fast path: if RichEdit says not modified and we agree, skip hashing
    if (!editor->IsModified() && !doc->isModified) {
        return;
    }

    // Determine modified state by comparing current text hash against the
    // clean (saved/loaded) text hash.
    std::wstring currentText = editor->GetText();
    bool modified = (std::hash<std::wstring>{}(currentText) != doc->cleanTextHash);

    // Keep the RichEdit flag in sync with our content-based check
    if (editor->IsModified() != modified) {
        editor->SetModified(modified);
    }

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
    if (!m_tabBar) return;

    DocumentState* doc = GetActiveDocument();
    if (!doc || !doc->editor) return;

    int tabId = doc->tabId;
    Editor* editor = doc->editor.get();

    // Reset all document state
    doc->cleanTextHash = 0;
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
    editor->Clear();
    editor->SetEncoding(doc->encoding);
    editor->SetLineEnding(doc->lineEnding);
    editor->SetSelection(0, 0);
    editor->SetBookmarks({});
    editor->SetFocus();
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
// Get the active document's editor
//------------------------------------------------------------------------------
Editor* DocumentManager::GetActiveEditor() {
    DocumentState* doc = GetActiveDocument();
    return doc ? doc->editor.get() : nullptr;
}

const Editor* DocumentManager::GetActiveEditor() const {
    const DocumentState* doc = GetActiveDocument();
    return doc ? doc->editor.get() : nullptr;
}

//------------------------------------------------------------------------------
// Apply global settings (font, word wrap, etc.) to all editors
//------------------------------------------------------------------------------
void DocumentManager::ApplySettingsToAllEditors(const AppSettings& settings) {
    for (auto& doc : m_documents) {
        if (!doc.editor) continue;
        Editor* editor = doc.editor.get();
        editor->SetFont(settings.fontName, settings.fontSize,
                        settings.fontWeight, settings.fontItalic);
        editor->SetWordWrap(settings.wordWrap);
        editor->SetTabSize(settings.tabSize);
        editor->SetScrollLines(settings.scrollLines);
        editor->SetRTL(settings.rightToLeft);
        editor->SetShowWhitespace(settings.showWhitespace);
        editor->SetSpellCheck(settings.spellCheckEnabled);
        editor->ApplyZoom(settings.zoomLevel);
    }
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
