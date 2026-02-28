//==============================================================================
// QNote - A Lightweight Notepad Clone
// MainWindowFile.cpp - File operations (open, save, new, revert, etc.)
//==============================================================================

#include "MainWindow.h"
#include "resource.h"
#include <shellapi.h>
#include <sstream>

namespace QNote {

//------------------------------------------------------------------------------
// Helper: check if a string is empty or contains only whitespace
//------------------------------------------------------------------------------
static bool IsWhitespaceOnly(const std::wstring& text) {
    return text.find_first_not_of(L" \t\r\n") == std::wstring::npos;
}

//------------------------------------------------------------------------------
// WM_DROPFILES handler
//------------------------------------------------------------------------------
void MainWindow::OnDropFiles(HDROP hDrop) {
    wchar_t filePath[MAX_PATH] = {};
    
    // Get the number of dropped files
    UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
    
    // Open each dropped file in a new tab
    for (UINT i = 0; i < fileCount; i++) {
        if (DragQueryFileW(hDrop, i, filePath, MAX_PATH) > 0) {
            // Check if already open
            int existingTab = m_documentManager->FindDocumentByPath(filePath);
            if (existingTab >= 0) {
                if (i == fileCount - 1) {
                    OnTabSelected(existingTab);
                }
                continue;
            }
            
            // For the first file, reuse current tab if empty/untitled
            if (i == 0) {
                auto* activeDoc = m_documentManager->GetActiveDocument();
                if (activeDoc && activeDoc->isNewFile && !activeDoc->isModified &&
                    m_editor && IsWhitespaceOnly(m_editor->GetText())) {
                    LoadFile(filePath);
                    continue;
                }
            }
            
            FileReadResult result = FileIO::ReadFile(filePath);
            if (result.success) {
                m_documentManager->OpenDocument(
                    filePath, result.content, result.detectedEncoding, result.detectedLineEnding);
                UpdateActiveEditor();
                m_currentFile = filePath;
                m_isNewFile = false;
                m_isNoteMode = false;
                m_currentNoteId.clear();
                m_settingsManager->AddRecentFile(filePath);
                UpdateRecentFilesMenu();
            }
        }
    }
    
    DragFinish(hDrop);
    UpdateTitle();
    UpdateStatusBar();
    
    // Bring window to front
    SetForegroundWindow(m_hwnd);
}

//------------------------------------------------------------------------------
// File -> New
//------------------------------------------------------------------------------
void MainWindow::OnFileNew() {
    // Create a new tab with an empty document
    OnTabNew();
}

//------------------------------------------------------------------------------
// File -> Open
//------------------------------------------------------------------------------
void MainWindow::OnFileOpen() {
    std::wstring filePath;
    if (FileIO::ShowOpenDialog(m_hwnd, filePath)) {
        // Check if already open in another tab
        int existingTab = m_documentManager->FindDocumentByPath(filePath);
        if (existingTab >= 0) {
            OnTabSelected(existingTab);
            return;
        }
        
        // If current tab is untitled, unmodified, and empty - reuse it
        auto* activeDoc = m_documentManager->GetActiveDocument();
        if (activeDoc && activeDoc->isNewFile && !activeDoc->isModified && 
            m_editor && IsWhitespaceOnly(m_editor->GetText())) {
            // Reuse current tab
            LoadFile(filePath);
        } else {
            // Open in new tab
            FileReadResult result = FileIO::ReadFile(filePath);
            if (result.success) {
                int tabId = m_documentManager->OpenDocument(
                    filePath, result.content, result.detectedEncoding, result.detectedLineEnding);
                if (tabId >= 0) {
                    UpdateActiveEditor();
                    m_currentFile = filePath;
                    m_isNewFile = false;
                    m_isNoteMode = false;
                    m_currentNoteId.clear();
                    m_settingsManager->AddRecentFile(filePath);
                    UpdateRecentFilesMenu();
                    UpdateTitle();
                    UpdateStatusBar();
                }
            } else {
                MessageBoxW(m_hwnd, result.errorMessage.c_str(), L"Error Opening File",
                            MB_OK | MB_ICONERROR);
            }
        }
    }
}

//------------------------------------------------------------------------------
// File -> Save
//------------------------------------------------------------------------------
bool MainWindow::OnFileSave() {
    if (m_isNewFile || m_currentFile.empty()) {
        return OnFileSaveAs();
    }
    
    bool result = SaveFile(m_currentFile);
    if (result && m_documentManager) {
        m_documentManager->SetDocumentModified(m_documentManager->GetActiveTabId(), false);
    }
    return result;
}

//------------------------------------------------------------------------------
// File -> Save As
//------------------------------------------------------------------------------
bool MainWindow::OnFileSaveAs() {
    if (!m_editor) return false;
    std::wstring filePath;
    TextEncoding encoding = m_editor->GetEncoding();
    
    if (FileIO::ShowSaveDialog(m_hwnd, filePath, encoding, m_currentFile)) {
        m_editor->SetEncoding(encoding);
        if (SaveFile(filePath)) {
            m_currentFile = filePath;
            m_isNewFile = false;
            m_editor->SetFilePath(filePath);
            m_settingsManager->AddRecentFile(filePath);
            UpdateRecentFilesMenu();
            
            // Update document manager with new file path
            if (m_documentManager) {
                int activeTab = m_documentManager->GetActiveTabId();
                m_documentManager->SetDocumentFilePath(activeTab, filePath);
                m_documentManager->SetDocumentModified(activeTab, false);
            }
            
            UpdateTitle();
            return true;
        }
    }
    
    return false;
}

//------------------------------------------------------------------------------
// File -> Open Recent
//------------------------------------------------------------------------------
void MainWindow::OnFileOpenRecent(int index) {
    const auto& recentFiles = m_settingsManager->GetSettings().recentFiles;
    
    if (index >= 0 && index < static_cast<int>(recentFiles.size())) {
        std::wstring filePath = recentFiles[index];
        
        // Check if already open
        int existingTab = m_documentManager->FindDocumentByPath(filePath);
        if (existingTab >= 0) {
            OnTabSelected(existingTab);
            return;
        }
        
        // Check if file still exists
        if (GetFileAttributesW(filePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            MessageBoxW(m_hwnd, L"The file no longer exists.", L"QNote", MB_OK | MB_ICONWARNING);
            m_settingsManager->RemoveRecentFile(filePath);
            UpdateRecentFilesMenu();
            return;
        }
        
        // Open in new tab if current tab has content, otherwise reuse
        auto* activeDoc = m_documentManager->GetActiveDocument();
        if (activeDoc && activeDoc->isNewFile && !activeDoc->isModified &&
            m_editor && IsWhitespaceOnly(m_editor->GetText())) {
            LoadFile(filePath);
        } else {
            FileReadResult result = FileIO::ReadFile(filePath);
            if (result.success) {
                m_documentManager->OpenDocument(
                    filePath, result.content, result.detectedEncoding, result.detectedLineEnding);
                UpdateActiveEditor();
                m_currentFile = filePath;
                m_isNewFile = false;
                m_isNoteMode = false;
                m_currentNoteId.clear();
                m_settingsManager->AddRecentFile(filePath);
                UpdateRecentFilesMenu();
                UpdateTitle();
                UpdateStatusBar();
            }
        }
    }
}

//------------------------------------------------------------------------------
// File -> New Window
//------------------------------------------------------------------------------
void MainWindow::OnFileNewWindow() {
    // Get the path to our executable
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    
    // Launch a new instance
    ShellExecuteW(nullptr, L"open", exePath, nullptr, nullptr, SW_SHOWNORMAL);
}

//------------------------------------------------------------------------------
// Prompt to save changes
//------------------------------------------------------------------------------
bool MainWindow::PromptSaveChanges() {
    if (!m_editor) {
        return true;
    }
    
    if (!m_editor->IsModified()) {
        return true;
    }
    
    // An untitled file with whitespace-only content has nothing to save
    if (m_isNewFile && IsWhitespaceOnly(m_editor->GetText())) {
        return true;
    }
    
    // Skip the dialog if the user has disabled it
    if (!m_settingsManager->GetSettings().promptSaveOnClose) {
        return true;
    }
    
    std::wstring message = L"Do you want to save changes";
    if (!m_isNewFile && !m_currentFile.empty()) {
        message += L" to " + FileIO::GetFileName(m_currentFile);
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

//------------------------------------------------------------------------------
// Load file
//------------------------------------------------------------------------------
bool MainWindow::LoadFile(const std::wstring& filePath) {
    // Check file size to decide between normal and streamed read
    FileReadResult result;
    {
        HandleGuard hf(CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        LARGE_INTEGER fsize = {};
        bool isLarge = (hf.valid() && GetFileSizeEx(hf.get(), &fsize) &&
                        fsize.QuadPart > 100LL * 1024 * 1024);
        // Close the probe handle before reading
        hf = HandleGuard();

        if (isLarge) {
            result = FileIO::ReadFileLarge(filePath, m_hwndStatus);
        } else {
            result = FileIO::ReadFile(filePath);
        }
    }
    
    if (!result.success) {
        MessageBoxW(m_hwnd, result.errorMessage.c_str(), L"Error Opening File", 
                    MB_OK | MB_ICONERROR);
        return false;
    }
    
    // Use streamed (chunked) SetText for large content so the UI stays
    // responsive while the RichEdit control ingests multi-GB text.
    static constexpr size_t LARGE_TEXT_THRESHOLD = 100ULL * 1024 * 1024 / sizeof(wchar_t);
    if (result.content.size() > LARGE_TEXT_THRESHOLD) {
        m_editor->SetTextStreamed(result.content, m_hwndStatus);
    } else {
        m_editor->SetText(result.content);
    }
    m_editor->SetEncoding(result.detectedEncoding);
    m_editor->SetLineEnding(result.detectedLineEnding);
    m_editor->SetModified(false);
    m_editor->SetFilePath(filePath);
    
    m_currentFile = filePath;
    m_isNewFile = false;
    
    // Sync with document manager
    if (m_documentManager) {
        int activeTab = m_documentManager->GetActiveTabId();
        m_documentManager->SetDocumentFilePath(activeTab, filePath);
        m_documentManager->SetDocumentModified(activeTab, false);
        
        auto* doc = m_documentManager->GetActiveDocument();
        if (doc) {
            doc->encoding = result.detectedEncoding;
            doc->lineEnding = result.detectedLineEnding;
            doc->isNewFile = false;
            doc->isNoteMode = false;
            doc->noteId.clear();
        }
    }
    
    // Add to recent files
    m_settingsManager->AddRecentFile(filePath);
    UpdateRecentFilesMenu();
    
    UpdateTitle();
    UpdateStatusBar();
    
    // Start monitoring the file for external changes
    StartFileMonitoring();
    
    // Move cursor to start
    m_editor->SetSelection(0, 0);
    m_editor->SetFocus();
    
    return true;
}

//------------------------------------------------------------------------------
// Save file
//------------------------------------------------------------------------------
bool MainWindow::SaveFile(const std::wstring& filePath) {
    if (!m_editor) return false;
    std::wstring text = m_editor->GetText();
    
    FileWriteResult result = FileIO::WriteFile(
        filePath,
        text,
        m_editor->GetEncoding(),
        m_editor->GetLineEnding()
    );
    
    if (!result.success) {
        MessageBoxW(m_hwnd, result.errorMessage.c_str(), L"Error Saving File",
                    MB_OK | MB_ICONERROR);
        return false;
    }
    
    m_editor->SetModified(false);
    
    // Delete auto-save backup on successful save
    DeleteAutoSaveBackup();
    
    // Refresh file write time to avoid false change detection
    m_ignoreNextFileChange = true;
    StartFileMonitoring();
    
    // Add to recent files
    m_settingsManager->AddRecentFile(filePath);
    UpdateRecentFilesMenu();
    
    UpdateTitle();
    
    return true;
}

//------------------------------------------------------------------------------
// Create new document
//------------------------------------------------------------------------------
void MainWindow::NewDocument() {
    // Reset the document state in the DocumentManager (text, path, title, etc.)
    if (m_documentManager) {
        m_documentManager->ResetActiveDocument();
    }
    
    m_currentFile.clear();
    m_isNewFile = true;
    m_isNoteMode = false;
    m_currentNoteId.clear();
    
    StartFileMonitoring();
    UpdateTitle();
    UpdateStatusBar();
    
    // Update line numbers gutter
    if (m_lineNumbersGutter && m_lineNumbersGutter->IsVisible()) {
        m_lineNumbersGutter->Update();
    }
    
    m_editor->SetFocus();
}

//------------------------------------------------------------------------------
// Auto-save file backup (.autosave file)
//------------------------------------------------------------------------------
void MainWindow::AutoSaveFileBackup() {
    // Only auto-save in file mode with a real file
    if (m_isNoteMode || m_isNewFile || m_currentFile.empty()) {
        return;
    }
    
    // Check if auto-save is enabled and file is modified
    if (!m_settingsManager->GetSettings().fileAutoSave || !m_editor->IsModified()) {
        return;
    }
    
    // Create auto-save path
    std::wstring autoSavePath = m_currentFile + L".autosave";
    
    // Write the backup file
    std::wstring text = m_editor->GetText();
    FileWriteResult result = FileIO::WriteFile(
        autoSavePath,
        text,
        m_editor->GetEncoding(),
        m_editor->GetLineEnding()
    );
    
    // Silent failure - don't bother user with auto-save errors
    (void)result;
}

//------------------------------------------------------------------------------
// Delete auto-save backup file
//------------------------------------------------------------------------------
void MainWindow::DeleteAutoSaveBackup() {
    if (m_currentFile.empty()) {
        return;
    }
    
    std::wstring autoSavePath = m_currentFile + L".autosave";
    
    // Delete the backup file if it exists
    if (GetFileAttributesW(autoSavePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        DeleteFileW(autoSavePath.c_str());
    }
}

//------------------------------------------------------------------------------
// File -> Revert to Saved
//------------------------------------------------------------------------------
void MainWindow::OnFileRevert() {
    if (m_currentFile.empty() || m_isNewFile) {
        MessageBoxW(m_hwnd, L"No file to revert to.", L"QNote", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    if (m_editor->IsModified()) {
        int result = MessageBoxW(m_hwnd,
            L"Discard all changes and reload the file from disk?",
            L"Revert to Saved", MB_YESNO | MB_ICONWARNING);
        if (result != IDYES) return;
    }
    
    m_ignoreNextFileChange = true;
    LoadFile(m_currentFile);
}

//------------------------------------------------------------------------------
// File -> Open Containing Folder
//------------------------------------------------------------------------------
void MainWindow::OnFileOpenContainingFolder() {
    if (m_currentFile.empty() || m_isNewFile) {
        MessageBoxW(m_hwnd, L"No file is currently open.", L"QNote", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    // Use explorer.exe /select to open the folder and highlight the file
    std::wstring params = L"/select,\"" + m_currentFile + L"\"";
    ShellExecuteW(nullptr, L"open", L"explorer.exe", params.c_str(), nullptr, SW_SHOWNORMAL);
}

//------------------------------------------------------------------------------
// File change monitoring
//------------------------------------------------------------------------------
void MainWindow::StartFileMonitoring() {
    if (!m_currentFile.empty() && !m_isNewFile) {
        m_lastWriteTime = GetFileLastWriteTime(m_currentFile);
    } else {
        m_lastWriteTime = {};
    }
}

void MainWindow::CheckFileChanged() {
    if (m_currentFile.empty() || m_isNewFile || m_isNoteMode) return;
    if (m_ignoreNextFileChange) {
        m_ignoreNextFileChange = false;
        return;
    }
    
    // Check if file still exists
    if (GetFileAttributesW(m_currentFile.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    
    FILETIME currentTime = GetFileLastWriteTime(m_currentFile);
    if (currentTime.dwHighDateTime == 0 && currentTime.dwLowDateTime == 0) return;
    
    if (CompareFileTime(&currentTime, &m_lastWriteTime) != 0) {
        m_lastWriteTime = currentTime;
        
        // If the document has unsaved changes, ask before reloading
        if (m_editor->IsModified()) {
            int result = MessageBoxW(m_hwnd,
                L"The file has been modified outside QNote.\n"
                L"Do you want to reload it? Unsaved changes will be lost.",
                L"File Changed", MB_YESNO | MB_ICONWARNING);
            if (result != IDYES) {
                return;
            }
        }
        
        m_ignoreNextFileChange = true;
        LoadFile(m_currentFile);
    }
}

FILETIME MainWindow::GetFileLastWriteTime(const std::wstring& path) {
    FILETIME ft = {};
    // Use FILE_READ_ATTRIBUTES instead of GENERIC_READ to avoid triggering
    // access time updates or antivirus scans that could modify timestamps
    HANDLE hFile = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, 
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        GetFileTime(hFile, nullptr, nullptr, &ft);
        CloseHandle(hFile);
    }
    return ft;
}

//------------------------------------------------------------------------------
// File -> Save All
//------------------------------------------------------------------------------
void MainWindow::OnFileSaveAll() {
    if (!m_documentManager) return;

    // Save current editor state first
    m_documentManager->SaveCurrentState();

    int activeTab = m_documentManager->GetActiveTabId();
    auto ids = m_documentManager->GetAllTabIds();

    for (int id : ids) {
        auto* doc = m_documentManager->GetDocument(id);
        if (!doc || !doc->isModified) continue;

        // Switch to tab so editor has its content
        if (id != m_documentManager->GetActiveTabId()) {
            OnTabSelected(id);
        }

        if (doc->isNewFile || doc->filePath.empty()) {
            // Prompt Save As for untitled files
            OnFileSaveAs();
        } else {
            if (SaveFile(doc->filePath)) {
                m_documentManager->SetDocumentModified(id, false);
            }
        }
    }

    // Switch back to the originally active tab
    if (activeTab != m_documentManager->GetActiveTabId()) {
        OnTabSelected(activeTab);
    }
}

//------------------------------------------------------------------------------
// File -> Close All
//------------------------------------------------------------------------------
void MainWindow::OnFileCloseAll() {
    OnTabCloseAll();
}

//------------------------------------------------------------------------------
// File -> Open from Clipboard
//------------------------------------------------------------------------------
void MainWindow::OnFileOpenFromClipboard() {
    if (!OpenClipboard(m_hwnd)) return;

    std::wstring clipText;
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        LPCWSTR pText = static_cast<LPCWSTR>(GlobalLock(hData));
        if (pText) {
            clipText = pText;
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();

    if (clipText.empty()) return;

    // Create a new tab with the clipboard content
    if (m_documentManager) {
        auto* activeDoc = m_documentManager->GetActiveDocument();
        if (activeDoc && activeDoc->isNewFile && !activeDoc->isModified &&
            IsWhitespaceOnly(m_editor->GetText())) {
            // Reuse current empty tab
            m_editor->SetText(clipText);
        } else {
            // New tab
            m_documentManager->SaveCurrentState();
            int newTab = m_documentManager->NewDocument();
            OnTabSelected(newTab);
            m_editor->SetText(clipText);
        }
        UpdateTitle();
    }
}

} // namespace QNote
