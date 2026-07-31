@echo off
REM =========================================================================
REM hvigorw_wrapper.bat - Windows Batch Wrapper for hvigorw CLI Build
REM =========================================================================
REM Fixes @ohos/hvigor module resolution by creating symbolic links
REM in the project's node_modules before running hvigorw.
REM =========================================================================

setlocal

set "HVIGOR_BIN=C:\Program Files\Huawei\DevEco Studio\tools\hvigor\bin"
set "HVIGOR_TOOLS=C:\Program Files\Huawei\DevEco Studio\tools\hvigor"
set "PROJ_NM=C:\Toro\GodotHOS\node_modules\@ohos"

set "DEVECO_SDK_HOME=C:\Users\happyelements\AppData\Local\OpenHarmony\Sdk\20"
set "JAVA_HOME=C:\Program Files\Huawei\DevEco Studio\jbr"

REM Ensure @ohos/hvigor link exists in project node_modules
if not exist "%PROJ_NM%\hvigor" (
    mklink /J "%PROJ_NM%\hvigor" "%HVIGOR_TOOLS%\hvigor" 2>nul
)

REM Ensure @ohos/hvigor exists inside hvigor-ohos-plugin's node_modules
set "PLUGIN_NM=%HVIGOR_TOOLS%\hvigor-ohos-plugin\node_modules\@ohos"
if not exist "%PLUGIN_NM%\hvigor" (
    mklink /J "%PLUGIN_NM%\hvigor" "%HVIGOR_TOOLS%\hvigor" 2>nul || (
        echo Failed to create junction - need admin privileges
        echo Please run this script as Administrator, or use DevEco Studio GUI
        exit /b 1
    )
)

echo [wrapper] Environment configured. Running hvigorw %*
echo.

"%HVIGOR_BIN%\hvigorw" %*

endlocal
