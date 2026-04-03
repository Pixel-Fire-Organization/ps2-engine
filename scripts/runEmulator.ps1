# runEmulator.ps1
# Usage: ./scripts/runEmulator.ps1 [[-IsoPath] <String>] [[-Pcsx2Path] <String>]
# Example: ./scripts/runEmulator.ps1 -Pcsx2Path "D:\Tools\pcsx2.exe"

param(
    [string]$IsoPath,
    [string]$Pcsx2Path
)

# 1. Resolve PCSX2 Path
if (-not $Pcsx2Path) {
    # Common Windows locations
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

# 2. Resolve ISO Path
$ProjectRoot = Get-Item -Path "$PSScriptRoot\.."
if (-not $IsoPath) {
    $IsoPath = Join-Path $ProjectRoot.FullName "exec\engine.iso"
} else {
    $IsoPath = Resolve-Path $IsoPath -ErrorAction SilentlyContinue
}

if (-not $IsoPath -or -not (Test-Path $IsoPath)) {
    Write-Error "ISO not found at '$IsoPath'. Please build the project first."
    exit 1
}

Write-Host "=== Launching PCSX2 ===" -ForegroundColor Cyan
Write-Host "PCSX2: $Pcsx2Path"
Write-Host "ISO:   $IsoPath"

Start-Process -FilePath $Pcsx2Path -ArgumentList "-portable", "-batch", "`"$IsoPath`""
