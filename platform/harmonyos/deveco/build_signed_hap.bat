@echo off
setlocal

set "BUILD_MODE=%~1"
if "%BUILD_MODE%"=="" set "BUILD_MODE=debug"

cd /d "%~dp0" || exit /b 1

if not exist "signing.local.json" (
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0configure_local_signing.ps1"
    if errorlevel 1 exit /b %ERRORLEVEL%
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0sign_unsigned_hap.ps1" -BuildMode "%BUILD_MODE%"
exit /b %ERRORLEVEL%
