//==============================================================================
// QNote - A Lightweight Notepad Clone
// MainWindowPrint.cpp - Page setup and printing
//==============================================================================

#include "MainWindow.h"
#include "resource.h"
#include <CommCtrl.h>
#include <Commdlg.h>
#include <ctime>
#include <algorithm>
#include <sstream>
#include <cmath>

namespace QNote {

//------------------------------------------------------------------------------
// File -> Page Setup
//------------------------------------------------------------------------------
void MainWindow::OnFilePageSetup() {
    // Initialize page setup dialog
    if (m_pageSetup.lStructSize == 0) {
        m_pageSetup.lStructSize = sizeof(m_pageSetup);
        m_pageSetup.hwndOwner = m_hwnd;
        m_pageSetup.Flags = PSD_DEFAULTMINMARGINS | PSD_MARGINS;
        
        // Default margins (in thousandths of inches)
        m_pageSetup.rtMargin.left = 1000;
        m_pageSetup.rtMargin.top = 1000;
        m_pageSetup.rtMargin.right = 1000;
        m_pageSetup.rtMargin.bottom = 1000;
    }
    
    PageSetupDlgW(&m_pageSetup);
}

//------------------------------------------------------------------------------
// Print Tab Selection dialog helpers
//------------------------------------------------------------------------------
struct PrintTabSelectionData {
    struct TabInfo {
        int tabId;
        std::wstring title;
        bool selected;
    };
    std::vector<TabInfo> tabs;
    int activeTabId;
};

static INT_PTR CALLBACK PrintTabSelectionDlgProc(HWND hwnd, UINT msg,
                                                  WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            auto* data = reinterpret_cast<PrintTabSelectionData*>(lParam);
            SetWindowLongPtrW(hwnd, DWLP_USER, reinterpret_cast<LONG_PTR>(data));

            HWND hList = GetDlgItem(hwnd, IDC_PT_LIST);
            ListView_SetExtendedListViewStyle(hList,
                LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);

            // Add a single column spanning the full width
            RECT rc;
            GetClientRect(hList, &rc);
            LVCOLUMNW col = {};
            col.mask = LVCF_WIDTH;
            col.cx = rc.right - rc.left - GetSystemMetrics(SM_CXVSCROLL);
            ListView_InsertColumn(hList, 0, &col);

            // Insert items
            for (int i = 0; i < static_cast<int>(data->tabs.size()); ++i) {
                LVITEMW lvi = {};
                lvi.mask = LVIF_TEXT;
                lvi.iItem = i;
                lvi.pszText = const_cast<LPWSTR>(data->tabs[i].title.c_str());
                ListView_InsertItem(hList, &lvi);
                // Pre-check the active tab
                if (data->tabs[i].tabId == data->activeTabId) {
                    ListView_SetCheckState(hList, i, TRUE);
                }
            }

            // Center dialog on parent
            HWND parent = GetParent(hwnd);
            if (parent) {
                RECT rcP, rcD;
                GetWindowRect(parent, &rcP);
                GetWindowRect(hwnd, &rcD);
                int x = rcP.left + ((rcP.right - rcP.left) - (rcD.right - rcD.left)) / 2;
                int y = rcP.top + ((rcP.bottom - rcP.top) - (rcD.bottom - rcD.top)) / 2;
                SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }

            return TRUE;
        }

        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            if (id == IDOK) {
                auto* data = reinterpret_cast<PrintTabSelectionData*>(
                    GetWindowLongPtrW(hwnd, DWLP_USER));
                HWND hList = GetDlgItem(hwnd, IDC_PT_LIST);
                bool anySelected = false;
                for (int i = 0; i < static_cast<int>(data->tabs.size()); ++i) {
                    data->tabs[i].selected = ListView_GetCheckState(hList, i) != 0;
                    if (data->tabs[i].selected) anySelected = true;
                }
                if (!anySelected) {
                    MessageBoxW(hwnd, L"Please select at least one tab to print.",
                                L"Print", MB_OK | MB_ICONINFORMATION);
                    return TRUE;
                }
                EndDialog(hwnd, IDOK);
                return TRUE;
            }
            if (id == IDCANCEL) {
                EndDialog(hwnd, IDCANCEL);
                return TRUE;
            }
            if (id == IDC_PT_SELECTALL) {
                HWND hList = GetDlgItem(hwnd, IDC_PT_LIST);
                int count = ListView_GetItemCount(hList);
                for (int i = 0; i < count; ++i)
                    ListView_SetCheckState(hList, i, TRUE);
                return TRUE;
            }
            if (id == IDC_PT_SELECTNONE) {
                HWND hList = GetDlgItem(hwnd, IDC_PT_LIST);
                int count = ListView_GetItemCount(hList);
                for (int i = 0; i < count; ++i)
                    ListView_SetCheckState(hList, i, FALSE);
                return TRUE;
            }
            break;
        }

        case WM_CLOSE:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

//------------------------------------------------------------------------------
// File -> Print (shows print preview first, then prints)
//------------------------------------------------------------------------------
void MainWindow::OnFilePrint() {
    // Initialize page setup if not done yet
    if (m_pageSetup.lStructSize == 0) {
        m_pageSetup.lStructSize = sizeof(m_pageSetup);
        m_pageSetup.hwndOwner = m_hwnd;
        m_pageSetup.Flags = PSD_DEFAULTMINMARGINS | PSD_MARGINS;
        m_pageSetup.rtMargin.left = 1000;
        m_pageSetup.rtMargin.top = 1000;
        m_pageSetup.rtMargin.right = 1000;
        m_pageSetup.rtMargin.bottom = 1000;
    }

    // Gather text and document name - allow multi-tab selection
    std::wstring text;
    std::wstring docName;
    bool hasSelection = false;

    // Check if current editor has a selection
    if (m_editor) {
        std::wstring selText = m_editor->GetSelectedText();
        if (!selText.empty()) {
            hasSelection = true;
        }
    }

    if (m_documentManager && m_documentManager->GetDocumentCount() > 1) {
        // Multiple tabs open - show tab selection dialog
        PrintTabSelectionData selData;
        selData.activeTabId = m_documentManager->GetActiveTabId();

        auto tabIds = m_documentManager->GetAllTabIds();
        for (int id : tabIds) {
            auto* doc = m_documentManager->GetDocument(id);
            if (doc) {
                PrintTabSelectionData::TabInfo info;
                info.tabId = id;
                info.title = doc->GetDisplayTitle();
                info.selected = false;
                selData.tabs.push_back(std::move(info));
            }
        }

        INT_PTR result = DialogBoxParamW(
            m_hInstance, MAKEINTRESOURCEW(IDD_PRINTTABS),
            m_hwnd, PrintTabSelectionDlgProc,
            reinterpret_cast<LPARAM>(&selData));

        if (result != IDOK) return;

        // Gather text from selected tabs
        int selectedCount = 0;
        for (const auto& tab : selData.tabs) {
            if (tab.selected) selectedCount++;
        }

        bool first = true;
        for (const auto& tab : selData.tabs) {
            if (!tab.selected) continue;

            auto* doc = m_documentManager->GetDocument(tab.tabId);
            if (!doc || !doc->editor) continue;

            // Save the active editor state before reading other tabs
            std::wstring tabText;
            if (tab.tabId == m_documentManager->GetActiveTabId()) {
                tabText = doc->editor->GetText();
            } else {
                // For non-active tabs, use the stored content
                tabText = doc->editor->GetText();
            }

            if (tabText.empty()) continue;

            if (!first) {
                text += L"\r\n";
            }

            text += tabText;
            first = false;
        }

        if (text.empty()) return;

        // Build combined document name
        if (selectedCount == 1) {
            for (const auto& tab : selData.tabs) {
                if (tab.selected) {
                    docName = tab.title;
                    break;
                }
            }
        } else {
            docName = std::to_wstring(selectedCount) + L" tabs";
        }
    } else {
        // Single tab - use current editor directly
        if (!m_editor) return;

        // If there's a selection, ask user whether to print selection or full document
        if (hasSelection) {
            int choice = MessageBoxW(m_hwnd,
                L"You have text selected. Do you want to print only the selected text?\n\n"
                L"Click Yes to print selection, No to print the full document.",
                L"Print",
                MB_YESNOCANCEL | MB_ICONQUESTION);
            if (choice == IDCANCEL) return;
            if (choice == IDYES) {
                text = m_editor->GetSelectedText();
            } else {
                text = m_editor->GetText();
                hasSelection = false;
            }
        } else {
            text = m_editor->GetText();
        }
        if (text.empty()) return;
        docName = m_isNewFile ? L"Untitled" : FileIO::GetFileName(m_currentFile);
    }
    const AppSettings& settings = m_settingsManager->GetSettings();

    // Show print preview dialog with persistent printer settings
    PrintSettings ps = PrintPreviewWindow::Show(
        m_hwnd, m_hInstance, text, docName,
        settings.fontName, settings.fontSize, settings.fontWeight, settings.fontItalic,
        m_pageSetup, hasSelection,
        settings.printQuality, settings.paperSource, settings.paperSize,
        settings.duplex, settings.pageFilter, settings.condensed, settings.formFeed);

    if (!ps.accepted) return;

    // Persist margins from page 1's settings
    if (!ps.pageSettings.empty()) {
        m_pageSetup.rtMargin.left   = ps.pageSettings[0].marginLeft;
        m_pageSetup.rtMargin.top    = ps.pageSettings[0].marginTop;
        m_pageSetup.rtMargin.right  = ps.pageSettings[0].marginRight;
        m_pageSetup.rtMargin.bottom = ps.pageSettings[0].marginBottom;
    }

    // Persist printer settings back to AppSettings
    {
        AppSettings& mutableSettings = m_settingsManager->GetSettings();
        mutableSettings.printQuality = ps.printQuality;
        mutableSettings.paperSource  = ps.paperSource;
        mutableSettings.paperSize    = ps.paperSize;
        mutableSettings.duplex       = ps.duplex;
        mutableSettings.pageFilter   = ps.pageFilter;
        mutableSettings.condensed    = ps.condensed;
        mutableSettings.formFeed     = ps.formFeed;
        (void)m_settingsManager->Save();
    }

    // Now show the system print dialog and print
    PRINTDLGW pd = {};
    pd.lStructSize = sizeof(pd);
    pd.hwndOwner = m_hwnd;
    pd.Flags = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE;
    pd.nCopies = 1;
    pd.nFromPage = 1;
    pd.nToPage = 1;
    pd.nMinPage = 1;
    pd.nMaxPage = 0xFFFF;

    // Apply printer settings (orientation, quality, paper, duplex, etc.)
    {
        PRINTDLGW pdQuery = {};
        pdQuery.lStructSize = sizeof(pdQuery);
        pdQuery.hwndOwner = m_hwnd;
        pdQuery.Flags = PD_RETURNDEFAULT;
        if (PrintDlgW(&pdQuery)) {
            if (pdQuery.hDevMode) {
                DEVMODEW* dm = reinterpret_cast<DEVMODEW*>(GlobalLock(pdQuery.hDevMode));
                if (dm) {
                    // Orientation
                    if (ps.landscape) {
                        dm->dmOrientation = DMORIENT_LANDSCAPE;
                        dm->dmFields |= DM_ORIENTATION;
                    }

                    // Print quality (Draft/Normal/High/Letter Quality)
                    switch (ps.printQuality) {
                        case 1:  // Draft
                            dm->dmPrintQuality = DMRES_DRAFT;
                            dm->dmFields |= DM_PRINTQUALITY;
                            break;
                        case 2:  // High
                            dm->dmPrintQuality = DMRES_HIGH;
                            dm->dmFields |= DM_PRINTQUALITY;
                            break;
                        case 3:  // Letter Quality
                            dm->dmPrintQuality = DMRES_HIGH;
                            dm->dmFields |= DM_PRINTQUALITY;
                            break;
                        default: // Normal (0) - use printer default
                            dm->dmPrintQuality = DMRES_MEDIUM;
                            dm->dmFields |= DM_PRINTQUALITY;
                            break;
                    }

                    // Paper source / bin
                    switch (ps.paperSource) {
                        case 1:  // Upper Tray
                            dm->dmDefaultSource = DMBIN_UPPER;
                            dm->dmFields |= DM_DEFAULTSOURCE;
                            break;
                        case 2:  // Lower Tray
                            dm->dmDefaultSource = DMBIN_LOWER;
                            dm->dmFields |= DM_DEFAULTSOURCE;
                            break;
                        case 3:  // Manual Feed
                            dm->dmDefaultSource = DMBIN_MANUAL;
                            dm->dmFields |= DM_DEFAULTSOURCE;
                            break;
                        case 4:  // Tractor Feed
                            dm->dmDefaultSource = DMBIN_TRACTOR;
                            dm->dmFields |= DM_DEFAULTSOURCE;
                            break;
                        case 5:  // Continuous
                            dm->dmDefaultSource = DMBIN_AUTO;
                            dm->dmFields |= DM_DEFAULTSOURCE;
                            break;
                        default: // Auto (0)
                            dm->dmDefaultSource = DMBIN_AUTO;
                            dm->dmFields |= DM_DEFAULTSOURCE;
                            break;
                    }

                    // Paper size
                    switch (ps.paperSize) {
                        case 1:  // Letter
                            dm->dmPaperSize = DMPAPER_LETTER;
                            dm->dmFields |= DM_PAPERSIZE;
                            break;
                        case 2:  // Legal
                            dm->dmPaperSize = DMPAPER_LEGAL;
                            dm->dmFields |= DM_PAPERSIZE;
                            break;
                        case 3:  // A4
                            dm->dmPaperSize = DMPAPER_A4;
                            dm->dmFields |= DM_PAPERSIZE;
                            break;
                        case 4:  // A5
                            dm->dmPaperSize = DMPAPER_A5;
                            dm->dmFields |= DM_PAPERSIZE;
                            break;
                        case 5:  // B5
                            dm->dmPaperSize = DMPAPER_B5;
                            dm->dmFields |= DM_PAPERSIZE;
                            break;
                        case 6:  // Envelope #10
                            dm->dmPaperSize = DMPAPER_ENV_10;
                            dm->dmFields |= DM_PAPERSIZE;
                            break;
                        case 7:  // Envelope DL
                            dm->dmPaperSize = DMPAPER_ENV_DL;
                            dm->dmFields |= DM_PAPERSIZE;
                            break;
                        case 8:  // Envelope C5
                            dm->dmPaperSize = DMPAPER_ENV_C5;
                            dm->dmFields |= DM_PAPERSIZE;
                            break;
                        case 9:  // Fanfold US (14.875" x 11")
                            dm->dmPaperSize = DMPAPER_FANFOLD_US;
                            dm->dmFields |= DM_PAPERSIZE;
                            break;
                        case 10: // Fanfold EU (8.5" x 12")
                            dm->dmPaperSize = DMPAPER_FANFOLD_STD_GERMAN;
                            dm->dmFields |= DM_PAPERSIZE;
                            break;
                        case 11: // Custom Continuous
                            dm->dmPaperSize = DMPAPER_USER;
                            dm->dmPaperWidth = 2159;   // 8.5" in tenths of mm
                            dm->dmPaperLength = 2794;  // 11" in tenths of mm (adjustable)
                            dm->dmFields |= DM_PAPERSIZE | DM_PAPERLENGTH | DM_PAPERWIDTH;
                            break;
                        default: // Printer default (0) - don't change
                            break;
                    }

                    // Duplex printing
                    switch (ps.duplex) {
                        case 1:  // Long Edge
                            dm->dmDuplex = DMDUP_VERTICAL;
                            dm->dmFields |= DM_DUPLEX;
                            break;
                        case 2:  // Short Edge
                            dm->dmDuplex = DMDUP_HORIZONTAL;
                            dm->dmFields |= DM_DUPLEX;
                            break;
                        default: // None (0)
                            dm->dmDuplex = DMDUP_SIMPLEX;
                            dm->dmFields |= DM_DUPLEX;
                            break;
                    }

                    GlobalUnlock(pdQuery.hDevMode);
                }
                pd.hDevMode = pdQuery.hDevMode;
                if (pdQuery.hDevNames) pd.hDevNames = pdQuery.hDevNames;
            } else {
                // hDevMode is null but hDevNames may have been allocated - free it
                if (pdQuery.hDevNames) GlobalFree(pdQuery.hDevNames);
            }
        }
    }

    if (!PrintDlgW(&pd)) {
        if (pd.hDC) DeleteDC(pd.hDC);
        if (pd.hDevMode) GlobalFree(pd.hDevMode);
        if (pd.hDevNames) GlobalFree(pd.hDevNames);
        return;
    }

    HDC hdc = pd.hDC;
    if (!hdc) {
        MessageBoxW(m_hwnd, L"Failed to create printer device context.", L"Print Error",
                    MB_OK | MB_ICONERROR);
        return;
    }

    // Set document info
    DOCINFOW di = {};
    di.cbSize = sizeof(di);
    di.lpszDocName = docName.c_str();

    if (StartDocW(hdc, &di) <= 0) {
        MessageBoxW(m_hwnd, L"Failed to start print job.", L"Print Error",
                    MB_OK | MB_ICONERROR);
        DeleteDC(hdc);
        if (pd.hDevMode) GlobalFree(pd.hDevMode);
        if (pd.hDevNames) GlobalFree(pd.hDevNames);
        return;
    }

    int printerPageW = GetDeviceCaps(hdc, HORZRES);
    int printerPageH = GetDeviceCaps(hdc, VERTRES);
    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
    int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);

    int totalPages = static_cast<int>(ps.pageLines.size());

    // Determine which pages to print based on page range
    std::vector<int> pagesToPrint;
    if (ps.pageRange.empty()) {
        for (int i = 0; i < totalPages; ++i) pagesToPrint.push_back(i);
    } else {
        // Parse range string like "1-3,5,7-9"
        std::wistringstream ss(ps.pageRange);
        std::wstring token;
        while (std::getline(ss, token, L',')) {
            size_t s = token.find_first_not_of(L" \t");
            size_t e = token.find_last_not_of(L" \t");
            if (s == std::wstring::npos) continue;
            token = token.substr(s, e - s + 1);
            size_t dash = token.find(L'-');
            if (dash != std::wstring::npos) {
                int from = _wtoi(token.substr(0, dash).c_str());
                int to   = _wtoi(token.substr(dash + 1).c_str());
                if (from < 1) from = 1;
                if (to > totalPages) to = totalPages;
                for (int i = from; i <= to; ++i) pagesToPrint.push_back(i - 1);
            } else {
                int p = _wtoi(token.c_str());
                if (p >= 1 && p <= totalPages) pagesToPrint.push_back(p - 1);
            }
        }
        std::sort(pagesToPrint.begin(), pagesToPrint.end());
        pagesToPrint.erase(std::unique(pagesToPrint.begin(), pagesToPrint.end()),
                           pagesToPrint.end());
        if (pagesToPrint.empty()) {
            for (int i = 0; i < totalPages; ++i) pagesToPrint.push_back(i);
        }
    }

    // Apply page filter (Odd/Even pages only)
    if (ps.pageFilter == 1) {
        // Odd pages only (1, 3, 5, ...)
        std::vector<int> filtered;
        for (int p : pagesToPrint) {
            if ((p + 1) % 2 == 1) filtered.push_back(p);  // p is 0-indexed, page 1 is odd
        }
        pagesToPrint = std::move(filtered);
    } else if (ps.pageFilter == 2) {
        // Even pages only (2, 4, 6, ...)
        std::vector<int> filtered;
        for (int p : pagesToPrint) {
            if ((p + 1) % 2 == 0) filtered.push_back(p);
        }
        pagesToPrint = std::move(filtered);
    }

    if (pagesToPrint.empty()) {
        MessageBoxW(m_hwnd, L"No pages match the selected page filter.", L"Print",
                    MB_OK | MB_ICONINFORMATION);
        DeleteDC(hdc);
        if (pd.hDevMode) GlobalFree(pd.hDevMode);
        if (pd.hDevNames) GlobalFree(pd.hDevNames);
        return;
    }

    // Token expansion for headers/footers
    auto expandTokens = [&](const std::wstring& tmpl, int pageNum) -> std::wstring {
        std::wstring result;
        result.reserve(tmpl.size() + 32);
        for (size_t i = 0; i < tmpl.size(); ++i) {
            if (tmpl[i] == L'&' && i + 1 < tmpl.size()) {
                wchar_t tok = tmpl[++i];
                switch (tok) {
                    case L'f': result += docName; break;
                    case L'p': result += std::to_wstring(pageNum); break;
                    case L'P': result += std::to_wstring(totalPages); break;
                    case L'd': {
                        time_t t = time(nullptr); struct tm lt; localtime_s(&lt, &t);
                        wchar_t db[32]; wcsftime(db, 32, L"%m/%d/%Y", &lt);
                        result += db; break;
                    }
                    case L't': {
                        time_t t = time(nullptr); struct tm lt; localtime_s(&lt, &t);
                        wchar_t tb[32]; wcsftime(tb, 32, L"%I:%M %p", &lt);
                        result += tb; break;
                    }
                    default: result += L'&'; result += tok; break;
                }
            } else {
                result += tmpl[i];
            }
        }
        return result;
    };

    // Handle multiple copies with collation
    int numCopies = ps.copies;
    if (numCopies < 1) numCopies = 1;
    if (numCopies > 99) numCopies = 99;
    bool collate = ps.collate;

    // Build the print order based on collation
    // Collated:     1,2,3, 1,2,3, 1,2,3 (complete set, then repeat)
    // Uncollated:   1,1,1, 2,2,2, 3,3,3 (each page repeated, then next)
    std::vector<int> printOrder;
    if (collate) {
        for (int copy = 0; copy < numCopies; ++copy) {
            for (int pi = 0; pi < static_cast<int>(pagesToPrint.size()); ++pi) {
                printOrder.push_back(pagesToPrint[pi]);
            }
        }
    } else {
        for (int pi = 0; pi < static_cast<int>(pagesToPrint.size()); ++pi) {
            for (int copy = 0; copy < numCopies; ++copy) {
                printOrder.push_back(pagesToPrint[pi]);
            }
        }
    }

    // Print each page in order
    for (int i = 0; i < static_cast<int>(printOrder.size()); ++i) {
        int p = printOrder[i];
        if (StartPage(hdc) <= 0) break;

        const PageSettings& pg = ps.pageSettings[p];
        const auto& lines = ps.pageLines[p];

        int leftMarg   = MulDiv(pg.marginLeft,   dpiX, 1000);
        int topMarg    = MulDiv(pg.marginTop,    dpiY, 1000);
        int rightMarg  = MulDiv(pg.marginRight,  dpiX, 1000);
        int bottomMarg = MulDiv(pg.marginBottom, dpiY, 1000);

        // Gutter margin (extra inside margin for binding)
        int gutterMarg = MulDiv(pg.gutterMil, dpiX, 1000);

        int printW = printerPageW - leftMarg - rightMarg - gutterMarg;
        int printH = printerPageH - topMarg  - bottomMarg;

        // Calculate border padding for text area
        int padPx = 0;
        if (pg.borderEnabled && pg.borderPaddingPt > 0) {
            padPx = MulDiv(pg.borderPaddingPt, dpiY, 72);
        }
        int leftMargText   = leftMarg   + padPx + gutterMarg;
        int topMargText    = topMarg    + padPx;
        int rightMargText  = rightMarg  + padPx;
        int bottomMargText = bottomMarg + padPx;
        int printWText = printerPageW - leftMargText - rightMargText;
        int printHText = printerPageH - topMargText  - bottomMargText;

        // Create font for this page
        LOGFONTW lf = {};
        lf.lfHeight = -MulDiv(pg.fontSize, dpiY, 72);
        lf.lfWeight = pg.fontWeight;
        lf.lfItalic = pg.fontItalic ? TRUE : FALSE;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
        lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        lf.lfQuality = DEFAULT_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;

        // Condensed mode: use a narrower font width to fit more chars per line
        if (ps.condensed) {
            // Set width to ~60% of normal for condensed printing
            // This is especially useful for dot matrix printers
            LOGFONTW tempLf = lf;
            tempLf.lfWidth = 0;
            wcscpy_s(tempLf.lfFaceName, pg.fontName.c_str());
            HFONT hTempFont = CreateFontIndirectW(&tempLf);
            SelectObject(hdc, hTempFont);
            TEXTMETRICW tempMetrics;
            GetTextMetricsW(hdc, &tempMetrics);
            lf.lfWidth = MulDiv(tempMetrics.tmAveCharWidth, 60, 100);
            if (lf.lfWidth < 1) lf.lfWidth = 1;
            SelectObject(hdc, GetStockObject(SYSTEM_FONT));
            DeleteObject(hTempFont);
        }

        wcscpy_s(lf.lfFaceName, pg.fontName.c_str());

        HFONT hFont = CreateFontIndirectW(&lf);
        SelectObject(hdc, hFont);

        TEXTMETRICW metrics;
        GetTextMetricsW(hdc, &metrics);
        int lineHeight = metrics.tmHeight + metrics.tmExternalLeading;

        // Apply line spacing multiplier
        int spacedLineHeight = MulDiv(lineHeight, pg.lineSpacing, 100);
        if (spacedLineHeight < lineHeight) spacedLineHeight = lineHeight;

        // Calculate line number gutter width
        int gutterWidth = 0;
        if (pg.lineNumbers) {
            SIZE gutterSz;
            GetTextExtentPoint32W(hdc, L"99999 ", 6, &gutterSz);
            gutterWidth = gutterSz.cx;
        }

        // Create line number font (use custom or fallback to body font)
        HFONT hLineNumFont = nullptr;
        if (pg.lineNumbers) {
            LOGFONTW lfNum = {};
            if (pg.lineNumberFontName.empty() || pg.lineNumberFontSize == 0) {
                lfNum = lf;
            } else {
                lfNum.lfHeight = -MulDiv(pg.lineNumberFontSize, dpiY, 72);
                lfNum.lfWeight = pg.lineNumberFontWeight ? pg.lineNumberFontWeight : FW_NORMAL;
                lfNum.lfItalic = pg.lineNumberFontItalic ? TRUE : FALSE;
                lfNum.lfCharSet = DEFAULT_CHARSET;
                lfNum.lfOutPrecision = OUT_DEFAULT_PRECIS;
                lfNum.lfClipPrecision = CLIP_DEFAULT_PRECIS;
                lfNum.lfQuality = DEFAULT_QUALITY;
                lfNum.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
                wcscpy_s(lfNum.lfFaceName, pg.lineNumberFontName.c_str());
            }
            hLineNumFont = CreateFontIndirectW(&lfNum);
        }

        // Header (centered across full page width, in top margin)
        if (!pg.headerText.empty()) {
            std::wstring hdr = expandTokens(pg.headerText, p + 1);
            int hdrY = (topMarg - lineHeight) / 2;
            if (hdrY < 0) hdrY = 0;
            RECT rcH = { 0, hdrY, printerPageW, hdrY + lineHeight };
            DrawTextW(hdc, hdr.c_str(), static_cast<int>(hdr.length()),
                      &rcH, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        // Watermark (behind body text)
        if (pg.watermarkEnabled && !pg.watermarkText.empty()) {
            // Font height: use custom size if set, otherwise auto-size
            int wmFontH;
            if (pg.watermarkFontSize > 0) {
                wmFontH = -MulDiv(pg.watermarkFontSize, dpiY, 72);
            } else {
                wmFontH = -(std::min)(printW, printH) / 2;
                if (wmFontH > -20) wmFontH = -20;
            }

            // Calculate diagonal angle based on page aspect ratio
            double angle = atan2(static_cast<double>(printH), static_cast<double>(printW));
            int escapement = static_cast<int>(angle * 1800.0 / 3.14159265);

            LOGFONTW wmLf = {};
            wmLf.lfHeight = wmFontH;
            wmLf.lfWeight = pg.watermarkFontWeight;
            wmLf.lfItalic = pg.watermarkFontItalic ? TRUE : FALSE;
            wmLf.lfCharSet = DEFAULT_CHARSET;
            wmLf.lfEscapement = escapement;
            wmLf.lfOrientation = escapement;
            wmLf.lfQuality = DEFAULT_QUALITY;
            wcscpy_s(wmLf.lfFaceName, pg.watermarkFontName.c_str());
            HFONT hWmFont = CreateFontIndirectW(&wmLf);
            HFONT hPrevFont = static_cast<HFONT>(SelectObject(hdc, hWmFont));

            // Center of printable area
            int cx = leftMarg + printW / 2;
            int cy = topMarg + printH / 2;

            UINT oldAlign = SetTextAlign(hdc, TA_CENTER | TA_BASELINE);
            SetTextColor(hdc, pg.watermarkColor);
            TextOutW(hdc, cx, cy, pg.watermarkText.c_str(),
                     static_cast<int>(pg.watermarkText.length()));
            SetTextAlign(hdc, oldAlign);

            SelectObject(hdc, hPrevFont);
            DeleteObject(hWmFont);
            // Restore font and color for body text
            SelectObject(hdc, hFont);
            SetTextColor(hdc, RGB(0, 0, 0));
        }

        // Body lines with column support
        int numCols = pg.columns;
        if (numCols < 1) numCols = 1;
        if (numCols > 3) numCols = 3;

        int colGapPx = MulDiv(pg.columnGapPt, dpiX, 72);
        int columnWidth = printWText - gutterWidth;
        if (numCols > 1) {
            int totalGapPx = (numCols - 1) * colGapPx;
            columnWidth = (printWText - gutterWidth - totalGapPx) / numCols;
            if (columnWidth < 50) columnWidth = 50;
        }

        int linesPerColumn = printHText / spacedLineHeight;
        if (linesPerColumn < 1) linesPerColumn = 1;

        int lineNum = (p < static_cast<int>(ps.pageFirstLineNumber.size()))
                      ? ps.pageFirstLineNumber[p] : 1;
        int lineIdx = 0;
        int totalLines = static_cast<int>(lines.size());

        for (int col = 0; col < numCols && lineIdx < totalLines; ++col) {
            int colX = leftMargText + gutterWidth + col * (columnWidth + colGapPx);
            int y = topMargText;

            for (int row = 0; row < linesPerColumn && lineIdx < totalLines; ++row, ++lineIdx) {
                const auto& lineText = lines[lineIdx];

                // Draw line number (only for first column)
                if (pg.lineNumbers && gutterWidth > 0 && col == 0) {
                    wchar_t numBuf[16];
                    swprintf_s(numBuf, L"%d", lineNum);
                    if (hLineNumFont) SelectObject(hdc, hLineNumFont);
                    SetTextColor(hdc, pg.lineNumberColor);
                    RECT rcNum = { leftMargText, y, leftMargText + gutterWidth - 4, y + lineHeight };
                    DrawTextW(hdc, numBuf, -1, &rcNum, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);
                    SetTextColor(hdc, RGB(0, 0, 0));
                    if (hLineNumFont) SelectObject(hdc, hFont);
                }

                TextOutW(hdc, colX, y, lineText.c_str(),
                         static_cast<int>(lineText.length()));
                y += spacedLineHeight;
                lineNum++;
            }
        }

        // Footer (in bottom margin)
        int ftrY = topMarg + printH + (bottomMarg - lineHeight) / 2;
        if (ftrY + lineHeight > printerPageH) ftrY = printerPageH - lineHeight;

        if (!pg.footerLeft.empty()) {
            std::wstring fL = expandTokens(pg.footerLeft, p + 1);
            RECT rcFL = { leftMarg, ftrY, leftMarg + printW, ftrY + lineHeight };
            DrawTextW(hdc, fL.c_str(), static_cast<int>(fL.length()),
                      &rcFL, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
        }
        if (!pg.footerRight.empty()) {
            std::wstring fR = expandTokens(pg.footerRight, p + 1);
            RECT rcFR = { leftMarg, ftrY, leftMarg + printW, ftrY + lineHeight };
            DrawTextW(hdc, fR.c_str(), static_cast<int>(fR.length()),
                      &rcFR, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);
        }

        // Page border
        if (pg.borderEnabled) {
            int borderPx = MulDiv(pg.borderWidthPt, dpiY, 72);
            if (borderPx < 1) borderPx = 1;

            int penStyle = PS_SOLID;
            switch (pg.borderStyle) {
                case 1: penStyle = PS_DASH; break;
                case 2: penStyle = PS_DOT; break;
                case 3: penStyle = PS_SOLID; break;
            }

            HPEN hBorderPen = CreatePen(penStyle, borderPx, pg.borderColor);
            HPEN hOldBorderPen = static_cast<HPEN>(SelectObject(hdc, hBorderPen));

            int bx1 = leftMarg;
            int by1 = topMarg;
            int bx2 = printerPageW - rightMarg;
            int by2 = printerPageH - bottomMarg;

            if (pg.borderTop)    { MoveToEx(hdc, bx1, by1, nullptr); LineTo(hdc, bx2, by1); }
            if (pg.borderBottom) { MoveToEx(hdc, bx1, by2, nullptr); LineTo(hdc, bx2, by2); }
            if (pg.borderLeft)   { MoveToEx(hdc, bx1, by1, nullptr); LineTo(hdc, bx1, by2); }
            if (pg.borderRight)  { MoveToEx(hdc, bx2, by1, nullptr); LineTo(hdc, bx2, by2); }

            // Double border: draw inner parallel line
            if (pg.borderStyle == 3) {
                int gap = borderPx * 2;
                if (gap < 2) gap = 2;
                if (pg.borderTop)    { MoveToEx(hdc, bx1 + gap, by1 + gap, nullptr); LineTo(hdc, bx2 - gap, by1 + gap); }
                if (pg.borderBottom) { MoveToEx(hdc, bx1 + gap, by2 - gap, nullptr); LineTo(hdc, bx2 - gap, by2 - gap); }
                if (pg.borderLeft)   { MoveToEx(hdc, bx1 + gap, by1 + gap, nullptr); LineTo(hdc, bx1 + gap, by2 - gap); }
                if (pg.borderRight)  { MoveToEx(hdc, bx2 - gap, by1 + gap, nullptr); LineTo(hdc, bx2 - gap, by2 - gap); }
            }

            SelectObject(hdc, hOldBorderPen);
            DeleteObject(hBorderPen);
        }

        SelectObject(hdc, GetStockObject(SYSTEM_FONT));
        if (hLineNumFont) DeleteObject(hLineNumFont);
        DeleteObject(hFont);
        EndPage(hdc);
    }

    // Send form feed after job if requested (useful for dot matrix / continuous feed)
    if (ps.formFeed) {
        // Start an extra blank page which triggers form feed on the printer
        if (StartPage(hdc) > 0) {
            EndPage(hdc);
        }
    }

    // Cleanup
    EndDoc(hdc);
    DeleteDC(hdc);
    if (pd.hDevMode) GlobalFree(pd.hDevMode);
    if (pd.hDevNames) GlobalFree(pd.hDevNames);
}

} // namespace QNote
