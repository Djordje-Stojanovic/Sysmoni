@echo off
setlocal

set "REPO_ROOT=%~dp0"
set "BUILD_SCRIPT=%REPO_ROOT%installer\windows\build_platform_native.ps1"
set "EXE_DIST=%REPO_ROOT%build\platform-native\dist\aura.exe"
set "EXE_RELEASE=%REPO_ROOT%build\platform-native\Release\aura.exe"
set "EXE_FLAT=%REPO_ROOT%build\platform-native\aura.exe"

if exist "%EXE_DIST%" goto run_dist
if exist "%EXE_RELEASE%" goto run_release
if exist "%EXE_FLAT%" goto run_flat

echo [aura] native binary not found, building once...
powershell -NoProfile -ExecutionPolicy Bypass -File "%BUILD_SCRIPT%"
if errorlevel 1 (
  echo [aura] build failed.
  exit /b 1
)

if exist "%EXE_DIST%" goto run_dist
if exist "%EXE_RELEASE%" goto run_release
if exist "%EXE_FLAT%" goto run_flat

echo [aura] build finished but aura.exe was not found.
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
