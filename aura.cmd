@echo off
setlocal

set "REPO_ROOT=%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%REPO_ROOT%installer\windows\aura_launcher.ps1" %*
exit /b %errorlevel%
