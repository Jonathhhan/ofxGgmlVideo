@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-video-montage-workflow.ps1" %*
exit /b %ERRORLEVEL%
