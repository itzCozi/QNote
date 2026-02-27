//==============================================================================
// QNote - A Lightweight Notepad Clone
// PrintPreviewWindow.cpp - Print preview dialog implementation
//==============================================================================

#include "PrintPreviewWindow.h"
#include "resource.h"
#include <CommCtrl.h>
#include <Commdlg.h>
#include <algorithm>
#include <sstream>
#include <ctime>
#include <cstdio>

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
                                       const PAGESETUPDLGW& pageSetup) {
    PrintPreviewWindow instance;
    instance.m_hInstance = hInstance;
    instance.m_text      = text;
    instance.m_docName   = docName;

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
        ps.pageLines.resize(instance.m_pages.size());
        for (size_t i = 0; i < instance.m_pages.size(); ++i)
            ps.pageLines[i] = std::move(instance.m_pages[i].lines);
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
                s_instance->OnMouseWheel(delta);
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

    int pageWidthPx  = MulDiv(m_pageWidthMil,  refDPI, 1000);
    int pageHeightPx = MulDiv(m_pageHeightMil, refDPI, 1000);
    int leftPx   = MulDiv(base.marginLeft,   refDPI, 1000);
    int rightPx  = MulDiv(base.marginRight,  refDPI, 1000);
    int topPx    = MulDiv(base.marginTop,    refDPI, 1000);
    int bottomPx = MulDiv(base.marginBottom, refDPI, 1000);

    int printWidth  = pageWidthPx - leftPx - rightPx;
    int printHeight = pageHeightPx - topPx - bottomPx;
    if (printWidth  <= 0) printWidth  = 100;
    if (printHeight <= 0) printHeight = lineHeight;

    int linesPerPage = printHeight / lineHeight;
    if (linesPerPage < 1) linesPerPage = 1;

    // Word-wrap text into lines
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
            SIZE sz;
            GetTextExtentPoint32W(hdc, rawLine.c_str(),
                                  static_cast<int>(rawLine.length()), &sz);
            if (sz.cx <= printWidth) {
                allLines.push_back(rawLine);
                break;
            }
            size_t breakPos = rawLine.length();
            for (size_t i = rawLine.length(); i > 0; --i) {
                GetTextExtentPoint32W(hdc, rawLine.c_str(),
                                      static_cast<int>(i), &sz);
                if (sz.cx <= printWidth) {
                    size_t lastSpace = rawLine.rfind(L' ', i - 1);
                    if (lastSpace != std::wstring::npos && lastSpace > 0)
                        breakPos = lastSpace + 1;
                    else
                        breakPos = i;
                    break;
                }
            }
            if (breakPos >= rawLine.length()) breakPos = 1;
            allLines.push_back(rawLine.substr(0, breakPos));
            rawLine = rawLine.substr(breakPos);
            size_t start = rawLine.find_first_not_of(L' ');
            if (start != std::wstring::npos && start > 0)
                rawLine = rawLine.substr(start);
        }
    }

    // Distribute lines into pages
    int idx = 0;
    int total = static_cast<int>(allLines.size());
    if (total == 0) {
        m_pages.push_back(PageData{});
    } else {
        while (idx < total) {
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
    L.pageW = ctrlW - 2 * sideMargin;
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

    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    if (lineHeight < 1) lineHeight = 1;

    int pageNum = pageIdx + 1;
    int textX = px + mL;
    int textW = pw - mL - mR;

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

    RECT rcBodyClip = { px + mL, py + mT, px + pw - mR, py + ph - mB };
    HRGN hBodyRgn = CreateRectRgnIndirect(&rcBodyClip);
    ExtSelectClipRgn(hdc, hBodyRgn, RGN_AND);

    if (pageIdx >= 0 && pageIdx < static_cast<int>(m_pages.size())) {
        int y = py + mT;
        for (const auto& line : m_pages[pageIdx].lines) {
            if (y + lineHeight > py + ph - mB) break;
            if (!line.empty())
                TextOutW(hdc, textX, y, line.c_str(),
                         static_cast<int>(line.length()));
            y += lineHeight;
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

    // Restore DC and clean up font
    RestoreDC(hdc, savedDC);
    DeleteObject(hFont);
}

//------------------------------------------------------------------------------
// Handle spin button delta: ±0.05 inches per click
//------------------------------------------------------------------------------
void PrintPreviewWindow::OnSpinDelta(int editId, int delta) {
    wchar_t buf[64] = {};
    GetDlgItemTextW(m_hwnd, editId, buf, 64);
    double val = _wtof(buf);

    val -= delta * 0.05;
    if (val < 0.0)  val = 0.0;
    if (val > 5.0)  val = 5.0;

    swprintf_s(buf, L"%.2f", val);
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

} // namespace QNote
