$progressPreference = 'silentlyContinue'
Invoke-WebRequest -Uri https://aka.ms/vs/15/release/VC_redist.x64.exe -OutFile VC_redist.x64.exe
Start-Process -FilePath "VC_redist.x64.exe" -ArgumentList "/passive" -Wait -Passthru
Start-Process msiexec.exe -Wait -ArgumentList '/I greenplum-clients-x86_64.msi /quiet'
$env:PATH="C:\Program Files\Greenplum\greenplum-clients\bin;C:\Program Files\curl-win64-mingw\bin;" + $env:PATH
