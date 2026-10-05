@echo off
setlocal EnableExtensions

rem FEC-RTTY Windows build helper.
rem Usage: build.bat [clean]

for %%I in ("%~dp0.") do set "ROOT=%%~fI"
set "BUILD=%ROOT%\build"
set "CONFIG=Release"
if not defined FECTTY_BUILD_JOBS set "FECTTY_BUILD_JOBS=1"
set "GENERATOR_ARGS="
set "FECTTY_CXX_BIN="

rem Prefer the available MinGW toolchain on Windows. If it is not present,
rem leave generator selection to CMake (for example, Visual Studio).
where g++ >nul 2>nul
if not errorlevel 1 (
    where mingw32-make >nul 2>nul
    if not errorlevel 1 set "GENERATOR_ARGS=-G "MinGW Makefiles""
)

if /I "%~1"=="clean" (
    if exist "%BUILD%" rmdir /s /q "%BUILD%"
    if exist "%BUILD%" (
        echo Failed to remove "%BUILD%".
        exit /b 1
    )
)

where cmake >nul 2>nul
if not "%errorlevel%"=="0" (
    echo CMake was not found on PATH.
    exit /b 1
)

echo Configuring FEC-RTTY in "%BUILD%"...
rem An existing cache owns its generator; do not switch it just because PATH changed.
if exist "%BUILD%\CMakeCache.txt" set "GENERATOR_ARGS="
cmake %GENERATOR_ARGS% -S "%ROOT%" -B "%BUILD%" -DCMAKE_BUILD_TYPE=%CONFIG% -DFECTTY_WITH_PORTAUDIO=OFF -DFECTTY_WITH_HAMLIB=OFF -DFECTTY_WITH_QT=OFF
if not "%errorlevel%"=="0" (
    echo CMake configuration failed.
    exit /b 1
)

echo Building %CONFIG%...
cmake --build "%BUILD%" --config %CONFIG% --parallel %FECTTY_BUILD_JOBS%
if not "%errorlevel%"=="0" (
    echo Build failed.
    exit /b 1
)

rem Make MinGW-built executables runnable from the build directory when the
rem compiler runtime is installed separately from the application.
for /f "tokens=2 delims==" %%I in ('findstr /b "CMAKE_CXX_COMPILER:FILEPATH= CMAKE_CXX_COMPILER:STRING= CMAKE_CXX_COMPILER:UNINITIALIZED=" "%BUILD%\CMakeCache.txt"') do set "FECTTY_CXX_BIN=%%~dpI"
if defined FECTTY_CXX_BIN set "PATH=%FECTTY_CXX_BIN%;%PATH%"
if defined FECTTY_CXX_BIN for %%D in (libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll) do (
    if exist "%FECTTY_CXX_BIN%%%D" (
        copy /Y "%FECTTY_CXX_BIN%%%D" "%BUILD%\%%D" >nul
        if errorlevel 1 exit /b 1
    )
)

echo Running regression tests...
ctest --test-dir "%BUILD%" --build-config %CONFIG% --output-on-failure
if not "%errorlevel%"=="0" (
    echo Tests failed.
    exit /b 1
)

echo.
echo FEC-RTTY build and tests completed successfully.
echo Binaries: "%BUILD%"
exit /b 0
