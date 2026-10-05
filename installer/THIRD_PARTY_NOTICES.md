# FEC-RTTY: third-party notices

The Windows installer and portable release use the dependencies below.
License texts and notices accompany the payload in `licenses/`.
No general license has been selected for FEC-RTTY's own source.

## Qt 6.8.3

Copyright The Qt Company Ltd. and other contributors. Qt Core, GUI, Widgets,
Network, SVG and the shipped Qt Base/SVG plugins are used as dynamically linked
libraries under **LGPL-3.0**. LGPL-3.0 and GPL-3.0 texts are included.
Qt's incorporated third-party components retain their respective notices/licenses
in the complete upstream sources; SDK SPDX metadata is included for reference.

The unmodified, complete source archives are included inside every Windows
package at `licenses/qt/source/` and are separately attached to the same
[release page](https://github.com/M0NXD/FEC-RTTY/releases/tag/v0.46.0):

| Archive | SHA256 |
|---|---|
| `qtbase-everywhere-src-6.8.3.tar.xz` | `56001B905601BB9023D399F3BA780D7FA940F3E4861E496A7C490331F49E0B80` |
| `qtsvg-everywhere-src-6.8.3.tar.xz` | `35EB516460F00F264EB504BAA253432384351CF23FB9980A5857190E8DEEF438` |

[Official source archive](https://download.qt.io/archive/qt/6.8/6.8.3/submodules/) ·
[Qt third-party attribution](https://doc.qt.io/qt-6.8/licenses-used-in-qt.html) ·
[LGPL terms](https://doc.qt.io/qt-6.8/lgpl.html).

You may modify and replace the LGPL libraries, and reverse engineer FEC-RTTY
to debug those modifications. No application restriction overrides these
rights. DLLs/plugins are not encrypted or locked; use an interface-compatible
replacement. [Replacement/build instructions](https://github.com/M0NXD/FEC-RTTY/blob/main/installer/resources/REPLACING_QT.md) are
also supplied at `licenses/qt/REPLACING_QT.md` inside the packages.
The application contains no library-source changes or LGPL-only code-copying
license claim for its own source.

The optional Mesa software-OpenGL and Microsoft D3D compiler DLLs deployed by
some SDKs are not distributed in these packages. This Widgets interface uses
raster painting. SDK inventories can describe components not shipped at runtime.

## Hamlib 4.7.2 and libusb 1.0.30

The GUI dynamically links unmodified Hamlib (LGPL-2.1) and libusb
(LGPL-2.1-or-later). Complete source archives, license/copyright notices and
libusb MSYS2 build recipe accompany the packages. No Hamlib GPL helper EXEs
are distributed. Modification/replacement/debugging rights are preserved;
see [CAT library replacement](https://github.com/M0NXD/FEC-RTTY/blob/main/installer/resources/REPLACING_CAT.md).
The offline copy is installed at `licenses/REPLACING_CAT.html`.

[Hamlib upstream](https://github.com/Hamlib/Hamlib/releases/tag/4.7.2) ·
[libusb upstream](https://github.com/libusb/libusb/releases/tag/v1.0.30).
OmniRig is an optional separately installed COM server, not bundled.

## PortAudio

PortAudio is used by the optional audio backend. Its upstream MIT-style
copyright/license notice is included at `licenses/portaudio/LICENSE.txt`.
[Project](https://www.portaudio.com/) · [Source](https://github.com/PortAudio/portaudio).
It is an application library, not a separately installed sound driver.

## GCC and MinGW-w64 runtime libraries

Separate GUI/MSVCRT-compatible and CLI/UCRT folders are intentional.
GCC runtime libraries are supplied under their GNU license texts and GCC Runtime
Library Exception; notices are in `licenses/gcc/`. The MinGW pthread notice
is in `licenses/winpthreads/`.
[GCC source](https://gcc.gnu.org/git.html) · [MinGW-w64 source](https://www.mingw-w64.org/).
Do not put these DLLs in Windows system directories or exchange GUI/bin runtimes.

## Inno Setup 6.7.3

Setup/uninstall is produced by Inno Setup. Its license accompanies the installer
at `licenses/inno/License.txt`.
[Official project/source](https://jrsoftware.org/).
The development compiler/IDE is not installed on the recipient's machine.

## VB-CABLE

VB-CABLE is **not bundled**. For optional same-machine audio tests, obtain it
separately from [VB-Audio](https://vb-audio.com/Cable/) and follow the vendor's
[licensing](https://vb-audio.com/Services/licensing.htm) and installation instructions.
FEC-RTTY does not install, upgrade or uninstall shared audio drivers.
