@echo off
REM ---------------------------------------------------------------------------
REM  Clear the xray shader cache.
REM  Can be run from either place:
REM    - the game folder (after JSGME installs MT-CN), or
REM    - MODS\MT-CN (the JSGME source folder)
REM  Run it AFTER installing MT-CN and AFTER uninstalling it: the compiled cache
REM  belongs to the shader set / engine that produced it.
REM ---------------------------------------------------------------------------
setlocal enabledelayedexpansion
set "CACHE="
for %%D in ("%~dp0" "%~dp0..\..\" "%~dp0..\") do (
  if not defined CACHE if exist "%%~fDappdata" set "CACHE=%%~fDappdata\shaders_cache"
)
if not defined CACHE (
  echo [ERROR] Could not find the game folder's appdata near this script.
  echo         Tried:
  echo           %~dp0appdata
  echo           %~dp0..\..\appdata
  echo           %~dp0..\appdata
  echo         Run this from the game folder ^(the one with bin\ and appdata\^)
  echo         or from MODS\MT-CN.
  pause
  exit /b 1
)
if not exist "%CACHE%" (
  echo Shader cache is already empty: "%CACHE%"
  pause
  exit /b 0
)
for /f %%C in ('dir /s /b /a-d "%CACHE%" 2^>nul ^| find /c /v ""') do set BEFORE=%%C
echo Clearing: "%CACHE%"
echo Files before: %BEFORE%
rmdir /s /q "%CACHE%"
echo Done. The next launch recompiles shaders once ^(slower startup^).
pause
