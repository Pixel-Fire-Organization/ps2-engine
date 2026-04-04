@echo off

set "FILE_PATH=%~1"
set "PCSX2_PATH=%~2"

if "%FILE_PATH%"=="" set "FILE_PATH=.\exec\engine.iso"

echo === Launching PCSX2 with %FILE_PATH% ===
if "%PCSX2_PATH%"=="" (
    powershell -ExecutionPolicy Bypass -File "%~dp0runEmulator.ps1" -FilePath "%FILE_PATH%"
) else (
    powershell -ExecutionPolicy Bypass -File "%~dp0runEmulator.ps1" -FilePath "%FILE_PATH%" -Pcsx2Path "%PCSX2_PATH%"
)
