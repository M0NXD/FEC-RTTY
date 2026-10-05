# Testing FEC-RTTY

Build first using [Building](BUILDING.md). Commands run from the repository
root and use the MinGW output layout. No test-host SDK or device mapping is
assumed. Prebuilt Windows packages are also available from the latest release.
Audio-only bench tests use NullRig. Optional CAT tests use Dummy/loopback
backends, not a physical transmitter. See [Radio control](RADIO_CONTROL.md).

## Tests without audio hardware

The direct-GUI harness allows a 20-second receive tail after estimated playback
duration (override with `-ReceiveTailSeconds`, 8–60 seconds). This gives queued
decoder work time to complete before its bench timer closes RX. It does not fix
the application's documented Stop/backlog behavior or prove complete draining.

The core helper already runs these two CTest suites:

~~~powershell
ctest --test-dir source/build --build-config Release --output-on-failure
.\source\build\fectty-sim.exe "HELLO"
.\source\build\fectty-benchmark.exe
~~~

After a GUI build, run its four suites with:

~~~powershell
ctest --test-dir source/build-gui --build-config Release --output-on-failure
~~~

These exercise modem, parsing, text and waterfall code. They are not real-
radio tests or calibrated sensitivity measurements. Hardware diagnostics
are explicitly invoked rather than automatically transmitting during CTest.

## Prepare a virtual-cable audio test

Install [VB-CABLE](https://vb-audio.com/Cable/) separately using its vendor
instructions, including administrator approval/restart when required.
The source repository does not include or install the driver. Close other
applications transmitting into the cable.

For WinMM, enumerate from the core build:

~~~powershell
.\source\build\fectty-live.exe --list
~~~

Find input `CABLE Output` and output `CABLE Input`; record their current IDs.
Applications play into the cable's **Input** and record from its **Output**.
If PortAudio was built, enumerate separately:

~~~powershell
.\source\build-gui\fectty-portaudio-smoke.exe --list
~~~

PortAudio IDs have their own backend/host-API mapping; do not copy WinMM IDs.
Never select speakers, a microphone or a radio-connected output just to make
a virtual-cable script run. Verify endpoints before transmitting.

## Manual two-GUI check

Launch `source/build-gui/fectty-gui.exe` twice. In both windows select the
verified cable endpoints and same backend, keep `NullRig / bench mode`
and linked 1500 Hz centers, then start both sessions.

Send a short distinct message A to B. Check exact text, including blank lines
and a non-ASCII character. Wait for TX to finish, then send a different message
B to A. Check frame/CRC/gap/drop counters, not just waterfall activity.
See [GUI operation](GUI_PACKAGE.md) for tuning and diagnostics.

A single cable is a shared bus; both windows may hear their own sends.
Sequential reverse checks are not independent simultaneous paths.
Separate directions require suitable additional routing or hardware.

## Repeatable exact-text GUI test

Replace the example IDs below with verified current WinMM numbers:

~~~powershell
$guiExe = (Resolve-Path '.\source\build-gui\fectty-gui.exe').Path
$rxDevice = 0 # Replace with the verified CABLE Output input index.
$txDevice = 1 # Replace with the verified CABLE Input output index.
& .\tools\test-gui-direct.ps1 -GuiExe $guiExe -Backend winmm -InputDevice $rxDevice -OutputDevice $txDevice -SplitOffsets -EvidenceDir '.\evidence\manual-direct'
~~~

Two visible GUIs exchange exact Unicode/multiline payloads in both sequential
directions. JSON and RX screenshots go to the selected directory. Success
requires exact text/frame counts, successful playback, zero CRC/gaps/software
drops/reported interruptions and measured waterfall activity. A nonzero
exit or harness exception is a failed test.

Other explicit harnesses are `tools/test-gui-noise-prefix.ps1` (preceding/
continuous noise), `tools/test-gui-exit-status.ps1` (completed/cancelled sends)
and `tools/test-vb-audio.ps1` (standalone short/long payloads). Inspect parameters
and verify devices before use. Scripts write logs/reports and may overwrite
fixed-name WAV captures; preserve recordings you need.

## Report a result

Include application version/source revision, OS, backend/host API, endpoint
names and IDs, RX/TX offsets, gain/volume, expected/actual UTF-8 text,
frame/CRC/gap/acquisition counts, capture/processing/drop/interruption counters,
both exits, and relevant JSON/logs. Include reproducible steps and preserve
failed runs as well as successes.

Review captures/screenshots for private or station information and follow
[Publishing](PUBLISHING.md) before attaching them to a GitHub issue.
Historical hashes identify original artifacts, not redacted public copies.

Recorded [modem results](../evidence/BUG_FIXES_20261004_R11.md) and
[installer results](../evidence/INSTALLER_20261005_R12.md) came from one Windows
test host. They do not establish independent clock/RF behavior, clean-machine
deployment or safe radio drive. RX Stop can discard queued samples; zero
queue-overflow counts do not prove complete processing at shutdown.
