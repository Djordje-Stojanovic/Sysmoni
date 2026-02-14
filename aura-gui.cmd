@echo off
setlocal

set "REPO_ROOT=%~dp0"
call "%REPO_ROOT%aura.cmd" --gui %*
exit /b %errorlevel%
