; ==============================================================================
; QNote - Inno Setup Installer Script
; ==============================================================================
; Prerequisites: Install Inno Setup 6 from https://jrsoftware.org/isinfo.php
; Build: Open this file in Inno Setup Compiler and click Build > Compile
; Or from command line: iscc QNote.iss
; ==============================================================================

#define MyAppName "QNote"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Cooper Ransom"
#define MyAppURL "https://github.com/itzcozi/qnote"
#define MyAppExeName "QNote.exe"

[Setup]
; Unique app identifier (generate a new GUID for your app)
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
LicenseFile=..\LICENSE
OutputDir=..\dist
OutputBaseFilename=QNote-{#MyAppVersion}-Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
; Require admin for Program Files install + registry
PrivilegesRequired=admin
; Minimum Windows 10
MinVersion=10.0
; Architecture
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Uninstall info
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "associatetxt"; Description: "Associate with .txt files"; GroupDescription: "File Associations:"; Flags: unchecked
Name: "associatelog"; Description: "Associate with .log files"; GroupDescription: "File Associations:"; Flags: unchecked
Name: "associateini"; Description: "Associate with .ini files"; GroupDescription: "File Associations:"; Flags: unchecked
Name: "associatecfg"; Description: "Associate with .cfg files"; GroupDescription: "File Associations:"; Flags: unchecked
Name: "replacenotepad"; Description: "Replace Windows Notepad (intercepts notepad.exe calls)"; GroupDescription: "Notepad Replacement:"; Flags: unchecked

[Files]
; Main executable - adjust the source path to where your built exe is
Source: "..\build\QNote.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; App Paths - makes QNote findable via Start menu search and Run dialog
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\QNote.exe"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName}"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\QNote.exe"; ValueType: string; ValueName: "Path"; ValueData: "{app}"; Flags: uninsdeletekey

; Register QNote as a capable application for Open With
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\QNote.exe\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\QNote.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "QNote"; Flags: uninsdeletekey

; Register QNote ProgId
Root: HKLM; Subkey: "SOFTWARE\Classes\QNote.TextFile"; ValueType: string; ValueName: ""; ValueData: "Text Document - QNote"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\QNote.TextFile\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Classes\QNote.TextFile\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey

; .txt association (only if task selected)
Root: HKLM; Subkey: "SOFTWARE\Classes\.txt\OpenWithProgids"; ValueType: string; ValueName: "QNote.TextFile"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associatetxt
; .log association
Root: HKLM; Subkey: "SOFTWARE\Classes\.log\OpenWithProgids"; ValueType: string; ValueName: "QNote.TextFile"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associatelog
; .ini association
Root: HKLM; Subkey: "SOFTWARE\Classes\.ini\OpenWithProgids"; ValueType: string; ValueName: "QNote.TextFile"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associateini
; .cfg association
Root: HKLM; Subkey: "SOFTWARE\Classes\.cfg\OpenWithProgids"; ValueType: string; ValueName: "QNote.TextFile"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associatecfg

; Replace Notepad via IFEO (only if task selected)
; QNote handles the notepad.exe argument automatically
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe"; ValueType: string; ValueName: "Debugger"; ValueData: """{app}\{#MyAppExeName}"""; Flags: uninsdeletevalue; Tasks: replacenotepad

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
// Notify Windows that file associations have changed
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    // Notify shell of association changes
    // SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
    RegWriteStringValue(HKLM, 'SOFTWARE\RegisteredApplications', 'QNote',
      'SOFTWARE\QNote\Capabilities');
    RegWriteStringValue(HKLM, 'SOFTWARE\QNote\Capabilities', 'ApplicationName', 'QNote');
    RegWriteStringValue(HKLM, 'SOFTWARE\QNote\Capabilities', 'ApplicationDescription',
      'Lightweight Notepad replacement for Windows');
    RegWriteStringValue(HKLM, 'SOFTWARE\QNote\Capabilities\FileAssociations', '.txt', 'QNote.TextFile');
    RegWriteStringValue(HKLM, 'SOFTWARE\QNote\Capabilities\FileAssociations', '.log', 'QNote.TextFile');
    RegWriteStringValue(HKLM, 'SOFTWARE\QNote\Capabilities\FileAssociations', '.ini', 'QNote.TextFile');
    RegWriteStringValue(HKLM, 'SOFTWARE\QNote\Capabilities\FileAssociations', '.cfg', 'QNote.TextFile');
  end;
end;

// Clean up on uninstall
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
  begin
    RegDeleteKeyIncludingSubkeys(HKLM, 'SOFTWARE\QNote');
    RegDeleteValue(HKLM, 'SOFTWARE\RegisteredApplications', 'QNote');
  end;
end;
