REM vswhere is required, install it via `choco install vswhere`
@echo off
setlocal enabledelayedexpansion

REM /p:Platform=x64 is required: the solution only defines *|x64 mappings, and
REM without an explicit platform MSBuild may fail with MSB4126 instead of
REM guessing (zwork run 372f0ebd, hygiene #3).
for /f "usebackq tokens=*" %%i in (`vswhere -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
  "%%i" engine-vs2022.sln /p:Configuration=DX11 /p:Platform=x64
  "%%i" engine-vs2022.sln /p:Configuration=DX10 /p:Platform=x64
  "%%i" engine-vs2022.sln /p:Configuration=DX9 /p:Platform=x64
  "%%i" engine-vs2022.sln /p:Configuration=DX8 /p:Platform=x64
  "%%i" engine-vs2022.sln /p:Configuration=DX11-AVX /p:Platform=x64
  "%%i" engine-vs2022.sln /p:Configuration=DX10-AVX /p:Platform=x64
  "%%i" engine-vs2022.sln /p:Configuration=DX9-AVX /p:Platform=x64
  "%%i" engine-vs2022.sln /p:Configuration=DX8-AVX /p:Platform=x64
  pause
  exit /b !errorlevel!
)
