//==============================================================================
// QNote - A Lightweight Notepad Clone
// PrintPreviewWindow.h - Print preview dialog with per-page styling
//==============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Commdlg.h>
#include <string>
#include <vector>

namespace QNote {

//------------------------------------------------------------------------------
// Per-page settings (margins, header/footer, font)
//------------------------------------------------------------------------------
struct PageSettings {
    // Margins in thousandths of an inch
    int marginLeft   = 1000;
    int marginRight  = 1000;
    int marginTop    = 1000;
    int marginBottom = 1000;

    // Header/footer text (substitution tokens: &f, &p, &P, &d, &t)
    std::wstring headerText;
    std::wstring footerLeft;
    std::wstring footerRight = L"Page &p of &P";

    // Font
    std::wstring fontName;
    int          fontSize   = 11;
    int          fontWeight = FW_NORMAL;
    bool         fontItalic = false;
};

//------------------------------------------------------------------------------
// Print settings returned from the preview dialog
//------------------------------------------------------------------------------
struct PrintSettings {
    // Per-page settings (margins, header/footer, font)
    std::vector<PageSettings> pageSettings;

    // Paginated text lines per page
    std::vector<std::vector<std::wstring>> pageLines;

    // Paper size (thousandths of an inch)
    int pageWidthMil  = 8500;
    int pageHeightMil = 11000;

    // Set to true when user clicks Print
    bool accepted = false;
};

//------------------------------------------------------------------------------
// Print preview window - shows all pages in a scrollable preview with
// per-page margin, header/footer, and font controls.
//------------------------------------------------------------------------------
class PrintPreviewWindow {
public:
    PrintPreviewWindow() = default;
    ~PrintPreviewWindow() = default;

    PrintPreviewWindow(const PrintPreviewWindow&) = delete;
    PrintPreviewWindow& operator=(const PrintPreviewWindow&) = delete;

    // Show the print preview dialog (modal).
    static PrintSettings Show(HWND parent, HINSTANCE hInstance,
                              const std::wstring& text,
                              const std::wstring& docName,
                              const std::wstring& fontName,
                              int fontSize, int fontWeight, bool fontItalic,
                              const PAGESETUPDLGW& pageSetup);

private:
    // Dialog procedure
    static INT_PTR CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);

    // Message handlers
    void OnInit(HWND hwnd);
    void OnDrawItem(DRAWITEMSTRUCT* dis);
    void OnCommand(WORD id, WORD code);
    void OnSpinDelta(int editId, int delta);
    void OnMouseWheel(int delta);
    void OnPreviewClick();

    // Page management
    void Paginate();
    void SyncControlsToPage(int idx);
    void SyncPageFromControls();
    void SelectPage(int idx);
    void ScrollToPage(int idx);
    void ApplyToAllPages();
    void UpdatePageInfo();
    void InvalidatePreview();
    void UpdateFontLabel();
    void OnChooseFont();

    // Helpers
    std::wstring ExpandTokens(const std::wstring& tmpl, int pageNum, int totalPages) const;
    void DrawSinglePage(HDC hdc, int pageIdx, int px, int py, int pw, int ph,
                        double scaleX, double scaleY, bool selected, int totalPages);

    // Preview layout helper (avoids duplicating the sizing computation)
    struct PreviewLayout {
        int pageW = 0, pageH = 0, gap = 20;
        int contentX = 0;      // page column x-offset relative to control origin
        int totalH = 0;        // total content height in pixels
        int maxScroll = 0;     // maximum scroll offset
    };
    PreviewLayout GetPreviewLayout() const;

private:
    HWND        m_hwnd      = nullptr;
    HINSTANCE   m_hInstance  = nullptr;

    // Input data
    std::wstring m_text;
    std::wstring m_docName;

    // Per-page settings
    std::vector<PageSettings> m_pageSettings;

    // Paginated lines (each page is a vector of text lines)
    struct PageData { std::vector<std::wstring> lines; };
    std::vector<PageData> m_pages;

    int m_selectedPage = 0;
    int m_scrollY      = 0;

    // Paper dimensions (thousandths of an inch)
    int m_pageWidthMil  = 8500;
    int m_pageHeightMil = 11000;

    // Suppress control-change feedback during programmatic updates
    bool m_suppressSync = false;

    // Static instance for dialog-proc callback
    static PrintPreviewWindow* s_instance;
};

} // namespace QNote
