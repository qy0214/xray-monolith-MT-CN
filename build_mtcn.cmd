@echo off
REM ============================================================
REM  MT-CN engine build script (DX11 / DX11-AVX)
REM  Usage: double-click, or run from a command prompt:
REM         build_mtcn.cmd          -> DX11
REM         build_mtcn.cmd AVX      -> DX11-AVX  (the one the NS7 test env uses)
REM ============================================================
setlocal

set "SRC=%~dp0src"
set "SLN=%SRC%\engine-vs2022.sln"

REM ---- clear proxy env vars ----
REM MSBuild/CL can abort with MSB6001 "An item with the same key has already
REM been added" when both "http_proxy" and "HTTP_PROXY" (differing only in
REM case) exist in the environment. Building needs no network, so drop them.
set http_proxy=
set HTTP_PROXY=
set https_proxy=
set HTTPS_PROXY=
set all_proxy=
set ALL_PROXY=
set no_proxy=
set NO_PROXY=

if not exist "%SLN%" (
    echo [ERROR] Solution not found: %SLN%
    echo         Run this script from the repo root.
    if "%DOPAUSE%"=="1" pause
    exit /b 1
)

REM ---- locate MSBuild: vswhere first, then a known fixed path ----
set "MSBUILD="
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>nul`) do (
    if not defined MSBUILD set "MSBUILD=%%i"
)
if not defined MSBUILD (
    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" (
        set "MSBUILD=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    )
)
if not defined MSBUILD (
    echo [ERROR] MSBuild.exe not found.
    echo         Install "Visual Studio 2022 Build Tools", or run msbuild
    echo         from a "x64 Native Tools Command Prompt".
    if "%DOPAUSE%"=="1" pause
    exit /b 1
)

if /i "%~1"=="AVX" (set "CFG=DX11-AVX") else (set "CFG=DX11")

REM ---- pause control ----
REM Double-click runs keep the window open via pause; automated runs pass
REM NOPAUSE as any extra argument, otherwise pause blocks forever on stdin.
set "DOPAUSE=1"
if /i "%~2"=="NOPAUSE" set "DOPAUSE=0"
if /i "%~3"=="NOPAUSE" set "DOPAUSE=0"

echo.
echo Solution : %SLN%
echo Config   : %CFG% ^| x64
echo MSBuild  : %MSBUILD%
echo.

"%MSBUILD%" "%SLN%" /p:Configuration=%CFG% /p:Platform=x64 /m /nologo /v:minimal
set RC=%errorlevel%

echo.
if "%RC%"=="0" (
    echo [OK] Build succeeded. Output: _build\_game\bin_dbg\
) else (
    echo [FAIL] Build failed. Exit code %RC%
)
if "%DOPAUSE%"=="1" pause
exit /b %RC%
