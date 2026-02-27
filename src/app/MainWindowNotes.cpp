//==============================================================================
// QNote - A Lightweight Notepad Clone
// MainWindowNotes.cpp - Notes operations (import, export, note management)
//==============================================================================

#include "MainWindow.h"
#include "resource.h"
#include <Commdlg.h>

namespace QNote {

//------------------------------------------------------------------------------
// Notes -> Export Notes
//------------------------------------------------------------------------------
void MainWindow::OnNotesExport() {
    if (!m_noteStore || m_noteStore->GetNoteCount() == 0) {
        MessageBoxW(m_hwnd, L"There are no notes to export.", L"QNote", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    OPENFILENAMEW ofn = {};
    wchar_t filePath[MAX_PATH] = L"notes_export.json";
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Export Notes";
    ofn.lpstrDefExt = L"json";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    
    if (GetSaveFileNameW(&ofn)) {
        if (m_noteStore->ExportNotes(filePath)) {
            std::wstring msg = L"Successfully exported " + std::to_wstring(m_noteStore->GetNoteCount()) + L" notes.";
            MessageBoxW(m_hwnd, msg.c_str(), L"Export Complete", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxW(m_hwnd, L"Failed to export notes.", L"Export Error", MB_OK | MB_ICONERROR);
        }
    }
}

//------------------------------------------------------------------------------
// Notes -> Import Notes
//------------------------------------------------------------------------------
void MainWindow::OnNotesImport() {
    OPENFILENAMEW ofn = {};
    wchar_t filePath[MAX_PATH] = {};
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Import Notes";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    
    if (GetOpenFileNameW(&ofn)) {
        if (!m_noteStore) {
            InitializeNoteStore();
        }
        
        size_t countBefore = m_noteStore->GetNoteCount();
        if (m_noteStore->ImportNotes(filePath)) {
            size_t added = m_noteStore->GetNoteCount() - countBefore;
            std::wstring msg = L"Successfully imported " + std::to_wstring(added) + L" new notes.";
            MessageBoxW(m_hwnd, msg.c_str(), L"Import Complete", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxW(m_hwnd, L"Failed to import notes. The file may be invalid.", L"Import Error", MB_OK | MB_ICONERROR);
        }
    }
}

//------------------------------------------------------------------------------
// Initialize note store and related components
//------------------------------------------------------------------------------
void MainWindow::InitializeNoteStore() {
    // Initialize the note store
    if (m_noteStore) {
        (void)m_noteStore->Initialize();
    }
    
    // Create capture window
    m_captureWindow = std::make_unique<CaptureWindow>();
    (void)m_captureWindow->Create(m_hInstance, m_noteStore.get());
    // Set callback for when user wants to edit in main window
    m_captureWindow->SetEditCallback([this](const std::wstring& noteId) {
        OpenNoteFromId(noteId);
    });
    
    // Create note list window
    m_noteListWindow = std::make_unique<NoteListWindow>();
    (void)m_noteListWindow->Create(m_hInstance, m_hwnd, m_noteStore.get());
    // Set callback for opening notes
    m_noteListWindow->SetOpenCallback([this](const Note& note) {
        LoadNoteIntoEditor(note);
    });
    
    // Register global hotkey (Ctrl+Shift+Q)
    if (m_hotkeyManager) {
        (void)m_hotkeyManager->Register(m_hwnd, HOTKEY_QUICKCAPTURE);
    }
}

//------------------------------------------------------------------------------
// Auto-save current note
//------------------------------------------------------------------------------
void MainWindow::AutoSaveCurrentNote() {
    if (!m_isNoteMode || !m_noteStore || !m_editor) return;
    
    std::wstring content = m_editor->GetText();
    
    if (m_currentNoteId.empty()) {
        // Create a new note
        Note newNote = m_noteStore->CreateNote(content);
        m_currentNoteId = newNote.id;
    } else {
        // Update existing note
        auto existingNote = m_noteStore->GetNote(m_currentNoteId);
        if (existingNote) {
            Note updated = *existingNote;
            updated.content = content;
            (void)m_noteStore->UpdateNote(updated);
        }
    }
    
    m_editor->SetModified(false);
    UpdateTitle();
}

//------------------------------------------------------------------------------
// Load a note into the editor
//------------------------------------------------------------------------------
void MainWindow::LoadNoteIntoEditor(const Note& note) {
    if (!m_editor) return;
    // Save current note if in note mode
    if (m_isNoteMode && m_editor->IsModified()) {
        AutoSaveCurrentNote();
    }
    
    // Switch to note mode
    m_isNoteMode = true;
    m_currentNoteId = note.id;
    m_currentFile.clear();
    m_isNewFile = true;
    
    // Load note content
    m_editor->SetText(note.content);
    m_editor->SetModified(false);
    m_editor->SetSelection(0, 0);
    m_editor->SetFocus();
    
    UpdateTitle();
    UpdateStatusBar();
    
    // Bring window to front
    SetForegroundWindow(m_hwnd);
}

//------------------------------------------------------------------------------
// Open a note by ID
//------------------------------------------------------------------------------
void MainWindow::OpenNoteFromId(const std::wstring& noteId) {
    if (!m_noteStore) return;
    
    auto note = m_noteStore->GetNote(noteId);
    if (note) {
        LoadNoteIntoEditor(*note);
    }
}

//------------------------------------------------------------------------------
// Handle global hotkey
//------------------------------------------------------------------------------
void MainWindow::OnHotkey(int hotkeyId) {
    if (hotkeyId == HOTKEY_QUICKCAPTURE) {
        OnNotesQuickCapture();
    }
}

//------------------------------------------------------------------------------
// Notes -> New Note
//------------------------------------------------------------------------------
void MainWindow::OnNotesNew() {
    if (!m_editor) return;
    // Save current note if in note mode
    if (m_isNoteMode && m_editor->IsModified()) {
        AutoSaveCurrentNote();
    }
    
    // Switch to note mode with a new note
    m_isNoteMode = true;
    m_currentNoteId.clear();
    m_currentFile.clear();
    m_isNewFile = true;
    
    m_editor->Clear();
    m_editor->SetModified(false);
    m_editor->SetFocus();
    
    UpdateTitle();
    UpdateStatusBar();
}

//------------------------------------------------------------------------------
// Notes -> Quick Capture
//------------------------------------------------------------------------------
void MainWindow::OnNotesQuickCapture() {
    if (m_captureWindow) {
        m_captureWindow->Toggle();
    }
}

//------------------------------------------------------------------------------
// Notes -> All Notes
//------------------------------------------------------------------------------
void MainWindow::OnNotesAllNotes() {
    if (m_noteListWindow) {
        m_noteListWindow->SetViewMode(NoteListViewMode::AllNotes);
        m_noteListWindow->Show();
    }
}

//------------------------------------------------------------------------------
// Notes -> Pinned Notes
//------------------------------------------------------------------------------
void MainWindow::OnNotesPinned() {
    if (m_noteListWindow) {
        m_noteListWindow->SetViewMode(NoteListViewMode::Pinned);
        m_noteListWindow->Show();
    }
}

//------------------------------------------------------------------------------
// Notes -> Timeline
//------------------------------------------------------------------------------
void MainWindow::OnNotesTimeline() {
    if (m_noteListWindow) {
        m_noteListWindow->SetViewMode(NoteListViewMode::Timeline);
        m_noteListWindow->Show();
    }
}

//------------------------------------------------------------------------------
// Notes -> Search
//------------------------------------------------------------------------------
void MainWindow::OnNotesSearch() {
    if (m_noteListWindow) {
        m_noteListWindow->SetViewMode(NoteListViewMode::AllNotes);
        m_noteListWindow->Show();
        // Focus the search box (will need to add this method to NoteListWindow)
    }
}

//------------------------------------------------------------------------------
// Notes -> Pin/Unpin Current Note
//------------------------------------------------------------------------------
void MainWindow::OnNotesPinCurrent() {
    if (!m_isNoteMode || m_currentNoteId.empty() || !m_noteStore) {
        MessageBoxW(m_hwnd, L"No note is currently being edited.", L"QNote", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    (void)m_noteStore->TogglePin(m_currentNoteId);
    
    auto note = m_noteStore->GetNote(m_currentNoteId);
    if (note) {
        std::wstring msg = note->isPinned ? L"Note pinned." : L"Note unpinned.";
        // Could show in status bar instead
        UpdateTitle();
    }
}

//------------------------------------------------------------------------------
// Notes -> Save Note Now
//------------------------------------------------------------------------------
void MainWindow::OnNotesSaveNow() {
    if (m_isNoteMode) {
        AutoSaveCurrentNote();
        // Brief visual feedback could be added here
    } else {
        // In file mode, use regular save
        OnFileSave();
    }
}

//------------------------------------------------------------------------------
// Notes -> Delete Current Note
//------------------------------------------------------------------------------
void MainWindow::OnNotesDeleteCurrent() {
    if (!m_isNoteMode || m_currentNoteId.empty() || !m_noteStore) {
        MessageBoxW(m_hwnd, L"No note is currently being edited.", L"QNote", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    auto note = m_noteStore->GetNote(m_currentNoteId);
    std::wstring title = note ? note->GetDisplayTitle() : L"this note";
    
    std::wstring msg = L"Delete \"" + title + L"\"? This cannot be undone.";
    if (MessageBoxW(m_hwnd, msg.c_str(), L"Delete Note", MB_YESNO | MB_ICONWARNING) == IDYES) {
        (void)m_noteStore->DeleteNote(m_currentNoteId);
        
        // Start a new note
        OnNotesNew();
        
        // Refresh note list if visible
        if (m_noteListWindow && m_noteListWindow->IsVisible()) {
            m_noteListWindow->RefreshList();
        }
    }
}

//------------------------------------------------------------------------------
// Notes -> Favorites (reuse Pinned notes as favorites)
//------------------------------------------------------------------------------
void MainWindow::OnNotesFavorites() {
    // Favorites = Pinned notes. Reuse the existing pinned view.
    OnNotesPinned();
}

//------------------------------------------------------------------------------
// Notes -> Duplicate Note
//------------------------------------------------------------------------------
void MainWindow::OnNotesDuplicate() {
    if (!m_isNoteMode || m_currentNoteId.empty() || !m_noteStore) {
        // For file mode, duplicate the current document content to a new tab
        if (m_documentManager) {
            m_documentManager->SaveCurrentState();
            std::wstring content = m_editor->GetText();
            int newTab = m_documentManager->NewDocument();
            OnTabSelected(newTab);
            m_editor->SetText(content);
            UpdateTitle();
        }
        return;
    }

    // In note mode, create a copy of the current note
    auto note = m_noteStore->GetNote(m_currentNoteId);
    if (!note.has_value()) return;

    Note newNote = m_noteStore->CreateNote(note->content);
    newNote.title = L"Copy of " + note->GetDisplayTitle();
    (void)m_noteStore->UpdateNote(newNote);

    // Open the new note
    OpenNoteFromId(newNote.id);
}

} // namespace QNote
