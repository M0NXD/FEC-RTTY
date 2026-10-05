@echo off
setlocal
set "ROOT=%~dp0.."
set "EXE=%ROOT%\bin\fectty-live.exe"
if not exist "%EXE%" set "EXE=%ROOT%\source\build\fectty-live.exe"
if not exist "%EXE%" (
  echo Build the project first with source\build.bat.
  exit /b 1
)
"%EXE%" --list
