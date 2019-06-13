set GPDB_INSTALL_PATH=%1
set VERSION=%2
echo %VERSION% > %GPDB_INSTALL_PATH%\VERSION
copy ..\..\..\..\..\NOTICE %GPDB_INSTALL_PATH%
copy ..\..\..\..\..\LICENSE %GPDB_INSTALL_PATH%
copy ..\..\..\scripts\greenplum_clients_path.bat %GPDB_INSTALL_PATH%
mkdir %GPDB_INSTALL_PATH%\lib\python\yaml
copy ..\..\..\..\..\gpMgmt\bin\gpload.py %GPDB_INSTALL_PATH%\bin
perl -pi.bak -e "s,\$Revision\$,%VERSION%," %GPDB_INSTALL_PATH%\bin\gpload.py
copy ..\..\..\..\..\gpMgmt\bin\gpload.bat %GPDB_INSTALL_PATH%\bin
copy ..\..\..\..\..\gpMgmt\bin\ext\yaml\* %GPDB_INSTALL_PATH%\lib\python\yaml
perl -p -e "s,__VERSION_PLACEHOLDER__,%VERSION%," greenplum-clients.wxs > greenplum-clients-%VERSION%.wxs
candle.exe -nologo greenplum-clients-%VERSION%.wxs -out greenplum-clients-%VERSION%.wixobj -dSRCDIR=%GPDB_INSTALL_PATH% -dVERSION=%VERSION%
light.exe -nologo -sval greenplum-clients-%VERSION%.wixobj -out greenplum-clients-x86_64.msi