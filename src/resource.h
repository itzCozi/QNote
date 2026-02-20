//==============================================================================
// QNote - A Lightweight Notepad Clone
// resource.h - Resource identifiers
//==============================================================================

#pragma once

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
#define IDM_VIEW_ZOOMIN                 4002
#define IDM_VIEW_ZOOMOUT                4003
#define IDM_VIEW_ZOOMRESET              4004
#define IDM_VIEW_DARKMODE               4005
#define IDM_VIEW_THEME_SYSTEM           4006
#define IDM_VIEW_THEME_LIGHT            4007
#define IDM_VIEW_THEME_DARK             4008

// Encoding submenu
#define IDM_ENCODING_UTF8               5001
#define IDM_ENCODING_UTF8BOM            5002
#define IDM_ENCODING_UTF16LE            5003
#define IDM_ENCODING_UTF16BE            5004
#define IDM_ENCODING_ANSI               5005

// Help menu
#define IDM_HELP_ABOUT                  6001

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

// Custom messages
#define WM_APP_UPDATETITLE              (WM_APP + 1)
#define WM_APP_UPDATESTATUS             (WM_APP + 2)
#define WM_APP_FILECHANGED              (WM_APP + 3)

// Timer IDs
#define TIMER_STATUSUPDATE              1

// Version info
#define VER_MAJOR                       1
#define VER_MINOR                       0
#define VER_PATCH                       0
#define VER_BUILD                       0

#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)
#define VER_STRING STRINGIFY(VER_MAJOR) "." STRINGIFY(VER_MINOR) "." STRINGIFY(VER_PATCH) "." STRINGIFY(VER_BUILD)
