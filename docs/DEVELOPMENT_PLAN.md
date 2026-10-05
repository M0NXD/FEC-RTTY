# FEC-RTTY development roadmap

The project began as a 24-hour digital-mode experiment. This roadmap describes
application 0.45.1 and remaining engineering work, not a promise that every
planned feature is implemented. See the [project guide](PROJECT_GUIDE.md)
for behavior and the [test guide](TESTING.md) for verification procedures.

## Current scope

Keep one fixed 50-baud/48 kHz four-tone FSK waveform with convolutional FEC,
direct UTF-8 frames, CRC-gated delivery, and no encryption. The GUI is linkless:
no HELLO/ACK negotiation or retransmission. Radio CAT/PTT remains disabled;
`NullRig / bench mode` is the normal test configuration.

A protocol-breaking change requires an explicit version/compatibility decision,
updated specification and independent vectors. A successful local audio test
alone is not an interoperability or RF acceptance gate.

## Completed milestones

| Milestone | Result |
|---|---|
| v0.33/v0.34 | Windows actual-audio baseline and persistent multi-frame receiver |
| v0.38 | Qt GUI, device controls, output level, settings and bench diagnostics |
| v0.43/v0.44 | Return to fixed waveform; direct text with preserved newlines |
| v0.44.1 | Unicode, cancellable streaming TX, parsing/settings/device audits |
| v0.45.0 | Live waterfall and linked/split RX/TX audio tuning |
| v0.45.1 | Noise-prefixed acquisition and automatic-send completion fixes |
| Installer experiment | Per-user offline packaging, repair and standard Windows uninstall |

Dated evidence is indexed in [PROJECT_INDEX.md](../PROJECT_INDEX.md). GUI design
requirements in [GUI_DESIGN.md](GUI_DESIGN.md) distinguish current controls from
future service/logbook/accessibility work.

The v0.41/v0.42 addressed-ARQ sessions and weak-signal/timing experiments are
historical, not additions to the active GUI feature list. The
[original planning brief](DEVELOPMENT_PLAN_SOURCE.txt) is archived input.

## Remaining work, in order

1. **RX shutdown/backlog:** reproduce deliberately queued audio at Stop, define
   drain/cancellation behavior, fix confirmed loss and add a deterministic
   regression. Zero overflow counters do not prove complete processing.
2. **Independent audio:** test separate computers and clocks, long duration,
   unplug/replug, and independent reverse paths. A shared virtual cable does
   not validate simultaneous isolated directions.
3. **Interoperability:** freeze complete byte/bit/symbol/WAV vectors and check
   them with an independent implementation.
4. **Radio integration:** define and test safe CAT/PTT lead/tail, watchdog,
   disconnect and shutdown behavior before enabling keying. Then perform
   controlled actual-radio audio/RF tests, with drive/ALC and failure captures.
5. **UI/accessibility:** extend monitor/scaling, keyboard/screen-reader and
   contrast acceptance. Logbook/macros/identity and separate preference
   profiles remain future product work.
6. **Distribution:** select a general project source license; test clean Windows
   installations and upgrades; decide signing. Maintain dependency notices,
   corresponding sources, replacement rights and artifact-privacy checks.

Public Windows downloads are on the
[latest release](https://github.com/M0NXD/FEC-RTTY/releases/latest). The
[installer workflow](INSTALLER.md) needs separately staged pinned release inputs and
is not reproducible solely by running its script in a fresh GitHub clone.

## Verification and maintenance

Use canonical `source/` and current `docs/`, not archived release copies.
Keep GUI/CLI on the same modem implementation and callbacks free of filesystem,
CAT/COM and blocking UI work. Optional dependencies must not prevent a core build.

For each change, run applicable CTest suites and add regressions for defects.
For audio changes, compare exact text, frames, CRC/gaps, acquisitions, processed
versus captured samples, software drops/host interruptions and both process
exits. Record device names/IDs/API, tuning/volume, version and artifact hashes.
Preserve unsuccessful diagnostic runs and state what was not tested.

Do not infer RF readiness from simulation or cable tests. Keep recorded
historical measurements separate from newly executed checks. Run the
[documentation/privacy checks](PUBLISHING.md) before pushing; generated SDKs,
binaries, captures and private data remain outside Git.
