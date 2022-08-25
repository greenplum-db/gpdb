$progressPreference = 'silentlyContinue'
Start-Process msiexec.exe -Wait -ArgumentList '/I greenplum-clients-x86_64.msi /quiet'
$env:PATH="C:\Program Files\Greenplum\greenplum-clients\bin;C:\Program Files\curl-win64-mingw\bin;" + $env:PATH
