//==============================================================================
// QNote - A Lightweight Notepad Clone
// MainWindowTabs.cpp - Tab management operations
//==============================================================================

#include "MainWindow.h"
#include "resource.h"
#include <shellapi.h>

namespace QNote {

//------------------------------------------------------------------------------
// Helper: check if a string is empty or contains only whitespace
//------------------------------------------------------------------------------
static bool IsWhitespaceOnly(const std::wstring& text) {
    return text.find_first_not_of(L" \t\r\n") == std::wstring::npos;
}

//------------------------------------------------------------------------------
// Tab -> New Tab (Ctrl+T)
//------------------------------------------------------------------------------
void MainWindow::OnTabNew() {
    if (m_documentManager) {
        m_documentManager->NewDocument();
        UpdateActiveEditor();
        m_currentFile.clear();
        m_isNewFile = true;
        m_isNoteMode = false;
        m_currentNoteId.clear();
        StartFileMonitoring();
        UpdateTitle();
        UpdateStatusBar();
        
        if (m_lineNumbersGutter && m_lineNumbersGutter->IsVisible()) {
            m_lineNumbersGutter->Update();
        }
    }
}

//------------------------------------------------------------------------------
// Tab -> Close Tab (Ctrl+W) 
//------------------------------------------------------------------------------
void MainWindow::OnTabClose() {
    if (!m_documentManager || !m_tabBar) return;
    
    int activeTab = m_documentManager->GetActiveTabId();
    OnTabCloseRequested(activeTab);
}

//------------------------------------------------------------------------------
// Tab selected (user clicked a different tab)
//------------------------------------------------------------------------------
void MainWindow::OnTabSelected(int tabId) {
    if (!m_documentManager) return;
    
    m_documentManager->SwitchToDocument(tabId);
    
    // Update the active editor pointer and sub-components
    UpdateActiveEditor();
    
    // Update MainWindow state from the new active document
    auto* doc = m_documentManager->GetActiveDocument();
    if (doc) {
        m_currentFile = doc->filePath;
        m_isNewFile = doc->isNewFile;
        m_isNoteMode = doc->isNoteMode;
        m_currentNoteId = doc->noteId;
    }
    
    // Update file monitoring for the newly active file
    StartFileMonitoring();
    
    UpdateTitle();
    UpdateStatusBar();
    
    // Update line numbers gutter
    if (m_lineNumbersGutter && m_lineNumbersGutter->IsVisible()) {
        m_lineNumbersGutter->SetFont(m_editor->GetFont());
        m_lineNumbersGutter->Update();
    }
}

//------------------------------------------------------------------------------
// Tab close requested (user clicked X on a tab)
//------------------------------------------------------------------------------
void MainWindow::OnTabCloseRequested(int tabId) {
    if (!m_documentManager || !m_tabBar) return;
    
    // Don't close the last tab - instead clear it
    if (m_documentManager->GetDocumentCount() <= 1) {
        auto* doc = m_documentManager->GetActiveDocument();
        if (doc && doc->isModified) {
            if (!PromptSaveTab(tabId)) return;
        }
        // Reset the current tab instead of closing
        NewDocument();
        return;
    }
    
    // Check for unsaved changes
    if (!PromptSaveTab(tabId)) return;
    
    m_documentManager->CloseDocument(tabId);
    
    // Update the active editor pointer after closing
    UpdateActiveEditor();
    
    // Update MainWindow state from new active document
    auto* doc = m_documentManager->GetActiveDocument();
    if (doc) {
        m_currentFile = doc->filePath;
        m_isNewFile = doc->isNewFile;
        m_isNoteMode = doc->isNoteMode;
        m_currentNoteId = doc->noteId;
    }
    
    UpdateTitle();
    UpdateStatusBar();
}

//------------------------------------------------------------------------------
// Tab renamed via double-click
//------------------------------------------------------------------------------
void MainWindow::OnTabRenamed(int tabId) {
    if (!m_documentManager || !m_tabBar) return;
    
    auto* tabItem = m_tabBar->GetTab(tabId);
    if (tabItem) {
        m_documentManager->SetDocumentTitle(tabId, tabItem->title);
        UpdateTitle();
    }
}

//------------------------------------------------------------------------------
// Tab pin toggled via context menu
//------------------------------------------------------------------------------
void MainWindow::OnTabPinToggled(int tabId) {
    if (!m_documentManager) return;
    
    auto* doc = m_documentManager->GetDocument(tabId);
    if (doc) {
        bool newPinState = !doc->isPinned;
        m_documentManager->SetDocumentPinned(tabId, newPinState);
    }
}

//------------------------------------------------------------------------------
// Close other tabs
//------------------------------------------------------------------------------
void MainWindow::OnTabCloseOthers(int tabId) {
    if (!m_documentManager) return;
    
    auto ids = m_documentManager->GetAllTabIds();
    for (int id : ids) {
        if (id == tabId) continue;
        auto* doc = m_documentManager->GetDocument(id);
        if (doc && doc->isModified) {
            if (!PromptSaveTab(id)) return;
        }
    }
    
    m_documentManager->CloseOtherDocuments(tabId);
    OnTabSelected(tabId);
}

//------------------------------------------------------------------------------
// Close all tabs
//------------------------------------------------------------------------------
void MainWindow::OnTabCloseAll() {
    if (!m_documentManager) return;
    
    auto ids = m_documentManager->GetAllTabIds();
    for (int id : ids) {
        auto* doc = m_documentManager->GetDocument(id);
        if (doc && doc->isModified) {
            if (!PromptSaveTab(id)) return;
        }
    }
    
    m_documentManager->CloseAllDocuments();
    m_editor = nullptr;
    
    // Create a fresh tab
    OnTabNew();
}

//------------------------------------------------------------------------------
// Close tabs to the right
//------------------------------------------------------------------------------
void MainWindow::OnTabCloseToRight(int tabId) {
    if (!m_documentManager) return;
    
    // Check for unsaved changes in tabs to the right
    auto ids = m_documentManager->GetAllTabIds();
    bool found = false;
    for (int id : ids) {
        if (id == tabId) { found = true; continue; }
        if (!found) continue;
        auto* doc = m_documentManager->GetDocument(id);
        if (doc && doc->isModified) {
            if (!PromptSaveTab(id)) return;
        }
    }
    
    m_documentManager->CloseDocumentsToRight(tabId);
}

//------------------------------------------------------------------------------
// Tab detached (dragged outside tab bar) -> open in new window
//------------------------------------------------------------------------------
void MainWindow::OnTabDetached(int tabId) {
    if (!m_documentManager) return;

    auto* doc = m_documentManager->GetDocument(tabId);
    if (!doc) return;

    // If the tab has a saved file, launch a new instance with that file
    if (!doc->filePath.empty() && !doc->isNewFile) {
        // Save current content first if modified
        if (doc->isModified) {
            if (tabId != m_documentManager->GetActiveTabId()) {
                OnTabSelected(tabId);
            }
            if (!OnFileSave()) return; // cancelled
        }

        // Get our executable path
        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);

        // Get the drop position from the tab bar so the new window opens there
        POINT dropPos = m_tabBar->GetLastDetachPosition();

        // Launch new instance with the file path and position
        std::wstring cmdLine = L"\"" + std::wstring(exePath) + L"\" \"" + doc->filePath + L"\"";
        cmdLine += L" --pos " + std::to_wstring(dropPos.x) + L"," + std::to_wstring(dropPos.y);        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        if (CreateProcessW(nullptr, &cmdLine[0], nullptr, nullptr, FALSE,
                           0, nullptr, nullptr, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            // Close the tab in current window
            m_documentManager->CloseDocument(tabId);
            UpdateActiveEditor();
            
            // Sync MainWindow state with new active document
            auto* newDoc = m_documentManager->GetActiveDocument();
            if (newDoc) {
                m_currentFile = newDoc->filePath;
                m_isNewFile = newDoc->isNewFile;
                m_isNoteMode = newDoc->isNoteMode;
                m_currentNoteId = newDoc->noteId;
            }
            UpdateTitle();
            UpdateStatusBar();
        }
    } else {
        // Untitled tab - can't easily transfer, just inform user
        MessageBoxW(m_hwnd,
                    L"Save the file first to detach it to a new window.",
                    L"QNote", MB_OK | MB_ICONINFORMATION);
    }
}

//------------------------------------------------------------------------------
// Prompt to save a specific tab's document
//------------------------------------------------------------------------------
bool MainWindow::PromptSaveTab(int tabId) {
    if (!m_documentManager) return true;
    
    // Sync editor content to document state if this is the active tab
    if (tabId == m_documentManager->GetActiveTabId()) {
        m_documentManager->SaveCurrentState();
    }
    
    auto* doc = m_documentManager->GetDocument(tabId);
    if (!doc || !doc->isModified) return true;
    
    // An untitled file with whitespace-only content has nothing to save
    if (doc->isNewFile && IsWhitespaceOnly(doc->editor ? doc->editor->GetText() : L"")) return true;
    
    // Skip the dialog if the user has disabled it
    if (!m_settingsManager->GetSettings().promptSaveOnClose) {
        return true;
    }
    
    // Switch to the tab so user can see it
    if (tabId != m_documentManager->GetActiveTabId()) {
        OnTabSelected(tabId);
    }
    
    std::wstring message = L"Do you want to save changes";
    if (!doc->filePath.empty()) {
        message += L" to " + doc->GetDisplayTitle();
    }
    message += L"?";
    
    int result = MessageBoxW(m_hwnd, message.c_str(), L"QNote",
                             MB_YESNOCANCEL | MB_ICONWARNING);
    
    switch (result) {
        case IDYES:
            return OnFileSave();
        case IDNO:
            return true;
        case IDCANCEL:
        default:
            return false;
    }
}

} // namespace QNote
