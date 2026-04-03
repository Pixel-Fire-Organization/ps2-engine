@echo off

set "ISO_PATH=%~1"
set "PCSX2_PATH=%~2"

if "%ISO_PATH%"=="" set "ISO_PATH=.\exec\engine.iso"

echo === Launching PCSX2 with %ISO_PATH% ===
if "%PCSX2_PATH%"=="" (
    powershell -ExecutionPolicy Bypass -File "%~dp0runEmulator.ps1" -IsoPath "%ISO_PATH%"
) else (
    powershell -ExecutionPolicy Bypass -File "%~dp0runEmulator.ps1" -IsoPath "%ISO_PATH%" -Pcsx2Path "%PCSX2_PATH%"
)
