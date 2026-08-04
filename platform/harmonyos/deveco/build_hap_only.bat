@echo off
REM =========================================================================
REM build_hap_only.bat - Package the HAP without rebuilding libgodot.so
REM =========================================================================
REM hvigorw only resolves its module paths correctly when launched from cmd
REM with a native Windows working directory; invoking it from an MSYS shell
REM makes it report "Path not found" for the entry module. Scripts running
REM under bash should therefore call this wrapper instead of hvigorw directly.
REM
REM Usage: build_hap_only.bat [debug|release]
REM =========================================================================
setlocal

set "BUILD_MODE=%~1"
if "%BUILD_MODE%"=="" set "BUILD_MODE=debug"

cd /d "%~dp0" || exit /b 1

if not defined DEVECO_SDK_HOME (
    set "DEVECO_SDK_HOME=C:\Users\happyelements\AppData\Local\OpenHarmony\Sdk\20"
)
if not defined JAVA_HOME (
    set "JAVA_HOME=C:\Program Files\Huawei\DevEco Studio\jbr"
)
if not defined NODE_HOME (
    set "NODE_HOME=C:\Program Files\Huawei\DevEco Studio\tools\node"
)
set "PATH=%NODE_HOME%;%JAVA_HOME%\bin;%PATH%"

call hvigorw.bat assembleHap --mode module -p product=default -p buildMode=%BUILD_MODE% --no-daemon
exit /b %ERRORLEVEL%
