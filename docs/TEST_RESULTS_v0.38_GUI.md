# FEC-RTTY v0.38 GUI preview test results

> Dated development/test record. Statements below apply to the recorded version and test host, not every reader's setup.
> Device IDs are examples; generated artifacts named here are not public downloads.
> Use the [current testing guide](TESTING.md) and [documentation index](../PROJECT_INDEX.md) for current instructions.

Test date: 2026-10-04 on the Windows bench machine; the GUI acceptance was
re-run after the live receive-path fix.

## Build and regression gates

- `source\build.bat` — completed successfully.
- CTest — 1/1 test passed.
- Simulator — exact `CQ CQ CQ DE G0ABC K`, 3 valid frames, 0 CRC failures, 0 sequence gaps.
- Benchmark — 7 valid frames at each tested noise level 0.02, 0.05, 0.10, 0.15, and 0.20; all text checks passed.
- Qt GUI — built successfully with the project-local Qt 6.8.3 SDK and the MSYS2 MinGW toolchain.
- The permanent 2,048-sample incremental GUI-stream regression test passed as part of CTest.
- Windows rigctld loopback — local ephemeral TCP listener accepted a connection from `NetRigctlControl`; the new regression passed without requiring an external rigctld service.
- rigctld command mock — the regression server received `F 14070000`, `M PKTUSB 0`, `T 1`, and `T 0` from the real `NetRigctlControl` implementation.
- OmniRig COM — the installed `OmniRig.OmniRigX` provider connected and disconnected successfully; no radio-affecting property was written.
- PortAudio — built through the local MSYS2 pkg-config package and linked into the Qt GUI.

## Real VB-Audio GUI receive

Configuration:

- RX: WinMM `CABLE Output` / device 0
- TX: WinMM `CABLE Input` / device 1
- Format: 48,000 Hz, mono, 16-bit
- Radio control: NullRig; CAT/PTT disabled
- Sender: rebuilt `fectty-live.exe`

Result:

- Sent: `EARLY GUI RX TEST`
- GUI decoded: `EARLY GUI RX TEST`
- Valid frames: 2
- CRC failures: 0
- Sequence gaps: 0
- Acquisitions: 1
- Dropped samples: 0
- GUI status: Audio `Running`, link reached `LOCKED`

## Real VB-Audio GUI transmit / CLI receive

Configuration:

- GUI RX: `No RX (TX-only bench)`
- GUI TX: WinMM `CABLE Input` / device 1
- CLI RX: WinMM `CABLE Output` / device 0
- Format: 48,000 Hz, mono, 16-bit
- Radio control: NullRig; CAT/PTT disabled

Result:

- GUI sent: `GUI TX REVERSE 20261004`
- CLI decoded frames: `GUI TX R` + `EVERSE 2` + `0261004`
- Reconstructed text: `GUI TX REVERSE 20261004`
- Valid frames: 3
- CRC failures: 0
- Sequence gaps: 0
- Acquisitions: 1
- Reacquisitions: 0
- Dropped samples: 0
- GUI event: `TX complete; modem audio sent`

## Real VB-Audio GUI receive — 20-frame stress pass

The GUI was launched from the packaged directory with RX `winmm:0` and TX
`winmm:1`. The CLI transmitter sent 20 distinct 8-byte payloads:
`A0000000` through `T0000000`.

Result:

- GUI displayed all 20 payloads in order.
- Valid frames: 20
- CRC failures: 0
- Sequence gaps: 0
- Dropped samples: 0
- Captured RX samples: 6,008,832
- Processed RX samples: 6,008,832
- Audio remained `Running`; the link went to `LOST` only after the deliberate post-test quiet period.

The fix keeps the idle audio gate before a burst, then forwards every captured
sample chunk once a burst has begun. This preserves the sample timeline for
the stateful receiver.

## Real VB-Audio GUI transmit — 20-frame reverse pass

The GUI ran in TX-only mode with `No RX (TX-only bench)` and transmitted the
same 20 distinct payloads to the CLI receiver on `CABLE Output`.

Result:

- CLI reconstructed all 20 payloads in order.
- Valid frames: 20
- CRC failures: 0
- Sequence gaps: 0
- Acquisitions: 1
- Reacquisitions: 0
- Dropped samples: 0
- GUI event: `TX complete; modem audio sent`

## Rebuilt dependency-free live profile

- Short profile (`HELLO`): exact text, 1 valid frame, 0 CRC failures, 0 gaps, 1 acquisition, 0 drops.
- 20-frame profile (160 bytes): exact text, 20 valid frames, 0 CRC failures, 0 gaps, 1 acquisition, 0 drops.
- Existing v0.34 evidence also records exact 50-frame and 64-frame/512-byte VB-Audio passes.
- The packaged `releases\fectty-v0.38-gui-preview-20261004\gui\fectty-gui.exe` launched normally with its deployed Qt runtime.
- The packaged `bin\fectty-live.exe --list` found `CABLE Output` as input 0 and `CABLE Input` as output 1; packaged audio smoke captured 143,360 samples in 3 seconds with 0 drops.

## Two GUI instances on the installed VB-Audio cable

Two copies of the packaged GUI were run concurrently with WinMM RX
`CABLE Output` and TX `CABLE Input`, both in NullRig mode.

Forward pass:

- Sender and receiver both displayed `A0000000`, `B0000000`, `C0000000`.
- Each instance reported 3 valid frames, 0 CRC failures, 0 sequence gaps, and 0 dropped samples.
- The sending instance reported `TX complete; modem audio sent`.

Reverse pass:

- The sender and receiver both displayed `D0000000`, `E0000000`, `F0000000`.
- Each instance again reported 3 valid frames, 0 CRC failures, 0 sequence gaps, and 0 dropped samples.
- The sending instance again reported `TX complete; modem audio sent`.

This validates the complete GUI-to-GUI audio path on this single installed
virtual cable. Both applications decode the real waveform from the cable; no
text shortcut or CAT/PTT path is involved.

## PortAudio backend loopback

The packaged `gui\fectty-portaudio-smoke.exe` enumerated the PortAudio device
set, opened `portaudio:1` (`CABLE Output`) and `portaudio:5` (`CABLE Input`),
transmitted the modem waveform, and decoded it back through the captured
stream:

- Transmitted: `yes`
- Captured samples: `417792`
- Decoded text: `PORTAUDIO SMOKE`
- Dropped samples: `0`

The packaged Qt GUI was also launched with `--autostart --rx none --tx
portaudio:5 --send "PORTAUDIO GUI TX"`; a separate WinMM receiver decoded
  `PORTAUDI` and `O` as the exact reconstructed message, with 2 valid frames,
  0 CRC failures, 0 sequence gaps, 1 acquisition, and 0 dropped samples.

## 4K and high-DPI startup matrix

The packaged GUI was started as a visible normal Windows process on the
3840x2160 desktop at Qt scale factors 100%, 125%, 150%, and 200%. Every case
created a valid main window, stayed responsive, and retained the expected
title. A 150% resize smoke also exercised 1920x1080, 2560x1440, and 3840x2160
requests without a crash or loss of responsiveness. The detailed record is
in `evidence/gui-scale-matrix-20261004.md`.

## Visual and contrast audit

The Qt theme was tightened after an explicit foreground/background audit. The
primary action buttons now exceed 5.5:1 contrast in normal and hover states;
inputs, disabled controls, tabs, status pills, tooltips, dock titles, and the
status bar all have explicit dark-theme colors rather than platform fallbacks.
Status pills were given consistent widths and activity text now wraps cleanly.
Detailed ratios and the palette record are in `evidence/gui-visual-audit-20261004.md`.

## Output-volume safety control

The Audio page now includes a separate `Output volume` slider from 0% to
100%, with a live percentage readout and a tooltip explaining that it is the
final level sent to the selected output. The existing modem setting is labeled
`Modem TX gain` so the two controls are not confused. Output volume is applied
after modem gain, is clamped to the safe range, and is saved in the user
settings. The default is 100% to preserve the previous waveform level; users
can reduce it before connecting a radio to avoid overdriving the transmitter.

For the output-volume update, the source regression suite completed with
CTest 1/1 passed, the Qt GUI rebuilt successfully, and the packaged
`gui\fectty-portaudio-smoke.exe portaudio:1 portaudio:5 3` check reported
`transmitted=yes`, `captured=428032`, `dropped=0`, and exact decoded text
`PORTAUDIO SMOKE`. The refreshed packaged GUI launched with title
`FEC-RTTY - M0NXD` and remained responsive.

## Remaining limitations

- This is a GUI preview milestone, not an on-air release. The live GUI now implements WinMM and PortAudio audio plus NullRig bench operation. The Radio page wires NullRig, rigctld TCP, and OmniRig connection/test paths. rigctld command formatting and OmniRig COM connection/cleanup are covered locally, but no GUI session calls CAT/PTT and no radio-affecting command was issued during this bench run.
- The GUI runtime package is separate from the dependency-free bench package because the GUI uses Qt plus the MSYS2 MinGW runtime, while the bench binaries use the existing UCRT MinGW runtime. Do not mix the identically named runtime DLLs between `bin` and `gui`.
- The package includes the deployed Qt runtime but not the approximately 1.08 GB Qt SDK. Rebuilding the GUI from source requires the local/project Qt 6.8.3 SDK or an equivalent Qt 6 SDK; `build-gui.bat` prefers `C:\msys64\mingw64` to keep the Qt CRT compatible.
- A single VB-Audio cable validates one direction at a time. The reverse GUI test used the GUI TX-only mode and the CLI receiver; two independent simultaneous GUI receivers require a second independent virtual cable or an audio router.
- PTT and CAT remain deliberately disabled for this bench work; an on-air radio and PTT timing are intentionally outside this safe test. This is the only deliberate hardware limitation remaining, because the requested bench configuration keeps radio control disabled.

## Final package refresh

After the Radio backend connection path was wired and the Windows socket
handle was widened to pointer size, the final staged copies were checked
again:

- `bin\fectty-tests.exe` — `All tests passed`.
- `bin\fectty-live.exe --list` — `CABLE Output` input 0 and `CABLE Input`
  output 1 were present.
- `bin\fectty-audio-smoke.exe winmm:0 winmm:1 3` — `samples=141312`
  and `dropped=0`.
- `gui\fectty-portaudio-smoke.exe portaudio:1 portaudio:5 3` — exact
  `PORTAUDIO SMOKE`, `417792` captured, `0` dropped.
- Packaged PortAudio GUI TX to packaged WinMM CLI RX — exact reconstructed
  `PORTAUDIO GUI TX`, 2 valid frames, 0 CRC failures, 0 sequence gaps, 1
  acquisition, and 0 dropped samples.
- The final packaged GUI launched from `gui\fectty-gui.exe` and remained
  responsive. The GUI was left in NullRig bench mode; CAT/PTT was not
  enabled.
