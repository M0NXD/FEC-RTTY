# Replacing the CAT libraries

The GUI dynamically links unmodified Hamlib 4.7.2 and libusb 1.0.30.
Hamlib is LGPL-2.1; libusb is LGPL-2.1-or-later. Their notices, full
corresponding source archives, and the libusb MSYS2 build recipe are included
under `licenses/hamlib/` and `licenses/libusb/`. No Hamlib GPL command-line
executables are included in the application package.

You may replace these libraries with compatible x64 builds, modify their
sources, and reverse-engineer the application as necessary to debug those
modifications. This permission is not a general license for the application.
Keep copies of the original DLLs first. Close every application instance,
then replace `gui/libhamlib-4.dll` or `gui/libusb-1.0.dll`. Preserve the
Hamlib 4 C ABI and the libusb 1.0 C ABI. Use a matching MSVCRT MinGW toolchain;
do not overwrite the application's compiler-runtime DLLs with incompatible
copies. A setup repair restores the packaged libraries.

Hamlib's source archive contains its configure/build scripts and Windows
cross-build support. libusb's archive includes configure and Windows backends;
the accompanying PKGBUILD records the MSYS2 package build used here. See the
projects' README/INSTALL files and the application's build documentation.
Build the GUI against your Hamlib SDK with `HAMLIB_ROOT`, and select your
libusb DLL directory with `LIBUSB_ROOT`. Library sources are not a complete
Qt/compiler development SDK.

Upstream sources:

- [Hamlib 4.7.2](https://github.com/Hamlib/Hamlib/releases/tag/4.7.2)
- [libusb 1.0.30](https://github.com/libusb/libusb/releases/tag/v1.0.30)
- [MSYS2 libusb recipe](https://github.com/msys2/MINGW-packages/tree/master/mingw-w64-libusb)
