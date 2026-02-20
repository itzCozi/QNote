#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Removes the QNote Notepad replacement registration.

.DESCRIPTION
    Deletes the IFEO "Debugger" value that was set by install-as-notepad.ps1,
    restoring the original Windows Notepad behaviour.
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ifeoPaths = @(
    "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe"
)

if ([Environment]::Is64BitOperatingSystem) {
    $ifeoPaths += "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe"
}

$removed = $false

foreach ($path in $ifeoPaths) {
    if (Test-Path $path) {
        $prop = Get-ItemProperty -Path $path -Name "Debugger" -ErrorAction SilentlyContinue
        if ($prop) {
            Remove-ItemProperty -Path $path -Name "Debugger"
            Write-Host "Removed Debugger value from: $path"
            $removed = $true
        }

        # Remove the key entirely if it is now empty (no remaining values or subkeys).
        $key = Get-Item -Path $path
        if ($key.ValueCount -eq 0 -and $key.SubKeyCount -eq 0) {
            Remove-Item -Path $path -Force
            Write-Host "Removed empty key: $path"
        }
    }
}

if ($removed) {
    Write-Host ""
    Write-Host "Done. Windows Notepad has been restored as the default." -ForegroundColor Green
} else {
    Write-Host "No QNote IFEO registration found — nothing to remove." -ForegroundColor Yellow
}
