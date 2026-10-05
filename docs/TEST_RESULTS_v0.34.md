# FEC-RTTY v0.34 test ledger

> Dated development/test record. Statements below apply to the recorded version and test host, not every reader's setup.
> Device IDs are examples; generated artifacts named here are not public downloads.
> Use the [current testing guide](TESTING.md) and [documentation index](../PROJECT_INDEX.md) for current instructions.

Date of current bench work: 2026-10-03.

## Build and automated tests

- `build.bat`: completed successfully with the dependency-free MinGW configuration; it now also copies the MinGW runtime DLLs beside the build executables for direct launching.
- `fectty-tests.exe`: passed after the stateful receiver, decoder, recovery, and incremental multi-frame changes.
- Simulator and benchmark regressions: passed in the v0.33 baseline and retained as regression gates.

## Real VB-Audio cable results already observed

The tested device mapping was input `0 = CABLE Output` and output `1 = CABLE Input`. Windows device indices can change, so always confirm with `fectty-live.exe --list`.

| Message | Frames | CRC failures | Sequence gaps | Acquisitions | Reacquisitions | Exact text |
|---:|---:|---:|---:|---:|---:|---|
| `HELLO` | 1 | 0 | 0 | 1 | 0 | yes |
| 160 bytes | 20 | 0 | 0 | 1 | 0 | yes |
| 400 bytes | 50 | 0 | 0 | 1 | 0 | yes |
| 512 bytes | 64 | 0 | 0 | 1 | 0 | yes |

The final 512-byte run sent 7,273,920 modem samples and captured 10,559,488 samples over the 220-second receive window. Peak was 0.5, RMS was 0.293388, and the receiver ended in `LOST` after the post-transmission silence. The run's evidence is `evidence/vb-long-20261003-231121-rx.log`, `evidence/vb-long-20261003-231121-tx.log`, and the matching RX WAV capture.

During development, two runtime issues were found and fixed: a fixed 120-second TX watchdog was too short for the 512-byte waveform, and a damaged-frame recovery path could repeatedly re-demodulate the same buffer and overrun the audio queue. The final run had zero dropped samples. A temporary 50-frame attempt before the failure-latch fix was intentionally discarded from the acceptance results.

## Completion and limitation

The v0.34 live acceptance gate and package are complete. A true independent reverse-direction test requires a second virtual cable or an audio router; the installed single VB-Audio cable verifies the tested `CABLE Input` playback to `CABLE Output` capture path only. CAT/PTT and radio control were disabled throughout.
