//==============================================================================
// QNote - A Lightweight Notepad Clone
// MainWindowPrint.cpp - Page setup and printing
//==============================================================================

#include "MainWindow.h"
#include "resource.h"
#include <CommCtrl.h>
#include <Commdlg.h>
#include <ctime>

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
        text = m_editor->GetText();
        if (text.empty()) return;
        docName = m_isNewFile ? L"Untitled" : FileIO::GetFileName(m_currentFile);
    }
    const AppSettings& settings = m_settingsManager->GetSettings();

    // Show print preview dialog
    PrintSettings ps = PrintPreviewWindow::Show(
        m_hwnd, m_hInstance, text, docName,
        settings.fontName, settings.fontSize, settings.fontWeight, settings.fontItalic,
        m_pageSetup);

    if (!ps.accepted) return;

    // Persist margins from page 1's settings
    if (!ps.pageSettings.empty()) {
        m_pageSetup.rtMargin.left   = ps.pageSettings[0].marginLeft;
        m_pageSetup.rtMargin.top    = ps.pageSettings[0].marginTop;
        m_pageSetup.rtMargin.right  = ps.pageSettings[0].marginRight;
        m_pageSetup.rtMargin.bottom = ps.pageSettings[0].marginBottom;
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

    if (!PrintDlgW(&pd)) {
        if (pd.hDC) DeleteDC(pd.hDC);
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

    // Print each page with its own per-page settings
    for (int p = 0; p < totalPages; ++p) {
        if (StartPage(hdc) <= 0) break;

        const PageSettings& pg = ps.pageSettings[p];
        const auto& lines = ps.pageLines[p];

        int leftMarg   = MulDiv(pg.marginLeft,   dpiX, 1000);
        int topMarg    = MulDiv(pg.marginTop,    dpiY, 1000);
        int rightMarg  = MulDiv(pg.marginRight,  dpiX, 1000);
        int bottomMarg = MulDiv(pg.marginBottom, dpiY, 1000);
        int printW = printerPageW - leftMarg - rightMarg;
        int printH = printerPageH - topMarg  - bottomMarg;

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
        wcscpy_s(lf.lfFaceName, pg.fontName.c_str());

        HFONT hFont = CreateFontIndirectW(&lf);
        SelectObject(hdc, hFont);

        TEXTMETRICW metrics;
        GetTextMetricsW(hdc, &metrics);
        int lineHeight = metrics.tmHeight + metrics.tmExternalLeading;

        // Header (centered across full page width, in top margin)
        if (!pg.headerText.empty()) {
            std::wstring hdr = expandTokens(pg.headerText, p + 1);
            int hdrY = (topMarg - lineHeight) / 2;
            if (hdrY < 0) hdrY = 0;
            RECT rcH = { 0, hdrY, printerPageW, hdrY + lineHeight };
            DrawTextW(hdc, hdr.c_str(), static_cast<int>(hdr.length()),
                      &rcH, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        // Body lines
        int y = topMarg;
        for (const auto& lineText : lines) {
            TextOutW(hdc, leftMarg, y, lineText.c_str(),
                     static_cast<int>(lineText.length()));
            y += lineHeight;
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

        SelectObject(hdc, GetStockObject(SYSTEM_FONT));
        DeleteObject(hFont);
        EndPage(hdc);
    }

    // Cleanup
    EndDoc(hdc);
    DeleteDC(hdc);
    if (pd.hDevMode) GlobalFree(pd.hDevMode);
    if (pd.hDevNames) GlobalFree(pd.hDevNames);
}

} // namespace QNote
