@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-model-informed-montage-smoke.ps1" %*
exit /b %errorlevel%
