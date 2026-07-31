@echo off
set "DEVECO_SDK_HOME=C:\Program Files\Huawei\DevEco Studio\sdk"
set "JAVA_HOME=C:\Program Files\Huawei\DevEco Studio\jbr"
set "PATH=%JAVA_HOME%\bin;%PATH%"
cd /d "C:\Toro\GodotHOS"
echo DEVECO_SDK_HOME=%DEVECO_SDK_HOME%
echo JAVA_HOME=%JAVA_HOME%
java -version
"C:\Program Files\Huawei\DevEco Studio\tools\hvigor\bin\hvigorw.bat" assembleHap --mode module -p product=default -p buildMode=debug
