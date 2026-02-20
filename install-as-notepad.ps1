#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Registers QNote as the Windows Notepad replacement.

.DESCRIPTION
    Uses the Image File Execution Options (IFEO) registry key to intercept all
    notepad.exe launches and redirect them to QNote.  This works for every call
    to Notepad — context-menu "Edit", Win+R notepad, or programmatic invocations —
    regardless of file-type associations.

    Run uninstall-as-notepad.ps1 to revert to the original Notepad.

.PARAMETER QNotePath
    Full path to QNote.exe.  If omitted the script looks for QNote.exe in the
    same directory as this script, then in common installation locations.

.EXAMPLE
    .\install-as-notepad.ps1
    .\install-as-notepad.ps1 -QNotePath "C:\Tools\QNote\QNote.exe"
#>

param (
    [string]$QNotePath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# Locate QNote.exe
# ---------------------------------------------------------------------------
if (-not $QNotePath) {
    $candidates = @(
        Join-Path $PSScriptRoot "QNote.exe",
        Join-Path $PSScriptRoot "build\QNote.exe",
        Join-Path $PSScriptRoot "build\Release\QNote.exe",
        "$env:ProgramFiles\QNote\QNote.exe",
        "${env:ProgramFiles(x86)}\QNote\QNote.exe",
        "$env:LOCALAPPDATA\QNote\QNote.exe"
    )

    foreach ($c in $candidates) {
        if (Test-Path $c -PathType Leaf) {
            $QNotePath = $c
            break
        }
    }
}

if (-not $QNotePath -or -not (Test-Path $QNotePath -PathType Leaf)) {
    Write-Error @"
Could not find QNote.exe.  Please provide the path explicitly:

    .\install-as-notepad.ps1 -QNotePath "C:\Path\To\QNote.exe"
"@
    exit 1
}

$QNotePath = (Resolve-Path $QNotePath).Path
Write-Host "Using QNote: $QNotePath"

# ---------------------------------------------------------------------------
# IFEO registry paths
# Both 32-bit and 64-bit notepad need to be covered on 64-bit Windows.
# ---------------------------------------------------------------------------
$ifeoPaths = @(
    "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe"
)

# On 64-bit Windows there is also a 32-bit view; cover it via Wow6432Node.
if ([Environment]::Is64BitOperatingSystem) {
    $ifeoPaths += "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe"
}

foreach ($path in $ifeoPaths) {
    if (-not (Test-Path $path)) {
        New-Item -Path $path -Force | Out-Null
    }
    Set-ItemProperty -Path $path -Name "Debugger" -Value "`"$QNotePath`"" -Type String
    Write-Host "Set IFEO key: $path"
}

Write-Host ""
Write-Host "Success! QNote is now the default Notepad replacement." -ForegroundColor Green
Write-Host "All future notepad.exe launches will open QNote instead."
Write-Host ""
Write-Host "To revert, run:  .\uninstall-as-notepad.ps1"
