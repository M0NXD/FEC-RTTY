@echo off
setlocal EnableExtensions

rem FEC-RTTY Qt GUI build helper. Requires the local Qt SDK under ..\third_party\qt.
for %%I in ("%~dp0.") do set "ROOT=%%~fI"
if not defined QT_ROOT for %%I in ("%ROOT%\..\third_party\qt\6.8.3\mingw_64") do set "QT_ROOT=%%~fI"
set "BUILD=%ROOT%\build-gui"
set "WINDEPLOYQT=%QT_ROOT%\bin\windeployqt.exe"
set "PORTAUDIO_ARGS=-DFECTTY_WITH_PORTAUDIO=OFF"
set "PORTAUDIO_TARGET="
if not defined FECTTY_BUILD_JOBS set "FECTTY_BUILD_JOBS=1"
if not defined HAMLIB_ROOT for %%I in ("%ROOT%\..\third_party\hamlib-4.7.2\sdk\hamlib-w64-4.7.2") do set "HAMLIB_ROOT=%%~fI"
if not defined LIBUSB_ROOT for %%I in ("%ROOT%\..\third_party\libusb-1.0.30\mingw64") do set "LIBUSB_ROOT=%%~fI"
if not exist "%HAMLIB_ROOT%\include\hamlib\rig.h" (
  echo Hamlib SDK not found. Set HAMLIB_ROOT to the official Windows x64 SDK.
  exit /b 1
)

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
if not "%errorlevel%"=="0" (
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
  cmake -G "MinGW Makefiles" -S "%ROOT%" -B "%BUILD%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_ROOT%" -DCMAKE_C_COMPILER="%TOOLCHAIN_ROOT%\bin\gcc.exe" -DCMAKE_CXX_COMPILER="%TOOLCHAIN_ROOT%\bin\g++.exe" -DCMAKE_MAKE_PROGRAM="%TOOLCHAIN_ROOT%\bin\mingw32-make.exe" -DFECTTY_WITH_QT=ON %PORTAUDIO_ARGS% -DFECTTY_WITH_HAMLIB=ON -DHAMLIB_ROOT="%HAMLIB_ROOT%"
) else (
  cmake -G "MinGW Makefiles" -S "%ROOT%" -B "%BUILD%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_ROOT%" -DFECTTY_WITH_QT=ON %PORTAUDIO_ARGS% -DFECTTY_WITH_HAMLIB=ON -DHAMLIB_ROOT="%HAMLIB_ROOT%"
)
if not "%errorlevel%"=="0" exit /b 1

echo Building Qt GUI...
cmake --build "%BUILD%" --config Release --target fectty-gui fectty-tests fectty-audit-tests fectty-cat-tests fectty-gui-text-tests fectty-waterfall-tests fectty-noise-prefix-bench %PORTAUDIO_TARGET% --parallel %FECTTY_BUILD_JOBS%
if not "%errorlevel%"=="0" exit /b 1

echo Deploying Qt and compiler runtime beside the GUI...
"%WINDEPLOYQT%" --release --no-translations --compiler-runtime "%BUILD%\fectty-gui.exe"
if not "%errorlevel%"=="0" exit /b 1

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
for %%D in (libhamlib-4.dll) do (
  if not exist "%HAMLIB_ROOT%\bin\%%D" exit /b 1
  copy /y "%HAMLIB_ROOT%\bin\%%D" "%BUILD%\%%D" >nul
  if errorlevel 1 exit /b 1
)
if not exist "%LIBUSB_ROOT%\bin\libusb-1.0.dll" exit /b 1
copy /y "%LIBUSB_ROOT%\bin\libusb-1.0.dll" "%BUILD%\libusb-1.0.dll" >nul
if errorlevel 1 exit /b 1
ctest --test-dir "%BUILD%" --build-config Release --output-on-failure
if not "%errorlevel%"=="0" exit /b 1

echo Qt GUI build and tests completed: "%BUILD%\fectty-gui.exe"
