# Building FEC-RTTY from source

These instructions start from a fresh clone of
[M0NXD/FEC-RTTY](https://github.com/M0NXD/FEC-RTTY), not a developer's existing
SDK or runtime folders. Windows x64 is the recorded build/bench environment.
The application version is 0.46.0.

## What you need

- [CMake](https://cmake.org/download/) 3.20 or newer, available on `PATH`.
- A C++20 compiler and its build tool. Windows acceptance used MinGW GCC 15.2
  with `mingw32-make`; equivalent deployment/audio acceptance is not claimed
  for other generators/toolchains.
- Git for cloning, or the GitHub source archive.
- For the GUI: a matching Qt 6 Widgets SDK and `windeployqt`; the tested SDK
  was Qt 6.8.3 for MinGW x64. See [Qt for Windows](https://doc.qt.io/qt-6.8/windows.html).
- PortAudio development files/pkg-config are optional for the GUI. WinMM
  does not need PortAudio. Core builds keep Hamlib off; the release GUI helper
  requires the Hamlib 4.7.2 x64 SDK plus a matching libusb 1.0.30 DLL. Obtain
  [Hamlib](https://github.com/Hamlib/Hamlib/releases/tag/4.7.2) and
  [libusb](https://github.com/libusb/libusb/releases/tag/v1.0.30) separately.
  Set `HAMLIB_ROOT` to the SDK (include/lib/bin) and `LIBUSB_ROOT` to a matching
  MSVCRT x64 directory containing bin/libusb-1.0.dll.

The checkout includes no compiler, Qt SDK or runtime package. Build helpers
do not install dependencies. A virtual cable is not needed to compile or run
deterministic tests; it is needed only for cable-based audio examples.

## Core tools: simplest Windows build

From the repository root in PowerShell:

~~~powershell
& .\source\build.bat
~~~

The helper configures Release with Qt/PortAudio/Hamlib disabled, builds the
tools, copies available MinGW runtime DLLs and runs CTest. Check for a nonzero
exit before continuing. With MinGW, outputs include:

~~~text
source/build/fectty-sim.exe
source/build/fectty-benchmark.exe
source/build/fectty-live.exe
source/build/fectty-tests.exe
source/build/fectty-audit-tests.exe
~~~

The core build does not produce a GUI. Multi-configuration generators normally
put executables under `Release`; the simple audio scripts assume the MinGW
layout. Use a fresh build directory when changing compilers.
`source/build.bat clean` removes the generated `source/build` tree.

## GUI build

Use the SDK/compiler combination appropriate to your Qt package. Recorded
Windows acceptance paired Qt 6.8.3's official MinGW package with an
MSVCRT-compatible MSYS2 `mingw64` compiler; a UCRT/Qt mixture caused startup
corruption during development. A different Qt distribution may need a
different matching compiler. Do not mix CRT DLLs between builds.

MSYS2 now lists `MINGW64` as a deprecated environment; it is the recorded
test baseline, not a general recommendation for a new toolchain installation.
See [MSYS2 environments](https://www.msys2.org/docs/environments/).
Qt 6.8 documents MinGW-w64 13.1 as a supported compiler. Follow the SDK's
documented compiler pairing and verify a different pairing yourself rather
than assuming that every compiler named MinGW is interchangeable.

Install the SDK/toolchain using their own instructions. For MSYS2, see
[Getting started](https://www.msys2.org/). Replace the placeholders below
with directories on your own computer before running:

~~~powershell
$env:QT_ROOT = (Resolve-Path '<Qt SDK directory>').Path
$env:TOOLCHAIN_ROOT = (Resolve-Path '<compatible MinGW directory>').Path
$env:HAMLIB_ROOT = (Resolve-Path '<Hamlib x64 SDK directory>').Path
$env:LIBUSB_ROOT = (Resolve-Path '<matching libusb directory with bin subfolder>').Path
& .\source\build-gui.bat
.\source\build-gui\fectty-gui.exe
~~~

`QT_ROOT` must contain `lib/cmake/Qt6/Qt6Config.cmake` and `bin/windeployqt.exe`.
`TOOLCHAIN_ROOT` must contain `bin/g++.exe`, `gcc.exe` and `mingw32-make.exe`.
CMake must remain available on `PATH`. With no override, the helper looks for
Qt under `third_party/qt/6.8.3/mingw_64` and the conventional MSYS2 `mingw64`
install; neither directory is supplied by GitHub.

The helper enables PortAudio if that toolchain contains its development
pkg-config file, builds GUI/test tools, deploys Qt/compiler/optional PortAudio
DLLs and runs five CTest suites, including CAT regressions. See
[Qt deployment](https://doc.qt.io/qt-6.8/windows-deployment.html) for the role
of `windeployqt`. Keep the deployed `source/build-gui` directory's DLLs and
plugin folders with the executable. A missing Vulkan-header notice does not
by itself prove a build failed; check the actual build/test exit.

The helpers default to one compile job to avoid memory exhaustion. Override
`FECTTY_BUILD_JOBS` only when enough memory is available. Nonzero and crash
exit codes are failures. The GUI SDK lookup uses `HAMLIB_ROOT`; CMake direct
builds can use `-DFECTTY_WITH_HAMLIB=ON -DHAMLIB_ROOT=<SDK-root>`.

## Explicit CMake build

From the repository root, for a separate core build directory:

~~~powershell
cmake -S source -B source/build-manual -DCMAKE_BUILD_TYPE=Release -DFECTTY_WITH_QT=OFF -DFECTTY_WITH_PORTAUDIO=OFF -DFECTTY_WITH_HAMLIB=OFF
cmake --build source/build-manual --config Release --parallel 1
ctest --test-dir source/build-manual --build-config Release --output-on-failure
~~~

Optional switches are `FECTTY_WITH_QT`, `FECTTY_WITH_PORTAUDIO` and
`FECTTY_WITH_HAMLIB`. Enabling one requires its development dependencies.
Plain CMake builds do not perform the Windows helper's runtime deployment.
`cmake --install` is not a complete Windows GUI deployment.
Non-Windows core builds may be useful for development, but Windows GUI/audio/
installer acceptance does not certify other platforms.

## Installer and common failures

The installer builder is a pinned release-engineering workflow, not a fresh-
clone quick start. Its accepted ZIP/runtime inputs are not public downloads.
See [Installer](INSTALLER.md) before attempting to package a build.

- Compiler/generator cache conflict: use a new generated build directory.
- Qt not found: check `QT_ROOT` points to the SDK, not just deployed DLLs.
- Launch DLL/plugin errors: keep deployed files intact; verify the compiler/
  Qt pairing and `platforms/qwindows.dll`. Do not download random DLL replacements.
- PortAudio absent: check its development files and pkg-config availability,
  or use WinMM. Device names/indices differ between the two backends.

Next: [GUI operation](GUI_PACKAGE.md) and [Testing](TESTING.md).
