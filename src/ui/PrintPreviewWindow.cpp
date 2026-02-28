//==============================================================================
// QNote - A Lightweight Notepad Clone
// PrintPreviewWindow.cpp - Print preview dialog implementation
//==============================================================================

#include "PrintPreviewWindow.h"
#include "FileIO.h"
#include "resource.h"
#include <CommCtrl.h>
#include <Commdlg.h>
#include <algorithm>
#include <sstream>
#include <ctime>
#include <cstdio>
#include <cmath>

namespace QNote {

// Static instance pointer for dialog callback
PrintPreviewWindow* PrintPreviewWindow::s_instance = nullptr;

//------------------------------------------------------------------------------
// Show print preview dialog (static entry point)
//------------------------------------------------------------------------------
PrintSettings PrintPreviewWindow::Show(HWND parent, HINSTANCE hInstance,
                                       const std::wstring& text,
                                       const std::wstring& docName,
                                       const std::wstring& fontName,
                                       int fontSize, int fontWeight, bool fontItalic,
                                       const PAGESETUPDLGW& pageSetup,
                                       bool hasSelection,
                                       int defaultPrintQuality,
                                       int defaultPaperSource,
                                       int defaultPaperSize,
                                       int defaultDuplex,
                                       int defaultPageFilter,
                                       bool defaultCondensed,
                                       bool defaultFormFeed) {
    PrintPreviewWindow instance;
    instance.m_hInstance = hInstance;
    instance.m_text      = text;
    instance.m_docName   = docName;
    instance.m_hasSelection = hasSelection;

    // Load persistent printer settings as defaults
    instance.m_printQuality = defaultPrintQuality;
    instance.m_paperSource  = defaultPaperSource;
    instance.m_paperSize    = defaultPaperSize;
    instance.m_duplex       = defaultDuplex;
    instance.m_pageFilter   = defaultPageFilter;
    instance.m_condensed    = defaultCondensed;
    instance.m_formFeed     = defaultFormFeed;

    // Seed the initial page-0 settings from editor font + page-setup margins
    PageSettings base;
    base.marginLeft   = pageSetup.rtMargin.left   ? pageSetup.rtMargin.left   : 1000;
    base.marginRight  = pageSetup.rtMargin.right  ? pageSetup.rtMargin.right  : 1000;
    base.marginTop    = pageSetup.rtMargin.top    ? pageSetup.rtMargin.top    : 1000;
    base.marginBottom = pageSetup.rtMargin.bottom ? pageSetup.rtMargin.bottom : 1000;
    base.fontName   = fontName;
    base.fontSize   = fontSize;
    base.fontWeight = fontWeight;
    base.fontItalic = fontItalic;
    instance.m_pageSettings.push_back(base);

    // Try to obtain real paper size from the default printer
    PRINTDLGW pd = {};
    pd.lStructSize = sizeof(pd);
    pd.hwndOwner = parent;
    pd.Flags = PD_RETURNDEFAULT | PD_RETURNDC;
    if (PrintDlgW(&pd)) {
        if (pd.hDC) {
            int dpiX = GetDeviceCaps(pd.hDC, LOGPIXELSX);
            int dpiY = GetDeviceCaps(pd.hDC, LOGPIXELSY);
            int pxW  = GetDeviceCaps(pd.hDC, PHYSICALWIDTH);
            int pxH  = GetDeviceCaps(pd.hDC, PHYSICALHEIGHT);
            if (dpiX > 0 && dpiY > 0 && pxW > 0 && pxH > 0) {
                instance.m_pageWidthMil  = MulDiv(pxW, 1000, dpiX);
                instance.m_pageHeightMil = MulDiv(pxH, 1000, dpiY);
            }
            DeleteDC(pd.hDC);
        }
        if (pd.hDevMode)  GlobalFree(pd.hDevMode);
        if (pd.hDevNames) GlobalFree(pd.hDevNames);
    }

    // Store original paper size for orientation swapping
    instance.m_origPageWidthMil  = instance.m_pageWidthMil;
    instance.m_origPageHeightMil = instance.m_pageHeightMil;

    s_instance = &instance;

    INT_PTR result = DialogBoxParamW(
        hInstance,
        MAKEINTRESOURCEW(IDD_PRINTPREVIEW),
        parent,
        DlgProc,
        0
    );

    s_instance = nullptr;

    // Build the PrintSettings result
    PrintSettings ps;
    if (result == IDOK) {
        ps.accepted = true;
        ps.pageSettings  = std::move(instance.m_pageSettings);
        ps.pageWidthMil  = instance.m_pageWidthMil;
        ps.pageHeightMil = instance.m_pageHeightMil;
        ps.landscape     = instance.m_landscape;
        ps.pageFirstLineNumber = std::move(instance.m_pageFirstLineNum);
        ps.pageLines.resize(instance.m_pages.size());
        for (size_t i = 0; i < instance.m_pages.size(); ++i)
            ps.pageLines[i] = std::move(instance.m_pages[i].lines);

        // Read page range
        wchar_t rangeBuf[128] = {};
        GetDlgItemTextW(instance.m_hwnd, IDC_PP_RANGE_EDIT, rangeBuf, 128);
        bool rangeAll = IsDlgButtonChecked(instance.m_hwnd, IDC_PP_RANGE_ALL) == BST_CHECKED;
        if (!rangeAll) {
            ps.pageRange = rangeBuf;
        }

        // Read print options
        ps.copies  = instance.m_copies;
        ps.collate = instance.m_collate;

        // Read printer settings
        ps.printQuality = instance.m_printQuality;
        ps.paperSource  = instance.m_paperSource;
        ps.paperSize    = instance.m_paperSize;
        ps.duplex       = instance.m_duplex;
        ps.pageFilter   = instance.m_pageFilter;
        ps.condensed    = instance.m_condensed;
        ps.formFeed     = instance.m_formFeed;
        ps.isDotMatrix  = instance.m_isDotMatrix;
    }
    return ps;
}

//------------------------------------------------------------------------------
// Dialog procedure
//------------------------------------------------------------------------------
INT_PTR CALLBACK PrintPreviewWindow::DlgProc(HWND hwnd, UINT msg,
                                              WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG:
            if (s_instance) s_instance->OnInit(hwnd);
            return TRUE;

        case WM_COMMAND:
            if (s_instance)
                s_instance->OnCommand(LOWORD(wParam), HIWORD(wParam));
            return TRUE;

        case WM_DRAWITEM:
            if (s_instance && wParam == IDC_PP_PREVIEW) {
                s_instance->OnDrawItem(reinterpret_cast<DRAWITEMSTRUCT*>(lParam));
                return TRUE;
            }
            break;

        case WM_NOTIFY: {
            NMHDR* pnm = reinterpret_cast<NMHDR*>(lParam);
            if (s_instance && pnm && pnm->code == UDN_DELTAPOS) {
                NMUPDOWN* pud = reinterpret_cast<NMUPDOWN*>(lParam);
                HWND hBuddy = reinterpret_cast<HWND>(
                    SendMessageW(pnm->hwndFrom, UDM_GETBUDDY, 0, 0));
                int editId = GetDlgCtrlID(hBuddy);
                s_instance->OnSpinDelta(editId, pud->iDelta);
                return TRUE;
            }
            break;
        }

        case WM_MOUSEWHEEL:
            if (s_instance) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                bool ctrlHeld = (LOWORD(wParam) & MK_CONTROL) != 0;
                if (ctrlHeld) {
                    s_instance->OnMouseWheelZoom(delta);
                } else {
                    s_instance->OnMouseWheel(delta);
                }
            }
            return TRUE;
    }
    return FALSE;
}

//------------------------------------------------------------------------------
// Initialize dialog
//------------------------------------------------------------------------------
void PrintPreviewWindow::OnInit(HWND hwnd) {
    m_hwnd = hwnd;

    // Center on parent
    HWND parent = GetParent(hwnd);
    if (parent) {
        RECT rcParent, rcDlg;
        GetWindowRect(parent, &rcParent);
        GetWindowRect(hwnd, &rcDlg);
        int dlgW = rcDlg.right  - rcDlg.left;
        int dlgH = rcDlg.bottom - rcDlg.top;
        int x = rcParent.left + (rcParent.right  - rcParent.left - dlgW) / 2;
        int y = rcParent.top  + (rcParent.bottom - rcParent.top  - dlgH) / 2;
        SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }

    // Create up/down spin controls for each margin edit
    auto createSpin = [&](int editId) {
        HWND hEdit = GetDlgItem(m_hwnd, editId);
        RECT rc;
        GetWindowRect(hEdit, &rc);
        MapWindowPoints(HWND_DESKTOP, m_hwnd, reinterpret_cast<POINT*>(&rc), 2);
        HWND hSpin = CreateWindowExW(0, UPDOWN_CLASSW, nullptr,
            WS_CHILD | WS_VISIBLE | UDS_ALIGNRIGHT | UDS_ARROWKEYS,
            rc.right, rc.top, 0, rc.bottom - rc.top,
            m_hwnd, nullptr, m_hInstance, nullptr);
        SendMessageW(hSpin, UDM_SETBUDDY, reinterpret_cast<WPARAM>(hEdit), 0);
    };
    createSpin(IDC_PP_MARGIN_LEFT);
    createSpin(IDC_PP_MARGIN_RIGHT);
    createSpin(IDC_PP_MARGIN_TOP);
    createSpin(IDC_PP_MARGIN_BOTTOM);

    // Populate border style combo
    HWND hStyleCombo = GetDlgItem(m_hwnd, IDC_PP_BORDER_STYLE);
    SendMessageW(hStyleCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Solid"));
    SendMessageW(hStyleCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Dashed"));
    SendMessageW(hStyleCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Dotted"));
    SendMessageW(hStyleCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Double"));
    SendMessageW(hStyleCombo, CB_SETCURSEL, 0, 0);

    // Spin for border width
    createSpin(IDC_PP_BORDER_WIDTH);

    // Populate line spacing combo
    HWND hSpacingCombo = GetDlgItem(m_hwnd, IDC_PP_LINE_SPACING);
    SendMessageW(hSpacingCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"1.0"));
    SendMessageW(hSpacingCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"1.15"));
    SendMessageW(hSpacingCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"1.5"));
    SendMessageW(hSpacingCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"2.0"));
    SendMessageW(hSpacingCombo, CB_SETCURSEL, 0, 0);

    // Populate orientation combo
    HWND hOrientCombo = GetDlgItem(m_hwnd, IDC_PP_ORIENTATION);
    SendMessageW(hOrientCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Portrait"));
    SendMessageW(hOrientCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Landscape"));
    SendMessageW(hOrientCombo, CB_SETCURSEL, 0, 0);

    // Populate columns combo
    HWND hColsCombo = GetDlgItem(m_hwnd, IDC_PP_COLUMNS);
    SendMessageW(hColsCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"1"));
    SendMessageW(hColsCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"2"));
    SendMessageW(hColsCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"3"));
    SendMessageW(hColsCombo, CB_SETCURSEL, 0, 0);

    // Initialize column gap and gutter
    SetDlgItemTextW(m_hwnd, IDC_PP_COL_GAP, L"12");
    SetDlgItemTextW(m_hwnd, IDC_PP_GUTTER, L"0.00");
    createSpin(IDC_PP_COL_GAP);
    createSpin(IDC_PP_GUTTER);
    UpdateColumnControlStates();

    // Initialize copies and collate
    SetDlgItemTextW(m_hwnd, IDC_PP_COPIES, L"1");
    createSpin(IDC_PP_COPIES);
    CheckDlgButton(m_hwnd, IDC_PP_COLLATE, BST_CHECKED);

    // Initialize line number styling controls
    UpdateLineNumberControlStates();

    // Default page range to "All"
    CheckRadioButton(m_hwnd, IDC_PP_RANGE_ALL, IDC_PP_RANGE_PAGES, IDC_PP_RANGE_ALL);
    SetDlgItemTextW(m_hwnd, IDC_PP_RANGE_EDIT, L"");
    EnableWindow(GetDlgItem(m_hwnd, IDC_PP_RANGE_EDIT), FALSE);

    // Default watermark text
    SetDlgItemTextW(m_hwnd, IDC_PP_WATERMARK_TEXT, L"DRAFT");

    // Initialize printer controls
    {
        // Print quality combo
        HWND hQuality = GetDlgItem(m_hwnd, IDC_PP_PRINT_QUALITY);
        SendMessageW(hQuality, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Normal"));
        SendMessageW(hQuality, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Draft"));
        SendMessageW(hQuality, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"High"));
        SendMessageW(hQuality, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Letter Quality"));
        SendMessageW(hQuality, CB_SETCURSEL, 0, 0);

        // Paper source combo
        HWND hSource = GetDlgItem(m_hwnd, IDC_PP_PAPER_SOURCE);
        SendMessageW(hSource, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Auto Select"));
        SendMessageW(hSource, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Upper Tray"));
        SendMessageW(hSource, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Lower Tray"));
        SendMessageW(hSource, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Manual Feed"));
        SendMessageW(hSource, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Tractor Feed"));
        SendMessageW(hSource, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Continuous"));
        SendMessageW(hSource, CB_SETCURSEL, 0, 0);

        // Paper size combo
        HWND hPaper = GetDlgItem(m_hwnd, IDC_PP_PAPER_SIZE);
        SendMessageW(hPaper, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Printer Default"));
        SendMessageW(hPaper, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Letter (8.5\" x 11\")"));
        SendMessageW(hPaper, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Legal (8.5\" x 14\")"));
        SendMessageW(hPaper, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"A4 (210 x 297mm)"));
        SendMessageW(hPaper, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"A5 (148 x 210mm)"));
        SendMessageW(hPaper, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"B5 (182 x 257mm)"));
        SendMessageW(hPaper, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Envelope #10"));
        SendMessageW(hPaper, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Envelope DL"));
        SendMessageW(hPaper, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Envelope C5"));
        SendMessageW(hPaper, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Fanfold US (14.875\" x 11\")"));
        SendMessageW(hPaper, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Fanfold EU (8.5\" x 12\")"));
        SendMessageW(hPaper, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Continuous Custom"));
        SendMessageW(hPaper, CB_SETCURSEL, 0, 0);

        // Duplex combo
        HWND hDuplex = GetDlgItem(m_hwnd, IDC_PP_DUPLEX);
        SendMessageW(hDuplex, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"None"));
        SendMessageW(hDuplex, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Long"));
        SendMessageW(hDuplex, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Short"));
        SendMessageW(hDuplex, CB_SETCURSEL, 0, 0);

        // Page filter combo
        HWND hFilter = GetDlgItem(m_hwnd, IDC_PP_PAGE_FILTER);
        SendMessageW(hFilter, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"All"));
        SendMessageW(hFilter, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Odd"));
        SendMessageW(hFilter, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Even"));
        SendMessageW(hFilter, CB_SETCURSEL, 0, 0);

        // Default condensed and form feed checkboxes
        CheckDlgButton(m_hwnd, IDC_PP_CONDENSED, m_condensed ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(m_hwnd, IDC_PP_FORMFEED, m_formFeed ? BST_CHECKED : BST_UNCHECKED);

        // Apply persistent settings to combos
        SendMessageW(GetDlgItem(m_hwnd, IDC_PP_PRINT_QUALITY), CB_SETCURSEL, m_printQuality, 0);
        SendMessageW(GetDlgItem(m_hwnd, IDC_PP_PAPER_SOURCE), CB_SETCURSEL, m_paperSource, 0);
        SendMessageW(GetDlgItem(m_hwnd, IDC_PP_PAPER_SIZE), CB_SETCURSEL, m_paperSize, 0);
        SendMessageW(GetDlgItem(m_hwnd, IDC_PP_DUPLEX), CB_SETCURSEL, m_duplex, 0);
        SendMessageW(GetDlgItem(m_hwnd, IDC_PP_PAGE_FILTER), CB_SETCURSEL, m_pageFilter, 0);

        // Detect printer type and update UI (may override quality/source for dot matrix)
        DetectPrinterType();
    }

    // Update title to indicate selection if applicable
    if (m_hasSelection) {
        std::wstring title = L"Print Preview (Selection)";
        SetWindowTextW(m_hwnd, title.c_str());
    }

    // Paginate (creates pages from page-0 settings, extends m_pageSettings)
    Paginate();

    // Populate controls from page 0
    m_suppressSync = true;
    SyncControlsToPage(0);
    m_suppressSync = false;

    UpdatePageInfo();
}

//------------------------------------------------------------------------------
// Handle commands
//------------------------------------------------------------------------------
void PrintPreviewWindow::OnCommand(WORD id, WORD code) {
    switch (id) {
        case IDC_PP_PRINT:
            SyncPageFromControls();
            EndDialog(m_hwnd, IDOK);
            break;

        case IDCANCEL:
            EndDialog(m_hwnd, IDCANCEL);
            break;

        case IDC_PP_FONT_BTN:
            OnChooseFont();
            break;

        case IDC_PP_APPLYALL:
            ApplyToAllPages();
            break;

        case IDC_PP_PREVPAGE:
            if (m_selectedPage > 0)
                SelectPage(m_selectedPage - 1);
            break;

        case IDC_PP_NEXTPAGE:
            if (m_selectedPage + 1 < static_cast<int>(m_pages.size()))
                SelectPage(m_selectedPage + 1);
            break;

        // Preview click → select page
        case IDC_PP_PREVIEW:
            if (code == STN_CLICKED)
                OnPreviewClick();
            break;

        // Margin edits → update selected page, repaginate if page 0
        case IDC_PP_MARGIN_LEFT:
        case IDC_PP_MARGIN_RIGHT:
        case IDC_PP_MARGIN_TOP:
        case IDC_PP_MARGIN_BOTTOM:
            if (code == EN_CHANGE && !m_suppressSync) {
                SyncPageFromControls();
                if (m_selectedPage == 0) {
                    Paginate();
                    UpdatePageInfo();
                }
                InvalidatePreview();
            }
            break;

        // Header/footer edits → update selected page, redraw only
        case IDC_PP_HEADER:
        case IDC_PP_FOOTER_LEFT:
        case IDC_PP_FOOTER_RIGHT:
            if (code == EN_CHANGE && !m_suppressSync) {
                SyncPageFromControls();
                InvalidatePreview();
            }
            break;

        // Border controls
        case IDC_PP_BORDER_ENABLE:
            if (!m_suppressSync) {
                SyncPageFromControls();
                UpdateBorderControlStates();
                InvalidatePreview();
            }
            break;

        case IDC_PP_BORDER_STYLE:
            if (code == CBN_SELCHANGE && !m_suppressSync) {
                SyncPageFromControls();
                InvalidatePreview();
            }
            break;

        case IDC_PP_BORDER_WIDTH:
            if (code == EN_CHANGE && !m_suppressSync) {
                SyncPageFromControls();
                InvalidatePreview();
            }
            break;

        case IDC_PP_BORDER_TOP:
        case IDC_PP_BORDER_BOTTOM:
        case IDC_PP_BORDER_LEFT:
        case IDC_PP_BORDER_RIGHT:
            if (!m_suppressSync) {
                SyncPageFromControls();
                InvalidatePreview();
            }
            break;

        // Line numbers checkbox
        case IDC_PP_LINE_NUMBERS:
            if (!m_suppressSync) {
                SyncPageFromControls();
                UpdateLineNumberControlStates();
                Paginate();
                UpdatePageInfo();
                InvalidatePreview();
            }
            break;

        // Line spacing combo
        case IDC_PP_LINE_SPACING:
            if (code == CBN_SELCHANGE && !m_suppressSync) {
                SyncPageFromControls();
                Paginate();
                UpdatePageInfo();
                InvalidatePreview();
            }
            break;

        // Orientation combo
        case IDC_PP_ORIENTATION:
            if (code == CBN_SELCHANGE && !m_suppressSync) {
                OnOrientationChange();
            }
            break;

        // Watermark controls
        case IDC_PP_WATERMARK_ENABLE:
            if (!m_suppressSync) {
                SyncPageFromControls();
                UpdateWatermarkControlStates();
                InvalidatePreview();
            }
            break;

        case IDC_PP_WATERMARK_TEXT:
            if (code == EN_CHANGE && !m_suppressSync) {
                SyncPageFromControls();
                InvalidatePreview();
            }
            break;

        case IDC_PP_WATERMARK_COLOR:
            OnWatermarkColorPick();
            break;

        case IDC_PP_WATERMARK_FONT_BTN:
            OnWatermarkChooseFont();
            break;

        // Page range radio buttons
        case IDC_PP_RANGE_ALL:
            EnableWindow(GetDlgItem(m_hwnd, IDC_PP_RANGE_EDIT), FALSE);
            break;

        case IDC_PP_RANGE_PAGES:
            EnableWindow(GetDlgItem(m_hwnd, IDC_PP_RANGE_EDIT), TRUE);
            SetFocus(GetDlgItem(m_hwnd, IDC_PP_RANGE_EDIT));
            break;

        // Columns combo
        case IDC_PP_COLUMNS:
            if (code == CBN_SELCHANGE && !m_suppressSync) {
                SyncPageFromControls();
                UpdateColumnControlStates();
                Paginate();
                UpdatePageInfo();
                InvalidatePreview();
            }
            break;

        // Column gap edit
        case IDC_PP_COL_GAP:
            if (code == EN_CHANGE && !m_suppressSync) {
                SyncPageFromControls();
                InvalidatePreview();
            }
            break;

        // Gutter edit
        case IDC_PP_GUTTER:
            if (code == EN_CHANGE && !m_suppressSync) {
                SyncPageFromControls();
                Paginate();
                UpdatePageInfo();
                InvalidatePreview();
            }
            break;

        // Copies edit
        case IDC_PP_COPIES:
            if (code == EN_CHANGE && !m_suppressSync) {
                wchar_t buf[32] = {};
                GetDlgItemTextW(m_hwnd, IDC_PP_COPIES, buf, 32);
                m_copies = _wtoi(buf);
                if (m_copies < 1) m_copies = 1;
                if (m_copies > 99) m_copies = 99;
            }
            break;

        // Collate checkbox
        case IDC_PP_COLLATE:
            if (!m_suppressSync) {
                m_collate = IsDlgButtonChecked(m_hwnd, IDC_PP_COLLATE) == BST_CHECKED;
            }
            break;

        // Border more button (opens color dialog)
        case IDC_PP_BORDER_COLOR:
            OnBorderMore();
            break;

        // Save/Load settings
        case IDC_PP_SAVE_SETTINGS:
            OnSaveSettings();
            break;

        case IDC_PP_LOAD_SETTINGS:
            OnLoadSettings();
            break;

        // Border padding edit
        case IDC_PP_BORDER_PADDING:
            if (code == EN_CHANGE && !m_suppressSync) {
                SyncPageFromControls();
                InvalidatePreview();
            }
            break;

        case IDC_PP_LINENUM_COLOR:
            OnLineNumberColorPick();
            break;

        case IDC_PP_LINENUM_FONT:
            OnLineNumberChooseFont();
            break;

        case IDC_PP_ADD_FILES:
            OnAddFiles();
            break;

        // Printer settings combos
        case IDC_PP_PRINT_QUALITY:
            if (code == CBN_SELCHANGE && !m_suppressSync) {
                m_printQuality = static_cast<int>(SendMessageW(
                    GetDlgItem(m_hwnd, IDC_PP_PRINT_QUALITY), CB_GETCURSEL, 0, 0));
            }
            break;

        case IDC_PP_PAPER_SOURCE:
            if (code == CBN_SELCHANGE && !m_suppressSync) {
                m_paperSource = static_cast<int>(SendMessageW(
                    GetDlgItem(m_hwnd, IDC_PP_PAPER_SOURCE), CB_GETCURSEL, 0, 0));
                UpdatePrinterControlStates();
            }
            break;

        case IDC_PP_PAPER_SIZE:
            if (code == CBN_SELCHANGE && !m_suppressSync) {
                m_paperSize = static_cast<int>(SendMessageW(
                    GetDlgItem(m_hwnd, IDC_PP_PAPER_SIZE), CB_GETCURSEL, 0, 0));
            }
            break;

        case IDC_PP_DUPLEX:
            if (code == CBN_SELCHANGE && !m_suppressSync) {
                m_duplex = static_cast<int>(SendMessageW(
                    GetDlgItem(m_hwnd, IDC_PP_DUPLEX), CB_GETCURSEL, 0, 0));
            }
            break;

        case IDC_PP_PAGE_FILTER:
            if (code == CBN_SELCHANGE && !m_suppressSync) {
                m_pageFilter = static_cast<int>(SendMessageW(
                    GetDlgItem(m_hwnd, IDC_PP_PAGE_FILTER), CB_GETCURSEL, 0, 0));
            }
            break;

        case IDC_PP_CONDENSED:
            if (!m_suppressSync) {
                m_condensed = IsDlgButtonChecked(m_hwnd, IDC_PP_CONDENSED) == BST_CHECKED;
            }
            break;

        case IDC_PP_FORMFEED:
            if (!m_suppressSync) {
                m_formFeed = IsDlgButtonChecked(m_hwnd, IDC_PP_FORMFEED) == BST_CHECKED;
            }
            break;
    }
}

//------------------------------------------------------------------------------
// Populate controls from a page's settings
//------------------------------------------------------------------------------
void PrintPreviewWindow::SyncControlsToPage(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_pageSettings.size())) return;

    m_suppressSync = true;
    const PageSettings& ps = m_pageSettings[idx];

    auto setMargin = [&](int id, int milVal) {
        wchar_t buf[32];
        swprintf_s(buf, L"%.2f", milVal / 1000.0);
        SetDlgItemTextW(m_hwnd, id, buf);
    };
    setMargin(IDC_PP_MARGIN_LEFT,   ps.marginLeft);
    setMargin(IDC_PP_MARGIN_RIGHT,  ps.marginRight);
    setMargin(IDC_PP_MARGIN_TOP,    ps.marginTop);
    setMargin(IDC_PP_MARGIN_BOTTOM, ps.marginBottom);

    SetDlgItemTextW(m_hwnd, IDC_PP_HEADER,       ps.headerText.c_str());
    SetDlgItemTextW(m_hwnd, IDC_PP_FOOTER_LEFT,  ps.footerLeft.c_str());
    SetDlgItemTextW(m_hwnd, IDC_PP_FOOTER_RIGHT, ps.footerRight.c_str());

    // Border settings
    CheckDlgButton(m_hwnd, IDC_PP_BORDER_ENABLE, ps.borderEnabled ? BST_CHECKED : BST_UNCHECKED);
    SendMessageW(GetDlgItem(m_hwnd, IDC_PP_BORDER_STYLE), CB_SETCURSEL, ps.borderStyle, 0);
    {
        wchar_t widthBuf[32];
        swprintf_s(widthBuf, L"%d", ps.borderWidthPt);
        SetDlgItemTextW(m_hwnd, IDC_PP_BORDER_WIDTH, widthBuf);
        wchar_t padBuf[32];
        swprintf_s(padBuf, L"%d", ps.borderPaddingPt);
        SetDlgItemTextW(m_hwnd, IDC_PP_BORDER_PADDING, padBuf);
    }
    CheckDlgButton(m_hwnd, IDC_PP_BORDER_TOP,    ps.borderTop    ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(m_hwnd, IDC_PP_BORDER_BOTTOM, ps.borderBottom ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(m_hwnd, IDC_PP_BORDER_LEFT,   ps.borderLeft   ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(m_hwnd, IDC_PP_BORDER_RIGHT,  ps.borderRight  ? BST_CHECKED : BST_UNCHECKED);
    UpdateBorderControlStates();

    // Line numbers and line spacing
    CheckDlgButton(m_hwnd, IDC_PP_LINE_NUMBERS, ps.lineNumbers ? BST_CHECKED : BST_UNCHECKED);
    {
        int spacingIdx = 0;
        if (ps.lineSpacing == 115) spacingIdx = 1;
        else if (ps.lineSpacing == 150) spacingIdx = 2;
        else if (ps.lineSpacing == 200) spacingIdx = 3;
        SendMessageW(GetDlgItem(m_hwnd, IDC_PP_LINE_SPACING), CB_SETCURSEL, spacingIdx, 0);
    }
    UpdateLineNumberControlStates();

    // Watermark
    CheckDlgButton(m_hwnd, IDC_PP_WATERMARK_ENABLE, ps.watermarkEnabled ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemTextW(m_hwnd, IDC_PP_WATERMARK_TEXT, ps.watermarkText.c_str());
    UpdateWatermarkControlStates();
    UpdateWatermarkFontLabel();

    // Columns and gutter
    {
        int colIdx = ps.columns - 1;
        if (colIdx < 0) colIdx = 0;
        if (colIdx > 2) colIdx = 2;
        SendMessageW(GetDlgItem(m_hwnd, IDC_PP_COLUMNS), CB_SETCURSEL, colIdx, 0);

        wchar_t gapBuf[32];
        swprintf_s(gapBuf, L"%d", ps.columnGapPt);
        SetDlgItemTextW(m_hwnd, IDC_PP_COL_GAP, gapBuf);

        wchar_t gutterBuf[32];
        swprintf_s(gutterBuf, L"%.2f", ps.gutterMil / 1000.0);
        SetDlgItemTextW(m_hwnd, IDC_PP_GUTTER, gutterBuf);
    }
    UpdateColumnControlStates();

    UpdateFontLabel();
    m_suppressSync = false;
}

//------------------------------------------------------------------------------
// Read controls into the selected page's settings
//------------------------------------------------------------------------------
void PrintPreviewWindow::SyncPageFromControls() {
    if (m_selectedPage < 0 || m_selectedPage >= static_cast<int>(m_pageSettings.size()))
        return;

    PageSettings& ps = m_pageSettings[m_selectedPage];

    auto readMargin = [&](int id) -> int {
        wchar_t buf[64] = {};
        GetDlgItemTextW(m_hwnd, id, buf, 64);
        double val = _wtof(buf);
        if (val < 0.0) val = 0.0;
        if (val > 5.0) val = 5.0;
        return static_cast<int>(val * 1000.0 + 0.5);
    };
    ps.marginLeft   = readMargin(IDC_PP_MARGIN_LEFT);
    ps.marginRight  = readMargin(IDC_PP_MARGIN_RIGHT);
    ps.marginTop    = readMargin(IDC_PP_MARGIN_TOP);
    ps.marginBottom = readMargin(IDC_PP_MARGIN_BOTTOM);

    wchar_t buf[256] = {};
    GetDlgItemTextW(m_hwnd, IDC_PP_HEADER, buf, 256);
    ps.headerText = buf;
    GetDlgItemTextW(m_hwnd, IDC_PP_FOOTER_LEFT, buf, 256);
    ps.footerLeft = buf;
    GetDlgItemTextW(m_hwnd, IDC_PP_FOOTER_RIGHT, buf, 256);
    ps.footerRight = buf;

    // Border settings
    ps.borderEnabled = IsDlgButtonChecked(m_hwnd, IDC_PP_BORDER_ENABLE) == BST_CHECKED;
    ps.borderStyle = static_cast<int>(SendMessageW(
        GetDlgItem(m_hwnd, IDC_PP_BORDER_STYLE), CB_GETCURSEL, 0, 0));
    if (ps.borderStyle < 0) ps.borderStyle = 0;
    {
        wchar_t widthBuf[32] = {};
        GetDlgItemTextW(m_hwnd, IDC_PP_BORDER_WIDTH, widthBuf, 32);
        ps.borderWidthPt = _wtoi(widthBuf);
        if (ps.borderWidthPt < 1) ps.borderWidthPt = 1;
        if (ps.borderWidthPt > 10) ps.borderWidthPt = 10;

        wchar_t padBuf[32] = {};
        GetDlgItemTextW(m_hwnd, IDC_PP_BORDER_PADDING, padBuf, 32);
        ps.borderPaddingPt = _wtoi(padBuf);
        if (ps.borderPaddingPt < 0) ps.borderPaddingPt = 0;
        if (ps.borderPaddingPt > 72) ps.borderPaddingPt = 72;
    }
    ps.borderTop    = IsDlgButtonChecked(m_hwnd, IDC_PP_BORDER_TOP)    == BST_CHECKED;
    ps.borderBottom = IsDlgButtonChecked(m_hwnd, IDC_PP_BORDER_BOTTOM) == BST_CHECKED;
    ps.borderLeft   = IsDlgButtonChecked(m_hwnd, IDC_PP_BORDER_LEFT)   == BST_CHECKED;
    ps.borderRight  = IsDlgButtonChecked(m_hwnd, IDC_PP_BORDER_RIGHT)  == BST_CHECKED;

    // Line numbers
    ps.lineNumbers = IsDlgButtonChecked(m_hwnd, IDC_PP_LINE_NUMBERS) == BST_CHECKED;

    // Line spacing
    int spacingIdx = static_cast<int>(SendMessageW(
        GetDlgItem(m_hwnd, IDC_PP_LINE_SPACING), CB_GETCURSEL, 0, 0));
    switch (spacingIdx) {
        case 1: ps.lineSpacing = 115; break;
        case 2: ps.lineSpacing = 150; break;
        case 3: ps.lineSpacing = 200; break;
        default: ps.lineSpacing = 100; break;
    }

    // Watermark
    ps.watermarkEnabled = IsDlgButtonChecked(m_hwnd, IDC_PP_WATERMARK_ENABLE) == BST_CHECKED;
    {
        wchar_t wmBuf[256] = {};
        GetDlgItemTextW(m_hwnd, IDC_PP_WATERMARK_TEXT, wmBuf, 256);
        ps.watermarkText = wmBuf;
    }

    // Columns and gutter
    {
        int colIdx = static_cast<int>(SendMessageW(
            GetDlgItem(m_hwnd, IDC_PP_COLUMNS), CB_GETCURSEL, 0, 0));
        ps.columns = colIdx + 1;
        if (ps.columns < 1) ps.columns = 1;
        if (ps.columns > 3) ps.columns = 3;

        wchar_t gapBuf[32] = {};
        GetDlgItemTextW(m_hwnd, IDC_PP_COL_GAP, gapBuf, 32);
        ps.columnGapPt = _wtoi(gapBuf);
        if (ps.columnGapPt < 0) ps.columnGapPt = 0;
        if (ps.columnGapPt > 72) ps.columnGapPt = 72;

        wchar_t gutterBuf[64] = {};
        GetDlgItemTextW(m_hwnd, IDC_PP_GUTTER, gutterBuf, 64);
        double gutterVal = _wtof(gutterBuf);
        if (gutterVal < 0.0) gutterVal = 0.0;
        if (gutterVal > 2.0) gutterVal = 2.0;
        ps.gutterMil = static_cast<int>(gutterVal * 1000.0 + 0.5);
    }
}

//------------------------------------------------------------------------------
// Select a page: save edits, switch, populate controls, scroll into view
//------------------------------------------------------------------------------
void PrintPreviewWindow::SelectPage(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_pages.size())) return;
    if (idx == m_selectedPage) return;

    SyncPageFromControls();
    m_selectedPage = idx;
    SyncControlsToPage(idx);
    ScrollToPage(idx);
    UpdatePageInfo();
    InvalidatePreview();
}

//------------------------------------------------------------------------------
// Scroll so the selected page is visible
//------------------------------------------------------------------------------
void PrintPreviewWindow::ScrollToPage(int idx) {
    PreviewLayout L = GetPreviewLayout();
    int pageTop    = idx * (L.pageH + L.gap);
    int pageBottom = pageTop + L.pageH;

    HWND hPreview = GetDlgItem(m_hwnd, IDC_PP_PREVIEW);
    RECT rc;
    GetClientRect(hPreview, &rc);
    int ctrlH = rc.bottom - rc.top;

    if (pageTop < m_scrollY)
        m_scrollY = pageTop - L.gap / 2;
    else if (pageBottom > m_scrollY + ctrlH)
        m_scrollY = pageBottom - ctrlH + L.gap / 2;

    if (m_scrollY > L.maxScroll) m_scrollY = L.maxScroll;
    if (m_scrollY < 0) m_scrollY = 0;
}

//------------------------------------------------------------------------------
// Copy selected page's settings to ALL pages, then repaginate
//------------------------------------------------------------------------------
void PrintPreviewWindow::ApplyToAllPages() {
    SyncPageFromControls();
    if (m_pageSettings.empty()) return;

    PageSettings current = m_pageSettings[m_selectedPage];
    for (auto& ps : m_pageSettings)
        ps = current;

    // Ensure page 0 has the settings so repagination is correct
    m_pageSettings[0] = current;
    Paginate();
    UpdatePageInfo();
    InvalidatePreview();
}

//------------------------------------------------------------------------------
// Update "Page X of Y" label and enable/disable navigation buttons
//------------------------------------------------------------------------------
void PrintPreviewWindow::UpdatePageInfo() {
    int total = (std::max)(1, static_cast<int>(m_pages.size()));
    wchar_t buf[64];
    swprintf_s(buf, L"Page %d of %d", m_selectedPage + 1, total);
    SetDlgItemTextW(m_hwnd, IDC_PP_PAGEINFO, buf);

    EnableWindow(GetDlgItem(m_hwnd, IDC_PP_PREVPAGE), m_selectedPage > 0);
    EnableWindow(GetDlgItem(m_hwnd, IDC_PP_NEXTPAGE), m_selectedPage + 1 < total);
}

//------------------------------------------------------------------------------
// Invalidate the preview control
//------------------------------------------------------------------------------
void PrintPreviewWindow::InvalidatePreview() {
    HWND hwndPreview = GetDlgItem(m_hwnd, IDC_PP_PREVIEW);
    if (hwndPreview) InvalidateRect(hwndPreview, nullptr, TRUE);
}

//------------------------------------------------------------------------------
// Expand header/footer tokens
//------------------------------------------------------------------------------
std::wstring PrintPreviewWindow::ExpandTokens(const std::wstring& tmpl,
                                               int pageNum, int totalPages) const {
    std::wstring result;
    result.reserve(tmpl.size() + 32);
    for (size_t i = 0; i < tmpl.size(); ++i) {
        if (tmpl[i] == L'&' && i + 1 < tmpl.size()) {
            wchar_t tok = tmpl[++i];
            switch (tok) {
                case L'f': result += m_docName; break;
                case L'p': result += std::to_wstring(pageNum); break;
                case L'P': result += std::to_wstring(totalPages); break;
                case L'd': {
                    time_t t = time(nullptr);
                    struct tm lt;
                    localtime_s(&lt, &t);
                    wchar_t db[32];
                    wcsftime(db, 32, L"%m/%d/%Y", &lt);
                    result += db;
                    break;
                }
                case L't': {
                    time_t t = time(nullptr);
                    struct tm lt;
                    localtime_s(&lt, &t);
                    wchar_t tb[32];
                    wcsftime(tb, 32, L"%I:%M %p", &lt);
                    result += tb;
                    break;
                }
                default:
                    result += L'&';
                    result += tok;
                    break;
            }
        } else {
            result += tmpl[i];
        }
    }
    return result;
}

//------------------------------------------------------------------------------
// Paginate text using page-0 settings for text flow.
// After distributing lines, resizes m_pageSettings to match page count
// (new pages copy page-0; existing per-page overrides are preserved).
//------------------------------------------------------------------------------
void PrintPreviewWindow::Paginate() {
    if (m_pageSettings.empty()) return;

    const PageSettings& base = m_pageSettings[0];
    int oldCount = static_cast<int>(m_pageSettings.size());

    m_pages.clear();

    HDC hdc = GetDC(m_hwnd);
    if (!hdc) return;

    int refDPI = 96;
    LOGFONTW lf = {};
    lf.lfHeight         = -MulDiv(base.fontSize, refDPI, 72);
    lf.lfWeight         = base.fontWeight;
    lf.lfItalic         = base.fontItalic ? TRUE : FALSE;
    lf.lfCharSet        = DEFAULT_CHARSET;
    lf.lfOutPrecision   = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
    lf.lfQuality        = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, base.fontName.c_str());

    HFONT hFont = CreateFontIndirectW(&lf);
    HFONT hOldFont = static_cast<HFONT>(SelectObject(hdc, hFont));

    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;

    // Apply line spacing multiplier
    int spacedLineHeight = MulDiv(lineHeight, base.lineSpacing, 100);
    if (spacedLineHeight < lineHeight) spacedLineHeight = lineHeight;

    int pageWidthPx  = MulDiv(m_pageWidthMil,  refDPI, 1000);
    int pageHeightPx = MulDiv(m_pageHeightMil, refDPI, 1000);
    int leftPx   = MulDiv(base.marginLeft,   refDPI, 1000);
    int rightPx  = MulDiv(base.marginRight,  refDPI, 1000);
    int topPx    = MulDiv(base.marginTop,    refDPI, 1000);
    int bottomPx = MulDiv(base.marginBottom, refDPI, 1000);

    // Add gutter to the effective left margin
    int gutterPx = MulDiv(base.gutterMil, refDPI, 1000);

    int printWidth  = pageWidthPx - leftPx - rightPx - gutterPx;

    // If line numbers are enabled, reserve gutter space
    int lineNumGutterWidth = 0;
    if (base.lineNumbers) {
        // Measure width of a sample line number (e.g. "99999 ")
        SIZE gutterSz;
        GetTextExtentPoint32W(hdc, L"99999 ", 6, &gutterSz);
        lineNumGutterWidth = gutterSz.cx;
        printWidth -= lineNumGutterWidth;
    }

    // Handle columns
    int numCols = base.columns;
    if (numCols < 1) numCols = 1;
    if (numCols > 3) numCols = 3;

    int colGapPx = MulDiv(base.columnGapPt, refDPI, 72);
    int columnWidth = printWidth;
    if (numCols > 1) {
        // Total gap space = (numCols - 1) * colGapPx
        int totalGapPx = (numCols - 1) * colGapPx;
        columnWidth = (printWidth - totalGapPx) / numCols;
        if (columnWidth < 50) columnWidth = 50;
    }

    int printHeight = pageHeightPx - topPx - bottomPx;
    if (columnWidth <= 0) columnWidth = 100;
    if (printHeight <= 0) printHeight = spacedLineHeight;

    int linesPerColumn = printHeight / spacedLineHeight;
    if (linesPerColumn < 1) linesPerColumn = 1;


    // Word-wrap text into lines using column width
    std::vector<std::wstring> allLines;
    std::wistringstream stream(m_text);
    std::wstring rawLine;
    while (std::getline(stream, rawLine)) {
        if (!rawLine.empty() && rawLine.back() == L'\r')
            rawLine.pop_back();

        if (rawLine.empty()) {
            allLines.push_back(L"");
            continue;
        }

        while (!rawLine.empty()) {
            // Use GetTextExtentExPointW to find max chars that fit in one call (O(1) instead of O(n))
            int maxChars = 0;
            SIZE sz;
            GetTextExtentExPointW(hdc, rawLine.c_str(),
                                  static_cast<int>(rawLine.length()),
                                  columnWidth, &maxChars, nullptr, &sz);
            if (maxChars >= static_cast<int>(rawLine.length())) {
                allLines.push_back(rawLine);
                break;
            }
            // Find a word boundary to break at
            size_t breakPos = static_cast<size_t>(maxChars);
            if (breakPos > 0) {
                size_t lastSpace = rawLine.rfind(L' ', breakPos - 1);
                if (lastSpace != std::wstring::npos && lastSpace > 0)
                    breakPos = lastSpace + 1;
            }
            if (breakPos == 0) breakPos = 1;  // Ensure progress
            allLines.push_back(rawLine.substr(0, breakPos));
            rawLine = rawLine.substr(breakPos);
            size_t start = rawLine.find_first_not_of(L' ');
            if (start != std::wstring::npos && start > 0)
                rawLine = rawLine.substr(start);
        }
    }

    // Distribute lines into pages (each page has numCols * linesPerColumn lines)
    int linesPerPage = numCols * linesPerColumn;
    m_pageFirstLineNum.clear();
    int idx = 0;
    int total = static_cast<int>(allLines.size());
    if (total == 0) {
        m_pages.push_back(PageData{});
        m_pageFirstLineNum.push_back(1);
    } else {
        while (idx < total) {
            m_pageFirstLineNum.push_back(idx + 1);
            PageData page;
            for (int i = 0; i < linesPerPage && idx < total; ++i, ++idx)
                page.lines.push_back(allLines[idx]);
            m_pages.push_back(std::move(page));
        }
    }

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    ReleaseDC(m_hwnd, hdc);

    // Sync m_pageSettings count to match page count
    int newCount = static_cast<int>(m_pages.size());
    if (newCount > oldCount) {
        for (int i = oldCount; i < newCount; ++i)
            m_pageSettings.push_back(m_pageSettings[0]);
    } else if (newCount < oldCount) {
        m_pageSettings.resize(newCount);
    }

    // Clamp selected page
    if (m_selectedPage >= newCount)
        m_selectedPage = (std::max)(0, newCount - 1);
}

//------------------------------------------------------------------------------
// Compute the page layout geometry for the preview control
//------------------------------------------------------------------------------
PrintPreviewWindow::PreviewLayout PrintPreviewWindow::GetPreviewLayout() const {
    PreviewLayout L;
    HWND hPreview = GetDlgItem(m_hwnd, IDC_PP_PREVIEW);
    RECT rc;
    GetClientRect(hPreview, &rc);
    int ctrlW = rc.right  - rc.left;
    int ctrlH = rc.bottom - rc.top;

    double aspect = static_cast<double>(m_pageWidthMil) / m_pageHeightMil;
    L.gap = 20;

    // Size each page to fill the control width (with small side margins).
    // User scrolls to see additional pages.
    int sideMargin = 15;
    int basePageW = ctrlW - 2 * sideMargin;
    L.pageW = static_cast<int>(basePageW * m_zoom);
    L.pageH = static_cast<int>(L.pageW / aspect);

    L.contentX = (ctrlW - L.pageW) / 2;

    int n = (std::max)(1, static_cast<int>(m_pages.size()));
    L.totalH   = n * L.pageH + (n - 1) * L.gap;
    L.maxScroll = (std::max)(0, L.totalH - ctrlH);

    return L;
}

//------------------------------------------------------------------------------
// Owner-draw: render ALL pages stacked vertically, scrollable
//------------------------------------------------------------------------------
void PrintPreviewWindow::OnDrawItem(DRAWITEMSTRUCT* dis) {
    if (!dis || dis->CtlID != IDC_PP_PREVIEW) return;

    HDC hdc = dis->hDC;
    RECT rcCtrl = dis->rcItem;
    int ctrlH = rcCtrl.bottom - rcCtrl.top;

    // Fill background
    HBRUSH hBgBrush = CreateSolidBrush(RGB(200, 200, 200));
    FillRect(hdc, &rcCtrl, hBgBrush);
    DeleteObject(hBgBrush);

    PreviewLayout L = GetPreviewLayout();

    // Clamp scroll
    if (m_scrollY > L.maxScroll) m_scrollY = L.maxScroll;
    if (m_scrollY < 0) m_scrollY = 0;

    double scaleX = static_cast<double>(L.pageW) / m_pageWidthMil;
    double scaleY = static_cast<double>(L.pageH) / m_pageHeightMil;

    int numPages = (std::max)(1, static_cast<int>(m_pages.size()));

    // Clip to the control rect
    HRGN hCtrlClip = CreateRectRgnIndirect(&rcCtrl);
    SelectClipRgn(hdc, hCtrlClip);

    for (int i = 0; i < numPages; ++i) {
        int py = rcCtrl.top + i * (L.pageH + L.gap) - m_scrollY;

        // Skip off-screen pages
        if (py + L.pageH < rcCtrl.top || py > rcCtrl.bottom) continue;

        int px = rcCtrl.left + L.contentX;
        bool selected = (i == m_selectedPage);

        DrawSinglePage(hdc, i, px, py, L.pageW, L.pageH,
                        scaleX, scaleY, selected, numPages);

        // Draw page number label below the page
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, selected ? RGB(0, 120, 215) : RGB(100, 100, 100));
        wchar_t pageLbl[32];
        swprintf_s(pageLbl, L"%d", i + 1);
        RECT rcLbl = { px, py + L.pageH + 2, px + L.pageW, py + L.pageH + 16 };
        DrawTextW(hdc, pageLbl, -1, &rcLbl,
                  DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    SelectClipRgn(hdc, nullptr);
    DeleteObject(hCtrlClip);
}

//------------------------------------------------------------------------------
// Draw a single page at the given position with its per-page settings
//------------------------------------------------------------------------------
void PrintPreviewWindow::DrawSinglePage(HDC hdc, int pageIdx,
                                         int px, int py, int pw, int ph,
                                         double scaleX, double scaleY,
                                         bool selected, int totalPages) {
    if (pageIdx < 0 || pageIdx >= static_cast<int>(m_pageSettings.size())) return;
    const PageSettings& ps = m_pageSettings[pageIdx];

    // Drop shadow
    RECT rcShadow = { px + 3, py + 3, px + pw + 3, py + ph + 3 };
    HBRUSH hShadowBrush = CreateSolidBrush(RGB(130, 130, 130));
    FillRect(hdc, &rcShadow, hShadowBrush);
    DeleteObject(hShadowBrush);

    // White page
    RECT rcPage = { px, py, px + pw, py + ph };
    FillRect(hdc, &rcPage, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

    // Border (highlighted if selected)
    COLORREF borderColor = selected ? RGB(0, 120, 215) : RGB(0, 0, 0);
    int borderW = selected ? 2 : 1;
    HPEN hPen = CreatePen(PS_SOLID, borderW, borderColor);
    HPEN hOldPen = static_cast<HPEN>(SelectObject(hdc, hPen));
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, px, py, px + pw, py + ph);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);

    // Margin guides
    int mL = static_cast<int>(ps.marginLeft   * scaleX);
    int mR = static_cast<int>(ps.marginRight  * scaleX);
    int mT = static_cast<int>(ps.marginTop    * scaleY);
    int mB = static_cast<int>(ps.marginBottom * scaleY);

    // Gutter margin (extra inside margin for binding)
    int gutterPx = static_cast<int>(ps.gutterMil * scaleX);

    // Add border padding to margins for text area
    int padPx = 0;
    if (ps.borderEnabled && ps.borderPaddingPt > 0) {
        padPx = static_cast<int>(ps.borderPaddingPt * (1000.0 / 72.0) * scaleY + 0.5);
    }
    int mLText = mL + padPx + gutterPx;
    int mRText = mR + padPx;
    int mTText = mT + padPx;
    int mBText = mB + padPx;

    HPEN hMarginPen = CreatePen(PS_DOT, 1, RGB(180, 200, 240));
    HPEN hOldPen2 = static_cast<HPEN>(SelectObject(hdc, hMarginPen));
    MoveToEx(hdc, px + mL, py, nullptr);       LineTo(hdc, px + mL, py + ph);
    MoveToEx(hdc, px + pw - mR, py, nullptr);  LineTo(hdc, px + pw - mR, py + ph);
    MoveToEx(hdc, px, py + mT, nullptr);       LineTo(hdc, px + pw, py + mT);
    MoveToEx(hdc, px, py + ph - mB, nullptr);  LineTo(hdc, px + pw, py + ph - mB);
    SelectObject(hdc, hOldPen2);
    DeleteObject(hMarginPen);

    // Save DC state (font, clip, colors)
    int savedDC = SaveDC(hdc);

    // Create scaled font from this page's font settings
    int fontHeightMil = MulDiv(ps.fontSize, 1000, 72);
    int fontHeightPx  = static_cast<int>(fontHeightMil * scaleY);
    if (fontHeightPx < 2) fontHeightPx = 2;

    LOGFONTW lf = {};
    lf.lfHeight         = -fontHeightPx;
    lf.lfWeight         = ps.fontWeight;
    lf.lfItalic         = ps.fontItalic ? TRUE : FALSE;
    lf.lfCharSet        = DEFAULT_CHARSET;
    lf.lfOutPrecision   = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
    lf.lfQuality        = DRAFT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, ps.fontName.c_str());

    HFONT hFont = CreateFontIndirectW(&lf);
    SelectObject(hdc, hFont);

    // Create line number font (use custom or fallback to body font)
    HFONT hLineNumFont = nullptr;
    if (ps.lineNumbers) {
        LOGFONTW lfNum = {};
        if (ps.lineNumberFontName.empty() || ps.lineNumberFontSize == 0) {
            // Use body font for line numbers
            lfNum = lf;
        } else {
            int numFontHeightMil = MulDiv(ps.lineNumberFontSize, 1000, 72);
            int numFontHeightPx = static_cast<int>(numFontHeightMil * scaleY);
            if (numFontHeightPx < 2) numFontHeightPx = 2;
            lfNum.lfHeight = -numFontHeightPx;
            lfNum.lfWeight = ps.lineNumberFontWeight ? ps.lineNumberFontWeight : FW_NORMAL;
            lfNum.lfItalic = ps.lineNumberFontItalic ? TRUE : FALSE;
            lfNum.lfCharSet = DEFAULT_CHARSET;
            lfNum.lfOutPrecision = OUT_DEFAULT_PRECIS;
            lfNum.lfClipPrecision = CLIP_DEFAULT_PRECIS;
            lfNum.lfQuality = DRAFT_QUALITY;
            lfNum.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
            wcscpy_s(lfNum.lfFaceName, ps.lineNumberFontName.c_str());
        }
        hLineNumFont = CreateFontIndirectW(&lfNum);
    }

    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    if (lineHeight < 1) lineHeight = 1;

    // Apply line spacing multiplier
    int spacedLineHeight = MulDiv(lineHeight, ps.lineSpacing, 100);
    if (spacedLineHeight < lineHeight) spacedLineHeight = lineHeight;

    // Calculate line number gutter width
    int gutterWidth = 0;
    if (ps.lineNumbers) {
        SIZE gutterSz;
        GetTextExtentPoint32W(hdc, L"9999 ", 5, &gutterSz);
        gutterWidth = gutterSz.cx;
    }

    int pageNum = pageIdx + 1;
    int textX = px + mLText + gutterWidth;
    int textW = pw - mLText - mRText - gutterWidth;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));

    // --- Header (in top margin, clipped to page) ---
    RECT rcPageClip = { px + 1, py + 1, px + pw - 1, py + ph - 1 };
    HRGN hPageRgn = CreateRectRgnIndirect(&rcPageClip);
    ExtSelectClipRgn(hdc, hPageRgn, RGN_AND);

    if (!ps.headerText.empty()) {
        std::wstring hdr = ExpandTokens(ps.headerText, pageNum, totalPages);
        int hdrY = py + (mT - lineHeight) / 2;
        if (hdrY < py) hdrY = py;
        RECT rcH = { px + 1, hdrY, px + pw - 1, hdrY + lineHeight };
        DrawTextW(hdc, hdr.c_str(), static_cast<int>(hdr.length()),
                  &rcH, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    DeleteObject(hPageRgn);

    // --- Body text (clipped to margin area) ---
    RestoreDC(hdc, savedDC);
    savedDC = SaveDC(hdc);
    SelectObject(hdc, hFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));

    RECT rcBodyClip = { px + mLText, py + mTText, px + pw - mRText, py + ph - mBText };
    HRGN hBodyRgn = CreateRectRgnIndirect(&rcBodyClip);
    ExtSelectClipRgn(hdc, hBodyRgn, RGN_AND);

    // --- Watermark (behind body text) ---
    if (ps.watermarkEnabled && !ps.watermarkText.empty()) {
        // Calculate printable area dimensions (use margin area, not padded area)
        int areaW = pw - mL - mR;
        int areaH = ph - mT - mB;

        // Font height: use custom size if set, otherwise auto-size
        int wmFontH;
        if (ps.watermarkFontSize > 0) {
            // Convert point size to pixels at preview scale
            wmFontH = MulDiv(ps.watermarkFontSize, ph, m_pageHeightMil * 72 / 1000);
            if (wmFontH < 4) wmFontH = 4;
        } else {
            // Auto: use half the shorter dimension
            wmFontH = (std::min)(areaW, areaH) / 2;
            if (wmFontH < 8) wmFontH = 8;
        }

        // Calculate the diagonal angle based on actual page aspect ratio
        double angle = atan2(static_cast<double>(areaH), static_cast<double>(areaW));
        int escapement = static_cast<int>(angle * 1800.0 / 3.14159265);

        LOGFONTW wmLf = {};
        wmLf.lfHeight = -wmFontH;
        wmLf.lfWeight = ps.watermarkFontWeight;
        wmLf.lfItalic = ps.watermarkFontItalic ? TRUE : FALSE;
        wmLf.lfCharSet = DEFAULT_CHARSET;
        wmLf.lfEscapement = escapement;
        wmLf.lfOrientation = escapement;
        wmLf.lfQuality = ANTIALIASED_QUALITY;
        wcscpy_s(wmLf.lfFaceName, ps.watermarkFontName.c_str());
        HFONT hWmFont = CreateFontIndirectW(&wmLf);
        HFONT hPrevFont = static_cast<HFONT>(SelectObject(hdc, hWmFont));

        // Measure the text to center it properly
        SIZE wmSize;
        GetTextExtentPoint32W(hdc, ps.watermarkText.c_str(),
                              static_cast<int>(ps.watermarkText.length()), &wmSize);

        // Center of printable area
        int cx = px + mL + areaW / 2;
        int cy = py + mT + areaH / 2;

        // Use TA_CENTER | TA_BASELINE to draw centered at the middle of the page
        UINT oldAlign = SetTextAlign(hdc, TA_CENTER | TA_BASELINE);
        SetTextColor(hdc, ps.watermarkColor);
        TextOutW(hdc, cx, cy, ps.watermarkText.c_str(),
                 static_cast<int>(ps.watermarkText.length()));
        SetTextAlign(hdc, oldAlign);

        SelectObject(hdc, hPrevFont);
        DeleteObject(hWmFont);
        SetTextColor(hdc, RGB(0, 0, 0));
    }

    // Calculate column layout
    int numCols = ps.columns;
    if (numCols < 1) numCols = 1;
    if (numCols > 3) numCols = 3;

    int colGapPx = static_cast<int>(ps.columnGapPt * (1000.0 / 72.0) * scaleX + 0.5);
    int totalPrintW = pw - mLText - mRText - gutterWidth;
    int columnWidth = totalPrintW;
    if (numCols > 1) {
        int totalGapPx = (numCols - 1) * colGapPx;
        columnWidth = (totalPrintW - totalGapPx) / numCols;
        if (columnWidth < 10) columnWidth = 10;
    }

    int printHeight = ph - mTText - mBText;
    int linesPerColumn = printHeight / spacedLineHeight;
    if (linesPerColumn < 1) linesPerColumn = 1;

    if (pageIdx >= 0 && pageIdx < static_cast<int>(m_pages.size())) {
        int lineNum = (pageIdx < static_cast<int>(m_pageFirstLineNum.size()))
                      ? m_pageFirstLineNum[pageIdx] : 1;
        int lineIdx = 0;
        int totalLines = static_cast<int>(m_pages[pageIdx].lines.size());

        for (int col = 0; col < numCols && lineIdx < totalLines; ++col) {
            int colX = px + mLText + gutterWidth + col * (columnWidth + colGapPx);
            int y = py + mTText;

            for (int row = 0; row < linesPerColumn && lineIdx < totalLines; ++row, ++lineIdx) {
                if (y + lineHeight > py + ph - mBText) break;

                const auto& line = m_pages[pageIdx].lines[lineIdx];

                // Draw line number in gutter (only for first column)
                if (ps.lineNumbers && gutterWidth > 0 && col == 0) {
                    wchar_t numBuf[16];
                    swprintf_s(numBuf, L"%d", lineNum);
                    if (hLineNumFont) SelectObject(hdc, hLineNumFont);
                    SetTextColor(hdc, ps.lineNumberColor);
                    RECT rcNum = { px + mLText, y, px + mLText + gutterWidth - 2, y + lineHeight };
                    DrawTextW(hdc, numBuf, -1, &rcNum, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);
                    SetTextColor(hdc, RGB(0, 0, 0));
                    if (hLineNumFont) SelectObject(hdc, hFont);
                }

                if (!line.empty())
                    TextOutW(hdc, colX, y, line.c_str(),
                             static_cast<int>(line.length()));
                y += spacedLineHeight;
                lineNum++;
            }
        }
    }
    DeleteObject(hBodyRgn);

    // --- Footer (in bottom margin, clipped to page) ---
    RestoreDC(hdc, savedDC);
    savedDC = SaveDC(hdc);
    SelectObject(hdc, hFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));

    HRGN hFtrRgn = CreateRectRgnIndirect(&rcPageClip);
    ExtSelectClipRgn(hdc, hFtrRgn, RGN_AND);

    int footerY = py + ph - mB + (mB - lineHeight) / 2;
    if (footerY + lineHeight > py + ph) footerY = py + ph - lineHeight;

    if (!ps.footerLeft.empty()) {
        std::wstring fL = ExpandTokens(ps.footerLeft, pageNum, totalPages);
        RECT rcFL = { textX, footerY, textX + textW, footerY + lineHeight };
        DrawTextW(hdc, fL.c_str(), static_cast<int>(fL.length()),
                  &rcFL, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
    }
    if (!ps.footerRight.empty()) {
        std::wstring fR = ExpandTokens(ps.footerRight, pageNum, totalPages);
        RECT rcFR = { textX, footerY, textX + textW, footerY + lineHeight };
        DrawTextW(hdc, fR.c_str(), static_cast<int>(fR.length()),
                  &rcFR, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);
    }
    DeleteObject(hFtrRgn);

    // --- Page border (at margin boundaries) ---
    if (ps.borderEnabled) {
        RestoreDC(hdc, savedDC);
        savedDC = SaveDC(hdc);

        int borderPx = static_cast<int>(ps.borderWidthPt * (1000.0 / 72.0) * scaleY + 0.5);
        if (borderPx < 1) borderPx = 1;

        int penStyle = PS_SOLID;
        switch (ps.borderStyle) {
            case 1: penStyle = PS_DASH; break;
            case 2: penStyle = PS_DOT; break;
            case 3: penStyle = PS_SOLID; break;
        }

        HPEN hBorderPen = CreatePen(penStyle, borderPx, ps.borderColor);
        SelectObject(hdc, hBorderPen);

        HRGN hBorderClip = CreateRectRgnIndirect(&rcPageClip);
        SelectClipRgn(hdc, hBorderClip);

        int bx1 = px + mL;
        int by1 = py + mT;
        int bx2 = px + pw - mR;
        int by2 = py + ph - mB;

        if (ps.borderTop)    { MoveToEx(hdc, bx1, by1, nullptr); LineTo(hdc, bx2, by1); }
        if (ps.borderBottom) { MoveToEx(hdc, bx1, by2, nullptr); LineTo(hdc, bx2, by2); }
        if (ps.borderLeft)   { MoveToEx(hdc, bx1, by1, nullptr); LineTo(hdc, bx1, by2); }
        if (ps.borderRight)  { MoveToEx(hdc, bx2, by1, nullptr); LineTo(hdc, bx2, by2); }

        // Double border: draw inner parallel line
        if (ps.borderStyle == 3) {
            int gap = borderPx * 2;
            if (gap < 2) gap = 2;
            if (ps.borderTop)    { MoveToEx(hdc, bx1 + gap, by1 + gap, nullptr); LineTo(hdc, bx2 - gap, by1 + gap); }
            if (ps.borderBottom) { MoveToEx(hdc, bx1 + gap, by2 - gap, nullptr); LineTo(hdc, bx2 - gap, by2 - gap); }
            if (ps.borderLeft)   { MoveToEx(hdc, bx1 + gap, by1 + gap, nullptr); LineTo(hdc, bx1 + gap, by2 - gap); }
            if (ps.borderRight)  { MoveToEx(hdc, bx2 - gap, by1 + gap, nullptr); LineTo(hdc, bx2 - gap, by2 - gap); }
        }

        SelectObject(hdc, GetStockObject(BLACK_PEN));
        DeleteObject(hBorderPen);
        DeleteObject(hBorderClip);
    }

    // Restore DC and clean up fonts
    RestoreDC(hdc, savedDC);
    if (hLineNumFont) DeleteObject(hLineNumFont);
    DeleteObject(hFont);
}

//------------------------------------------------------------------------------
// Handle spin button delta: ±0.05 inches per click
//------------------------------------------------------------------------------
void PrintPreviewWindow::OnSpinDelta(int editId, int delta) {
    wchar_t buf[64] = {};
    GetDlgItemTextW(m_hwnd, editId, buf, 64);

    if (editId == IDC_PP_BORDER_WIDTH) {
        // Integer point value, \u00b11 per click
        int val = _wtoi(buf);
        val -= delta;
        if (val < 1)  val = 1;
        if (val > 10) val = 10;
        swprintf_s(buf, L"%d", val);
    } else {
        // Margin: floating point inches, \u00b10.05 per click
        double val = _wtof(buf);
        val -= delta * 0.05;
        if (val < 0.0)  val = 0.0;
        if (val > 5.0)  val = 5.0;
        swprintf_s(buf, L"%.2f", val);
    }

    SetDlgItemTextW(m_hwnd, editId, buf);
}

//------------------------------------------------------------------------------
// Mouse wheel → scroll the preview
//------------------------------------------------------------------------------
void PrintPreviewWindow::OnMouseWheel(int delta) {
    m_scrollY -= delta / 2;

    PreviewLayout L = GetPreviewLayout();
    if (m_scrollY > L.maxScroll) m_scrollY = L.maxScroll;
    if (m_scrollY < 0) m_scrollY = 0;

    InvalidatePreview();
}

//------------------------------------------------------------------------------
// Ctrl+Mouse wheel → zoom the preview
//------------------------------------------------------------------------------
void PrintPreviewWindow::OnMouseWheelZoom(int delta) {
    double zoomStep = 0.1;
    if (delta > 0) {
        m_zoom += zoomStep;
    } else {
        m_zoom -= zoomStep;
    }

    // Clamp zoom to reasonable range (25% to 400%)
    if (m_zoom < 0.25) m_zoom = 0.25;
    if (m_zoom > 4.0) m_zoom = 4.0;

    // Recalculate scroll limits with new zoom
    PreviewLayout L = GetPreviewLayout();
    if (m_scrollY > L.maxScroll) m_scrollY = L.maxScroll;
    if (m_scrollY < 0) m_scrollY = 0;

    InvalidatePreview();
}

//------------------------------------------------------------------------------
// Click on preview control → select the page under the cursor
//------------------------------------------------------------------------------
void PrintPreviewWindow::OnPreviewClick() {
    POINT pt;
    GetCursorPos(&pt);
    HWND hPreview = GetDlgItem(m_hwnd, IDC_PP_PREVIEW);
    ScreenToClient(hPreview, &pt);

    PreviewLayout L = GetPreviewLayout();
    int numPages = (std::max)(1, static_cast<int>(m_pages.size()));

    for (int i = 0; i < numPages; ++i) {
        int py = i * (L.pageH + L.gap) - m_scrollY;
        if (pt.y >= py && pt.y < py + L.pageH) {
            if (pt.x >= L.contentX && pt.x < L.contentX + L.pageW) {
                SelectPage(i);
                return;
            }
        }
    }
}

//------------------------------------------------------------------------------
// Open ChooseFont for the selected page
//------------------------------------------------------------------------------
void PrintPreviewWindow::OnChooseFont() {
    if (m_selectedPage < 0 || m_selectedPage >= static_cast<int>(m_pageSettings.size()))
        return;

    PageSettings& ps = m_pageSettings[m_selectedPage];

    LOGFONTW lf = {};
    lf.lfHeight  = -MulDiv(ps.fontSize, 96, 72);
    lf.lfWeight  = ps.fontWeight;
    lf.lfItalic  = ps.fontItalic ? TRUE : FALSE;
    lf.lfCharSet = DEFAULT_CHARSET;
    wcscpy_s(lf.lfFaceName, ps.fontName.c_str());

    CHOOSEFONTW cf = {};
    cf.lStructSize = sizeof(cf);
    cf.hwndOwner   = m_hwnd;
    cf.lpLogFont   = &lf;
    cf.iPointSize  = ps.fontSize * 10;
    cf.Flags       = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_EFFECTS;

    if (ChooseFontW(&cf)) {
        ps.fontName   = lf.lfFaceName;
        ps.fontSize   = cf.iPointSize / 10;
        ps.fontWeight = lf.lfWeight;
        ps.fontItalic = lf.lfItalic != FALSE;

        UpdateFontLabel();

        if (m_selectedPage == 0) {
            Paginate();
            UpdatePageInfo();
        }
        InvalidatePreview();
    }
}

//------------------------------------------------------------------------------
// Update font label to show selected page's font
//------------------------------------------------------------------------------
void PrintPreviewWindow::UpdateFontLabel() {
    if (m_selectedPage < 0 || m_selectedPage >= static_cast<int>(m_pageSettings.size()))
        return;
    const PageSettings& ps = m_pageSettings[m_selectedPage];
    wchar_t buf[128];
    swprintf_s(buf, L"%s, %dpt%s%s",
               ps.fontName.c_str(),
               ps.fontSize,
               ps.fontWeight >= FW_BOLD ? L", Bold" : L"",
               ps.fontItalic ? L", Italic" : L"");
    SetDlgItemTextW(m_hwnd, IDC_PP_FONT_LABEL, buf);
}

//------------------------------------------------------------------------------
// Enable/disable border sub-controls based on the enable checkbox
//------------------------------------------------------------------------------
void PrintPreviewWindow::UpdateBorderControlStates() {
    bool enabled = IsDlgButtonChecked(m_hwnd, IDC_PP_BORDER_ENABLE) == BST_CHECKED;
    EnableWindow(GetDlgItem(m_hwnd, IDC_PP_BORDER_STYLE),   enabled);
    EnableWindow(GetDlgItem(m_hwnd, IDC_PP_BORDER_WIDTH),   enabled);
    EnableWindow(GetDlgItem(m_hwnd, IDC_PP_BORDER_PADDING), enabled);
    EnableWindow(GetDlgItem(m_hwnd, IDC_PP_BORDER_COLOR),   enabled);
    EnableWindow(GetDlgItem(m_hwnd, IDC_PP_BORDER_TOP),     enabled);
    EnableWindow(GetDlgItem(m_hwnd, IDC_PP_BORDER_BOTTOM),  enabled);
    EnableWindow(GetDlgItem(m_hwnd, IDC_PP_BORDER_LEFT),    enabled);
    EnableWindow(GetDlgItem(m_hwnd, IDC_PP_BORDER_RIGHT),   enabled);
}

//------------------------------------------------------------------------------
// Open color picker for the border color
//------------------------------------------------------------------------------
void PrintPreviewWindow::OnBorderColorPick() {
    if (m_selectedPage < 0 || m_selectedPage >= static_cast<int>(m_pageSettings.size()))
        return;

    PageSettings& ps = m_pageSettings[m_selectedPage];

    CHOOSECOLORW cc = {};
    cc.lStructSize  = sizeof(cc);
    cc.hwndOwner    = m_hwnd;
    cc.rgbResult    = ps.borderColor;
    cc.lpCustColors = m_customColors;
    cc.Flags        = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColorW(&cc)) {
        ps.borderColor = cc.rgbResult;
        InvalidatePreview();
    }
}

//------------------------------------------------------------------------------
// Enable/disable watermark sub-controls
//------------------------------------------------------------------------------
void PrintPreviewWindow::UpdateWatermarkControlStates() {
    bool enabled = IsDlgButtonChecked(m_hwnd, IDC_PP_WATERMARK_ENABLE) == BST_CHECKED;
    EnableWindow(GetDlgItem(m_hwnd, IDC_PP_WATERMARK_TEXT),     enabled);
    EnableWindow(GetDlgItem(m_hwnd, IDC_PP_WATERMARK_COLOR),    enabled);
    EnableWindow(GetDlgItem(m_hwnd, IDC_PP_WATERMARK_FONT_BTN), enabled);
}

//------------------------------------------------------------------------------
// Open color picker for watermark color
//------------------------------------------------------------------------------
void PrintPreviewWindow::OnWatermarkColorPick() {
    if (m_selectedPage < 0 || m_selectedPage >= static_cast<int>(m_pageSettings.size()))
        return;

    PageSettings& ps = m_pageSettings[m_selectedPage];

    CHOOSECOLORW cc = {};
    cc.lStructSize  = sizeof(cc);
    cc.hwndOwner    = m_hwnd;
    cc.rgbResult    = ps.watermarkColor;
    cc.lpCustColors = m_watermarkCustomColors;
    cc.Flags        = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColorW(&cc)) {
        ps.watermarkColor = cc.rgbResult;
        InvalidatePreview();
    }
}

//------------------------------------------------------------------------------
// Open font picker for watermark font
//------------------------------------------------------------------------------
void PrintPreviewWindow::OnWatermarkChooseFont() {
    if (m_selectedPage < 0 || m_selectedPage >= static_cast<int>(m_pageSettings.size()))
        return;

    PageSettings& ps = m_pageSettings[m_selectedPage];

    LOGFONTW lf = {};
    wcscpy_s(lf.lfFaceName, ps.watermarkFontName.c_str());
    // Use a reasonable display size for the dialog (actual rendering may auto-size)
    HDC hScreen = GetDC(nullptr);
    int dpiY = GetDeviceCaps(hScreen, LOGPIXELSY);
    ReleaseDC(nullptr, hScreen);
    int ptSize = (ps.watermarkFontSize > 0) ? ps.watermarkFontSize : 72;
    lf.lfHeight = -MulDiv(ptSize, dpiY, 72);
    lf.lfWeight = ps.watermarkFontWeight;
    lf.lfItalic = ps.watermarkFontItalic ? TRUE : FALSE;
    lf.lfCharSet = DEFAULT_CHARSET;

    CHOOSEFONTW cf = {};
    cf.lStructSize = sizeof(cf);
    cf.hwndOwner   = m_hwnd;
    cf.lpLogFont   = &lf;
    cf.iPointSize  = ptSize * 10;
    cf.Flags       = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_EFFECTS;
    cf.rgbColors   = ps.watermarkColor;

    if (ChooseFontW(&cf)) {
        ps.watermarkFontName   = lf.lfFaceName;
        ps.watermarkFontSize   = cf.iPointSize / 10;
        ps.watermarkFontWeight = lf.lfWeight;
        ps.watermarkFontItalic = lf.lfItalic != 0;
        ps.watermarkColor      = cf.rgbColors;
        UpdateWatermarkFontLabel();
        InvalidatePreview();
    }
}

//------------------------------------------------------------------------------
// Update the watermark font label to show current font info
//------------------------------------------------------------------------------
void PrintPreviewWindow::UpdateWatermarkFontLabel() {
    if (m_selectedPage < 0 || m_selectedPage >= static_cast<int>(m_pageSettings.size()))
        return;

    const PageSettings& ps = m_pageSettings[m_selectedPage];

    std::wstring label = ps.watermarkFontName;
    if (ps.watermarkFontSize > 0) {
        label += L", " + std::to_wstring(ps.watermarkFontSize) + L"pt";
    } else {
        label += L", Auto";
    }
    if (ps.watermarkFontWeight >= FW_BOLD)
        label += L", Bold";
    if (ps.watermarkFontItalic)
        label += L", Italic";

    SetDlgItemTextW(m_hwnd, IDC_PP_WATERMARK_FONT_LABEL, label.c_str());
}

//------------------------------------------------------------------------------
// Handle orientation change - swap page dimensions and repaginate
//------------------------------------------------------------------------------
void PrintPreviewWindow::OnOrientationChange() {
    int sel = static_cast<int>(SendMessageW(
        GetDlgItem(m_hwnd, IDC_PP_ORIENTATION), CB_GETCURSEL, 0, 0));
    bool landscape = (sel == 1);

    if (landscape == m_landscape) return;
    m_landscape = landscape;

    if (m_landscape) {
        m_pageWidthMil  = m_origPageHeightMil;
        m_pageHeightMil = m_origPageWidthMil;
    } else {
        m_pageWidthMil  = m_origPageWidthMil;
        m_pageHeightMil = m_origPageHeightMil;
    }

    SyncPageFromControls();
    Paginate();
    UpdatePageInfo();
    InvalidatePreview();
}

//------------------------------------------------------------------------------
// Parse a page range string like "1-3,5,7-9" into a sorted list of 0-based
// page indices. Returns all pages if the range is empty or invalid.
//------------------------------------------------------------------------------
std::vector<int> PrintPreviewWindow::ParsePageRange(
        const std::wstring& range, int totalPages) const {
    std::vector<int> result;
    if (range.empty()) {
        for (int i = 0; i < totalPages; ++i) result.push_back(i);
        return result;
    }

    std::wistringstream ss(range);
    std::wstring token;
    while (std::getline(ss, token, L',')) {
        // Trim whitespace
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
            for (int i = from; i <= to; ++i)
                result.push_back(i - 1);
        } else {
            int p = _wtoi(token.c_str());
            if (p >= 1 && p <= totalPages)
                result.push_back(p - 1);
        }
    }

    // Remove duplicates and sort
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());

    if (result.empty()) {
        for (int i = 0; i < totalPages; ++i) result.push_back(i);
    }
    return result;
}

//------------------------------------------------------------------------------
// Enable/disable column gap based on column count
//------------------------------------------------------------------------------
void PrintPreviewWindow::UpdateColumnControlStates() {
    int colIdx = static_cast<int>(SendMessageW(
        GetDlgItem(m_hwnd, IDC_PP_COLUMNS), CB_GETCURSEL, 0, 0));
    int cols = colIdx + 1;
    bool multiCol = (cols > 1);
    EnableWindow(GetDlgItem(m_hwnd, IDC_PP_COL_GAP), multiCol);
}

//------------------------------------------------------------------------------
// Show border settings dialog (currently opens color picker)
//------------------------------------------------------------------------------
void PrintPreviewWindow::OnBorderMore() {
    OnBorderColorPick();
}

//------------------------------------------------------------------------------
// Save print settings to a file
//------------------------------------------------------------------------------
void PrintPreviewWindow::OnSaveSettings() {
    SyncPageFromControls();
    if (m_pageSettings.empty()) return;
    const PageSettings& ps = m_pageSettings[0];

    // Get save file path
    wchar_t filePath[MAX_PATH] = L"PrintSettings.ini";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"Settings Files (*.ini)\0*.ini\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"ini";

    if (!GetSaveFileNameW(&ofn)) return;

    // Write settings to INI file
    auto writeInt = [&](const wchar_t* key, int val) {
        wchar_t buf[32];
        swprintf_s(buf, L"%d", val);
        WritePrivateProfileStringW(L"PrintSettings", key, buf, filePath);
    };
    auto writeStr = [&](const wchar_t* key, const std::wstring& val) {
        WritePrivateProfileStringW(L"PrintSettings", key, val.c_str(), filePath);
    };

    writeInt(L"MarginLeft", ps.marginLeft);
    writeInt(L"MarginRight", ps.marginRight);
    writeInt(L"MarginTop", ps.marginTop);
    writeInt(L"MarginBottom", ps.marginBottom);
    writeInt(L"GutterMil", ps.gutterMil);
    writeStr(L"HeaderText", ps.headerText);
    writeStr(L"FooterLeft", ps.footerLeft);
    writeStr(L"FooterRight", ps.footerRight);
    writeStr(L"FontName", ps.fontName);
    writeInt(L"FontSize", ps.fontSize);
    writeInt(L"FontWeight", ps.fontWeight);
    writeInt(L"FontItalic", ps.fontItalic ? 1 : 0);
    writeInt(L"Columns", ps.columns);
    writeInt(L"ColumnGapPt", ps.columnGapPt);
    writeInt(L"BorderEnabled", ps.borderEnabled ? 1 : 0);
    writeInt(L"BorderStyle", ps.borderStyle);
    writeInt(L"BorderWidthPt", ps.borderWidthPt);
    writeInt(L"BorderPaddingPt", ps.borderPaddingPt);
    writeInt(L"BorderTop", ps.borderTop ? 1 : 0);
    writeInt(L"BorderBottom", ps.borderBottom ? 1 : 0);
    writeInt(L"BorderLeft", ps.borderLeft ? 1 : 0);
    writeInt(L"BorderRight", ps.borderRight ? 1 : 0);
    writeInt(L"BorderColor", static_cast<int>(ps.borderColor));
    writeInt(L"LineNumbers", ps.lineNumbers ? 1 : 0);
    writeInt(L"LineSpacing", ps.lineSpacing);
    writeInt(L"LineNumberColor", static_cast<int>(ps.lineNumberColor));
    writeStr(L"LineNumberFontName", ps.lineNumberFontName);
    writeInt(L"LineNumberFontSize", ps.lineNumberFontSize);
    writeInt(L"LineNumberFontWeight", ps.lineNumberFontWeight);
    writeInt(L"LineNumberFontItalic", ps.lineNumberFontItalic ? 1 : 0);
    writeInt(L"WatermarkEnabled", ps.watermarkEnabled ? 1 : 0);
    writeStr(L"WatermarkText", ps.watermarkText);
    writeInt(L"WatermarkColor", static_cast<int>(ps.watermarkColor));
    writeStr(L"WatermarkFontName", ps.watermarkFontName);
    writeInt(L"WatermarkFontSize", ps.watermarkFontSize);
    writeInt(L"WatermarkFontWeight", ps.watermarkFontWeight);
    writeInt(L"WatermarkFontItalic", ps.watermarkFontItalic ? 1 : 0);
    writeInt(L"Landscape", m_landscape ? 1 : 0);
    writeInt(L"Copies", m_copies);
    writeInt(L"Collate", m_collate ? 1 : 0);

    MessageBoxW(m_hwnd, L"Print settings saved successfully.", L"Save Settings",
                MB_OK | MB_ICONINFORMATION);
}

//------------------------------------------------------------------------------
// Load print settings from a file
//------------------------------------------------------------------------------
void PrintPreviewWindow::OnLoadSettings() {
    // Get load file path
    wchar_t filePath[MAX_PATH] = L"";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"Settings Files (*.ini)\0*.ini\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"ini";

    if (!GetOpenFileNameW(&ofn)) return;

    // Read settings from INI file
    auto readInt = [&](const wchar_t* key, int defVal) -> int {
        return GetPrivateProfileIntW(L"PrintSettings", key, defVal, filePath);
    };
    auto readStr = [&](const wchar_t* key, const wchar_t* defVal) -> std::wstring {
        wchar_t buf[256] = {};
        GetPrivateProfileStringW(L"PrintSettings", key, defVal, buf, 256, filePath);
        return buf;
    };

    if (m_pageSettings.empty()) m_pageSettings.push_back(PageSettings{});
    PageSettings& ps = m_pageSettings[0];

    ps.marginLeft = readInt(L"MarginLeft", 1000);
    ps.marginRight = readInt(L"MarginRight", 1000);
    ps.marginTop = readInt(L"MarginTop", 1000);
    ps.marginBottom = readInt(L"MarginBottom", 1000);
    ps.gutterMil = readInt(L"GutterMil", 0);
    ps.headerText = readStr(L"HeaderText", L"");
    ps.footerLeft = readStr(L"FooterLeft", L"");
    ps.footerRight = readStr(L"FooterRight", L"Page &p of &P");
    ps.fontName = readStr(L"FontName", L"Consolas");
    ps.fontSize = readInt(L"FontSize", 11);
    ps.fontWeight = readInt(L"FontWeight", FW_NORMAL);
    ps.fontItalic = readInt(L"FontItalic", 0) != 0;
    ps.columns = readInt(L"Columns", 1);
    ps.columnGapPt = readInt(L"ColumnGapPt", 12);
    ps.borderEnabled = readInt(L"BorderEnabled", 0) != 0;
    ps.borderStyle = readInt(L"BorderStyle", 0);
    ps.borderWidthPt = readInt(L"BorderWidthPt", 1);
    ps.borderPaddingPt = readInt(L"BorderPaddingPt", 0);
    ps.borderTop = readInt(L"BorderTop", 1) != 0;
    ps.borderBottom = readInt(L"BorderBottom", 1) != 0;
    ps.borderLeft = readInt(L"BorderLeft", 1) != 0;
    ps.borderRight = readInt(L"BorderRight", 1) != 0;
    ps.borderColor = static_cast<COLORREF>(readInt(L"BorderColor", 0));
    ps.lineNumbers = readInt(L"LineNumbers", 0) != 0;
    ps.lineSpacing = readInt(L"LineSpacing", 100);
    ps.lineNumberColor = static_cast<COLORREF>(readInt(L"LineNumberColor", RGB(140, 140, 140)));
    ps.lineNumberFontName = readStr(L"LineNumberFontName", L"");
    ps.lineNumberFontSize = readInt(L"LineNumberFontSize", 0);
    ps.lineNumberFontWeight = readInt(L"LineNumberFontWeight", 0);
    ps.lineNumberFontItalic = readInt(L"LineNumberFontItalic", 0) != 0;
    ps.watermarkEnabled = readInt(L"WatermarkEnabled", 0) != 0;
    ps.watermarkText = readStr(L"WatermarkText", L"DRAFT");
    ps.watermarkColor = static_cast<COLORREF>(readInt(L"WatermarkColor", RGB(220, 220, 220)));
    ps.watermarkFontName = readStr(L"WatermarkFontName", L"Arial");
    ps.watermarkFontSize = readInt(L"WatermarkFontSize", 0);
    ps.watermarkFontWeight = readInt(L"WatermarkFontWeight", FW_BOLD);
    ps.watermarkFontItalic = readInt(L"WatermarkFontItalic", 0) != 0;

    // Update orientation
    int landscape = readInt(L"Landscape", 0);
    if (landscape != (m_landscape ? 1 : 0)) {
        m_landscape = landscape != 0;
        SendMessageW(GetDlgItem(m_hwnd, IDC_PP_ORIENTATION), CB_SETCURSEL, m_landscape ? 1 : 0, 0);
        if (m_landscape) {
            m_pageWidthMil = m_origPageHeightMil;
            m_pageHeightMil = m_origPageWidthMil;
        } else {
            m_pageWidthMil = m_origPageWidthMil;
            m_pageHeightMil = m_origPageHeightMil;
        }
    }

    // Update copies and collate
    m_copies = readInt(L"Copies", 1);
    m_collate = readInt(L"Collate", 1) != 0;
    {
        wchar_t buf[32];
        swprintf_s(buf, L"%d", m_copies);
        SetDlgItemTextW(m_hwnd, IDC_PP_COPIES, buf);
    }
    CheckDlgButton(m_hwnd, IDC_PP_COLLATE, m_collate ? BST_CHECKED : BST_UNCHECKED);

    // Apply to all pages
    for (size_t i = 1; i < m_pageSettings.size(); ++i)
        m_pageSettings[i] = ps;

    // Update UI
    m_suppressSync = true;
    SyncControlsToPage(m_selectedPage);
    m_suppressSync = false;

    Paginate();
    UpdatePageInfo();
    InvalidatePreview();

    MessageBoxW(m_hwnd, L"Print settings loaded successfully.", L"Load Settings",
                MB_OK | MB_ICONINFORMATION);
}

//------------------------------------------------------------------------------
// Update line number control states based on checkbox
//------------------------------------------------------------------------------
void PrintPreviewWindow::UpdateLineNumberControlStates() {
    if (m_pageSettings.empty()) return;
    const PageSettings& ps = m_pageSettings[m_selectedPage];
    BOOL enabled = ps.lineNumbers ? TRUE : FALSE;
    EnableWindow(GetDlgItem(m_hwnd, IDC_PP_LINENUM_COLOR), enabled);
    EnableWindow(GetDlgItem(m_hwnd, IDC_PP_LINENUM_FONT), enabled);
    UpdateLineNumberFontLabel();
}

//------------------------------------------------------------------------------
// Pick color for line numbers
//------------------------------------------------------------------------------
void PrintPreviewWindow::OnLineNumberColorPick() {
    if (m_pageSettings.empty()) return;
    PageSettings& ps = m_pageSettings[m_selectedPage];

    CHOOSECOLORW cc = {};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = m_hwnd;
    cc.lpCustColors = m_lineNumCustomColors;
    cc.rgbResult = ps.lineNumberColor;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColorW(&cc)) {
        ps.lineNumberColor = cc.rgbResult;
        InvalidatePreview();
    }
}

//------------------------------------------------------------------------------
// Choose font for line numbers
//------------------------------------------------------------------------------
void PrintPreviewWindow::OnLineNumberChooseFont() {
    if (m_pageSettings.empty()) return;
    PageSettings& ps = m_pageSettings[m_selectedPage];

    // If no custom font set, use body font as starting point
    std::wstring fontName = ps.lineNumberFontName.empty() ? ps.fontName : ps.lineNumberFontName;
    int fontSize = ps.lineNumberFontSize == 0 ? ps.fontSize : ps.lineNumberFontSize;
    int fontWeight = ps.lineNumberFontWeight == 0 ? ps.fontWeight : ps.lineNumberFontWeight;

    LOGFONTW lf = {};
    HDC hdc = GetDC(m_hwnd);
    lf.lfHeight = -MulDiv(fontSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(m_hwnd, hdc);
    lf.lfWeight = fontWeight;
    lf.lfItalic = ps.lineNumberFontItalic ? TRUE : FALSE;
    wcscpy_s(lf.lfFaceName, fontName.c_str());

    CHOOSEFONTW cf = {};
    cf.lStructSize = sizeof(cf);
    cf.hwndOwner = m_hwnd;
    cf.lpLogFont = &lf;
    cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_EFFECTS;

    if (ChooseFontW(&cf)) {
        ps.lineNumberFontName = lf.lfFaceName;
        ps.lineNumberFontSize = cf.iPointSize / 10;
        ps.lineNumberFontWeight = lf.lfWeight;
        ps.lineNumberFontItalic = lf.lfItalic != 0;
        UpdateLineNumberFontLabel();
        Paginate();
        UpdatePageInfo();
        InvalidatePreview();
    }
}

//------------------------------------------------------------------------------
// Update the line number font label
//------------------------------------------------------------------------------
void PrintPreviewWindow::UpdateLineNumberFontLabel() {
    if (m_pageSettings.empty()) return;
    const PageSettings& ps = m_pageSettings[m_selectedPage];

    std::wstring label;
    if (ps.lineNumberFontName.empty() || ps.lineNumberFontSize == 0) {
        label = L"(body font)";
    } else {
        wchar_t buf[128];
        swprintf_s(buf, L"%s, %dpt", ps.lineNumberFontName.c_str(), ps.lineNumberFontSize);
        label = buf;
    }
    SetDlgItemTextW(m_hwnd, IDC_PP_LINENUM_FONT_LABEL, label.c_str());
}

//------------------------------------------------------------------------------
// Add external files to the print preview
//------------------------------------------------------------------------------
void PrintPreviewWindow::OnAddFiles() {
    // Use a large buffer for multi-select (directory + null-separated filenames)
    const DWORD bufSize = 32768;
    std::vector<wchar_t> buf(bufSize, L'\0');

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFile = buf.data();
    ofn.nMaxFile = bufSize;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0"
                      L"Markdown (*.md;*.markdown)\0*.md;*.markdown\0"
                      L"Source Code (*.cpp;*.c;*.h;*.hpp;*.cs;*.java;*.py;*.js;*.ts)\0*.cpp;*.c;*.h;*.hpp;*.cs;*.java;*.py;*.js;*.ts\0"
                      L"Web Files (*.html;*.htm;*.css;*.xml;*.json)\0*.html;*.htm;*.css;*.xml;*.json\0"
                      L"Log Files (*.log)\0*.log\0"
                      L"All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 6;  // Default to All Files
    ofn.lpstrTitle = L"Add Files to Print Preview";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY
              | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

    if (!GetOpenFileNameW(&ofn))
        return;

    // Parse selected files - multi-select returns:
    //   directory\0file1\0file2\0\0   (multiple files)
    //   fullpath\0\0                  (single file)
    std::vector<std::wstring> filePaths;

    const wchar_t* p = buf.data();
    std::wstring first(p);
    p += first.size() + 1;

    if (*p == L'\0') {
        // Single file selected
        filePaths.push_back(first);
    } else {
        // Multiple files: first string is the directory
        std::wstring dir = first;
        if (!dir.empty() && dir.back() != L'\\')
            dir += L'\\';
        while (*p) {
            std::wstring fname(p);
            filePaths.push_back(dir + fname);
            p += fname.size() + 1;
        }
    }

    if (filePaths.empty())
        return;

    // Read each file and append to the current text
    int filesAdded = 0;
    for (const auto& path : filePaths) {
        FileReadResult result = FileIO::ReadFile(path);
        if (!result.success) {
            std::wstring msg = L"Could not read file:\n" + path;
            if (!result.errorMessage.empty())
                msg += L"\n\n" + result.errorMessage;
            MessageBoxW(m_hwnd, msg.c_str(), L"Add Files", MB_OK | MB_ICONWARNING);
            continue;
        }

        if (result.content.empty())
            continue;

        // Append a separator and the file content
        if (!m_text.empty())
            m_text += L"\r\n";
        m_text += result.content;
        filesAdded++;
    }

    if (filesAdded == 0)
        return;

    // Re-paginate with the updated text and refresh the preview
    Paginate();
    if (m_selectedPage >= static_cast<int>(m_pages.size()))
        m_selectedPage = static_cast<int>(m_pages.size()) - 1;
    UpdatePageInfo();
    InvalidatePreview();
}

//------------------------------------------------------------------------------
// Detect the default printer type (dot matrix / impact vs laser/inkjet)
//------------------------------------------------------------------------------
void PrintPreviewWindow::DetectPrinterType() {
    m_isDotMatrix = false;
    std::wstring printerInfo;

    PRINTDLGW pd = {};
    pd.lStructSize = sizeof(pd);
    pd.hwndOwner = m_hwnd;
    pd.Flags = PD_RETURNDEFAULT | PD_RETURNDC;

    if (PrintDlgW(&pd)) {
        if (pd.hDC) {
            // Check printer technology
            int tech = GetDeviceCaps(pd.hDC, TECHNOLOGY);
            int dpiX = GetDeviceCaps(pd.hDC, LOGPIXELSX);
            int dpiY = GetDeviceCaps(pd.hDC, LOGPIXELSY);

            // DT_CHARSTREAM (4) indicates a character-stream printer (dot matrix)
            // Also check for very low DPI which is typical of dot matrix (72-180 DPI)
            if (tech == DT_CHARSTREAM || (dpiX <= 180 && dpiY <= 180)) {
                m_isDotMatrix = true;
            }

            DeleteDC(pd.hDC);
        }

        // Get printer name from DEVNAMES
        if (pd.hDevNames) {
            DEVNAMES* dn = reinterpret_cast<DEVNAMES*>(GlobalLock(pd.hDevNames));
            if (dn) {
                const wchar_t* deviceName = reinterpret_cast<const wchar_t*>(dn) + dn->wDeviceOffset;
                printerInfo = deviceName;
                GlobalUnlock(pd.hDevNames);
            }
        }

        // Get additional info from DEVMODE
        if (pd.hDevMode) {
            DEVMODEW* dm = reinterpret_cast<DEVMODEW*>(GlobalLock(pd.hDevMode));
            if (dm) {
                // Check if printer name suggests dot matrix
                std::wstring devName = dm->dmDeviceName;
                // Common dot matrix printer keywords
                if (devName.find(L"LX-") != std::wstring::npos ||
                    devName.find(L"LQ-") != std::wstring::npos ||
                    devName.find(L"FX-") != std::wstring::npos ||
                    devName.find(L"DFX") != std::wstring::npos ||
                    devName.find(L"PLQ") != std::wstring::npos ||
                    devName.find(L"Dot Matrix") != std::wstring::npos ||
                    devName.find(L"dot matrix") != std::wstring::npos ||
                    devName.find(L"Impact") != std::wstring::npos ||
                    devName.find(L"Passbook") != std::wstring::npos ||
                    devName.find(L"OKI") != std::wstring::npos ||
                    devName.find(L"ML-") != std::wstring::npos) {
                    m_isDotMatrix = true;
                }
                GlobalUnlock(pd.hDevMode);
            }
            GlobalFree(pd.hDevMode);
        }
        if (pd.hDevNames) GlobalFree(pd.hDevNames);
    }

    // Update printer info label
    if (!printerInfo.empty()) {
        std::wstring info = L"Printer: " + printerInfo;
        if (m_isDotMatrix) {
            info += L"  [Dot Matrix / Impact]";
        }
        SetDlgItemTextW(m_hwnd, IDC_PP_PRINTER_INFO, info.c_str());
    } else {
        SetDlgItemTextW(m_hwnd, IDC_PP_PRINTER_INFO, L"Printer: (default)");
    }

    // Auto-configure for dot matrix if detected
    if (m_isDotMatrix) {
        // Suggest Draft quality for speed
        m_printQuality = 1;  // Draft
        SendMessageW(GetDlgItem(m_hwnd, IDC_PP_PRINT_QUALITY), CB_SETCURSEL, 1, 0);

        // Suggest Tractor Feed
        m_paperSource = 4;  // Tractor Feed
        SendMessageW(GetDlgItem(m_hwnd, IDC_PP_PAPER_SOURCE), CB_SETCURSEL, 4, 0);

        // Enable form feed by default for dot matrix
        m_formFeed = true;
        CheckDlgButton(m_hwnd, IDC_PP_FORMFEED, BST_CHECKED);
    }

    UpdatePrinterControlStates();
}

//------------------------------------------------------------------------------
// Update printer control enable states based on paper source / type
//------------------------------------------------------------------------------
void PrintPreviewWindow::UpdatePrinterControlStates() {
    // Enable condensed mode for all printers (useful for wide text)
    // Tractor/continuous sources make condensed more relevant
    bool isTractorOrContinuous = (m_paperSource == 4 || m_paperSource == 5);

    // Show/hide condensed and form feed based on context
    // (always enabled - useful for any printer, but especially dot matrix)
    EnableWindow(GetDlgItem(m_hwnd, IDC_PP_CONDENSED), TRUE);
    EnableWindow(GetDlgItem(m_hwnd, IDC_PP_FORMFEED), TRUE);

    // If tractor feed selected, suggest Fanfold paper sizes
    if (isTractorOrContinuous && m_paperSize == 0) {
        // Don't auto-switch paper size, but it's available
    }
}

} // namespace QNote
