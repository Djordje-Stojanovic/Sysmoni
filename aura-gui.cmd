@echo off
setlocal

set "REPO_ROOT=%~dp0"
set "BUILD_SCRIPT=%REPO_ROOT%installer\windows\build_shell_native.ps1"
set "EXE_DIST=%REPO_ROOT%build\shell-native\dist\aura_shell.exe"
set "EXE_RELEASE=%REPO_ROOT%build\shell-native\Release\aura_shell.exe"
set "EXE_FLAT=%REPO_ROOT%build\shell-native\aura_shell.exe"

if exist "%EXE_DIST%" goto run_dist
if exist "%EXE_RELEASE%" goto run_release
if exist "%EXE_FLAT%" goto run_flat

echo [aura-gui] native GUI binary not found, building once...
powershell -NoProfile -ExecutionPolicy Bypass -File "%BUILD_SCRIPT%"
if errorlevel 1 (
  echo [aura-gui] build failed.
  echo [aura-gui] If Qt6 is missing, run:
  echo   powershell -ExecutionPolicy Bypass -File .\installer\windows\build_shell_native.ps1 -Qt6Dir "C:\Qt\6.x.x\msvc2022_64\lib\cmake\Qt6"
  exit /b 1
)

if exist "%EXE_DIST%" goto run_dist
if exist "%EXE_RELEASE%" goto run_release
if exist "%EXE_FLAT%" goto run_flat

echo [aura-gui] build finished but aura_shell.exe was not found.
exit /b 1

:run_dist
"%EXE_DIST%" %*
exit /b %errorlevel%

:run_release
"%EXE_RELEASE%" %*
exit /b %errorlevel%

:run_flat
"%EXE_FLAT%" %*
exit /b %errorlevel%
