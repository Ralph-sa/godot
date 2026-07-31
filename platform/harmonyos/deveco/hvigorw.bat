@echo off
set "DEVECO_SDK_HOME=C:\Program Files\Huawei\DevEco Studio\sdk"
set "JAVA_HOME=C:\Program Files\Huawei\DevEco Studio\jbr"
set "PATH=%JAVA_HOME%\bin;%PATH%"
"C:\Program Files\Huawei\DevEco Studio\tools\hvigor\bin\hvigorw.bat" %*
