@echo off
setlocal EnableExtensions

rem FEC-RTTY Qt GUI build helper. Requires the local Qt SDK under ..\third_party\qt.
for %%I in ("%~dp0.") do set "ROOT=%%~fI"
if not defined QT_ROOT for %%I in ("%ROOT%\..\third_party\qt\6.8.3\mingw_64") do set "QT_ROOT=%%~fI"
set "BUILD=%ROOT%\build-gui"
set "WINDEPLOYQT=%QT_ROOT%\bin\windeployqt.exe"
set "PORTAUDIO_ARGS=-DFECTTY_WITH_PORTAUDIO=OFF"
set "PORTAUDIO_TARGET="

rem The official Qt MinGW package uses the MSVCRT runtime. Prefer the
rem installed MSYS2 MinGW toolchain so Qt and the application share one CRT.
if not defined TOOLCHAIN_ROOT if exist "C:\msys64\mingw64\bin\g++.exe" set "TOOLCHAIN_ROOT=C:\msys64\mingw64"
if defined TOOLCHAIN_ROOT if exist "%TOOLCHAIN_ROOT%\lib\pkgconfig\portaudio-2.0.pc" (
  set "PORTAUDIO_ARGS=-DFECTTY_WITH_PORTAUDIO=ON"
  set "PORTAUDIO_TARGET=fectty-portaudio-smoke"
)

if not exist "%QT_ROOT%\lib\cmake\Qt6\Qt6Config.cmake" (
  echo Qt 6.8.3 was not found at "%QT_ROOT%".
  echo Install the local dependency or set up an equivalent Qt 6 SDK.
  exit /b 1
)
if not exist "%WINDEPLOYQT%" (
  echo Qt deployment tool was not found at "%WINDEPLOYQT%".
  exit /b 1
)
where cmake >nul 2>nul
if errorlevel 1 (
  echo CMake was not found on PATH.
  exit /b 1
)

if defined TOOLCHAIN_ROOT (
  set "PATH=%TOOLCHAIN_ROOT%\bin;%QT_ROOT%\bin;%PATH%"
) else (
  set "PATH=%QT_ROOT%\bin;%PATH%"
)
echo Configuring Qt GUI in "%BUILD%"...
if defined TOOLCHAIN_ROOT (
  cmake -G "MinGW Makefiles" -S "%ROOT%" -B "%BUILD%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_ROOT%" -DCMAKE_C_COMPILER="%TOOLCHAIN_ROOT%\bin\gcc.exe" -DCMAKE_CXX_COMPILER="%TOOLCHAIN_ROOT%\bin\g++.exe" -DCMAKE_MAKE_PROGRAM="%TOOLCHAIN_ROOT%\bin\mingw32-make.exe" -DFECTTY_WITH_QT=ON %PORTAUDIO_ARGS% -DFECTTY_WITH_HAMLIB=OFF
) else (
  cmake -G "MinGW Makefiles" -S "%ROOT%" -B "%BUILD%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_ROOT%" -DFECTTY_WITH_QT=ON %PORTAUDIO_ARGS% -DFECTTY_WITH_HAMLIB=OFF
)
if errorlevel 1 exit /b 1

echo Building Qt GUI...
cmake --build "%BUILD%" --config Release --target fectty-gui fectty-tests fectty-audit-tests fectty-gui-text-tests fectty-waterfall-tests fectty-noise-prefix-bench %PORTAUDIO_TARGET% --parallel
if errorlevel 1 exit /b 1

echo Deploying Qt and compiler runtime beside the GUI...
"%WINDEPLOYQT%" --release --no-translations --compiler-runtime "%BUILD%\fectty-gui.exe"
if errorlevel 1 exit /b 1

if defined TOOLCHAIN_ROOT (
  for %%D in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
    if not exist "%TOOLCHAIN_ROOT%\bin\%%D" (
      echo Required MinGW runtime was not found: "%TOOLCHAIN_ROOT%\bin\%%D"
      exit /b 1
    )
    copy /y "%TOOLCHAIN_ROOT%\bin\%%D" "%BUILD%\%%D" >nul
    if errorlevel 1 exit /b 1
  )
)

if defined PORTAUDIO_TARGET (
  if not exist "%TOOLCHAIN_ROOT%\bin\libportaudio.dll" (
    echo PortAudio was enabled but libportaudio.dll was not found.
    exit /b 1
  )
  copy /y "%TOOLCHAIN_ROOT%\bin\libportaudio.dll" "%BUILD%\libportaudio.dll" >nul
  if errorlevel 1 exit /b 1
)

echo Running core, GUI text, and waterfall regression tests...
ctest --test-dir "%BUILD%" --build-config Release --output-on-failure
if errorlevel 1 exit /b 1

echo Qt GUI build and tests completed: "%BUILD%\fectty-gui.exe"
