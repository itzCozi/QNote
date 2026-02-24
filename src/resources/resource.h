//==============================================================================
// QNote - A Lightweight Notepad Clone
// resource.h - Resource identifiers
//==============================================================================

#pragma once

// Application name (used in window titles, message boxes, etc.)
#define APP_NAME                        L"QNote"
#define APP_VERSION                     L"1.0.0"

// Standard control identifier for static text (if not defined by system headers)
#ifndef IDC_STATIC
#define IDC_STATIC                      (-1)
#endif

// Application icon
#define IDI_APP_ICON                    100

// Menu resource
#define IDR_MAIN_MENU                   101
#define IDR_ACCELERATORS                102

// File menu
#define IDM_FILE_NEW                    1001
#define IDM_FILE_OPEN                   1002
#define IDM_FILE_SAVE                   1003
#define IDM_FILE_SAVEAS                 1004
#define IDM_FILE_NEWWINDOW              1006
#define IDM_FILE_PAGESETUP              1007
#define IDM_FILE_PRINT                  1008
#define IDM_FILE_EXIT                   1005
#define IDM_FILE_RECENT_BASE            1100  // 1100-1109 for recent files

// Edit menu
#define IDM_EDIT_UNDO                   2001
#define IDM_EDIT_REDO                   2002
#define IDM_EDIT_CUT                    2003
#define IDM_EDIT_COPY                   2004
#define IDM_EDIT_PASTE                  2005
#define IDM_EDIT_DELETE                 2006
#define IDM_EDIT_SELECTALL              2007
#define IDM_EDIT_FIND                   2008
#define IDM_EDIT_FINDNEXT               2009
#define IDM_EDIT_REPLACE                2010
#define IDM_EDIT_GOTO                   2011
#define IDM_EDIT_DATETIME               2012

// Format menu
#define IDM_FORMAT_WORDWRAP             3001
#define IDM_FORMAT_FONT                 3002
#define IDM_FORMAT_TABSIZE              3003
#define IDM_FORMAT_EOL_CRLF             3004
#define IDM_FORMAT_EOL_LF               3005
#define IDM_FORMAT_EOL_CR               3006
#define IDM_FORMAT_RTL                  3007

// View menu
#define IDM_VIEW_STATUSBAR              4001
#define IDM_VIEW_LINENUMBERS            4005
#define IDM_VIEW_ZOOMIN                 4002
#define IDM_VIEW_ZOOMOUT                4003
#define IDM_VIEW_ZOOMRESET              4004

// Encoding submenu
#define IDM_ENCODING_UTF8               5001
#define IDM_ENCODING_UTF8BOM            5002
#define IDM_ENCODING_UTF16LE            5003
#define IDM_ENCODING_UTF16BE            5004
#define IDM_ENCODING_ANSI               5005

// Help menu
#define IDM_HELP_ABOUT                  6001

// Notes menu (new features)
#define IDM_NOTES_NEW                   7001
#define IDM_NOTES_QUICKCAPTURE          7002
#define IDM_NOTES_ALLNOTES              7003
#define IDM_NOTES_PINNED                7004
#define IDM_NOTES_TIMELINE              7005
#define IDM_NOTES_SEARCH                7006
#define IDM_NOTES_PINNOTE               7007
#define IDM_NOTES_DELETENOTE            7008
#define IDM_NOTES_SAVENOW               7009

// Tab menu
#define IDM_TAB_NEW                     8001
#define IDM_TAB_CLOSE                   8002
#define IDM_TAB_NEXT                    8003
#define IDM_TAB_PREV                    8004

// File menu (additional)
#define IDM_FILE_REVERT                 1009
#define IDM_FILE_OPENCONTAINING         1010

// Edit menu (text manipulation)
#define IDM_EDIT_UPPERCASE              2020
#define IDM_EDIT_LOWERCASE              2021
#define IDM_EDIT_SORTLINES_ASC          2022
#define IDM_EDIT_SORTLINES_DESC         2023
#define IDM_EDIT_TRIMWHITESPACE         2024
#define IDM_EDIT_REMOVEDUPLICATES       2025

// Reopen with encoding
#define IDM_ENCODING_REOPEN_UTF8        5101
#define IDM_ENCODING_REOPEN_UTF8BOM     5102
#define IDM_ENCODING_REOPEN_UTF16LE     5103
#define IDM_ENCODING_REOPEN_UTF16BE     5104
#define IDM_ENCODING_REOPEN_ANSI        5105

// System tray
#define IDM_TRAY_SHOW                   9001
#define IDM_TRAY_CAPTURE                9002
#define IDM_TRAY_EXIT                   9003

// Dialog controls
#define IDD_FIND                        200
#define IDD_REPLACE                     201
#define IDD_GOTO                        202
#define IDD_ABOUT                       203
#define IDD_TABSIZE                     204

// Find/Replace dialog controls
#define IDC_FIND_EDIT                   1001
#define IDC_REPLACE_EDIT                1002
#define IDC_FIND_NEXT                   1003
#define IDC_REPLACE_BTN                 1004
#define IDC_REPLACE_ALL                 1005
#define IDC_MATCH_CASE                  1006
#define IDC_WRAP_AROUND                 1007
#define IDC_USE_REGEX                   1008
#define IDC_DIRECTION_UP                1009
#define IDC_DIRECTION_DOWN              1010

// GoTo dialog controls
#define IDC_GOTO_LINE                   1011

// Tab size dialog controls
#define IDC_TABSIZE_EDIT                1012

// Status bar parts
#define SB_PART_POSITION                0
#define SB_PART_ENCODING                1
#define SB_PART_EOL                     2
#define SB_PART_ZOOM                    3
#define SB_PART_COUNTS                  4

// Custom messages
#define WM_APP_UPDATETITLE              (WM_APP + 1)
#define WM_APP_UPDATESTATUS             (WM_APP + 2)
#define WM_APP_FILECHANGED              (WM_APP + 3)
#define WM_APP_OPENNOTE                 (WM_APP + 4)
#define WM_APP_TRAYICON                 (WM_APP + 5)

// Timer IDs
#define TIMER_STATUSUPDATE              1
#define TIMER_AUTOSAVE                  2
#define TIMER_FILEAUTOSAVE              3
#define TIMER_FILEWATCH                 4

// Global hotkey ID
#define HOTKEY_QUICKCAPTURE             1

// Version info
#define VER_MAJOR                       1
#define VER_MINOR                       0
#define VER_PATCH                       0
#define VER_BUILD                       0

#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)
#define VER_STRING STRINGIFY(VER_MAJOR) "." STRINGIFY(VER_MINOR) "." STRINGIFY(VER_PATCH) "." STRINGIFY(VER_BUILD)
