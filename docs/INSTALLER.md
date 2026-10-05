# FEC-RTTY Windows installation

FEC-RTTY began as a 24-hour digital-mode experiment. The current Windows
release packages application **v0.46.1**, installer revision **1**, including
optional CAT support and its Hamlib/libusb runtime dependencies. The modem
waveform and unencrypted messages are unchanged. Arming is off by default.

## Download, install and run

Get the installer or portable ZIP from the
[latest release](https://github.com/M0NXD/FEC-RTTY/releases/latest).
Compare its SHA256 with the supplied checksum file. Checksums detect changes,
not publisher identity; the experimental installer is unsigned.

Requires Windows 10 1809+ or Windows 11 **x64**, with a 48 kHz mono-capable
audio device. ARM64/32-bit Windows are not claimed. The OS baseline follows
[Qt 6.8's supported platforms](https://doc.qt.io/qt-6.8/supported-platforms.html).

Run `FEC-RTTY-0.46.1-Setup-x64.exe`. It installs for the current user, normally
at `%LOCALAPPDATA%\Programs\FEC-RTTY`, without elevation. Launch
**FEC-RTTY - M0NXD** from the Start menu; the same executable can run twice.
Offline documentation starts at the installed `index.html`.

For the portable ZIP, extract every file into an empty folder and run
`gui/fectty-gui.exe`. Do not run the EXE from inside the ZIP. Portable use
does not register an uninstaller; delete the extracted application files when
finished, keeping any user files you want to retain.

The packages include Qt 6.8.3, PortAudio, Hamlib 4.7.2, libusb 1.0.30 and matching MinGW DLLs.
OmniRig and manufacturer-specific radio USB drivers are separate optional inputs.
No compiler, Qt SDK, CMake, Python, .NET or separate Visual C++ redistributable
is needed to run this build. Installation makes no downloads, does not change
PATH or system DLLs, and does not replace Windows/audio drivers.
CLI/UCRT DLLs stay in `bin/`; GUI/MSVCRT-compatible DLLs stay in `gui/`.
Never exchange the same-named DLLs between them.

Source, offline documentation, test helpers, dependency notices, complete
unmodified Qt Base/SVG, Hamlib and libusb sources, library replacement
instructions and a SHA256 payload inventory are included.
Those sources are not a ready-to-run development SDK.

## Standard Windows uninstall and repair

Use **Settings → Apps → Installed apps** (Windows 11), **Apps & features**
(Windows 10), or **Control Panel → Programs and Features**, then choose
**FEC-RTTY - M0NXD → Uninstall**. A Start-menu uninstall shortcut is also supplied.
Close installed copies first.

Uninstall removes application-owned files/shortcuts and registration, not
user-created files, saved messages, shared drivers or operator settings at
`%LOCALAPPDATA%\FEC-RTTY\FEC-RTTY\fectty.ini`.

Rerun setup to repair the same release. Stable identity
`AppId=FEC-RTTY-M0NXD` permits repair of its registered installation; setup
refuses an unrelated nonempty folder or source checkout. Repair restores
bundled DLLs, including any Qt DLLs you replaced yourself. No future-version
migration or downgrade guarantee is implied.

## Optional virtual cable

For same-machine bench tests, obtain [VB-CABLE](https://vb-audio.com/Cable/)
directly from its vendor, read its instructions/terms and follow its separate
administrator/restart workflow. FEC-RTTY does not bundle, install, upgrade or
uninstall that driver. Ordinary sound-card operation does not need it.
See [Testing](TESTING.md) for endpoint verification and sequential audio tests.

## Rebuilding packages

Building the application itself is documented in [BUILDING.md](BUILDING.md).
The release builder needs PowerShell 7, a compatible Inno Setup compiler,
the recorded v0.46.1 runtime/source archive and installer/accepted-release.json,
Hamlib/libusb sources/license inputs, Qt SDK license/SPDX inputs,
compiler/PortAudio notices and official Qt Base/SVG 6.8.3 source archives.
These are separate inputs, not included in a fresh source checkout.

~~~powershell
pwsh -NoProfile -File .\installer\build-installer.ps1
~~~

Overrides: `-ReleaseDir`, `-AcceptedArchive`, `-IsccExe`, `-QtRoot`,
`-RuntimeLicenseRoot`, `-QtLicenseDir`, `-QtSourceDir`, `-OutputDir`.
There are no downloads in this builder. Supply an empty output folder.
Default inputs live in ignored `third_party/` and local `releases/`;
outputs are written to `releases/v0.46.1-public/`.

Every input runtime/plugin is compared to its accepted archive; GUI/live hashes
and canonical source/tests are checked. Qt/Hamlib/libusb source archives are SHA256-pinned.
Only the optional `opengl32sw.dll` and `D3Dcompiler_47.dll` are omitted from the
verified runtime inventory; the Widgets interface uses raster painting.
The builder renders and validates offline navigation, copies notices/source,
records file hashes and source-tree provenance, then creates setup and a
portable ZIP from the same payload. A new code version needs an updated
accepted runtime/hash contract, not bypassed checks.

[Third-party notices](../installer/THIRD_PARTY_NOTICES.md) explain dependency
licenses and replacement rights. The application source license remains
unselected; binary availability does not grant a general source license.

## Acceptance and limits

~~~powershell
pwsh -NoProfile -File .\installer\test-installer.ps1
pwsh -NoProfile -File .\installer\test-installer.ps1 -TestAudio
pwsh -NoProfile -File .\installer\test-installer.ps1 -TestAudio -IsolatedShortcuts
~~~

The test refuses an existing installed FEC-RTTY or pre-existing shortcuts.
For existing shortcuts, -IsolatedShortcuts uses a unique test Start-menu group,
leaves the desktop shortcut untouched and verifies existing shortcut hashes.
This alternative does not test creating a desktop shortcut. It still refuses
an existing registered installation, which must not be replaced for a bench test.
It installs into an isolated spaces/Unicode path, rejects an unrelated
occupied folder, checks hashes/registry/shortcuts, runs regressions with a
Windows-only PATH, opens two visible GUI windows, tests repair and invokes
the exact registered Windows uninstaller. Settings, user files and cable
drivers must survive. Raw/private outputs remain in ignored staging;
`acceptance.public.json` is its redacted companion.

Audio mode resolves current WinMM and PortAudio MME cable IDs by name before
transmitting. Device numbers are not universal or stable.
It sends exact Unicode/multiline payloads sequentially in both directions
on each backend, with real Hamlib Dummy PTT key/release and no physical CAT.
See [CAT results](../evidence/CAT_RELEASE_20261005.md) and [Radio control](RADIO_CONTROL.md).

Host acceptance is not a clean Windows VM test or an automated Settings-UI click.
Other computers/accounts, unplug/replug, long independent clocks, migration,
all scales/devices, code signing/SmartScreen reputation and actual RF remain
unverified. See the [project limitations](PROJECT_GUIDE.md#12-history-and-remaining-work).
