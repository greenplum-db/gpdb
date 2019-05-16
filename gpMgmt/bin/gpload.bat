@echo off
set FOUND=
for %%X in (python.exe) do (set FOUND=%%~$PATH:X)
if not defined FOUND (
    echo "Ensure python2.7 64-bit is installed and set in PATH"
    exit /B 1
)

python "%~dp0\gpload.py" %*