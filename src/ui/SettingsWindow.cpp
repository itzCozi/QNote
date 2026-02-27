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
#include <ShlObj.h>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

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

static constexpr int BEHAVIOR_PAGE_IDS[] = {
    IDC_SET_LBL_MINIMIZE, IDC_SET_MINIMIZE_MODE,
    IDC_SET_AUTOUPDATE, IDC_SET_PORTABLE,
    IDC_SET_PROMPTSAVE,
    IDC_SET_LBL_SAVESTYLE, IDC_SET_SAVESTYLE,
    IDC_SET_LBL_AUTOSAVEDELAY, IDC_SET_AUTOSAVEDELAY
};

static constexpr int FILEASSOC_PAGE_IDS[] = {
    IDC_SET_ASSOC_TXT, IDC_SET_ASSOC_LOG, IDC_SET_ASSOC_MD,
    IDC_SET_ASSOC_INI, IDC_SET_ASSOC_CFG, IDC_SET_ASSOC_JSON,
    IDC_SET_ASSOC_XML, IDC_SET_ASSOC_CSV, IDC_SET_ASSOC_APPLY
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
                case IDC_SET_ASSOC_APPLY:
                    if (s_instance) s_instance->OnApplyAssociations();
                    return TRUE;
                case IDC_SET_SAVESTYLE:
                    if (HIWORD(wParam) == CBN_SELCHANGE && s_instance) {
                        HWND hwndSS = GetDlgItem(hwnd, IDC_SET_SAVESTYLE);
                        bool isAuto = (SendMessageW(hwndSS, CB_GETCURSEL, 0, 0) == 1);
                        EnableWindow(GetDlgItem(hwnd, IDC_SET_AUTOSAVEDELAY), isAuto);
                        EnableWindow(GetDlgItem(hwnd, IDC_SET_LBL_AUTOSAVEDELAY), isAuto);
                    }
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
    
    tci.pszText = const_cast<wchar_t*>(L"Behavior");
    TabCtrl_InsertItem(hwndTab, 3, &tci);
    
    tci.pszText = const_cast<wchar_t*>(L"File Assoc.");
    TabCtrl_InsertItem(hwndTab, 4, &tci);
    
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
    setVisible(BEHAVIOR_PAGE_IDS, _countof(BEHAVIOR_PAGE_IDS), page == 3);
    setVisible(FILEASSOC_PAGE_IDS, _countof(FILEASSOC_PAGE_IDS), page == 4);
    
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
    
    // --- Behavior page ---
    HWND hwndMin = GetDlgItem(m_hwnd, IDC_SET_MINIMIZE_MODE);
    SendMessageW(hwndMin, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Minimize to Taskbar"));
    SendMessageW(hwndMin, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Minimize to System Tray"));
    SendMessageW(hwndMin, CB_SETCURSEL, m_editSettings.minimizeMode, 0);
    
    CheckDlgButton(m_hwnd, IDC_SET_AUTOUPDATE,
        m_editSettings.autoUpdate ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(m_hwnd, IDC_SET_PORTABLE,
        m_editSettings.portableMode ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(m_hwnd, IDC_SET_PROMPTSAVE,
        m_editSettings.promptSaveOnClose ? BST_CHECKED : BST_UNCHECKED);
    
    // Save style combo
    HWND hwndSaveStyle = GetDlgItem(m_hwnd, IDC_SET_SAVESTYLE);
    SendMessageW(hwndSaveStyle, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Manual (Ctrl+S)"));
    SendMessageW(hwndSaveStyle, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Auto Save"));
    SendMessageW(hwndSaveStyle, CB_SETCURSEL,
        (m_editSettings.saveStyle == SaveStyle::AutoSave) ? 1 : 0, 0);
    
    // Auto-save delay
    SetDlgItemInt(m_hwnd, IDC_SET_AUTOSAVEDELAY, m_editSettings.autoSaveDelayMs, FALSE);
    
    // Show/hide delay field based on save style
    bool isAutoSave = (m_editSettings.saveStyle == SaveStyle::AutoSave);
    EnableWindow(GetDlgItem(m_hwnd, IDC_SET_AUTOSAVEDELAY), isAutoSave);
    EnableWindow(GetDlgItem(m_hwnd, IDC_SET_LBL_AUTOSAVEDELAY), isAutoSave);
    
    // --- File Associations page ---
    // Check which extensions are currently associated
    auto isAssociated = [](const wchar_t* ext) -> bool {
        wchar_t buf[MAX_PATH] = {};
        DWORD len = MAX_PATH;
        HRESULT hr = AssocQueryStringW(ASSOCF_NONE, ASSOCSTR_EXECUTABLE,
                                       ext, L"open", buf, &len);
        if (SUCCEEDED(hr)) {
            // Check if it contains our exe name
            std::wstring path(buf);
            return path.find(L"QNote") != std::wstring::npos ||
                   path.find(L"qnote") != std::wstring::npos;
        }
        return false;
    };
    CheckDlgButton(m_hwnd, IDC_SET_ASSOC_TXT, isAssociated(L".txt") ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(m_hwnd, IDC_SET_ASSOC_LOG, isAssociated(L".log") ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(m_hwnd, IDC_SET_ASSOC_MD, isAssociated(L".md") ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(m_hwnd, IDC_SET_ASSOC_INI, isAssociated(L".ini") ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(m_hwnd, IDC_SET_ASSOC_CFG, isAssociated(L".cfg") ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(m_hwnd, IDC_SET_ASSOC_JSON, isAssociated(L".json") ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(m_hwnd, IDC_SET_ASSOC_XML, isAssociated(L".xml") ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(m_hwnd, IDC_SET_ASSOC_CSV, isAssociated(L".csv") ? BST_CHECKED : BST_UNCHECKED);
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
    
    // --- Behavior page ---
    HWND hwndMin = GetDlgItem(m_hwnd, IDC_SET_MINIMIZE_MODE);
    m_editSettings.minimizeMode = static_cast<int>(SendMessageW(hwndMin, CB_GETCURSEL, 0, 0));
    if (m_editSettings.minimizeMode < 0) m_editSettings.minimizeMode = 1;
    
    m_editSettings.autoUpdate = IsDlgButtonChecked(m_hwnd, IDC_SET_AUTOUPDATE) == BST_CHECKED;
    m_editSettings.portableMode = IsDlgButtonChecked(m_hwnd, IDC_SET_PORTABLE) == BST_CHECKED;
    m_editSettings.promptSaveOnClose = IsDlgButtonChecked(m_hwnd, IDC_SET_PROMPTSAVE) == BST_CHECKED;
    
    // Save style
    HWND hwndSaveStyle = GetDlgItem(m_hwnd, IDC_SET_SAVESTYLE);
    int ssIdx = static_cast<int>(SendMessageW(hwndSaveStyle, CB_GETCURSEL, 0, 0));
    m_editSettings.saveStyle = (ssIdx == 1) ? SaveStyle::AutoSave : SaveStyle::Manual;
    
    // Auto-save delay
    int delay = static_cast<int>(GetDlgItemInt(m_hwnd, IDC_SET_AUTOSAVEDELAY, nullptr, FALSE));
    m_editSettings.autoSaveDelayMs = (std::max)(1000, (std::min)(60000, delay));
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

//------------------------------------------------------------------------------
// Apply file type associations (write to HKCU registry)
//------------------------------------------------------------------------------
void SettingsWindow::OnApplyAssociations() {
    // Get the path to QNote.exe
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    struct AssocEntry {
        int ctrlId;
        const wchar_t* ext;
    };
    
    AssocEntry entries[] = {
        { IDC_SET_ASSOC_TXT,  L".txt" },
        { IDC_SET_ASSOC_LOG,  L".log" },
        { IDC_SET_ASSOC_MD,   L".md" },
        { IDC_SET_ASSOC_INI,  L".ini" },
        { IDC_SET_ASSOC_CFG,  L".cfg" },
        { IDC_SET_ASSOC_JSON, L".json" },
        { IDC_SET_ASSOC_XML,  L".xml" },
        { IDC_SET_ASSOC_CSV,  L".csv" },
    };

    std::wstring progId = L"QNote.TextFile";
    std::wstring openCmd = L"\"" + std::wstring(exePath) + L"\" \"%1\"";

    // Create the ProgId key
    HKEY hProgId = nullptr;
    std::wstring progIdPath = L"Software\\Classes\\" + progId;
    RegCreateKeyExW(HKEY_CURRENT_USER, progIdPath.c_str(), 0, nullptr,
                    REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hProgId, nullptr);
    if (hProgId) {
        std::wstring desc = L"QNote Text File";
        RegSetValueExW(hProgId, nullptr, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(desc.c_str()),
                       static_cast<DWORD>((desc.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hProgId);
    }

    // Set shell\open\command
    std::wstring cmdPath = progIdPath + L"\\shell\\open\\command";
    HKEY hCmd = nullptr;
    RegCreateKeyExW(HKEY_CURRENT_USER, cmdPath.c_str(), 0, nullptr,
                    REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hCmd, nullptr);
    if (hCmd) {
        RegSetValueExW(hCmd, nullptr, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(openCmd.c_str()),
                       static_cast<DWORD>((openCmd.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hCmd);
    }

    // Set/remove association for each extension
    for (const auto& entry : entries) {
        bool checked = IsDlgButtonChecked(m_hwnd, entry.ctrlId) == BST_CHECKED;
        std::wstring extKeyPath = std::wstring(L"Software\\Classes\\") + entry.ext;

        if (checked) {
            // Associate
            HKEY hExtKey = nullptr;
            RegCreateKeyExW(HKEY_CURRENT_USER, extKeyPath.c_str(), 0, nullptr,
                            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hExtKey, nullptr);
            if (hExtKey) {
                RegSetValueExW(hExtKey, nullptr, 0, REG_SZ,
                               reinterpret_cast<const BYTE*>(progId.c_str()),
                               static_cast<DWORD>((progId.size() + 1) * sizeof(wchar_t)));
                RegCloseKey(hExtKey);
            }
        } else {
            // Remove if currently set to us
            HKEY hExtKey = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, extKeyPath.c_str(), 0, KEY_READ | KEY_WRITE,
                              &hExtKey) == ERROR_SUCCESS) {
                wchar_t buf[256] = {};
                DWORD size = sizeof(buf);
                if (RegQueryValueExW(hExtKey, nullptr, nullptr, nullptr,
                                     reinterpret_cast<BYTE*>(buf), &size) == ERROR_SUCCESS) {
                    if (std::wstring(buf) == progId) {
                        RegDeleteValueW(hExtKey, nullptr);
                    }
                }
                RegCloseKey(hExtKey);
            }
        }
    }

    // Notify shell of changes
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    MessageBoxW(m_hwnd, L"File associations have been updated.", L"QNote",
                MB_OK | MB_ICONINFORMATION);
}

} // namespace QNote
