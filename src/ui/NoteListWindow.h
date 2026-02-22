//==============================================================================
// QNote - A Lightweight Notepad Clone
// NoteListWindow.h - Note browser with timeline, search, and pin support
//==============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <CommCtrl.h>
#include <string>
#include <functional>
#include <vector>
#include "NoteStore.h"

namespace QNote {

//------------------------------------------------------------------------------
// Callback when user wants to open a note in the main editor
//------------------------------------------------------------------------------
using NoteOpenCallback = std::function<void(const Note& note)>;
using NoteDeleteCallback = std::function<void(const std::wstring& noteId)>;

//------------------------------------------------------------------------------
// View mode for the note list
//------------------------------------------------------------------------------
enum class NoteListViewMode {
    AllNotes,      // Show all notes
    Pinned,        // Show only pinned notes
    Timeline,      // Show notes grouped by date
    Search         // Show search results
};

//------------------------------------------------------------------------------
// Note list window class
//------------------------------------------------------------------------------
class NoteListWindow {
public:
    NoteListWindow();
    ~NoteListWindow();
    
    // Prevent copying
    NoteListWindow(const NoteListWindow&) = delete;
    NoteListWindow& operator=(const NoteListWindow&) = delete;
    
    // Initialize and create the window
    [[nodiscard]] bool Create(HINSTANCE hInstance, HWND parentWindow, NoteStore* noteStore);
    
    // Show/hide
    void Show();
    void Hide();
    void Toggle();
    [[nodiscard]] bool IsVisible() const;
    
    // Set view mode
    void SetViewMode(NoteListViewMode mode);
    [[nodiscard]] NoteListViewMode GetViewMode() const { return m_viewMode; }
    
    // Refresh the list
    void RefreshList();
    
    // Perform search
    void Search(const std::wstring& query);
    void ClearSearch();
    
    // Set callbacks
    void SetOpenCallback(NoteOpenCallback callback) { m_openCallback = callback; }
    void SetDeleteCallback(NoteDeleteCallback callback) { m_deleteCallback = callback; }
    
    // Get window handle
    [[nodiscard]] HWND GetHandle() const { return m_hwnd; }
    
private:
    // Window procedure
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Message handlers
    void OnCreate();
    void OnDestroy();
    void OnSize(int width, int height);
    void OnCommand(WORD id, WORD code, HWND hwndCtl);
    void OnNotify(NMHDR* pnmh);
    LRESULT OnCtlColorEdit(HDC hdc, HWND hwndEdit);
    LRESULT OnCtlColorStatic(HDC hdc, HWND hwndStatic);
    
    // List operations
    void PopulateList();
    void AddNoteToList(const Note& note, int index);
    void OnListItemActivated(int index);
    void OnListItemRightClick(int index, POINT pt);
    
    // Context menu
    void ShowContextMenu(int index, POINT pt);
    void OnContextMenuPin(int index);
    void OnContextMenuDelete(int index);
    void OnContextMenuOpen(int index);
    
    // UI helpers
    void CreateControls();
    void UpdateViewModeUI();
    void UpdateStatusText();
    
    // Get display string for a date
    [[nodiscard]] std::wstring GetDateGroupHeader(time_t timestamp) const;
    
private:
    HWND m_hwnd = nullptr;
    HWND m_hwndParent = nullptr;
    HWND m_hwndList = nullptr;
    HWND m_hwndSearch = nullptr;
    HWND m_hwndSearchBtn = nullptr;
    HWND m_hwndClearBtn = nullptr;
    HWND m_hwndAllBtn = nullptr;
    HWND m_hwndPinnedBtn = nullptr;
    HWND m_hwndTimelineBtn = nullptr;
    HWND m_hwndStatus = nullptr;
    HINSTANCE m_hInstance = nullptr;
    HFONT m_hFont = nullptr;
    HFONT m_hBoldFont = nullptr;
    HIMAGELIST m_hImageList = nullptr;
    
    NoteStore* m_noteStore = nullptr;
    NoteListViewMode m_viewMode = NoteListViewMode::AllNotes;
    std::wstring m_currentSearch;
    std::vector<Note> m_displayedNotes;
    
    NoteOpenCallback m_openCallback;
    NoteDeleteCallback m_deleteCallback;
    
    // Window dimensions
    static constexpr int DEFAULT_WIDTH = 500;
    static constexpr int DEFAULT_HEIGHT = 600;
    static constexpr int TOOLBAR_HEIGHT = 36;
    static constexpr int SEARCH_HEIGHT = 28;
    static constexpr int STATUS_HEIGHT = 22;
    static constexpr int PADDING = 8;
    
    // Window class name
    static constexpr wchar_t WINDOW_CLASS[] = L"QNoteListWindow";
    
    // Control IDs
    static constexpr int IDC_SEARCH_EDIT = 2001;
    static constexpr int IDC_SEARCH_BTN = 2002;
    static constexpr int IDC_CLEAR_BTN = 2003;
    static constexpr int IDC_ALL_BTN = 2004;
    static constexpr int IDC_PINNED_BTN = 2005;
    static constexpr int IDC_TIMELINE_BTN = 2006;
    static constexpr int IDC_NOTE_LIST = 2007;
    
    // Context menu IDs
    static constexpr int IDM_CTX_OPEN = 3001;
    static constexpr int IDM_CTX_PIN = 3002;
    static constexpr int IDM_CTX_DELETE = 3003;
};

} // namespace QNote
