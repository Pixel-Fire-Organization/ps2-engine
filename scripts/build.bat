@echo off

if "%WSL_DISTRO%"=="" set WSL_DISTRO=Ubuntu

if not "%~1"=="" set WSL_DISTRO=%~1

echo === Starting Build in WSL (%WSL_DISTRO%) ===
pushd %~dp0..
wsl -d %WSL_DISTRO% bash ./scripts/build.sh
if %ERRORLEVEL% NEQ 0 (
    echo === Build Failed ===
    popd
    exit /b %ERRORLEVEL%
)
echo === Build Completed Successfully ===
popd
