@echo off
echo Setting up PATH for Greenplum Clients
set GPHOME_CLIENTS=%~dp0
set PATH=%PATH%;%GPHOME_CLIENTS%bin;%GPHOME_CLIENTS%lib
echo.
echo CLIENTS environment variables configured successfully.
echo.
