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

    // Gutter margin (extra inside margin) in thousandths of an inch
    int gutterMil = 0;

    // Header/footer text (substitution tokens: &f, &p, &P, &d, &t)
    std::wstring headerText;
    std::wstring footerLeft;
    std::wstring footerRight = L"Page &p of &P";

    // Font
    std::wstring fontName;
    int          fontSize   = 11;
    int          fontWeight = FW_NORMAL;
    bool         fontItalic = false;

    // Columns
    int columns     = 1;        // 1, 2, or 3 columns
    int columnGapPt = 12;       // Gap between columns in points

    // Border
    bool     borderEnabled   = false;
    int      borderStyle     = 0;     // 0=Solid, 1=Dash, 2=Dot, 3=Double
    int      borderWidthPt   = 1;     // Width in points (1-10)
    int      borderPaddingPt = 0;     // Padding between border and text in points
    bool     borderTop       = true;
    bool     borderBottom    = true;
    bool     borderLeft      = true;
    bool     borderRight     = true;
    COLORREF borderColor     = RGB(0, 0, 0);

    // Line numbers
    bool         lineNumbers         = false;
    COLORREF     lineNumberColor     = RGB(140, 140, 140);
    std::wstring lineNumberFontName;  // Empty = use body font
    int          lineNumberFontSize  = 0;   // 0 = use body font size
    int          lineNumberFontWeight = FW_NORMAL;
    bool         lineNumberFontItalic = false;

    // Line spacing multiplier (100 = 1.0x, 115 = 1.15x, 150 = 1.5x, 200 = 2.0x)
    int lineSpacing = 100;

    // Watermark
    bool         watermarkEnabled    = false;
    std::wstring watermarkText       = L"DRAFT";
    COLORREF     watermarkColor      = RGB(220, 220, 220);
    std::wstring watermarkFontName   = L"Arial";
    int          watermarkFontSize   = 0;          // 0 = auto-size
    int          watermarkFontWeight = FW_BOLD;
    bool         watermarkFontItalic = false;
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

    // Orientation: false=Portrait, true=Landscape
    bool landscape = false;

    // Page range: empty = all, otherwise e.g. "1-3,5"
    std::wstring pageRange;

    // First original line number for each page (for line number printing)
    std::vector<int> pageFirstLineNumber;

    // Print options
    int  copies  = 1;      // Number of copies (1-99)
    bool collate = true;   // Collate copies (1,2,3,1,2,3 vs 1,1,2,2,3,3)

    // Printer settings (office/dot matrix features)
    int  printQuality = 0;    // 0=Normal, 1=Draft, 2=High, 3=Letter Quality
    int  paperSource  = 0;    // 0=Auto, 1=Upper, 2=Lower, 3=Manual, 4=Tractor, 5=Continuous
    int  paperSize    = 0;    // 0=Default, 1=Letter, 2=Legal, 3=A4, 4=A5, 5=B5,
                              // 6=Env#10, 7=EnvDL, 8=EnvC5, 9=Fanfold US, 10=Fanfold EU,
                              // 11=Custom Continuous
    int  duplex       = 0;    // 0=None, 1=Long Edge, 2=Short Edge
    int  pageFilter   = 0;    // 0=All, 1=Odd Only, 2=Even Only
    bool condensed    = false; // Condensed/compressed mode (more chars per line)
    bool formFeed     = false; // Send form feed after job
    bool isDotMatrix  = false; // Auto-detected: true if printer is dot matrix/impact
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
                              const PAGESETUPDLGW& pageSetup,
                              bool hasSelection = false,
                              int defaultPrintQuality = 0,
                              int defaultPaperSource = 0,
                              int defaultPaperSize = 0,
                              int defaultDuplex = 0,
                              int defaultPageFilter = 0,
                              bool defaultCondensed = false,
                              bool defaultFormFeed = false);

private:
    // Dialog procedure
    static INT_PTR CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);

    // Message handlers
    void OnInit(HWND hwnd);
    void OnDrawItem(DRAWITEMSTRUCT* dis);
    void OnCommand(WORD id, WORD code);
    void OnSpinDelta(int editId, int delta);
    void OnMouseWheel(int delta);
    void OnMouseWheelZoom(int delta);
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
    void UpdateBorderControlStates();
    void OnBorderColorPick();
    void UpdateWatermarkControlStates();
    void OnWatermarkColorPick();
    void OnWatermarkChooseFont();
    void UpdateWatermarkFontLabel();
    void OnOrientationChange();
    void UpdateColumnControlStates();
    void OnSaveSettings();
    void OnLoadSettings();
    void OnBorderMore();
    void UpdateLineNumberControlStates();
    void OnLineNumberColorPick();
    void OnLineNumberChooseFont();
    void UpdateLineNumberFontLabel();
    void OnAddFiles();
    void DetectPrinterType();
    void UpdatePrinterControlStates();
    std::vector<int> ParsePageRange(const std::wstring& range, int totalPages) const;

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
    double m_zoom      = 1.0;  // Preview zoom factor (0.25 to 4.0)

    // Paper dimensions (thousandths of an inch)
    int m_pageWidthMil  = 8500;
    int m_pageHeightMil = 11000;
    int m_origPageWidthMil  = 8500;
    int m_origPageHeightMil = 11000;
    bool m_landscape = false;

    // Whether a text selection was provided
    bool m_hasSelection = false;

    // First original line number for each page (tracks wrapping)
    std::vector<int> m_pageFirstLineNum;

    // Suppress control-change feedback during programmatic updates
    bool m_suppressSync = false;

    // Custom colors for border color picker
    COLORREF m_customColors[16] = {};

    // Custom colors for watermark color picker
    COLORREF m_watermarkCustomColors[16] = {};

    // Custom colors for line number color picker
    COLORREF m_lineNumCustomColors[16] = {};

    // Print options
    int  m_copies  = 1;
    bool m_collate = true;

    // Printer settings
    int  m_printQuality = 0;   // 0=Normal, 1=Draft, 2=High, 3=Letter Quality
    int  m_paperSource  = 0;   // 0=Auto, 1=Upper, 2=Lower, 3=Manual, 4=Tractor, 5=Continuous
    int  m_paperSize    = 0;   // 0=Default
    int  m_duplex       = 0;   // 0=None
    int  m_pageFilter   = 0;   // 0=All
    bool m_condensed    = false;
    bool m_formFeed     = false;
    bool m_isDotMatrix  = false;

    // Static instance for dialog-proc callback
    static PrintPreviewWindow* s_instance;
};

} // namespace QNote
