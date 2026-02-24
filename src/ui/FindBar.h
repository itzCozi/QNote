//==============================================================================
// QNote - A Lightweight Notepad Clone
// FindBar.h - Chrome-style inline find/replace bar
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

namespace QNote {

// Forward declaration
class Editor;

//------------------------------------------------------------------------------
// FindBar mode
//------------------------------------------------------------------------------
enum class FindBarMode {
    Find,
    Replace
};

//------------------------------------------------------------------------------
// Find options
//------------------------------------------------------------------------------
struct FindOptions {
    bool matchCase = false;
    bool wholeWord = false;
    bool useRegex = false;
};

//------------------------------------------------------------------------------
// Chrome-style inline find/replace bar
//------------------------------------------------------------------------------
class FindBar {
public:
    FindBar() noexcept = default;
    ~FindBar();
    
    // Prevent copying
    FindBar(const FindBar&) = delete;
    FindBar& operator=(const FindBar&) = delete;
    
    // Create the find bar controls
    [[nodiscard]] bool Create(HWND parent, HINSTANCE hInstance, Editor* editor);
    
    // Destroy the find bar
    void Destroy() noexcept;
    
    // Show/hide the find bar
    void Show(FindBarMode mode = FindBarMode::Find);
    void Hide();
    [[nodiscard]] bool IsVisible() const noexcept { return m_visible; }
    
    // Get the current mode
    [[nodiscard]] FindBarMode GetMode() const noexcept { return m_mode; }
    
    // Toggle between find and replace modes
    void SetMode(FindBarMode mode);
    
    // Position/resize the find bar
    void Resize(int x, int y, int width) noexcept;
    
    // Get the height of the find bar
    [[nodiscard]] int GetHeight() const noexcept;
    
    // Set focus to the search box
    void Focus();
    
    // Handle keyboard shortcuts
    [[nodiscard]] bool HandleKeyDown(WPARAM vKey);
    
    // Process messages (call from parent's message loop)
    [[nodiscard]] bool IsDialogMessage(MSG* pMsg) noexcept;
    
    // Find operations
    void FindNext();
    void FindPrevious();
    void ReplaceNext();
    void ReplaceAll();
    
    // Get current search text
    [[nodiscard]] std::wstring GetSearchText() const;
    [[nodiscard]] std::wstring GetReplaceText() const;
    
    // Get current options
    [[nodiscard]] const FindOptions& GetOptions() const noexcept { return m_options; }
    
    // Populate search box with selected text
    void PopulateFromSelection();

private:
    // Window procedure for the find bar container
    static LRESULT CALLBACK FindBarProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Handle commands from child controls
    void OnCommand(WORD id, WORD code, HWND hwndCtl);
    
    // Create child controls
    void CreateControls();
    
    // Update match count display
    void UpdateMatchCount();
    
    // Update button states
    void UpdateButtonStates();
    
    // Perform find operation
    bool DoFind(bool forward);
    
    // Helper to create a button
    HWND CreateButton(int x, int y, int width, int height, const wchar_t* text, int id, bool isToggle = false);
    
    // Helper to create an edit control
    HWND CreateEdit(int x, int y, int width, int height, int id);
    
    // Apply font to a control
    void ApplyFont(HWND hwnd);
    
    // Calculate layout
    void LayoutControls();

private:
    HWND m_hwndParent = nullptr;
    HWND m_hwndContainer = nullptr;
    HINSTANCE m_hInstance = nullptr;
    Editor* m_editor = nullptr;
    
    // Child controls
    HWND m_hwndSearchEdit = nullptr;
    HWND m_hwndReplaceEdit = nullptr;
    HWND m_hwndMatchCount = nullptr;
    HWND m_hwndPrevBtn = nullptr;
    HWND m_hwndNextBtn = nullptr;
    HWND m_hwndCloseBtn = nullptr;
    HWND m_hwndReplaceBtn = nullptr;
    HWND m_hwndReplaceAllBtn = nullptr;
    HWND m_hwndCaseBtn = nullptr;
    HWND m_hwndWholeWordBtn = nullptr;
    HWND m_hwndRegexBtn = nullptr;
    
    // Labels
    HWND m_hwndSearchLabel = nullptr;
    HWND m_hwndReplaceLabel = nullptr;
    
    // State
    FindBarMode m_mode = FindBarMode::Find;
    FindOptions m_options;
    bool m_visible = false;
    int m_lastMatchCount = -1;
    int m_currentMatch = 0;
    
    // Font
    HFONT m_font = nullptr;
    
    // Layout constants
    static constexpr int FINDBAR_HEIGHT = 32;
    static constexpr int FINDBAR_HEIGHT_REPLACE = 64;
    static constexpr int CONTROL_HEIGHT = 24;
    static constexpr int BUTTON_WIDTH = 28;
    static constexpr int EDIT_WIDTH = 200;
    static constexpr int SPACING = 4;
    static constexpr int MARGIN = 6;
    
    // Control IDs (internal)
    static constexpr int ID_SEARCH_EDIT = 10001;
    static constexpr int ID_REPLACE_EDIT = 10002;
    static constexpr int ID_PREV_BTN = 10003;
    static constexpr int ID_NEXT_BTN = 10004;
    static constexpr int ID_CLOSE_BTN = 10005;
    static constexpr int ID_REPLACE_BTN = 10006;
    static constexpr int ID_REPLACE_ALL_BTN = 10007;
    static constexpr int ID_CASE_BTN = 10008;
    static constexpr int ID_WHOLEWORD_BTN = 10009;
    static constexpr int ID_REGEX_BTN = 10010;
    
    // Window class name
    static constexpr wchar_t FINDBAR_CLASS[] = L"QNoteFindBar";
    static bool s_classRegistered;
};

} // namespace QNote
