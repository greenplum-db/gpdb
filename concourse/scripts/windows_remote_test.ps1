Invoke-WebRequest -Uri https://aka.ms/vs/15/release/VC_redist.x64.exe -OutFile VC_redist.x64.exe
Start-Process -FilePath "VC_redist.x64.exe" -ArgumentList "/passive" -Wait -Passthru
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
Invoke-WebRequest -Uri https://curl.haxx.se/windows/dl-7.64.1/curl-7.64.1-win64-mingw.zip -OutFile curl-7.64.1-win64-mingw.zip
Expand-Archive -LiteralPath curl-7.64.1-win64-mingw.zip -DestinationPath "C:\Program Files\"
move "C:\Program Files\curl-*-win64-mingw" "C:\Program Files\curl-win64-mingw"
Start-Process msiexec.exe -Wait -ArgumentList '/I greenplum-clients-x86_64.msi /quiet'
$env:PATH="C:\Program Files\Greenplum\greenplum-clients\bin;C:\Program Files\curl-win64-mingw\bin;" + $env:PATH
