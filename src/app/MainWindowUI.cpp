//==============================================================================
// QNote - A Lightweight Notepad Clone
// MainWindowUI.cpp - UI helpers, keyboard shortcuts, session, system tray
//==============================================================================

#include "MainWindow.h"
#include "resource.h"
#include <CommCtrl.h>
#include <shellapi.h>
#include <functional>
#include <sstream>
#include <set>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace QNote {

//------------------------------------------------------------------------------
// Helper: check if a string is empty or contains only whitespace
//------------------------------------------------------------------------------
static bool IsWhitespaceOnly(const std::wstring& text) {
    return text.find_first_not_of(L" \t\r\n") == std::wstring::npos;
}

//------------------------------------------------------------------------------
// Load custom keyboard shortcuts
//------------------------------------------------------------------------------
void MainWindow::LoadKeyboardShortcuts() {
    std::wstring settingsPath = m_settingsManager->GetSettingsPath();
    size_t pos = settingsPath.rfind(L"config.ini");
    if (pos == std::wstring::npos) return;
    
    std::wstring shortcutsPath = settingsPath.substr(0, pos) + L"shortcuts.ini";
    
    // If no custom shortcuts file exists, keep the default accelerator table
    if (GetFileAttributesW(shortcutsPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return;
    }
    
    // Define command name to ID mapping
    struct CmdMapping {
        const wchar_t* name;
        WORD cmdId;
    };
    
    static const CmdMapping cmdMap[] = {
        // File
        { L"FileNew",          IDM_FILE_NEW },
        { L"FileNewWindow",    IDM_FILE_NEWWINDOW },
        { L"FileOpen",         IDM_FILE_OPEN },
        { L"FileSave",         IDM_FILE_SAVE },
        { L"FileSaveAs",       IDM_FILE_SAVEAS },
        { L"FilePrint",        IDM_FILE_PRINT },
        // Edit
        { L"EditUndo",         IDM_EDIT_UNDO },
        { L"EditRedo",         IDM_EDIT_REDO },
        { L"EditCut",          IDM_EDIT_CUT },
        { L"EditCopy",         IDM_EDIT_COPY },
        { L"EditPaste",        IDM_EDIT_PASTE },
        { L"EditDelete",       IDM_EDIT_DELETE },
        { L"EditSelectAll",    IDM_EDIT_SELECTALL },
        { L"EditFind",         IDM_EDIT_FIND },
        { L"EditFindNext",     IDM_EDIT_FINDNEXT },
        { L"EditReplace",      IDM_EDIT_REPLACE },
        { L"EditGoTo",         IDM_EDIT_GOTO },
        { L"EditDateTime",     IDM_EDIT_DATETIME },
        // Text Operations
        { L"EditUppercase",    IDM_EDIT_UPPERCASE },
        { L"EditLowercase",    IDM_EDIT_LOWERCASE },
        // View
        { L"ViewZoomIn",       IDM_VIEW_ZOOMIN },
        { L"ViewZoomOut",      IDM_VIEW_ZOOMOUT },
        { L"ViewZoomReset",    IDM_VIEW_ZOOMRESET },
        // Tabs
        { L"TabNew",           IDM_TAB_NEW },
        { L"TabClose",         IDM_TAB_CLOSE },
        { L"TabNext",          IDM_TAB_NEXT },
        { L"TabPrev",          IDM_TAB_PREV },
        // Notes
        { L"NotesCapture",     IDM_NOTES_QUICKCAPTURE },
        { L"NotesAllNotes",    IDM_NOTES_ALLNOTES },
        { L"NotesSearch",      IDM_NOTES_SEARCH },
        // Bookmarks
        { L"ToggleBookmark",   IDM_EDIT_TOGGLEBOOKMARK },
        { L"NextBookmark",     IDM_EDIT_NEXTBOOKMARK },
        { L"PrevBookmark",     IDM_EDIT_PREVBOOKMARK },
        { L"ClearBookmarks",   IDM_EDIT_CLEARBOOKMARKS },
    };
    
    std::vector<ACCEL> accels;
    
    for (const auto& cmd : cmdMap) {
        wchar_t buf[256] = {};
        GetPrivateProfileStringW(L"Shortcuts", cmd.name, L"", buf, 256, shortcutsPath.c_str());
        
        std::wstring shortcut = buf;
        if (shortcut.empty()) continue;
        
        // Parse "Ctrl+Shift+K" style string
        BYTE fVirt = FVIRTKEY;
        WORD key = 0;
        
        std::wstring remaining = shortcut;
        // Parse modifiers
        while (true) {
            size_t plusPos = remaining.find(L'+');
            if (plusPos == std::wstring::npos) break;
            
            std::wstring modifier = remaining.substr(0, plusPos);
            remaining = remaining.substr(plusPos + 1);
            
            if (_wcsicmp(modifier.c_str(), L"Ctrl") == 0) fVirt |= FCONTROL;
            else if (_wcsicmp(modifier.c_str(), L"Shift") == 0) fVirt |= FSHIFT;
            else if (_wcsicmp(modifier.c_str(), L"Alt") == 0) fVirt |= FALT;
        }
        
        // remaining is the key name
        key = ParseVirtualKey(remaining);
        if (key == 0) continue;
        
        ACCEL accel = {};
        accel.fVirt = fVirt;
        accel.key = key;
        accel.cmd = cmd.cmdId;
        accels.push_back(accel);
    }
    
    if (!accels.empty()) {
        HACCEL hNewAccel = CreateAcceleratorTableW(accels.data(), static_cast<int>(accels.size()));
        if (hNewAccel) {
            if (m_hAccel) {
                DestroyAcceleratorTable(m_hAccel);
            }
            m_hAccel = hNewAccel;
        }
    }
}

//------------------------------------------------------------------------------
// Create default shortcuts INI file
//------------------------------------------------------------------------------
void MainWindow::CreateDefaultShortcutsFile(const std::wstring& path) {
    std::wstring content;
    content += L"; =============================================================================\r\n";
    content += L"; QNote - Keyboard Shortcuts Configuration\r\n";
    content += L"; =============================================================================\r\n";
    content += L";\r\n";
    content += L"; Format:    CommandName=Modifier+Key\r\n";
    content += L"; Modifiers: Ctrl, Shift, Alt (combine with +)\r\n";
    content += L"; Keys:      A-Z, 0-9, F1-F12, Tab, Delete, Escape, Enter, Space,\r\n";
    content += L";            Plus, Minus, Home, End, PageUp, PageDown, Insert, Backspace\r\n";
    content += L";\r\n";
    content += L"; To customize a shortcut, change the value after the = sign.\r\n";
    content += L"; To disable a shortcut, delete the value (e.g., FileNew=).\r\n";
    content += L"; Save this file and restart QNote to apply changes.\r\n";
    content += L";\r\n";
    content += L"\r\n[Shortcuts]\r\n";
    content += L"\r\n";
    content += L"; --- File ---\r\n";
    content += L"FileNew=Ctrl+N\r\n";
    content += L"FileNewWindow=Ctrl+Shift+N\r\n";
    content += L"FileOpen=Ctrl+O\r\n";
    content += L"FileSave=Ctrl+S\r\n";
    content += L"FileSaveAs=Ctrl+Shift+S\r\n";
    content += L"FilePrint=Ctrl+P\r\n";
    content += L"\r\n";
    content += L"; --- Edit ---\r\n";
    content += L"EditUndo=Ctrl+Z\r\n";
    content += L"EditRedo=Ctrl+Y\r\n";
    content += L"EditCut=Ctrl+X\r\n";
    content += L"EditCopy=Ctrl+C\r\n";
    content += L"EditPaste=Ctrl+V\r\n";
    content += L"EditDelete=Delete\r\n";
    content += L"EditSelectAll=Ctrl+A\r\n";
    content += L"EditFind=Ctrl+F\r\n";
    content += L"EditFindNext=F3\r\n";
    content += L"EditReplace=Ctrl+H\r\n";
    content += L"EditGoTo=Ctrl+G\r\n";
    content += L"EditDateTime=F5\r\n";
    content += L"\r\n";
    content += L"; --- Text Operations ---\r\n";
    content += L"EditUppercase=Ctrl+Shift+U\r\n";
    content += L"EditLowercase=Ctrl+U\r\n";
    content += L"\r\n";
    content += L"; --- View ---\r\n";
    content += L"ViewZoomIn=Ctrl+Plus\r\n";
    content += L"ViewZoomOut=Ctrl+Minus\r\n";
    content += L"ViewZoomReset=Ctrl+0\r\n";
    content += L"\r\n";
    content += L"; --- Tabs ---\r\n";
    content += L"TabNew=Ctrl+T\r\n";
    content += L"TabClose=Ctrl+W\r\n";
    content += L"TabNext=Ctrl+Tab\r\n";
    content += L"TabPrev=Ctrl+Shift+Tab\r\n";
    content += L"\r\n";
    content += L"; --- Notes ---\r\n";
    content += L"NotesCapture=Ctrl+Shift+Q\r\n";
    content += L"NotesAllNotes=Ctrl+Shift+A\r\n";
    content += L"NotesSearch=Ctrl+Shift+F\r\n";
    content += L"\r\n";
    content += L"; --- Bookmarks ---\r\n";
    content += L"ToggleBookmark=F2\r\n";
    content += L"NextBookmark=Ctrl+F2\r\n";
    content += L"PrevBookmark=Shift+F2\r\n";
    content += L"ClearBookmarks=Ctrl+Shift+F2\r\n";
    
    (void)FileIO::WriteFile(path, content, TextEncoding::UTF8, LineEnding::CRLF);
}

//------------------------------------------------------------------------------
// Parse key name to virtual key code
//------------------------------------------------------------------------------
WORD MainWindow::ParseVirtualKey(const std::wstring& keyName) {
    if (keyName.length() == 1) {
        wchar_t c = towupper(keyName[0]);
        if (c >= L'A' && c <= L'Z') return static_cast<WORD>(c);
        if (c >= L'0' && c <= L'9') return static_cast<WORD>(c);
    }
    
    if (_wcsicmp(keyName.c_str(), L"F1") == 0) return VK_F1;
    if (_wcsicmp(keyName.c_str(), L"F2") == 0) return VK_F2;
    if (_wcsicmp(keyName.c_str(), L"F3") == 0) return VK_F3;
    if (_wcsicmp(keyName.c_str(), L"F4") == 0) return VK_F4;
    if (_wcsicmp(keyName.c_str(), L"F5") == 0) return VK_F5;
    if (_wcsicmp(keyName.c_str(), L"F6") == 0) return VK_F6;
    if (_wcsicmp(keyName.c_str(), L"F7") == 0) return VK_F7;
    if (_wcsicmp(keyName.c_str(), L"F8") == 0) return VK_F8;
    if (_wcsicmp(keyName.c_str(), L"F9") == 0) return VK_F9;
    if (_wcsicmp(keyName.c_str(), L"F10") == 0) return VK_F10;
    if (_wcsicmp(keyName.c_str(), L"F11") == 0) return VK_F11;
    if (_wcsicmp(keyName.c_str(), L"F12") == 0) return VK_F12;
    if (_wcsicmp(keyName.c_str(), L"Tab") == 0) return VK_TAB;
    if (_wcsicmp(keyName.c_str(), L"Delete") == 0) return VK_DELETE;
    if (_wcsicmp(keyName.c_str(), L"Escape") == 0) return VK_ESCAPE;
    if (_wcsicmp(keyName.c_str(), L"Enter") == 0) return VK_RETURN;
    if (_wcsicmp(keyName.c_str(), L"Space") == 0) return VK_SPACE;
    if (_wcsicmp(keyName.c_str(), L"Plus") == 0) return VK_OEM_PLUS;
    if (_wcsicmp(keyName.c_str(), L"Minus") == 0) return VK_OEM_MINUS;
    if (_wcsicmp(keyName.c_str(), L"Home") == 0) return VK_HOME;
    if (_wcsicmp(keyName.c_str(), L"End") == 0) return VK_END;
    if (_wcsicmp(keyName.c_str(), L"PageUp") == 0) return VK_PRIOR;
    if (_wcsicmp(keyName.c_str(), L"PageDown") == 0) return VK_NEXT;
    if (_wcsicmp(keyName.c_str(), L"Insert") == 0) return VK_INSERT;
    if (_wcsicmp(keyName.c_str(), L"Backspace") == 0) return VK_BACK;
    
    return 0;
}

//------------------------------------------------------------------------------
// Format accelerator key to display string
//------------------------------------------------------------------------------
std::wstring MainWindow::FormatAccelKey(BYTE fVirt, WORD key) {
    std::wstring result;
    if (fVirt & FCONTROL) result += L"Ctrl+";
    if (fVirt & FSHIFT) result += L"Shift+";
    if (fVirt & FALT) result += L"Alt+";
    
    if (key >= 'A' && key <= 'Z') result += static_cast<wchar_t>(key);
    else if (key >= '0' && key <= '9') result += static_cast<wchar_t>(key);
    else if (key == VK_F1) result += L"F1";
    else if (key == VK_F2) result += L"F2";
    else if (key == VK_F3) result += L"F3";
    else if (key == VK_F4) result += L"F4";
    else if (key == VK_F5) result += L"F5";
    else if (key == VK_F6) result += L"F6";
    else if (key == VK_F7) result += L"F7";
    else if (key == VK_F8) result += L"F8";
    else if (key == VK_F9) result += L"F9";
    else if (key == VK_F10) result += L"F10";
    else if (key == VK_F11) result += L"F11";
    else if (key == VK_F12) result += L"F12";
    else if (key == VK_TAB) result += L"Tab";
    else if (key == VK_DELETE) result += L"Delete";
    else if (key == VK_ESCAPE) result += L"Escape";
    else if (key == VK_RETURN) result += L"Enter";
    else if (key == VK_SPACE) result += L"Space";
    else if (key == VK_OEM_PLUS) result += L"Plus";
    else if (key == VK_OEM_MINUS) result += L"Minus";
    else if (key == VK_HOME) result += L"Home";
    else if (key == VK_END) result += L"End";
    else if (key == VK_PRIOR) result += L"PageUp";
    else if (key == VK_NEXT) result += L"PageDown";
    else if (key == VK_INSERT) result += L"Insert";
    else if (key == VK_BACK) result += L"Backspace";
    else result += L"?";
    
    return result;
}

//------------------------------------------------------------------------------
// Help -> About
//------------------------------------------------------------------------------
void MainWindow::OnHelpAbout() {
    m_dialogManager->ShowAboutDialog();
}

//------------------------------------------------------------------------------
// Help -> Check for Updates
// Queries GitHub Releases API and compares version to current
//------------------------------------------------------------------------------
void MainWindow::OnHelpCheckUpdate() {
    // Current version from resource.h
    const int currentMajor = VER_MAJOR;
    const int currentMinor = VER_MINOR;
    const int currentPatch = VER_PATCH;
    
    std::wstring latestVersion;
    std::wstring downloadUrl;
    bool updateAvailable = false;
    bool checkFailed = false;
    
    // Initialize WinHTTP
    HINTERNET hSession = WinHttpOpen(
        L"QNote Update Checker/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    
    if (!hSession) {
        checkFailed = true;
    }
    
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;
    
    if (!checkFailed) {
        hConnect = WinHttpConnect(hSession, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) {
            checkFailed = true;
        }
    }
    
    if (!checkFailed) {
        hRequest = WinHttpOpenRequest(
            hConnect,
            L"GET",
            L"/repos/itzcozi/qnote/releases/latest",
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);
        if (!hRequest) {
            checkFailed = true;
        }
    }
    
    if (!checkFailed) {
        // Add User-Agent header (required by GitHub API)
        WinHttpAddRequestHeaders(hRequest, L"User-Agent: QNote", (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
        
        if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            checkFailed = true;
        }
    }
    
    if (!checkFailed) {
        if (!WinHttpReceiveResponse(hRequest, nullptr)) {
            checkFailed = true;
        }
    }
    
    std::string responseBody;
    if (!checkFailed) {
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                break;
            }
            
            if (dwSize == 0) break;
            
            std::vector<char> buffer(dwSize + 1, 0);
            if (!WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                break;
            }
            
            responseBody.append(buffer.data(), dwDownloaded);
        } while (dwSize > 0);
    }
    
    // Cleanup
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);
    
    // Parse response (simple JSON parsing for "tag_name" field)
    if (!checkFailed && !responseBody.empty()) {
        // Look for "tag_name": "X.Y.Z" or "tag_name": "vX.Y.Z"
        std::string tagKey = "\"tag_name\"";
        size_t tagPos = responseBody.find(tagKey);
        if (tagPos != std::string::npos) {
            size_t colonPos = responseBody.find(':', tagPos);
            size_t quoteStart = responseBody.find('"', colonPos + 1);
            size_t quoteEnd = responseBody.find('"', quoteStart + 1);
            
            if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                std::string tag = responseBody.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                
                // Remove leading 'v' if present
                if (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V')) {
                    tag = tag.substr(1);
                }
                
                // Parse version X.Y.Z
                int remoteMajor = 0, remoteMinor = 0, remotePatch = 0;
                if (sscanf_s(tag.c_str(), "%d.%d.%d", &remoteMajor, &remoteMinor, &remotePatch) >= 2) {
                    latestVersion = std::wstring(tag.begin(), tag.end());
                    
                    // Compare versions
                    if (remoteMajor > currentMajor ||
                        (remoteMajor == currentMajor && remoteMinor > currentMinor) ||
                        (remoteMajor == currentMajor && remoteMinor == currentMinor && remotePatch > currentPatch)) {
                        updateAvailable = true;
                        downloadUrl = L"https://github.com/itzcozi/qnote/releases/latest";
                    }
                }
            }
        }
        
        // Look for html_url for the release page
        std::string urlKey = "\"html_url\"";
        size_t urlPos = responseBody.find(urlKey);
        if (urlPos != std::string::npos) {
            size_t colonPos = responseBody.find(':', urlPos);
            size_t quoteStart = responseBody.find('"', colonPos + 1);
            size_t quoteEnd = responseBody.find('"', quoteStart + 1);
            
            if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                std::string url = responseBody.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                downloadUrl = std::wstring(url.begin(), url.end());
            }
        }
    } else if (responseBody.empty()) {
        checkFailed = true;
    }
    
    // Show result to user
    if (checkFailed) {
        MessageBoxW(m_hwnd,
            L"Unable to check for updates.\n\n"
            L"Please check your internet connection or visit:\n"
            L"https://github.com/itzcozi/qnote/releases",
            L"Update Check Failed",
            MB_OK | MB_ICONWARNING);
    } else if (updateAvailable) {
        std::wstring message = L"A new version of QNote is available!\n\n"
            L"Current version: " + std::to_wstring(currentMajor) + L"." + 
            std::to_wstring(currentMinor) + L"." + std::to_wstring(currentPatch) + L"\n"
            L"Latest version: " + latestVersion + L"\n\n"
            L"Would you like to open the download page?";
        
        int result = MessageBoxW(m_hwnd, message.c_str(), L"Update Available", MB_YESNO | MB_ICONINFORMATION);
        if (result == IDYES) {
            ShellExecuteW(nullptr, L"open", downloadUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    } else {
        std::wstring message = L"You are running the latest version of QNote.\n\n"
            L"Version: " + std::to_wstring(currentMajor) + L"." + 
            std::to_wstring(currentMinor) + L"." + std::to_wstring(currentPatch);
        
        MessageBoxW(m_hwnd, message.c_str(), L"No Updates Available", MB_OK | MB_ICONINFORMATION);
    }
}

//------------------------------------------------------------------------------
// Update window title
//------------------------------------------------------------------------------
void MainWindow::UpdateTitle() {
    std::wstring title;
    
    // Add modification indicator (content-based)
    bool titleModified = false;
    if (m_documentManager) {
        auto* activeDoc = m_documentManager->GetActiveDocument();
        if (activeDoc) titleModified = activeDoc->isModified;
    } else {
        titleModified = m_editor ? m_editor->IsModified() : false;
    }
    if (titleModified) {
        title = L"*";
    }
    
    // Check if document manager has an active document with a custom title
    if (m_documentManager) {
        auto* doc = m_documentManager->GetActiveDocument();
        if (doc && !doc->customTitle.empty()) {
            title += doc->customTitle;
            title += L" - QNote";
            SetWindowTextW(m_hwnd, title.c_str());
            return;
        }
    }
    
    // Handle note mode vs file mode
    if (m_isNoteMode) {
        // Note mode - show note title
        if (m_currentNoteId.empty()) {
            title += L"New Note";
        } else if (m_noteStore) {
            auto note = m_noteStore->GetNoteSummary(m_currentNoteId);
            if (note) {
                std::wstring noteTitle = note->GetDisplayTitle();
                if (note->isPinned) {
                    title += L"\u2605 ";  // Star for pinned
                }
                title += noteTitle;
            } else {
                title += L"Note";
            }
        } else {
            title += L"Note";
        }
        title += L" - QNote Notes";
    } else {
        // File mode - show filename
        if (m_isNewFile || m_currentFile.empty()) {
            title += L"Untitled";
        } else {
            title += FileIO::GetFileName(m_currentFile);
        }
        title += L" - QNote";
    }
    
    SetWindowTextW(m_hwnd, title.c_str());
}

//------------------------------------------------------------------------------
// Update status bar
//------------------------------------------------------------------------------
void MainWindow::UpdateStatusBar() {
    if (!m_hwndStatus || !IsWindowVisible(m_hwndStatus)) {
        return;
    }
    if (!m_editor) return;
    
    // Line and column
    int line = m_editor->GetCurrentLine() + 1;
    int column = m_editor->GetCurrentColumn() + 1;
    
    // Check for selection
    DWORD selStart, selEnd;
    m_editor->GetSelection(selStart, selEnd);
    
    wchar_t posText[128];
    if (selStart != selEnd) {
        DWORD selLen = (selEnd > selStart) ? (selEnd - selStart) : (selStart - selEnd);
        swprintf_s(posText, L"Ln %d, Col %d  |  %lu sel", line, column, static_cast<unsigned long>(selLen));
    } else {
        swprintf_s(posText, L"Ln %d, Col %d", line, column);
    }
    SendMessageW(m_hwndStatus, SB_SETTEXTW, SB_PART_POSITION, reinterpret_cast<LPARAM>(posText));
    
    // Encoding
    const wchar_t* encodingText = EncodingToString(m_editor->GetEncoding());
    SendMessageW(m_hwndStatus, SB_SETTEXTW, SB_PART_ENCODING, reinterpret_cast<LPARAM>(encodingText));
    
    // Line ending
    const wchar_t* eolText = LineEndingToString(m_editor->GetLineEnding());
    SendMessageW(m_hwndStatus, SB_SETTEXTW, SB_PART_EOL, reinterpret_cast<LPARAM>(eolText));
    
    // Zoom
    wchar_t zoomText[32];
    swprintf_s(zoomText, L"%d%%", m_settingsManager->GetSettings().zoomLevel);
    SendMessageW(m_hwndStatus, SB_SETTEXTW, SB_PART_ZOOM, reinterpret_cast<LPARAM>(zoomText));
    
    // Word and character count
    int textLen = m_editor->GetTextLength();
    int wordCount = m_editor->GetWordCount();
    wchar_t countText[64];
    swprintf_s(countText, L"%d words, %d chars", wordCount, textLen);
    SendMessageW(m_hwndStatus, SB_SETTEXTW, SB_PART_COUNTS, reinterpret_cast<LPARAM>(countText));
}

//------------------------------------------------------------------------------
// Update menu item states
//------------------------------------------------------------------------------
void MainWindow::UpdateMenuState() {
    HMENU hMenu = GetMenu(m_hwnd);
    if (!hMenu || !m_editor) return;
    
    // Edit menu state
    bool canUndo = m_editor->CanUndo();
    bool canRedo = m_editor->CanRedo();
    bool hasSelection = false;
    DWORD start, end;
    m_editor->GetSelection(start, end);
    hasSelection = (start != end);
    
    // Check clipboard for paste
    bool canPaste = IsClipboardFormatAvailable(CF_UNICODETEXT) || 
                    IsClipboardFormatAvailable(CF_TEXT);
    
    EnableMenuItem(hMenu, IDM_EDIT_UNDO, MF_BYCOMMAND | (canUndo ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_EDIT_REDO, MF_BYCOMMAND | (canRedo ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_EDIT_CUT, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_EDIT_COPY, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_EDIT_PASTE, MF_BYCOMMAND | (canPaste ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_EDIT_DELETE, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
    
    // Text operation menu states
    EnableMenuItem(hMenu, IDM_EDIT_UPPERCASE, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_EDIT_LOWERCASE, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
    
    // File operation states
    bool hasFile = !m_isNewFile && !m_currentFile.empty();
    EnableMenuItem(hMenu, IDM_FILE_REVERT, MF_BYCOMMAND | (hasFile ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_FILE_OPENCONTAINING, MF_BYCOMMAND | (hasFile ? MF_ENABLED : MF_GRAYED));
    
    // Reopen with encoding requires a file
    EnableMenuItem(hMenu, IDM_ENCODING_REOPEN_UTF8, MF_BYCOMMAND | (hasFile ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_ENCODING_REOPEN_UTF8BOM, MF_BYCOMMAND | (hasFile ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_ENCODING_REOPEN_UTF16LE, MF_BYCOMMAND | (hasFile ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_ENCODING_REOPEN_UTF16BE, MF_BYCOMMAND | (hasFile ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_ENCODING_REOPEN_ANSI, MF_BYCOMMAND | (hasFile ? MF_ENABLED : MF_GRAYED));
    
    // Format menu state
    CheckMenuItem(hMenu, IDM_FORMAT_WORDWRAP, 
                  MF_BYCOMMAND | (m_editor->IsWordWrapEnabled() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_FORMAT_RTL,
                  MF_BYCOMMAND | (m_editor->IsRTL() ? MF_CHECKED : MF_UNCHECKED));
    
    // Line ending checkmarks
    LineEnding eol = m_editor->GetLineEnding();
    CheckMenuItem(hMenu, IDM_FORMAT_EOL_CRLF, 
                  MF_BYCOMMAND | (eol == LineEnding::CRLF ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_FORMAT_EOL_LF, 
                  MF_BYCOMMAND | (eol == LineEnding::LF ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_FORMAT_EOL_CR, 
                  MF_BYCOMMAND | (eol == LineEnding::CR ? MF_CHECKED : MF_UNCHECKED));
    
    // View menu state
    CheckMenuItem(hMenu, IDM_VIEW_STATUSBAR,
                  MF_BYCOMMAND | (m_settingsManager->GetSettings().showStatusBar ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_VIEW_LINENUMBERS,
                  MF_BYCOMMAND | (m_settingsManager->GetSettings().showLineNumbers ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_VIEW_SHOWWHITESPACE,
                  MF_BYCOMMAND | (m_editor->IsShowWhitespace() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_VIEW_SPELLCHECK,
                  MF_BYCOMMAND | (m_settingsManager->GetSettings().spellCheckEnabled ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_VIEW_ALWAYSONTOP,
                  MF_BYCOMMAND | (m_settingsManager->GetSettings().alwaysOnTop ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_VIEW_FULLSCREEN,
                  MF_BYCOMMAND | (m_isFullScreen ? MF_CHECKED : MF_UNCHECKED));
    
    // Encoding checkmarks
    TextEncoding enc = m_editor->GetEncoding();
    CheckMenuItem(hMenu, IDM_ENCODING_UTF8, 
                  MF_BYCOMMAND | (enc == TextEncoding::UTF8 ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_ENCODING_UTF8BOM, 
                  MF_BYCOMMAND | (enc == TextEncoding::UTF8_BOM ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_ENCODING_UTF16LE, 
                  MF_BYCOMMAND | (enc == TextEncoding::UTF16_LE ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_ENCODING_UTF16BE, 
                  MF_BYCOMMAND | (enc == TextEncoding::UTF16_BE ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_ENCODING_ANSI, 
                  MF_BYCOMMAND | (enc == TextEncoding::ANSI ? MF_CHECKED : MF_UNCHECKED));
    
    // Tools menu state
    EnableMenuItem(hMenu, IDM_TOOLS_COPYFILEPATH, MF_BYCOMMAND | (hasFile ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_TOOLS_OPENTERMINAL, MF_BYCOMMAND | (hasFile ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_TOOLS_URLENCODE, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_TOOLS_URLDECODE, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_TOOLS_BASE64ENCODE, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_TOOLS_BASE64DECODE, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_TOOLS_JOINLINES, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));

    // New edit items that need selection
    EnableMenuItem(hMenu, IDM_EDIT_TITLECASE, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_TOOLS_CALCULATE, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_TOOLS_RUNSELECTION, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, IDM_TOOLS_CONVERTEOL_SEL, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));

    // Open from clipboard needs clipboard text
    EnableMenuItem(hMenu, IDM_FILE_OPENFROMCLIPBOARD, MF_BYCOMMAND | (canPaste ? MF_ENABLED : MF_GRAYED));
}

//------------------------------------------------------------------------------
// Update recent files menu
//------------------------------------------------------------------------------
void MainWindow::UpdateRecentFilesMenu() {
    HMENU hMainMenu = GetMenu(m_hwnd);
    if (!hMainMenu) return;
    
    // Get File menu
    HMENU hFileMenu = GetSubMenu(hMainMenu, 0);
    if (!hFileMenu) return;
    
    // Find Recent Files submenu
    HMENU hRecentMenu = nullptr;
    int itemCount = GetMenuItemCount(hFileMenu);
    for (int i = 0; i < itemCount; i++) {
        HMENU hSub = GetSubMenu(hFileMenu, i);
        if (hSub) {
            wchar_t text[64] = {};
            GetMenuStringW(hFileMenu, i, text, 64, MF_BYPOSITION);
            if (wcsstr(text, L"Recent") != nullptr) {
                hRecentMenu = hSub;
                break;
            }
        }
    }
    
    if (!hRecentMenu) return;
    
    // Clear existing items
    while (GetMenuItemCount(hRecentMenu) > 0) {
        DeleteMenu(hRecentMenu, 0, MF_BYPOSITION);
    }
    
    // Add recent files
    const auto& recentFiles = m_settingsManager->GetSettings().recentFiles;
    
    if (recentFiles.empty()) {
        AppendMenuW(hRecentMenu, MF_STRING | MF_GRAYED, 0, L"(Empty)");
    } else {
        for (size_t i = 0; i < recentFiles.size(); i++) {
            std::wstring menuText;
            if (i < 9) {
                menuText = L"&" + std::to_wstring(i + 1) + L" ";
            } else {
                menuText = std::to_wstring(i + 1) + L" ";
            }
            menuText += recentFiles[i];
            
            AppendMenuW(hRecentMenu, MF_STRING, IDM_FILE_RECENT_BASE + static_cast<UINT>(i), 
                        menuText.c_str());
        }
    }
}

//------------------------------------------------------------------------------
// Create status bar
//------------------------------------------------------------------------------
void MainWindow::CreateStatusBar() {
    m_hwndStatus = CreateWindowExW(
        0,
        STATUSCLASSNAMEW,
        nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        m_hwnd,
        nullptr,
        m_hInstance,
        nullptr
    );
    
    if (m_hwndStatus) {
        // Set up status bar parts
        int widths[STATUS_PARTS] = { 150, 250, 330, 400, -1 };
        SendMessageW(m_hwndStatus, SB_SETPARTS, STATUS_PARTS, reinterpret_cast<LPARAM>(widths));
    }
    
    // Show/hide based on settings
    if (!m_settingsManager->GetSettings().showStatusBar) {
        ShowWindow(m_hwndStatus, SW_HIDE);
    }
}

//------------------------------------------------------------------------------
// Resize child controls
//------------------------------------------------------------------------------
void MainWindow::ResizeControls() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    
    int statusHeight = 0;
    int findBarHeight = 0;
    int gutterWidth = 0;
    int tabBarHeight = 0;
    int topOffset = 0;  // No gap below menu
    
    // Position status bar
    if (m_hwndStatus && IsWindowVisible(m_hwndStatus)) {
        SendMessageW(m_hwndStatus, WM_SIZE, 0, 0);
        
        RECT statusRect;
        GetWindowRect(m_hwndStatus, &statusRect);
        statusHeight = statusRect.bottom - statusRect.top;
    }
    
    // Position tab bar below menu
    if (m_tabBar && m_tabBar->GetHandle()) {
        tabBarHeight = m_tabBar->GetHeight();
        m_tabBar->Resize(0, topOffset, rc.right);
    }
    
    // Position find bar below tab bar
    if (m_findBar && m_findBar->IsVisible()) {
        findBarHeight = m_findBar->GetHeight();
        m_findBar->Resize(0, topOffset + tabBarHeight, rc.right);
    }
    
    // Calculate content area top position
    int contentTop = topOffset + tabBarHeight + findBarHeight;
    int contentHeight = rc.bottom - statusHeight - contentTop;
    
    // Position line numbers gutter
    if (m_lineNumbersGutter && m_lineNumbersGutter->IsVisible()) {
        gutterWidth = m_lineNumbersGutter->GetWidth();
        m_lineNumbersGutter->Resize(0, contentTop, contentHeight);
    }
    
    // Position editor next to gutter
    if (m_editor) {
        m_editor->Resize(gutterWidth, contentTop, rc.right - gutterWidth, contentHeight);
    }
    
    // Update gutter display after editor resize
    if (m_lineNumbersGutter && m_lineNumbersGutter->IsVisible()) {
        m_lineNumbersGutter->Update();
    }
}

//------------------------------------------------------------------------------
// Update m_editor pointer and sub-component references on tab switch
//------------------------------------------------------------------------------
void MainWindow::UpdateActiveEditor() {
    Editor* newEditor = m_documentManager ? m_documentManager->GetActiveEditor() : nullptr;
    if (!newEditor) return;
    
    m_editor = newEditor;
    
    // Set scroll callback on the new editor
    m_editor->SetScrollCallback(OnEditorScroll, this);
    
    // Update sub-component editor references
    if (m_findBar) m_findBar->SetEditor(m_editor);
    if (m_lineNumbersGutter) m_lineNumbersGutter->SetEditor(m_editor);
    if (m_dialogManager) m_dialogManager->SetEditor(m_editor);
    
    // Resize the new editor to fill the content area
    ResizeControls();
}

//------------------------------------------------------------------------------
// Editor scroll callback for line numbers sync
//------------------------------------------------------------------------------
void MainWindow::OnEditorScroll(void* userData) {
    MainWindow* self = static_cast<MainWindow*>(userData);
    if (self && self->m_lineNumbersGutter && self->m_lineNumbersGutter->IsVisible()) {
        self->m_lineNumbersGutter->OnEditorScroll();
    }
}

//------------------------------------------------------------------------------
// Session save/restore
//------------------------------------------------------------------------------
void MainWindow::SaveSession() {
    if (!m_documentManager || !m_settingsManager) return;
    
    // Build session file path
    std::wstring sessionPath = m_settingsManager->GetSettingsPath();
    size_t pos = sessionPath.rfind(L"config.ini");
    if (pos == std::wstring::npos) return;
    std::wstring sessionDir = sessionPath.substr(0, pos);
    sessionPath = sessionDir + L"session.ini";
    
    // Save current document state
    m_documentManager->SaveCurrentState();
    
    auto ids = m_documentManager->GetAllTabIds();
    int activeTabId = m_documentManager->GetActiveTabId();
    
    // Delete old session file
    DeleteFileW(sessionPath.c_str());
    
    // Don't save session if there's only one empty untitled tab
    if (ids.size() == 1) {
        auto* doc = m_documentManager->GetDocument(ids[0]);
        if (doc && doc->isNewFile && !doc->isModified && IsWhitespaceOnly(doc->editor ? doc->editor->GetText() : L"")) {
            return;
        }
    }
    
    // Write tab count and active index
    WritePrivateProfileStringW(L"Session", L"TabCount",
        std::to_wstring(ids.size()).c_str(), sessionPath.c_str());
    
    int activeIndex = 0;
    for (int i = 0; i < static_cast<int>(ids.size()); i++) {
        if (ids[i] == activeTabId) { activeIndex = i; break; }
    }
    WritePrivateProfileStringW(L"Session", L"ActiveIndex",
        std::to_wstring(activeIndex).c_str(), sessionPath.c_str());
    
    for (int i = 0; i < static_cast<int>(ids.size()); i++) {
        auto* doc = m_documentManager->GetDocument(ids[i]);
        if (!doc) continue;
        
        std::wstring section = L"Tab" + std::to_wstring(i);
        
        auto writeStr = [&](const wchar_t* key, const std::wstring& val) {
            WritePrivateProfileStringW(section.c_str(), key, val.c_str(), sessionPath.c_str());
        };
        auto writeInt = [&](const wchar_t* key, int val) {
            writeStr(key, std::to_wstring(val));
        };
        
        writeStr(L"FilePath", doc->filePath);
        writeStr(L"CustomTitle", doc->customTitle);
        writeInt(L"IsNewFile", doc->isNewFile ? 1 : 0);
        writeInt(L"IsPinned", doc->isPinned ? 1 : 0);
        writeInt(L"Encoding", static_cast<int>(doc->encoding));
        writeInt(L"LineEnding", static_cast<int>(doc->lineEnding));
        writeInt(L"CursorStart", static_cast<int>(doc->cursorStart));
        writeInt(L"CursorEnd", static_cast<int>(doc->cursorEnd));
        writeInt(L"FirstVisibleLine", doc->firstVisibleLine);
        writeInt(L"IsNoteMode", doc->isNoteMode ? 1 : 0);
        writeStr(L"NoteId", doc->noteId);
        
        // Save bookmarks as comma-separated line numbers
        std::wstring bmStr;
        for (int bm : doc->bookmarks) {
            if (!bmStr.empty()) bmStr += L",";
            bmStr += std::to_wstring(bm);
        }
        writeStr(L"Bookmarks", bmStr);
        
        writeInt(L"IsModified", doc->isModified ? 1 : 0);
        
        // For untitled or modified tabs, save content to sidecar file
        if (doc->isNewFile || doc->isModified) {
            // Get the latest text from the document's editor
            std::wstring textToSave = doc->editor ? doc->editor->GetText() : L"";
            std::wstring contentPath = sessionDir + L"session_tab" + std::to_wstring(i) + L".txt";
            (void)FileIO::WriteFile(contentPath, textToSave, TextEncoding::UTF8, LineEnding::LF);
            writeInt(L"HasSavedContent", 1);
        }
    }
}

void MainWindow::LoadSession() {
    if (!m_documentManager || !m_settingsManager) return;
    
    std::wstring sessionPath = m_settingsManager->GetSettingsPath();
    size_t pos = sessionPath.rfind(L"config.ini");
    if (pos == std::wstring::npos) return;
    std::wstring sessionDir = sessionPath.substr(0, pos);
    sessionPath = sessionDir + L"session.ini";
    
    if (GetFileAttributesW(sessionPath.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    
    int tabCount = GetPrivateProfileIntW(L"Session", L"TabCount", 0, sessionPath.c_str());
    int activeIndex = GetPrivateProfileIntW(L"Session", L"ActiveIndex", 0, sessionPath.c_str());
    
    if (tabCount <= 0) {
        DeleteFileW(sessionPath.c_str());
        return;
    }
    
    for (int i = 0; i < tabCount; i++) {
        std::wstring section = L"Tab" + std::to_wstring(i);
        wchar_t buf[4096] = {};
        
        GetPrivateProfileStringW(section.c_str(), L"FilePath", L"", buf, 4096, sessionPath.c_str());
        std::wstring filePath = buf;
        
        GetPrivateProfileStringW(section.c_str(), L"CustomTitle", L"", buf, 4096, sessionPath.c_str());
        std::wstring customTitle = buf;
        
        bool isNewFile = GetPrivateProfileIntW(section.c_str(), L"IsNewFile", 1, sessionPath.c_str()) != 0;
        bool isPinned = GetPrivateProfileIntW(section.c_str(), L"IsPinned", 0, sessionPath.c_str()) != 0;
        auto encoding = static_cast<TextEncoding>(GetPrivateProfileIntW(section.c_str(), L"Encoding", 1, sessionPath.c_str()));
        auto lineEnding = static_cast<LineEnding>(GetPrivateProfileIntW(section.c_str(), L"LineEnding", 0, sessionPath.c_str()));
        DWORD cursorStart = static_cast<DWORD>(GetPrivateProfileIntW(section.c_str(), L"CursorStart", 0, sessionPath.c_str()));
        DWORD cursorEnd = static_cast<DWORD>(GetPrivateProfileIntW(section.c_str(), L"CursorEnd", 0, sessionPath.c_str()));
        int firstVisibleLine = GetPrivateProfileIntW(section.c_str(), L"FirstVisibleLine", 0, sessionPath.c_str());
        bool isNoteMode = GetPrivateProfileIntW(section.c_str(), L"IsNoteMode", 0, sessionPath.c_str()) != 0;
        
        GetPrivateProfileStringW(section.c_str(), L"NoteId", L"", buf, 4096, sessionPath.c_str());
        std::wstring noteId = buf;
        
        // Parse bookmarks
        GetPrivateProfileStringW(section.c_str(), L"Bookmarks", L"", buf, 4096, sessionPath.c_str());
        std::set<int> bookmarks;
        {
            std::wstring bmStr = buf;
            std::wistringstream bmStream(bmStr);
            std::wstring token;
            while (std::getline(bmStream, token, L',')) {
                if (!token.empty()) {
                    try { bookmarks.insert(std::stoi(token)); } catch (...) {}
                }
            }
        }
        
        bool hasSavedContent = GetPrivateProfileIntW(section.c_str(), L"HasSavedContent", 0, sessionPath.c_str()) != 0;
        bool isModified = GetPrivateProfileIntW(section.c_str(), L"IsModified", 0, sessionPath.c_str()) != 0;
        
        std::wstring content;
        std::wstring cleanContent;  // The on-disk version (for modified detection)
        
        if (!isNewFile && !filePath.empty()) {
            // File-based tab: re-read from disk
            if (GetFileAttributesW(filePath.c_str()) == INVALID_FILE_ATTRIBUTES) continue;
            FileReadResult readResult = FileIO::ReadFile(filePath);
            if (!readResult.success) continue;
            cleanContent = readResult.content;
            encoding = readResult.detectedEncoding;
            lineEnding = readResult.detectedLineEnding;
            
            // If the tab had unsaved modifications, load from sidecar instead
            if (hasSavedContent && isModified) {
                std::wstring contentPath = sessionDir + L"session_tab" + std::to_wstring(i) + L".txt";
                FileReadResult sidecarResult = FileIO::ReadFile(contentPath);
                content = sidecarResult.success ? sidecarResult.content : cleanContent;
            } else {
                content = cleanContent;
            }
        } else if (hasSavedContent) {
            // Untitled/modified: read from sidecar
            std::wstring contentPath = sessionDir + L"session_tab" + std::to_wstring(i) + L".txt";
            FileReadResult readResult = FileIO::ReadFile(contentPath);
            if (readResult.success) content = readResult.content;
            cleanContent = isNewFile ? L"" : content;
        }
        
        if (i == 0) {
            // Reuse the initial empty tab created in OnCreate
            int tabId = m_documentManager->GetActiveTabId();
            auto* doc = m_documentManager->GetActiveDocument();
            if (doc) {
                doc->filePath = filePath;
                doc->customTitle = customTitle;
                doc->isNewFile = isNewFile;
                doc->isPinned = isPinned;
                doc->encoding = encoding;
                doc->lineEnding = lineEnding;
                doc->cursorStart = cursorStart;
                doc->cursorEnd = cursorEnd;
                doc->firstVisibleLine = firstVisibleLine;
                doc->isNoteMode = isNoteMode;
                doc->noteId = noteId;
                doc->isModified = isModified;
                doc->bookmarks = bookmarks;
                
                m_tabBar->SetTabTitle(tabId, doc->GetDisplayTitle());
                if (!filePath.empty()) m_tabBar->SetTabFilePath(tabId, filePath);
                if (isPinned) m_tabBar->SetTabPinned(tabId, true);
                if (doc->isModified) m_tabBar->SetTabModified(tabId, true);
                
                // Use the document's own editor (per-tab)
                if (doc->editor) {
                    doc->editor->SetText(content);
                    doc->editor->SetEncoding(encoding);
                    doc->editor->SetLineEnding(lineEnding);
                    doc->editor->SetModified(doc->isModified);
                    doc->editor->SetBookmarks(bookmarks);
                }
                // Use the editor's normalized text as cleanText so that
                // the modified-state comparison works correctly. RichEdit
                // normalizes line endings (e.g. \r\n -> \r), so we must
                // read back through the editor rather than using the raw
                // file content which may have different line endings.
                if (!doc->isModified && doc->editor) {
                    doc->cleanTextHash = std::hash<std::wstring>{}(doc->editor->GetText());
                } else if (doc->editor) {
                    // For modified docs, temporarily load clean content to
                    // get the normalized form, then restore modified content
                    doc->editor->SetText(cleanContent);
                    doc->cleanTextHash = std::hash<std::wstring>{}(doc->editor->GetText());
                    doc->editor->SetText(content);
                    doc->editor->SetModified(true);
                } else {
                    doc->cleanTextHash = std::hash<std::wstring>{}(cleanContent);
                }
                
                m_currentFile = filePath;
                m_isNewFile = isNewFile;
                m_isNoteMode = isNoteMode;
                m_currentNoteId = noteId;
            }
        } else {
            int tabId = m_documentManager->OpenDocument(filePath, content, encoding, lineEnding);
            auto* doc = m_documentManager->GetDocument(tabId);
            if (doc) {
                doc->customTitle = customTitle;
                doc->isNewFile = isNewFile;
                doc->isPinned = isPinned;
                doc->cursorStart = cursorStart;
                doc->cursorEnd = cursorEnd;
                doc->firstVisibleLine = firstVisibleLine;
                doc->isNoteMode = isNoteMode;
                doc->noteId = noteId;
                doc->bookmarks = bookmarks;
                
                // OpenDocument() already set cleanText from editor->GetText()
                // which is correctly normalized for RichEdit. Only override
                // it for modified docs where cleanContent differs from what
                // was loaded into the editor.
                if (isModified && doc->editor) {
                    doc->editor->SetText(cleanContent);
                    doc->cleanTextHash = std::hash<std::wstring>{}(doc->editor->GetText());
                    doc->editor->SetText(content);
                    doc->editor->SetModified(true);
                } else if (!isModified && doc->editor) {
                    doc->cleanTextHash = std::hash<std::wstring>{}(doc->editor->GetText());
                }
                
                doc->isModified = isModified;
                if (doc->editor) {
                    doc->editor->SetBookmarks(bookmarks);
                    doc->editor->SetModified(doc->isModified);
                }
                if (doc->isModified) {
                    m_tabBar->SetTabModified(tabId, true);
                }
                if (!customTitle.empty()) m_tabBar->SetTabTitle(tabId, customTitle);
                if (isPinned) m_tabBar->SetTabPinned(tabId, true);
            }
        }
    }
    
    // Switch to the previously active tab
    auto ids = m_documentManager->GetAllTabIds();
    if (activeIndex >= 0 && activeIndex < static_cast<int>(ids.size())) {
        OnTabSelected(ids[activeIndex]);
    }
    
    // Start monitoring the restored file
    StartFileMonitoring();
    
    // Clean up sidecar files
    for (int i = 0; i < tabCount; i++) {
        std::wstring contentPath = sessionDir + L"session_tab" + std::to_wstring(i) + L".txt";
        DeleteFileW(contentPath.c_str());
    }
    
    // Delete session file
    DeleteFileW(sessionPath.c_str());
    
    UpdateTitle();
    UpdateStatusBar();
}

//------------------------------------------------------------------------------
// System tray
//------------------------------------------------------------------------------
void MainWindow::InitializeSystemTray() {
    m_trayIcon = {};
    m_trayIcon.cbSize = sizeof(m_trayIcon);
    m_trayIcon.hWnd = m_hwnd;
    m_trayIcon.uID = 1;
    m_trayIcon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_trayIcon.uCallbackMessage = WM_APP_TRAYICON;
    m_trayIcon.hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wcscpy_s(m_trayIcon.szTip, L"QNote");
    
    Shell_NotifyIconW(NIM_ADD, &m_trayIcon);
    m_trayIconCreated = true;
}

void MainWindow::CleanupSystemTray() {
    if (m_trayIconCreated) {
        Shell_NotifyIconW(NIM_DELETE, &m_trayIcon);
        m_trayIconCreated = false;
    }
}

void MainWindow::ShowTrayContextMenu() {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_SHOW, L"&Show QNote");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_CAPTURE, L"Quick &Capture\tCtrl+Shift+Q");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_EXIT, L"E&xit");
    
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(m_hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);
}

void MainWindow::MinimizeToTray() {
    ShowWindow(m_hwnd, SW_HIDE);
    m_minimizedToTray = true;
}

void MainWindow::RestoreFromTray() {
    ShowWindow(m_hwnd, SW_SHOW);
    ShowWindow(m_hwnd, SW_RESTORE);
    SetForegroundWindow(m_hwnd);
    m_minimizedToTray = false;
}

} // namespace QNote
