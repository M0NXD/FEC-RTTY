# Qt source and replacement instructions

FEC-RTTY uses the Qt 6.8.3 shared libraries under LGPL-3.0. Copyright
The Qt Company Ltd. and other contributors. LGPL/GPL license texts accompany
the application in `licenses/qt/`. Third-party notices and source files for
Qt's bundled components are retained in the complete upstream source archives.

The unmodified official Qt Base and Qt SVG 6.8.3 source archives are included
in `licenses/qt/source/`, including upstream CMake/build scripts. They are also
published alongside the Windows binaries on the same GitHub release page.
No Qt source modifications were made for FEC-RTTY.

To build a replacement, extract the archives and follow Qt's included build
instructions. Use Windows x64, shared libraries, and a compatible MinGW/MSVCRT
toolchain. The recorded SDK is Qt 6.8.3 `mingw_64`; the application was built
with MSYS2 MinGW64 GCC 15.2. The SDK's SPDX build metadata is in `licenses/qt/sbom/`.
For Qt's Windows build configuration guidance, see
https://doc.qt.io/qt-6.8/windows-building.html.

Close all FEC-RTTY windows, back up `gui/`, and replace its Qt DLLs and associated
plugins together with your interface-compatible build. Keep
`gui/platforms/qwindows.dll` and its matching library dependencies. Launch
`gui/fectty-gui.exe` directly; no installer, privileged service, signing key,
online activation or integrity lock is required. The hash manifest is a
diagnostic inventory, not a runtime enforcement mechanism. Do not rerun the
installer's repair operation unless you want to restore its original DLLs.

You may modify/replace the LGPL libraries and reverse engineer the application
to debug those library modifications. No application restriction overrides
those LGPL rights. This notice is not a general open-source license for
FEC-RTTY's own code; a project license has not been selected.

Optional Mesa software-OpenGL and D3D compiler DLLs are not shipped by this
release; its Widgets interface uses raster painting. The application binaries,
Qt DLLs and runtime DLLs stay in their own directories, not Windows system folders.
