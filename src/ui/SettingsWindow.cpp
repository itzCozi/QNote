//==============================================================================
// QNote - A Lightweight Notepad Clone
// SettingsWindow.cpp - Application settings dialog implementation
//==============================================================================

#include "SettingsWindow.h"
#include "Settings.h"
#include "resource.h"
#include <CommCtrl.h>
#include <commdlg.h>
#include <Uxtheme.h>
#include <algorithm>

namespace QNote {

// Static instance pointer for dialog callback
SettingsWindow* SettingsWindow::s_instance = nullptr;

// Control IDs for each tab page (used for show/hide)
static constexpr int EDITOR_PAGE_IDS[] = {
    IDC_SET_WORDWRAP, IDC_SET_TABSIZE, IDC_SET_LBL_TABSIZE,
    IDC_SET_SCROLLLINES, IDC_SET_LBL_SCROLLLINES,
    IDC_SET_AUTOSAVE, IDC_SET_RTL
};

static constexpr int APPEARANCE_PAGE_IDS[] = {
    IDC_SET_LBL_FONT, IDC_SET_FONTPREVIEW, IDC_SET_FONTBUTTON,
    IDC_SET_STATUSBAR, IDC_SET_LINENUMBERS, IDC_SET_WHITESPACE,
    IDC_SET_LBL_ZOOM, IDC_SET_ZOOM
};

static constexpr int DEFAULTS_PAGE_IDS[] = {
    IDC_SET_LBL_ENCODING, IDC_SET_ENCODING,
    IDC_SET_LBL_LINEENDING, IDC_SET_LINEENDING
};

//------------------------------------------------------------------------------
// Show settings dialog (static entry point)
//------------------------------------------------------------------------------
bool SettingsWindow::Show(HWND parent, HINSTANCE hInstance, AppSettings& settings) {
    SettingsWindow instance;
    instance.m_hInstance = hInstance;
    instance.m_settings = &settings;
    instance.m_editSettings = settings;  // working copy
    instance.m_fontName = settings.fontName;
    instance.m_fontSize = settings.fontSize;
    instance.m_fontWeight = settings.fontWeight;
    instance.m_fontItalic = settings.fontItalic;
    
    s_instance = &instance;
    
    INT_PTR result = DialogBoxParamW(
        hInstance,
        MAKEINTRESOURCEW(IDD_SETTINGS),
        parent,
        DlgProc,
        0
    );
    
    s_instance = nullptr;
    
    if (result == IDOK) {
        settings = instance.m_editSettings;
        return true;
    }
    return false;
}

//------------------------------------------------------------------------------
// Dialog procedure
//------------------------------------------------------------------------------
INT_PTR CALLBACK SettingsWindow::DlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG:
            if (s_instance) {
                s_instance->OnInit(hwnd);
            }
            return TRUE;
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK:
                    if (s_instance) s_instance->OnOK();
                    return TRUE;
                case IDCANCEL:
                    EndDialog(hwnd, IDCANCEL);
                    return TRUE;
                case IDC_SET_FONTBUTTON:
                    if (s_instance) s_instance->OnFontButton();
                    return TRUE;
            }
            break;
        
        case WM_NOTIFY: {
            NMHDR* pnm = reinterpret_cast<NMHDR*>(lParam);
            if (pnm && pnm->idFrom == IDC_SETTINGS_TAB && pnm->code == TCN_SELCHANGE) {
                if (s_instance) s_instance->OnTabChanged();
            }
            break;
        }
    }
    return FALSE;
}

//------------------------------------------------------------------------------
// Initialize dialog
//------------------------------------------------------------------------------
void SettingsWindow::OnInit(HWND hwnd) {
    m_hwnd = hwnd;
    
    // Enable themed tab background so child controls match the tab interior
    EnableThemeDialogTexture(hwnd, ETDT_ENABLETAB);
    
    // Center dialog on parent window
    HWND parent = GetParent(hwnd);
    if (parent) {
        RECT rcParent, rcDlg;
        GetWindowRect(parent, &rcParent);
        GetWindowRect(hwnd, &rcDlg);
        int dlgW = rcDlg.right - rcDlg.left;
        int dlgH = rcDlg.bottom - rcDlg.top;
        int x = rcParent.left + (rcParent.right - rcParent.left - dlgW) / 2;
        int y = rcParent.top + (rcParent.bottom - rcParent.top - dlgH) / 2;
        SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
    
    // Initialize tab control
    HWND hwndTab = GetDlgItem(hwnd, IDC_SETTINGS_TAB);
    TCITEMW tci = {};
    tci.mask = TCIF_TEXT;
    
    tci.pszText = const_cast<wchar_t*>(L"Editor");
    TabCtrl_InsertItem(hwndTab, 0, &tci);
    
    tci.pszText = const_cast<wchar_t*>(L"Appearance");
    TabCtrl_InsertItem(hwndTab, 1, &tci);
    
    tci.pszText = const_cast<wchar_t*>(L"Defaults");
    TabCtrl_InsertItem(hwndTab, 2, &tci);
    
    // Populate controls from settings
    InitControlsFromSettings();
    
    // Show first page
    ShowPage(0);
}

//------------------------------------------------------------------------------
// Tab selection changed
//------------------------------------------------------------------------------
void SettingsWindow::OnTabChanged() {
    HWND hwndTab = GetDlgItem(m_hwnd, IDC_SETTINGS_TAB);
    int sel = TabCtrl_GetCurSel(hwndTab);
    if (sel >= 0 && sel < PAGE_COUNT) {
        ShowPage(sel);
    }
}

//------------------------------------------------------------------------------
// OK button - commit settings
//------------------------------------------------------------------------------
void SettingsWindow::OnOK() {
    ReadControlsToSettings();
    EndDialog(m_hwnd, IDOK);
}

//------------------------------------------------------------------------------
// Font chooser button
//------------------------------------------------------------------------------
void SettingsWindow::OnFontButton() {
    LOGFONTW lf = {};
    lf.lfWeight = m_fontWeight;
    lf.lfItalic = m_fontItalic ? TRUE : FALSE;
    lf.lfCharSet = DEFAULT_CHARSET;
    wcsncpy_s(lf.lfFaceName, m_fontName.c_str(), LF_FACESIZE - 1);
    
    // Convert point size to logical height
    HDC hdc = GetDC(m_hwnd);
    lf.lfHeight = -MulDiv(m_fontSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(m_hwnd, hdc);
    
    CHOOSEFONTW cf = {};
    cf.lStructSize = sizeof(cf);
    cf.hwndOwner = m_hwnd;
    cf.lpLogFont = &lf;
    cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_EFFECTS;
    cf.nFontType = SCREEN_FONTTYPE;
    
    if (ChooseFontW(&cf)) {
        m_fontName = lf.lfFaceName;
        m_fontSize = cf.iPointSize / 10;
        m_fontWeight = lf.lfWeight;
        m_fontItalic = lf.lfItalic != FALSE;
        UpdateFontPreview();
    }
}

//------------------------------------------------------------------------------
// Show/hide controls for the selected page
//------------------------------------------------------------------------------
void SettingsWindow::ShowPage(int page) {
    auto setVisible = [this](const int ids[], size_t count, bool visible) {
        int cmd = visible ? SW_SHOW : SW_HIDE;
        for (size_t i = 0; i < count; ++i) {
            HWND hwndCtrl = GetDlgItem(m_hwnd, ids[i]);
            if (hwndCtrl) ShowWindow(hwndCtrl, cmd);
        }
    };
    
    setVisible(EDITOR_PAGE_IDS, _countof(EDITOR_PAGE_IDS), page == 0);
    setVisible(APPEARANCE_PAGE_IDS, _countof(APPEARANCE_PAGE_IDS), page == 1);
    setVisible(DEFAULTS_PAGE_IDS, _countof(DEFAULTS_PAGE_IDS), page == 2);
    
    m_currentPage = page;
}

//------------------------------------------------------------------------------
// Populate controls from the working settings copy
//------------------------------------------------------------------------------
void SettingsWindow::InitControlsFromSettings() {
    // --- Editor page ---
    CheckDlgButton(m_hwnd, IDC_SET_WORDWRAP,
        m_editSettings.wordWrap ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(m_hwnd, IDC_SET_TABSIZE, m_editSettings.tabSize, FALSE);
    SetDlgItemInt(m_hwnd, IDC_SET_SCROLLLINES, m_editSettings.scrollLines, FALSE);
    CheckDlgButton(m_hwnd, IDC_SET_AUTOSAVE,
        m_editSettings.fileAutoSave ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(m_hwnd, IDC_SET_RTL,
        m_editSettings.rightToLeft ? BST_CHECKED : BST_UNCHECKED);
    
    // --- Appearance page ---
    UpdateFontPreview();
    CheckDlgButton(m_hwnd, IDC_SET_STATUSBAR,
        m_editSettings.showStatusBar ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(m_hwnd, IDC_SET_LINENUMBERS,
        m_editSettings.showLineNumbers ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(m_hwnd, IDC_SET_WHITESPACE,
        m_editSettings.showWhitespace ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(m_hwnd, IDC_SET_ZOOM, m_editSettings.zoomLevel, FALSE);
    
    // --- Defaults page: Encoding combo ---
    HWND hwndEnc = GetDlgItem(m_hwnd, IDC_SET_ENCODING);
    SendMessageW(hwndEnc, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"ANSI"));
    SendMessageW(hwndEnc, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"UTF-8"));
    SendMessageW(hwndEnc, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"UTF-8 with BOM"));
    SendMessageW(hwndEnc, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"UTF-16 LE"));
    SendMessageW(hwndEnc, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"UTF-16 BE"));
    
    int encIdx = 1;  // default UTF-8
    switch (m_editSettings.defaultEncoding) {
        case TextEncoding::ANSI:     encIdx = 0; break;
        case TextEncoding::UTF8:     encIdx = 1; break;
        case TextEncoding::UTF8_BOM: encIdx = 2; break;
        case TextEncoding::UTF16_LE: encIdx = 3; break;
        case TextEncoding::UTF16_BE: encIdx = 4; break;
    }
    SendMessageW(hwndEnc, CB_SETCURSEL, encIdx, 0);
    
    // --- Defaults page: Line ending combo ---
    HWND hwndEol = GetDlgItem(m_hwnd, IDC_SET_LINEENDING);
    SendMessageW(hwndEol, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Windows (CRLF)"));
    SendMessageW(hwndEol, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Unix (LF)"));
    SendMessageW(hwndEol, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Mac (CR)"));
    
    int eolIdx = 0;
    switch (m_editSettings.defaultLineEnding) {
        case LineEnding::CRLF: eolIdx = 0; break;
        case LineEnding::LF:   eolIdx = 1; break;
        case LineEnding::CR:   eolIdx = 2; break;
    }
    SendMessageW(hwndEol, CB_SETCURSEL, eolIdx, 0);
}

//------------------------------------------------------------------------------
// Read all controls back into the working settings copy
//------------------------------------------------------------------------------
void SettingsWindow::ReadControlsToSettings() {
    // --- Editor page ---
    m_editSettings.wordWrap = IsDlgButtonChecked(m_hwnd, IDC_SET_WORDWRAP) == BST_CHECKED;
    
    int tabSize = static_cast<int>(GetDlgItemInt(m_hwnd, IDC_SET_TABSIZE, nullptr, FALSE));
    m_editSettings.tabSize = (std::max)(1, (std::min)(16, tabSize));
    
    int scrollLines = static_cast<int>(GetDlgItemInt(m_hwnd, IDC_SET_SCROLLLINES, nullptr, FALSE));
    m_editSettings.scrollLines = (std::max)(0, (std::min)(20, scrollLines));
    
    m_editSettings.fileAutoSave = IsDlgButtonChecked(m_hwnd, IDC_SET_AUTOSAVE) == BST_CHECKED;
    m_editSettings.rightToLeft = IsDlgButtonChecked(m_hwnd, IDC_SET_RTL) == BST_CHECKED;
    
    // --- Appearance page (font tracked separately via font chooser) ---
    m_editSettings.fontName = m_fontName;
    m_editSettings.fontSize = m_fontSize;
    m_editSettings.fontWeight = m_fontWeight;
    m_editSettings.fontItalic = m_fontItalic;
    
    m_editSettings.showStatusBar = IsDlgButtonChecked(m_hwnd, IDC_SET_STATUSBAR) == BST_CHECKED;
    m_editSettings.showLineNumbers = IsDlgButtonChecked(m_hwnd, IDC_SET_LINENUMBERS) == BST_CHECKED;
    m_editSettings.showWhitespace = IsDlgButtonChecked(m_hwnd, IDC_SET_WHITESPACE) == BST_CHECKED;
    
    int zoom = static_cast<int>(GetDlgItemInt(m_hwnd, IDC_SET_ZOOM, nullptr, FALSE));
    m_editSettings.zoomLevel = (std::max)(25, (std::min)(500, zoom));
    
    // --- Defaults page: Encoding ---
    HWND hwndEnc = GetDlgItem(m_hwnd, IDC_SET_ENCODING);
    int encIdx = static_cast<int>(SendMessageW(hwndEnc, CB_GETCURSEL, 0, 0));
    switch (encIdx) {
        case 0: m_editSettings.defaultEncoding = TextEncoding::ANSI; break;
        case 1: m_editSettings.defaultEncoding = TextEncoding::UTF8; break;
        case 2: m_editSettings.defaultEncoding = TextEncoding::UTF8_BOM; break;
        case 3: m_editSettings.defaultEncoding = TextEncoding::UTF16_LE; break;
        case 4: m_editSettings.defaultEncoding = TextEncoding::UTF16_BE; break;
        default: break;
    }
    
    // --- Defaults page: Line ending ---
    HWND hwndEol = GetDlgItem(m_hwnd, IDC_SET_LINEENDING);
    int eolIdx = static_cast<int>(SendMessageW(hwndEol, CB_GETCURSEL, 0, 0));
    switch (eolIdx) {
        case 0: m_editSettings.defaultLineEnding = LineEnding::CRLF; break;
        case 1: m_editSettings.defaultLineEnding = LineEnding::LF; break;
        case 2: m_editSettings.defaultLineEnding = LineEnding::CR; break;
        default: break;
    }
}

//------------------------------------------------------------------------------
// Update the font preview label
//------------------------------------------------------------------------------
void SettingsWindow::UpdateFontPreview() {
    std::wstring preview = m_fontName + L", " + std::to_wstring(m_fontSize) + L"pt";
    if (m_fontWeight >= FW_BOLD) preview += L", Bold";
    if (m_fontItalic) preview += L", Italic";
    SetDlgItemTextW(m_hwnd, IDC_SET_FONTPREVIEW, preview.c_str());
}

} // namespace QNote
