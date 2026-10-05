# FEC-RTTY - M0NXD 0.46.1

Patch release for the experimental Windows x64 CAT/audio build. The original
24-hour experiment, fixed FEC waveform, UTF-8/newlines and unencrypted messages
remain unchanged.

## Downloads

- FEC-RTTY-0.46.1-Setup-x64.exe: offline per-user installer; standard Windows uninstall.
- FEC-RTTY-0.46.1-Windows-x64.zip: extract completely, then run gui/fectty-gui.exe.
- FEC-RTTY-0.46.1-source.zip: canonical source and public documentation.

## Changes in 0.46.1

- Accept standard plain Hamlib/rigctld getter replies (`f`, `m`, `t`) as well as
  extended `+f`, `+m`, `+t` replies.
- Verified read-only CAT connection to CAT4OM's Hamlib-compatible TCP endpoint
  on `127.0.0.1:4532`; the implementation remains process-agnostic.
- Added regression coverage for fragmented plain replies while retaining the
  existing extended-response tests.

## 0.46.0

Experimental Windows x64 release adding CAT radio control. The original
24-hour experiment, fixed FEC waveform, UTF-8/newlines and unencrypted messages
remain unchanged.

## Downloads

- FEC-RTTY-0.46.0-Setup-x64.exe: offline per-user installer; standard Windows uninstall.
- FEC-RTTY-0.46.0-Windows-x64.zip: extract completely, then run gui/fectty-gui.exe.
- FEC-RTTY-0.46.0-source.zip: canonical source and public documentation.

Obtain packages from the [latest release](https://github.com/M0NXD/FEC-RTTY/releases/latest)
and compare SHA256SUMS. Windows packages include Qt, PortAudio, Hamlib, libusb,
matching compiler runtimes, offline guides, notices and full corresponding
Qt/Hamlib/libusb sources. No developer SDK is needed to run them. OmniRig and
radio USB drivers are external optional prerequisites; VB-CABLE is separate.

## Changes

- Direct Hamlib serial/USB, rigctld TCP and OmniRig Rig 1/2 controls.
- Explicit RF dial/mode apply with live readback; waterfall offsets stay audio-only.
- Arming resets on launch; confirmed key/release, lead/tail, TX limit,
  cancellation, watchdog and force-release control.
- Correct OmniRig RX/TX/data enum values and COM thread ownership.
- Correct Hamlib microphone/data PTT commands and bounded TCP reply handling.
- Disable Hamlib frontend caching for readback; reject models lacking required
  getters rather than accepting a remembered transmit state as radio feedback.
- Background CAT worker, outside real-time audio callbacks.
- CAT regressions and visible GUI fault/audio tests.
- Low-memory build concurrency and process-crash exit-code handling hardened.

Read [Radio control](RADIO_CONTROL.md) before arming. Software acceptance is
not physical-radio/RF acceptance. The installer is unsigned. Clean-machine,
independent clocks/radios, every model and exhaustive UI/accessibility testing
remain unverified. RX shutdown/backlog and intermittent WinMM capture
limitations are not claimed fixed. No general project source license is selected.

See [CAT results](../evidence/CAT_RELEASE_20261005.md) for exact scope and
[Installation](INSTALLER.md) for repair/uninstall.
