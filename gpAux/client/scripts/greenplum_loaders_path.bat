@echo off
echo Setting up PATH for Greenplum Loaders
set GPHOME_LOADERS=%~dp0
set PATH=%PATH%;%GPHOME_LOADERS%bin;%GPHOME_LOADERS%lib
set PYTHONPATH=%GPHOME_LOADERS%bin\lib;%PYTHONPATH%
echo.
echo LOADERS environment variables configured successfully.
echo.
