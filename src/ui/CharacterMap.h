//==============================================================================
// QNote - A Lightweight Notepad Clone
// CharacterMap.h - Character Map / Emoji Picker popup window
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
#include <functional>

namespace QNote {

//------------------------------------------------------------------------------
// A single character entry displayed in the grid
//------------------------------------------------------------------------------
struct CharEntry {
    std::wstring ch;       // The character (may be multi-wchar for surrogate pairs)
    std::wstring name;     // Human-readable name shown in tooltip/status
};

//------------------------------------------------------------------------------
// A named category of characters
//------------------------------------------------------------------------------
struct CharCategory {
    std::wstring name;
    std::vector<CharEntry> entries;
};

//------------------------------------------------------------------------------
// Character Map popup window
//------------------------------------------------------------------------------
class CharacterMap {
public:
    CharacterMap() = default;
    ~CharacterMap();

    CharacterMap(const CharacterMap&) = delete;
    CharacterMap& operator=(const CharacterMap&) = delete;

    // Create and show the popup.  insertCallback is called with the chosen character.
    bool Show(HWND parent, HINSTANCE hInstance,
              std::function<void(const std::wstring&)> insertCallback);

    // Destroy the window
    void Close() noexcept;

    // Is the window currently visible?
    [[nodiscard]] bool IsVisible() const noexcept;

    // Handle dialog-style messages (Tab/Arrow key navigation)
    [[nodiscard]] bool IsDialogMessage(MSG* pMsg) noexcept;

    // Flattened list of visible chars (for keyboard nav)
    struct FlatEntry {
        int catIndex;
        int entryIndex;
        RECT cellRect;   // relative to content area
    };

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void OnPaint();
    void OnMouseMove(int x, int y);
    void OnLButtonDown(int x, int y);
    void OnKeyDown(WPARAM vk);
    void OnSize();
    void UpdateSearch(const std::wstring& query);
    void BuildCategories();
    int HitTest(int x, int y) const;
    void EnsureSelectedVisible();
    void ScrollTo(int pos);
    void UpdateScrollBar();

    // Layout constants
    static constexpr int CELL_SIZE = 36;
    static constexpr int CATEGORY_HEADER_H = 24;
    static constexpr int SEARCH_BAR_H = 28;
    static constexpr int STATUSBAR_H = 22;
    static constexpr int WINDOW_W = 400;
    static constexpr int WINDOW_H = 480;

    HWND m_hwnd = nullptr;
    HWND m_hwndSearch = nullptr;
    HWND m_hwndParent = nullptr;
    HINSTANCE m_hInstance = nullptr;
    HFONT m_fontGrid = nullptr;
    HFONT m_fontHeader = nullptr;
    HFONT m_fontSearch = nullptr;
    HFONT m_fontStatus = nullptr;

    std::function<void(const std::wstring&)> m_insertCallback;

    // All categories (source of truth)
    std::vector<CharCategory> m_allCategories;

    // Filtered view (after search)
    std::vector<CharCategory> m_categories;

    std::vector<FlatEntry> m_flatEntries;

    int m_selectedIndex = -1;
    int m_hoverIndex = -1;
    int m_scrollY = 0;
    int m_contentHeight = 0;
    int m_colCount = 1;

    static constexpr wchar_t WINDOW_CLASS[] = L"QNoteCharacterMap";
    static bool s_classRegistered;
};

} // namespace QNote
