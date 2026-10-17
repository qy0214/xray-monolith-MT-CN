@echo off
rem Host tests for engine-independent code. Exit code 0 = all checks passed.
setlocal
set MSBUILD="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
if not exist %MSBUILD% set MSBUILD=msbuild
%MSBUILD% /nologo /v:m /p:Configuration=Release /p:Platform=x64 "%~dp0tests.vcxproj"
if errorlevel 1 exit /b 1
"%~dp0build\tests.exe"
exit /b %errorlevel%
