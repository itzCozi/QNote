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
#define IDM_FORMAT_SCROLLLINES          3008

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
#define IDM_HELP_CHECKUPDATE            6002
#define IDM_HELP_WEBSITE                6003
#define IDM_HELP_BUGREPORT              6004

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

// View menu (additional)
#define IDM_VIEW_HIGHLIGHTLINE          4006
#define IDM_VIEW_SHOWWHITESPACE         4007

// Edit menu (bookmarks)
#define IDM_EDIT_TOGGLEBOOKMARK         2030
#define IDM_EDIT_NEXTBOOKMARK           2031
#define IDM_EDIT_PREVBOOKMARK           2032
#define IDM_EDIT_CLEARBOOKMARKS         2033

// Tools menu
#define IDM_TOOLS_EDITSHORTCUTS         10002
#define IDM_TOOLS_COPYFILEPATH          10003
#define IDM_TOOLS_AUTOSAVEOPTIONS       10004
#define IDM_TOOLS_URLENCODE             10005
#define IDM_TOOLS_URLDECODE             10006
#define IDM_TOOLS_BASE64ENCODE          10007
#define IDM_TOOLS_BASE64DECODE          10008
#define IDM_TOOLS_INSERTLOREM           10009
#define IDM_TOOLS_SPLITLINES            10010
#define IDM_TOOLS_TABSTOSPACES          10011
#define IDM_TOOLS_SPACESTOTABS          10012
#define IDM_TOOLS_WORDCOUNT             10013
#define IDM_TOOLS_REMOVEBLANKLINES      10014
#define IDM_TOOLS_JOINLINES             10015
#define IDM_TOOLS_FORMATJSON            10016
#define IDM_TOOLS_OPENTERMINAL          10017
#define IDM_TOOLS_MINIFYJSON            10018

// Notes menu (additional)
#define IDM_NOTES_EXPORT                7010
#define IDM_NOTES_IMPORT                7011
#define IDM_NOTES_FAVORITES             7012
#define IDM_NOTES_DUPLICATE             7013

// File menu (additional 2)
#define IDM_FILE_SAVEALL                1011
#define IDM_FILE_CLOSEALL               1012
#define IDM_FILE_OPENFROMCLIPBOARD      1013

// Edit menu (additional 2)
#define IDM_EDIT_TITLECASE              2026
#define IDM_EDIT_REVERSELINES           2027
#define IDM_EDIT_NUMBERLINES            2028
#define IDM_EDIT_TOGGLECOMMENT          2029
#define IDM_EDIT_REVERSESELECTION       2034

// View menu (additional 2)
#define IDM_VIEW_ALWAYSONTOP            4008
#define IDM_VIEW_FULLSCREEN             4009
#define IDM_VIEW_TOGGLEMENUBAR          4010
#define IDM_VIEW_SPELLCHECK             4011
#define IDM_VIEW_SYNTAXHIGHLIGHT        4012

// Tools menu (additional 2)
#define IDM_TOOLS_CALCULATE             10021
#define IDM_TOOLS_INSERTGUID            10022
#define IDM_TOOLS_INSERTFILEPATH        10023
#define IDM_TOOLS_CONVERTEOL_SEL        10024
#define IDM_TOOLS_CHECKSUM              10025
#define IDM_TOOLS_RUNSELECTION          10026

// Tools menu (Character Map & Clipboard History)
#define IDM_TOOLS_CHARMAP               10030
#define IDM_TOOLS_CLIPHISTORY           10031

// Tools menu (Settings)
#define IDM_TOOLS_SETTINGS              10020

// Spell check context menu
#define IDM_SPELL_SUGGEST_BASE          11000   // 11000-11004 for up to 5 suggestions
#define IDM_SPELL_ADDWORD               11010
#define IDM_SPELL_IGNORE                11011

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

// Scroll lines dialog
#define IDD_SCROLLLINES                 207
#define IDC_SCROLLLINES_EDIT            1023

// Settings dialog
#define IDD_SETTINGS                    208
#define IDC_SETTINGS_TAB                1200

// Settings dialog - Editor page
#define IDC_SET_WORDWRAP                1210
#define IDC_SET_TABSIZE                 1211
#define IDC_SET_LBL_TABSIZE             1212
#define IDC_SET_SCROLLLINES             1213
#define IDC_SET_LBL_SCROLLLINES         1214
#define IDC_SET_AUTOSAVE                1215
#define IDC_SET_RTL                     1216

// Settings dialog - Appearance page
#define IDC_SET_LBL_FONT                1220
#define IDC_SET_FONTPREVIEW             1221
#define IDC_SET_FONTBUTTON              1222
#define IDC_SET_STATUSBAR               1223
#define IDC_SET_LINENUMBERS             1224
#define IDC_SET_WHITESPACE              1225
#define IDC_SET_LBL_ZOOM                1226
#define IDC_SET_ZOOM                    1227

// Settings dialog - Defaults page
#define IDC_SET_LBL_ENCODING            1230
#define IDC_SET_ENCODING                1231
#define IDC_SET_LBL_LINEENDING          1232
#define IDC_SET_LINEENDING              1233

// Settings dialog - Behavior page
#define IDC_SET_MINIMIZE_MODE           1240
#define IDC_SET_LBL_MINIMIZE            1241
#define IDC_SET_AUTOUPDATE              1242
#define IDC_SET_PORTABLE                1243
#define IDC_SET_PROMPTSAVE              1244
#define IDC_SET_LBL_SAVESTYLE           1245
#define IDC_SET_SAVESTYLE               1246
#define IDC_SET_LBL_AUTOSAVEDELAY       1247
#define IDC_SET_AUTOSAVEDELAY           1248
#define IDC_SET_LBL_CLOSEMODE           1249
#define IDC_SET_CLOSEMODE               1260

// Settings dialog - File Associations page
#define IDC_SET_ASSOC_TXT               1250
#define IDC_SET_ASSOC_LOG               1251
#define IDC_SET_ASSOC_MD                1252
#define IDC_SET_ASSOC_INI               1253
#define IDC_SET_ASSOC_CFG               1254
#define IDC_SET_ASSOC_JSON              1255
#define IDC_SET_ASSOC_XML               1256
#define IDC_SET_ASSOC_CSV               1257
#define IDC_SET_ASSOC_APPLY             1258

// Print Preview dialog
#define IDD_PRINTPREVIEW                212
#define IDC_PP_PREVIEW                  1300
#define IDC_PP_MARGIN_LEFT              1301
#define IDC_PP_MARGIN_RIGHT             1302
#define IDC_PP_MARGIN_TOP               1303
#define IDC_PP_MARGIN_BOTTOM            1304
#define IDC_PP_HEADER                   1305
#define IDC_PP_FOOTER_LEFT              1306
#define IDC_PP_FOOTER_RIGHT             1320
#define IDC_PP_PREVPAGE                 1307
#define IDC_PP_NEXTPAGE                 1308
#define IDC_PP_PAGEINFO                 1309
#define IDC_PP_PRINT                    1310
#define IDC_PP_LBL_LEFT                 1311
#define IDC_PP_LBL_RIGHT                1312
#define IDC_PP_LBL_TOP                  1313
#define IDC_PP_LBL_BOTTOM               1314
#define IDC_PP_LBL_HEADER               1315
#define IDC_PP_LBL_FOOTER_LEFT          1316
#define IDC_PP_LBL_FOOTER_RIGHT         1321
#define IDC_PP_LBL_HINTS                1317
#define IDC_PP_GRP_MARGINS              1318
#define IDC_PP_GRP_HEADFOOT             1319
#define IDC_PP_GRP_FONT                 1322
#define IDC_PP_FONT_LABEL               1323
#define IDC_PP_FONT_BTN                 1324
#define IDC_PP_APPLYALL                 1325

// Print Preview - Border group
#define IDC_PP_GRP_BORDER               1340
#define IDC_PP_BORDER_ENABLE            1341
#define IDC_PP_BORDER_STYLE             1342
#define IDC_PP_BORDER_WIDTH             1343
#define IDC_PP_BORDER_COLOR             1344
#define IDC_PP_BORDER_TOP               1345
#define IDC_PP_BORDER_BOTTOM            1346
#define IDC_PP_BORDER_LEFT              1347
#define IDC_PP_BORDER_RIGHT             1348
#define IDC_PP_LBL_BORDER_STYLE         1349
#define IDC_PP_LBL_BORDER_WIDTH         1350
#define IDC_PP_BORDER_PADDING           1351
#define IDC_PP_LBL_BORDER_PADDING       1352

// Print Preview - Options group
#define IDC_PP_GRP_OPTIONS              1360
#define IDC_PP_LINE_NUMBERS             1361
#define IDC_PP_LINE_SPACING             1362
#define IDC_PP_LBL_LINE_SPACING         1363
#define IDC_PP_ORIENTATION              1364
#define IDC_PP_LBL_ORIENTATION          1365

// Print Preview - Watermark group
#define IDC_PP_GRP_WATERMARK            1370
#define IDC_PP_WATERMARK_ENABLE         1371
#define IDC_PP_WATERMARK_TEXT           1372
#define IDC_PP_WATERMARK_COLOR          1373
#define IDC_PP_LBL_WATERMARK_TEXT       1374
#define IDC_PP_WATERMARK_FONT_BTN       1375
#define IDC_PP_WATERMARK_FONT_LABEL     1376

// Print Preview - Page Range
#define IDC_PP_GRP_PAGERANGE            1380
#define IDC_PP_RANGE_ALL                1381
#define IDC_PP_RANGE_PAGES              1382
#define IDC_PP_RANGE_EDIT               1383
#define IDC_PP_LBL_RANGE_HINT          1384

// Print Preview - Layout group (Columns, Gutter)
#define IDC_PP_GRP_LAYOUT               1390
#define IDC_PP_COLUMNS                  1391
#define IDC_PP_LBL_COLUMNS              1392
#define IDC_PP_COL_GAP                  1393
#define IDC_PP_LBL_COL_GAP              1394
#define IDC_PP_GUTTER                   1395
#define IDC_PP_LBL_GUTTER               1396

// Print Preview - Line Number styling
#define IDC_PP_LINENUM_COLOR            1410
#define IDC_PP_LINENUM_FONT             1411
#define IDC_PP_LINENUM_FONT_LABEL       1412

// Print Preview - Print Options group (Copies, Collate, Remember)
#define IDC_PP_GRP_PRINTOPTS            1400
#define IDC_PP_COPIES                   1401
#define IDC_PP_LBL_COPIES               1402
#define IDC_PP_COLLATE                  1403
#define IDC_PP_SAVE_SETTINGS            1404
#define IDC_PP_LOAD_SETTINGS            1405
#define IDC_PP_ADD_FILES                1406

// Print Preview - Printer group (Quality, Paper Source, Paper Size, Duplex, Page Filter)
#define IDC_PP_GRP_PRINTER              1420
#define IDC_PP_PRINT_QUALITY            1421
#define IDC_PP_LBL_PRINT_QUALITY        1422
#define IDC_PP_PAPER_SOURCE             1423
#define IDC_PP_LBL_PAPER_SOURCE         1424
#define IDC_PP_PAPER_SIZE               1425
#define IDC_PP_LBL_PAPER_SIZE           1426
#define IDC_PP_DUPLEX                   1427
#define IDC_PP_LBL_DUPLEX               1428
#define IDC_PP_PAGE_FILTER              1429
#define IDC_PP_LBL_PAGE_FILTER          1430
#define IDC_PP_PRINTER_INFO             1431
#define IDC_PP_CONDENSED                1432
#define IDC_PP_FORMFEED                 1433

// Convert EOL selection dialog
#define IDD_CONVERTEOL                  209
#define IDC_EOL_CRLF                    1290
#define IDC_EOL_LF                      1291
#define IDC_EOL_CR                      1292

// Checksum dialog
#define IDD_CHECKSUM                    210
#define IDC_CHECKSUM_MD5                1270
#define IDC_CHECKSUM_SHA256             1271

// Run output dialog
#define IDD_RUNOUTPUT                   211
#define IDC_RUNOUTPUT_EDIT              1280

// Print Tab Selection dialog
#define IDD_PRINTTABS                   213
#define IDC_PT_LIST                     1330
#define IDC_PT_SELECTALL                1331
#define IDC_PT_SELECTNONE               1332

// Auto-save options dialog controls
#define IDD_AUTOSAVE                    205
#define IDC_AUTOSAVE_ENABLE             1020
#define IDC_AUTOSAVE_INTERVAL           1021

// Split lines dialog controls
#define IDD_SPLITLINES                  206
#define IDC_SPLITLINES_WIDTH            1022

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
#define TIMER_REALSAVE                  5
#define TIMER_UPDATECHECK               6
#define TIMER_SYNTAXHIGHLIGHT           7

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
