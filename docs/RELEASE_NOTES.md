# FEC-RTTY - M0NXD 0.45.1

Experimental Windows x64 release, packaging revision 2. This project began
as an experiment to build an amateur-radio digital mode in 24 hours.

## Downloads

- **Installer:** `FEC-RTTY-0.45.1-Setup-x64.exe`. Installs the application,
  private runtime dependencies, offline documentation and Start-menu shortcuts;
  uninstall through normal Windows Settings/Control Panel.
- **Portable:** `FEC-RTTY-0.45.1-Windows-x64.zip`. Extract completely and run
  `gui/fectty-gui.exe`. Keep every runtime/plugin folder intact.
- **Source:** `FEC-RTTY-0.45.1-source.zip`, or clone the repository.
- **Checksums:** `SHA256SUMS.txt`. Matching Qt Base/SVG 6.8.3 source archives
  are included inside the Windows packages and also attached separately.

Requires Windows 10 1809+ / Windows 11 x64 and a suitable 48 kHz audio device.
No Qt SDK, compiler or separate runtime installation is required to run it.
The installer is unsigned; Windows may display an unknown-publisher warning.
VB-CABLE is optional and not bundled: obtain it from its
[official vendor](https://vb-audio.com/Cable/) for same-machine cable testing.

Only the latest source and this current release are published. Older packages
and original Git history have been retained locally. Existing clones from
before the authorized history cleanup should be re-cloned.

## Included and tested

One fixed 50-baud/48 kHz four-tone FSK/FEC modem, direct UTF-8 text/newlines,
output-volume control, device selection and a live waterfall with linked/split
RX/TX offsets. This is not conventional Baudot RTTY. Messages are unencrypted;
radio CAT/PTT remains disabled/NullRig.

The application binaries are unchanged from v0.45.1. This packaging revision
adds current public documentation, dependency notices, corresponding Qt sources
and replacement instructions. Optional software-OpenGL/D3D DLLs are omitted.

Host acceptance passed **37 installer assertions**, including install, repair,
registered Windows uninstall, clean-PATH launch, four packaged regression
executables, file hashes and user-settings/driver preservation. Actual installed
modem-audio tests decoded exact Unicode/multiline text A→B and B→A through
WinMM and PortAudio. The ZIP verifies the same 512-file payload.

## Important limitations

An intermittent **WinMM RX buffer-requeue error** was observed, including one
run after all message text decoded. Prefer PortAudio for this cable bench if
it occurs, and verify reception after reopening the session. The API/driver
cause is unresolved; passing text checks do not certify continuous capture.

Stopping RX can abandon queued samples. The automated test now allows a
20-second receive tail; this is a test-timing workaround, not a drain fix.
Zero drop/CRC counters do not establish delivery or complete audio processing.
There is no ACK/retry and no delivery guarantee.

Independent computers/clocks, real radios/RF, clean Windows VMs, migration and
exhaustive UI/accessibility remain unverified. No general application source
license has been selected. LGPL Qt library rights, sources, license texts and
compatible-DLL replacement/reverse-engineering instructions accompany the packages.

[Exact results and artifact hashes](https://github.com/M0NXD/FEC-RTTY/blob/main/evidence/PUBLIC_RELEASE_20261005.md) ·
[Operator guide](https://github.com/M0NXD/FEC-RTTY/blob/main/docs/GUI_PACKAGE.md) ·
[Protocol](https://github.com/M0NXD/FEC-RTTY/blob/main/docs/PROTOCOL.md).
