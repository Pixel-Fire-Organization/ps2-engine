# runEmulator.ps1
# Usage: ./scripts/runEmulator.ps1 [[-FilePath] <String>] [[-Pcsx2Path] <String>]
# FilePath can be an ISO or an ELF — PCSX2 auto-detects the format.
# Example: ./scripts/runEmulator.ps1 -Pcsx2Path "D:\Tools\pcsx2.exe"

param(
    [string]$FilePath,
    [string]$Pcsx2Path
)

if (-not $Pcsx2Path) {

    $Pcsx2Paths = @(
        "C:\PCSX2\pcsx2-qt.exe",
        "C:\Program Files\PCSX2\pcsx2-qt.exe",
        "$env:LOCALAPPDATA\PCSX2\pcsx2-qt.exe"
    )

    foreach ($path in $Pcsx2Paths) {
        if (Test-Path $path) {
            $Pcsx2Path = $path
            break
        }
    }
}

if (-not $Pcsx2Path -or -not (Test-Path $Pcsx2Path)) {
    Write-Error "PCSX2 not found at '$Pcsx2Path'. Please provide the path via -Pcsx2Path or edit the script."
    exit 1
}


$ProjectRoot = Get-Item -Path "$PSScriptRoot\.."
if (-not $FilePath) {
    $FilePath = Join-Path $ProjectRoot.FullName "exec\engine.iso"
} else {
    $FilePath = Resolve-Path $FilePath -ErrorAction SilentlyContinue
}

if (-not $FilePath -or -not (Test-Path $FilePath)) {
    Write-Error "Game file not found at '$FilePath'. Please build the project first."
    exit 1
}

Write-Host "=== Launching PCSX2 ===" -ForegroundColor Cyan
Write-Host "PCSX2: $Pcsx2Path"
Write-Host "File:  $FilePath"

Start-Process -FilePath $Pcsx2Path -ArgumentList "-portable", "-batch", "`"$FilePath`""
