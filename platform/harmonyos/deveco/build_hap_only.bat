@echo off
REM Packages the HAP without rebuilding libgodot.so.
REM Useful when only ArkTS sources changed (e.g. smoke-test wiring).
setlocal
cd /d C:\Toro\GodotHOS\godot_src\platform\harmonyos\deveco
set "DEVECO_SDK_HOME=C:\Users\happyelements\AppData\Local\OpenHarmony\Sdk\20"
set "JAVA_HOME=C:\Program Files\Huawei\DevEco Studio\jbr"
call hvigorw.bat assembleHap --mode module -p product=default -p buildMode=debug --no-daemon
endlocal
