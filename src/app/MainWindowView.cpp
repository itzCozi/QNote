//==============================================================================
// QNote - A Lightweight Notepad Clone
// MainWindowView.cpp - View/Format menu operations
//==============================================================================

#include "MainWindow.h"
#include "resource.h"

namespace QNote {

//------------------------------------------------------------------------------
// Format menu handlers
//------------------------------------------------------------------------------
void MainWindow::OnFormatWordWrap() {
    bool newValue = !m_editor->IsWordWrapEnabled();
    m_settingsManager->GetSettings().wordWrap = newValue;
    if (m_documentManager) {
        m_documentManager->ApplySettingsToAllEditors(m_settingsManager->GetSettings());
    }
}

void MainWindow::OnFormatFont() {
    AppSettings& settings = m_settingsManager->GetSettings();
    
    std::wstring fontName = settings.fontName;
    int fontSize = settings.fontSize;
    int fontWeight = settings.fontWeight;
    bool fontItalic = settings.fontItalic;
    
    if (m_dialogManager->ShowFontDialog(fontName, fontSize, fontWeight, fontItalic)) {
        settings.fontName = fontName;
        settings.fontSize = fontSize;
        settings.fontWeight = fontWeight;
        settings.fontItalic = fontItalic;
        
        if (m_documentManager) {
            m_documentManager->ApplySettingsToAllEditors(settings);
        }
    }
}

void MainWindow::OnFormatTabSize() {
    int tabSize = m_settingsManager->GetSettings().tabSize;
    
    if (m_dialogManager->ShowTabSizeDialog(tabSize)) {
        m_settingsManager->GetSettings().tabSize = tabSize;
        if (m_documentManager) {
            m_documentManager->ApplySettingsToAllEditors(m_settingsManager->GetSettings());
        }
    }
}

void MainWindow::OnFormatScrollLines() {
    int scrollLines = m_settingsManager->GetSettings().scrollLines;
    
    if (m_dialogManager->ShowScrollLinesDialog(scrollLines)) {
        m_settingsManager->GetSettings().scrollLines = scrollLines;
        if (m_documentManager) {
            m_documentManager->ApplySettingsToAllEditors(m_settingsManager->GetSettings());
        }
    }
}

void MainWindow::OnFormatLineEnding(LineEnding ending) {
    m_editor->SetLineEnding(ending);
    if (m_documentManager) {
        m_documentManager->SyncModifiedState();
    }
    UpdateStatusBar();
    UpdateTitle();
}

//------------------------------------------------------------------------------
// Format -> Right-to-Left Reading Order
//------------------------------------------------------------------------------
void MainWindow::OnFormatRTL() {
    bool newValue = !m_editor->IsRTL();
    m_settingsManager->GetSettings().rightToLeft = newValue;
    if (m_documentManager) {
        m_documentManager->ApplySettingsToAllEditors(m_settingsManager->GetSettings());
    }
}

//------------------------------------------------------------------------------
// View menu handlers
//------------------------------------------------------------------------------
void MainWindow::OnViewStatusBar() {
    AppSettings& settings = m_settingsManager->GetSettings();
    settings.showStatusBar = !settings.showStatusBar;
    
    ShowWindow(m_hwndStatus, settings.showStatusBar ? SW_SHOW : SW_HIDE);
    ResizeControls();
}

void MainWindow::OnViewLineNumbers() {
    AppSettings& settings = m_settingsManager->GetSettings();
    settings.showLineNumbers = !settings.showLineNumbers;
    
    if (m_lineNumbersGutter) {
        m_lineNumbersGutter->SetFont(m_editor->GetFont());
        m_lineNumbersGutter->Show(settings.showLineNumbers);
    }
    ResizeControls();
}

void MainWindow::OnViewZoomIn() {
    AppSettings& settings = m_settingsManager->GetSettings();
    settings.zoomLevel = (std::min)(500, settings.zoomLevel + 10);
    if (m_documentManager) m_documentManager->ApplySettingsToAllEditors(settings);
    UpdateStatusBar();
}

void MainWindow::OnViewZoomOut() {
    AppSettings& settings = m_settingsManager->GetSettings();
    settings.zoomLevel = (std::max)(25, settings.zoomLevel - 10);
    if (m_documentManager) m_documentManager->ApplySettingsToAllEditors(settings);
    UpdateStatusBar();
}

void MainWindow::OnViewZoomReset() {
    AppSettings& settings = m_settingsManager->GetSettings();
    settings.zoomLevel = 100;
    if (m_documentManager) m_documentManager->ApplySettingsToAllEditors(settings);
    UpdateStatusBar();
}

//------------------------------------------------------------------------------\n// View -> Show Whitespace
//------------------------------------------------------------------------------
void MainWindow::OnViewShowWhitespace() {
    bool newState = !m_editor->IsShowWhitespace();
    m_settingsManager->GetSettings().showWhitespace = newState;
    if (m_documentManager) m_documentManager->ApplySettingsToAllEditors(m_settingsManager->GetSettings());
}

//------------------------------------------------------------------------------
// Encoding change handler
//------------------------------------------------------------------------------
void MainWindow::OnEncodingChange(TextEncoding encoding) {
    m_editor->SetEncoding(encoding);
    if (m_documentManager) {
        m_documentManager->SyncModifiedState();
    }
    UpdateStatusBar();
    UpdateTitle();
}

//------------------------------------------------------------------------------
// Reopen current file with a specific encoding
//------------------------------------------------------------------------------
void MainWindow::OnReopenWithEncoding(TextEncoding encoding) {
    if (m_currentFile.empty() || m_isNewFile) {
        MessageBoxW(m_hwnd, L"No file is currently open to reopen.", L"QNote", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    if (m_editor->IsModified()) {
        int result = MessageBoxW(m_hwnd, 
            L"The file has unsaved changes. Reopening with a different encoding will discard them.\nContinue?",
            L"QNote", MB_YESNO | MB_ICONWARNING);
        if (result != IDYES) return;
    }
    
    m_ignoreNextFileChange = true;
    FileReadResult result = FileIO::ReadFileWithEncoding(m_currentFile, encoding);
    if (result.success) {
        m_editor->SetText(result.content);
        m_editor->SetEncoding(encoding);
        m_editor->SetLineEnding(result.detectedLineEnding);
        m_editor->SetModified(false);
        m_editor->SetSelection(0, 0);
        
        // Update document manager
        if (m_documentManager) {
            auto* doc = m_documentManager->GetActiveDocument();
            if (doc) {
                doc->encoding = encoding;
                doc->lineEnding = result.detectedLineEnding;
                doc->text = result.content;
            }
            m_documentManager->SetDocumentModified(m_documentManager->GetActiveTabId(), false);
        }
        
        StartFileMonitoring();
        UpdateTitle();
        UpdateStatusBar();
    } else {
        MessageBoxW(m_hwnd, result.errorMessage.c_str(), L"Error Reopening File", MB_OK | MB_ICONERROR);
    }
}

//------------------------------------------------------------------------------
// View -> Always on Top
//------------------------------------------------------------------------------
void MainWindow::OnViewAlwaysOnTop() {
    auto& settings = m_settingsManager->GetSettings();
    settings.alwaysOnTop = !settings.alwaysOnTop;

    SetWindowPos(m_hwnd, settings.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    (void)m_settingsManager->Save();
}

//------------------------------------------------------------------------------
// View -> Full Screen (F11)
//------------------------------------------------------------------------------
void MainWindow::OnViewFullScreen() {
    if (!m_isFullScreen) {
        // Enter full screen
        m_preFullScreenStyle = GetWindowLong(m_hwnd, GWL_STYLE);
        GetWindowRect(m_hwnd, &m_preFullScreenRect);

        // Remove title bar and borders
        LONG style = m_preFullScreenStyle & ~(WS_CAPTION | WS_THICKFRAME | WS_BORDER);
        SetWindowLong(m_hwnd, GWL_STYLE, style);

        // Get the monitor for this window
        HMONITOR hMon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfoW(hMon, &mi);

        SetWindowPos(m_hwnd, HWND_TOP,
                     mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_NOZORDER);

        m_isFullScreen = true;
    } else {
        // Exit full screen
        SetWindowLong(m_hwnd, GWL_STYLE, m_preFullScreenStyle);
        SetWindowPos(m_hwnd, HWND_TOP,
                     m_preFullScreenRect.left, m_preFullScreenRect.top,
                     m_preFullScreenRect.right - m_preFullScreenRect.left,
                     m_preFullScreenRect.bottom - m_preFullScreenRect.top,
                     SWP_FRAMECHANGED | SWP_NOZORDER);

        m_isFullScreen = false;
    }

    // Reapply always-on-top if needed
    if (m_settingsManager->GetSettings().alwaysOnTop) {
        SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
}

//------------------------------------------------------------------------------
// View -> Toggle Menu Bar (Alt key)
//------------------------------------------------------------------------------
void MainWindow::OnViewToggleMenuBar() {
    auto& settings = m_settingsManager->GetSettings();
    settings.menuBarVisible = !settings.menuBarVisible;

    if (!settings.menuBarVisible) {
        m_savedMenu = GetMenu(m_hwnd);
        SetMenu(m_hwnd, nullptr);
    } else {
        if (m_savedMenu) {
            SetMenu(m_hwnd, m_savedMenu);
            m_savedMenu = nullptr;
        }
    }

    (void)m_settingsManager->Save();

    // Trigger resize
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    SendMessage(m_hwnd, WM_SIZE, SIZE_RESTORED,
                MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));
}

//------------------------------------------------------------------------------
// View -> Spell Check (F7)
//------------------------------------------------------------------------------
void MainWindow::OnViewSpellCheck() {
    auto& settings = m_settingsManager->GetSettings();
    settings.spellCheckEnabled = !settings.spellCheckEnabled;
    if (m_documentManager) {
        m_documentManager->ApplySettingsToAllEditors(settings);
    }
}

} // namespace QNote
