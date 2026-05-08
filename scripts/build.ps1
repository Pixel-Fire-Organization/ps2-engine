# build.ps1
# Usage: ./scripts/build.ps1 [[-WslDistro] <String>]

param(
    [string]$WslDistro = $(if ($env:PS2_WSL_DISTRO) { $env:PS2_WSL_DISTRO } else { "Ubuntu" })
)

$ProjectRoot = Get-Item -Path "$PSScriptRoot\.."
Push-Location $ProjectRoot.FullName

Write-Host "=== Starting Build in WSL ($WslDistro) ===" -ForegroundColor Cyan
wsl -d $WslDistro bash -lc "./scripts/build.sh"

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed with exit code $LASTEXITCODE"
} else {
    Write-Host "=== Build Completed Successfully ===" -ForegroundColor Green
}

Pop-Location
