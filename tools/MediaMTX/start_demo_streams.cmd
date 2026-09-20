@echo off
setlocal
title UE5 Drone Promo Streams

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0start_demo_streams.ps1"
set "EXIT_CODE=%ERRORLEVEL%"

echo.
if not "%EXIT_CODE%"=="0" echo Startup failed. Review the messages above.
echo Press any key to close this window. Background services will keep running.
pause >nul
exit /b %EXIT_CODE%
