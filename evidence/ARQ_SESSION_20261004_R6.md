# FEC-RTTY addressed ARQ/session verification — 2026-10-04

> Dated development/test record. Statements below apply to the recorded version and test host, not every reader's setup.
> Device IDs are examples; generated artifacts named here are not public downloads.
> Use the [current testing guide](../docs/TESTING.md) and [documentation index](../PROJECT_INDEX.md) for current instructions.

## Scope

This round adds addressed stop-and-wait ARQ to the one selectable FEC-RTTY
waveform. The GUI now sends addressed data by default, accepts only the
configured peer/source, transmits ACK/NAK/BUSY control frames automatically,
retries lost transfers, and applies deterministic bounded backoff. CAT/PTT
remained disabled and the radio backend remained NullRig/bench mode.

## Automated results

| Check | Result |
|---|---|
| `source\build.bat` | Passed; Release build and CTest 1/1 passed |
| `source\build-gui.bat` | Passed; Qt GUI and PortAudio smoke target deployed |
| Addressed frame encode/decode | Passed; destination/source filtering and CRC validation |
| Single ARQ burst with +250 ppm clock mismatch | Passed; frame recovered before a later frame was available |
| Dropped first data + dropped first control | Passed; exact text, 14 data attempts, 13 control attempts, 2 retransmissions, 1 duplicate frame, 750 ms maximum backoff |
| Weak-signal ARQ | Passed through noise 0.04, +35 Hz, +250 ppm |

## Physical WinMM GUI results

The installed device list was:

- Input `0`: `CABLE Output`
- Output `1`: `CABLE Input`
- Output `2`: `CABLE In 16ch`

Observed checks:

1. Rebuilt GUI station 1 → WinMM output 1, `--send ARQ`; `fectty-live`
   station-independent receiver on input 0 printed exact `RX: ARQ` modem data.
2. GUI station 1 → station 2 using outputs 1/2 and both input 0: the sender
   exited with status `0` after receiving an addressed ACK.
3. Reverse GUI station 2 → station 1 using outputs 2/1 and both input 0: the
   sender exited with status `0` after receiving an addressed ACK.
4. The packaged `releases\fectty-v0.41-arq-20261004\gui\fectty-gui.exe` repeated
   the station 1 → station 2 check with message `PKG` and exited with status
   `0` after the ACK round trip.

These two GUI checks prove the actual WinMM modem/ARQ audio path and both
application directions on the installed shared cable bus. They do not prove a
genuinely independent reverse physical path: this machine exposes one VB-Audio
capture endpoint, so independent two-cable or two-sound-card validation remains
an external hardware gate.

## Remaining limits

- There is no authentication, encryption, callsign exchange, capability
  negotiation, physical carrier-sense MAC, or hidden-terminal protection.
- The GUI ARQ session is currently one peer at a time and is limited to 765
  data bytes per transfer.
- No live HF, CAT, PTT, propagation-fade, or independent sound-card-clock test
  was performed.
