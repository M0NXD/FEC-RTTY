# FEC-RTTY v0.46.1 GUI operation

For the 24-hour experiment's purpose, complete architecture/source map,
settings, testing, troubleshooting, and remaining work, see the
[complete project guide](PROJECT_GUIDE.md).

## Build or obtain the GUI

Download the installer or portable ZIP from the
[latest release](https://github.com/M0NXD/FEC-RTTY/releases/latest), or follow
[BUILDING.md](BUILDING.md), then launch
`source/build-gui/fectty-gui.exe`. Keep the deployed DLLs and plugins intact.
In an extracted portable package the launch path is `gui/fectty-gui.exe`.
[INSTALLER.md](INSTALLER.md) describes installation and standard Windows uninstall.

For a virtual-cable bench test, install VB-CABLE separately. In Audio, choose:

- RX input: `CABLE Output`
- TX output: `CABLE Input`
- Backend: `Windows Audio (WinMM)` or `PortAudio`

Leave Radio on `NullRig / audio only`, then press `Start session`. Send a
message from another bench instance or the built `source/build/fectty-live.exe`.
The GUI keeps
PTT disarmed in NullRig and shows decoded text plus frame, CRC, gap, and drop
counters. The Modem page exposes the original fixed FEC-RTTY waveform: 50 baud,
48 kHz continuous-phase 4-FSK, rate-1/2 convolutional FEC, and 16-column
interleaving. There is no alternate modem profile selector and no link setup.
Messages are sent as direct plaintext FEC frames: there are no station IDs,
peer selection, HELLO exchange, ACK wait, retry loop, or encryption. Newlines
and blank lines are preserved in the conversation view.

UTF-8 characters are preserved even when their bytes cross a frame or display
update boundary. Control, addressed, and unsupported-version frames are not
shown as conversation text. Binary control characters are hidden while tabs
and line breaks remain visible.

## Waterfall and precise tuning

The live waterfall sits above the conversation and uses the selected RX audio,
not simulated signals. New rows appear at the top. Its 0–3500 Hz audio scale
shows signal strength and the four-tone receive span, with a teal RX center and
an orange dashed TX center.

- Left click/drag tunes RX; right or Ctrl click/drag tunes TX.
- The RX and TX numeric fields set the four-tone center to 0.1 Hz precision,
  between 300 and 3000 Hz. Click the **middle** of a signal's four-tone region,
  not one individual tone. Arrow keys on the waterfall move RX by 1 Hz;
  Shift uses 10 Hz, and Ctrl selects TX.
- `Link RX + TX` moves both together. Uncheck it for separate offsets. A
  receiver's RX center should match its sender's TX center.
- RX can retune while listening; it resets acquisition/decoder statistics and
  waits for the next complete burst. Conversation text is retained. Retuning
  during a frame can lose that frame. TX and linkage are locked during a send;
  linked RX is locked too so displayed markers cannot disagree with playback.
- Sensitivity changes display brightness only, not audio gain or TX volume.

These are audio offsets within the radio's passband, not radio RF/dial tuning.
RF tuning is separate on the Radio tab. Diagnostics show the acquired residual frequency
**offset**. The display uses a 4096-point Hann FFT (11.71875 Hz bin spacing,
42.67 ms row interval, about 10.24 seconds of history). Numeric tuning
precision is not a claim of 0.1 Hz spectral resolution or physical clock
accuracy. The waterfall is DPI-aware, bounded in memory, and continues
displaying input even when no text decodes. TX-only sessions have no RX signal
display and are labelled accordingly.

Output volume and modem gain take effect during a send as new short audio blocks
are queued. Stop interrupts playback, and failed or cancelled sends retain the
draft. Audio is generated a frame at a time with one acquisition field for the
complete message. PTT lead/tail fields are used when CAT is explicitly armed.

The Radio page supports direct Hamlib serial/USB, rigctld TCP and OmniRig
Rig 1/2. Connect reads live state only; Read copies the observed dial/mode;
Apply deliberately writes RF dial/mode. PTT is never armed automatically.
See [Radio control](RADIO_CONTROL.md) for configuration, lead/tail, TX limits,
watchdog, cancellation, force release and manual-unkey warnings.

For two direct-mode GUI instances over the installed shared VB-Audio cable,
select `CABLE Output` for RX and `CABLE Input` for TX in each window. Start
both sessions before sending; no receiver-side link or acknowledgement is required.
`No RX (TX-only bench)` is also valid when the instance is only transmitting.

A single VB-CABLE supports shared-bus two-window testing. Both receivers may
hear both transmitters, including their own sends. Transmit one at a time.
Independent physical directions require additional routing or hardware.

If PortAudio was enabled in your build, select its own VB-Audio device entries.
The smoke tool provides a repeatable check from `source/build-gui` (or the
separate runtime package's `gui` directory):

```text
fectty-portaudio-smoke.exe --list
fectty-portaudio-smoke.exe portaudio:1 portaudio:5 3
```

The numeric IDs above are R11 examples only. Enumerate and verify named cable
endpoints before using them. The second command transmits real modem audio;
it is not just an enumeration or silent connection test.

PortAudio builds require their matching deployed `libportaudio.dll`. Do not
exchange compiler runtime DLLs with a separately built core-tool directory.

The GUI supports explicit bench launches from its deployed executable directory.
The following numbers are examples only; verify the current cable IDs first:

```text
fectty-gui.exe --autostart --rx winmm:0 --tx winmm:1
fectty-gui.exe --autostart --rx none --tx winmm:1 --send "HELLO" --quit-after-send
fectty-gui.exe --autostart --rx winmm:0 --tx winmm:1 --run-seconds 15 --report rx.json
```

`--center-hz N` and `--volume N` override the bench run's center and output
volume (0–100 percent). Bench launches do not overwrite saved operator
settings. Invalid or unavailable device IDs are rejected instead of falling
back to another device. `--quit-after-send` requires a nonempty `--send`.

Run `tools\test-gui-direct.ps1` from the project to test two visible GUI
windows with exact Unicode/multiline JSON reports. The [test guide](TESTING.md)
uses source-build paths. Historical R11 PortAudio enumeration used
`-Backend portaudio -InputDevice 1 -OutputDevice 5` on the test host.
Always check `fectty-portaudio-smoke.exe --list` first: endpoint indices can
change when Windows devices change. The GUI remembers unique endpoint names
and host APIs, not just an index. Missing/ambiguous saved endpoints require
reselection. Legacy numeric-only preferences are not blindly restored.

The v0.45.1 receiver fixes bursts missed after background noise without
changing the modem waveform. Automatic `--send` runs return 2 if the send
fails, is cancelled by the timer/window closure, or never starts; completed
sends and ordinary receive-only/idle closures return 0. Report/startup errors
also remain nonzero. Draft text is preserved after a cancelled send.
`tools/test-gui-exit-status.ps1` checks these outcomes using an explicitly
selected cable output. `tools/test-gui-noise-prefix.ps1` pairs a visible GUI
with `fectty-noise-prefix-bench.exe` to check a real burst after one second
of noise, both with a clean burst and continued noise. The emitter refuses
an output number whose current name is not the VB-Audio CABLE Input.

`--rx-center-hz N` and `--tx-center-hz N` override independent offsets and
unlink them. `--retune-rx-hz N --retune-after S` exercises a live RX retune
after S seconds (default 3). `--snapshot PATH` saves a Qt-rendered GUI image
at the end of a bench run. `-SplitOffsets -RetuneRx` on the GUI test script
checks different sender RX/TX offsets and retunes the active receiver before
the incoming burst. Reports include generated waterfall rows and the last
non-silent spectral peak, as well as exact text and decoder counters.

## Source rebuild

The complete prerequisites and setup commands are in [BUILDING.md](BUILDING.md).
This section is a helper/deployment reference, not a substitute for SDK setup.

`source\build.bat` builds the dependency-free bench tools and runs CTest.
`source\build-gui.bat` builds the Qt GUI, automatically enables PortAudio when
the local MSYS2 package is present, and deploys the Qt, MinGW, and PortAudio
runtime DLLs beside the executable. Hamlib/libusb are enabled and deployed by
the release GUI helper; set `HAMLIB_ROOT` and `LIBUSB_ROOT` to your matching SDK/DLL roots. The GUI helper prefers the MSYS2 `mingw64`
toolchain because the tested official Qt MinGW package uses
the MSVCRT runtime. This avoids the startup heap corruption caused by mixing
the UCRT MinGW toolchain with Qt.

For an unpacked source package, set `QT_ROOT` to your Qt SDK directory and,
if needed, `TOOLCHAIN_ROOT` to the compatible MinGW installation. The GUI build
script honors these overrides and runs core, CAT, GUI text, and waterfall tests after
deployment. The core helper preserves an existing CMake generator and deploys
runtime DLLs from the compiler recorded in its cache.

The deployable GUI runtime is in `source\build-gui\` after a source build and
in `gui\` in a release package. The dependency-free bench runtime is in
`bin\`; keep their same-named MinGW runtime DLLs in their respective
directories.
