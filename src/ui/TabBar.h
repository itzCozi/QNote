//==============================================================================
// QNote - A Lightweight Notepad Clone
// TabBar.h - Notepad++-style tab bar for multi-document editing
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
// Tab item data
//------------------------------------------------------------------------------
struct TabItem {
    int id = -1;                     // Unique tab identifier
    std::wstring title;              // Display title
    std::wstring filePath;           // Associated file path (empty for untitled)
    bool isPinned = false;           // Whether the tab is pinned
    bool isModified = false;         // Whether the document has unsaved changes
};

//------------------------------------------------------------------------------
// Tab bar notification types
//------------------------------------------------------------------------------
enum class TabNotification {
    TabSelected,      // User clicked a different tab
    TabCloseRequested,// User clicked the X on a tab
    NewTabRequested,  // User clicked the + button
    TabRenamed,       // User renamed a tab via double-click
    TabPinToggled,    // User toggled pin state via context menu
    CloseOthers,      // Close all tabs except the one right-clicked
    CloseAll,         // Close all tabs
    CloseToRight,     // Close tabs to the right
};

//------------------------------------------------------------------------------
// Tab bar callback
//------------------------------------------------------------------------------
using TabCallback = std::function<void(TabNotification notification, int tabId)>;

//------------------------------------------------------------------------------
// TabBar - Custom owner-drawn tab control
//------------------------------------------------------------------------------
class TabBar {
public:
    TabBar() noexcept = default;
    ~TabBar();

    // Prevent copying
    TabBar(const TabBar&) = delete;
    TabBar& operator=(const TabBar&) = delete;

    // Create the tab bar window
    [[nodiscard]] bool Create(HWND parent, HINSTANCE hInstance);

    // Destroy the tab bar
    void Destroy() noexcept;

    // Get the tab bar window handle
    [[nodiscard]] HWND GetHandle() const noexcept { return m_hwnd; }

    // Tab management
    int AddTab(const std::wstring& title, const std::wstring& filePath = L"", bool pinned = false);
    void RemoveTab(int tabId);
    void SetActiveTab(int tabId);
    [[nodiscard]] int GetActiveTabId() const noexcept { return m_activeTabId; }
    [[nodiscard]] int GetTabCount() const noexcept { return static_cast<int>(m_tabs.size()); }

    // Update tab properties
    void SetTabTitle(int tabId, const std::wstring& title);
    void SetTabFilePath(int tabId, const std::wstring& filePath);
    void SetTabModified(int tabId, bool modified);
    void SetTabPinned(int tabId, bool pinned);

    // Query tab properties
    [[nodiscard]] const TabItem* GetTab(int tabId) const;
    [[nodiscard]] const TabItem* GetActiveTab() const;
    [[nodiscard]] std::vector<int> GetAllTabIds() const;
    [[nodiscard]] int GetTabIdByIndex(int index) const;

    // Layout
    void Resize(int x, int y, int width);
    [[nodiscard]] int GetHeight() const noexcept { return m_tabBarHeight; }

    // Set notification callback
    void SetCallback(TabCallback callback) { m_callback = std::move(callback); }

    // Tab rename control (public so subclass proc can call EndRename)
    void EndRename(bool accept);

private:
    // Window procedure
    static LRESULT CALLBACK TabBarProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // Painting
    void OnPaint();
    void DrawTab(HDC hdc, const RECT& rc, const TabItem& tab, bool isActive, bool isHovered, bool isCloseHovered);
    void DrawNewTabButton(HDC hdc, const RECT& rc, bool isHovered);

    // Hit testing
    int TabHitTest(int x, int y) const;          // Returns tab id or -1
    bool CloseButtonHitTest(int tabId, int x, int y) const;  // Is close btn hit?
    bool NewTabButtonHitTest(int x, int y) const;

    // Tab geometry
    RECT GetTabRect(int index) const;
    RECT GetCloseButtonRect(const RECT& tabRect) const;
    RECT GetNewTabButtonRect() const;

    // Context menu
    void ShowContextMenu(int tabId, int x, int y);

    // Tab rename (inline edit)
    void BeginRename(int tabId);

    // Emit notification
    void Notify(TabNotification notification, int tabId);

    // Find tab by id
    int FindTabIndex(int tabId) const;

    // Invalidate for repaint
    void Redraw() noexcept;

private:
    // DPI scaling
    void InitializeDPI();
    int Scale(int value) const noexcept { return MulDiv(value, m_dpi, 96); }

    // Update tooltip for current hover
    void UpdateTooltip();

private:
    HWND m_hwnd = nullptr;
    HWND m_hwndParent = nullptr;
    HWND m_hwndRenameEdit = nullptr;
    HWND m_hwndTooltip = nullptr;
    HINSTANCE m_hInstance = nullptr;
    HFONT m_font = nullptr;

    std::vector<TabItem> m_tabs;
    int m_activeTabId = -1;
    int m_nextTabId = 1;

    // DPI scaling
    int m_dpi = 96;
    int m_tabBarHeight = 30;
    int m_tabMinWidth = 50;
    int m_tabMaxWidth = 200;
    int m_tabPadding = 12;
    int m_closeBtnSize = 14;
    int m_closeBtnMargin = 6;
    int m_newTabBtnWidth = 28;

    // Mouse tracking state
    int m_hoveredTabId = -1;
    bool m_closeHovered = false;
    bool m_newTabHovered = false;
    bool m_trackingMouse = false;

    // Rename state
    int m_renamingTabId = -1;

    // Context menu tab
    int m_contextMenuTabId = -1;

    // Scroll state for many tabs
    int m_scrollOffset = 0;

    // Callback
    TabCallback m_callback;

    // Base layout constants (scaled by DPI)
    static constexpr int BASE_TAB_BAR_HEIGHT = 30;
    static constexpr int BASE_TAB_MIN_WIDTH = 50;
    static constexpr int BASE_TAB_MAX_WIDTH = 200;
    static constexpr int BASE_TAB_PADDING = 12;
    static constexpr int BASE_CLOSE_BTN_SIZE = 14;
    static constexpr int BASE_CLOSE_BTN_MARGIN = 6;
    static constexpr int BASE_NEW_TAB_BTN_WIDTH = 28;
    static constexpr int BASE_PIN_ICON_WIDTH = 14;

    // Timer IDs
    static constexpr UINT_PTR TOOLTIP_TIMER_ID = 1;
    static constexpr UINT TOOLTIP_DELAY_MS = 1000;  // 1 second

    // Tooltip tracking
    int m_tooltipTabId = -1;
    bool m_tooltipReady = false;  // True after 3s hover delay

    // Colors (light theme matching the app)
    static constexpr COLORREF CLR_BG          = RGB(255, 255, 255);
    static constexpr COLORREF CLR_TAB_ACTIVE  = RGB(255, 255, 255);
    static constexpr COLORREF CLR_TAB_INACTIVE= RGB(240, 240, 240);
    static constexpr COLORREF CLR_TAB_HOVER   = RGB(240, 240, 240);
    static constexpr COLORREF CLR_TEXT_ACTIVE  = RGB(30, 30, 30);
    static constexpr COLORREF CLR_TEXT_INACTIVE= RGB(100, 100, 100);
    static constexpr COLORREF CLR_MODIFIED     = RGB(0, 100, 200);
    static constexpr COLORREF CLR_CLOSE_HOVER  = RGB(200, 60, 60);
    static constexpr COLORREF CLR_CLOSE_X      = RGB(120, 120, 120);
    static constexpr COLORREF CLR_BORDER       = RGB(200, 200, 200);
    static constexpr COLORREF CLR_NEWTAB_HOVER = RGB(210, 210, 210);
    static constexpr COLORREF CLR_ACTIVE_LINE  = RGB(0, 122, 204);

    // Window class name
    static constexpr wchar_t TAB_CLASS[] = L"QNoteTabBar";
    static bool s_classRegistered;
};

} // namespace QNote
