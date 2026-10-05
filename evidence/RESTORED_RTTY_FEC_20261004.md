# Restored RTTY/FEC verification — 2026-10-04

> Dated development/test record. Statements below apply to the recorded version and test host, not every reader's setup.
> Device IDs are examples; generated artifacts named here are not public downloads.
> Use the [current testing guide](../docs/TESTING.md) and [documentation index](../PROJECT_INDEX.md) for current instructions.

## Change boundary

The weak-signal waveform upgrade was rolled back at commit `5066176`:
`Revert "Upgrade single FEC-RTTY profile for weak signals"`.

The active modem is again the original fixed profile:

- 50 baud, 48 kHz continuous-phase 4-FSK
- 1,425 / 1,475 / 1,525 / 1,575 Hz tones
- 25-symbol acquisition field
- Rate-1/2 K=7 convolutional FEC, octal 171/133
- 16-column interleaving, soft Viterbi decoding, CRC-16/CCITT-FALSE

The Qt GUI, WinMM/PortAudio audio selection, output-volume control, and NullRig
bench default remain available. The active GUI path uses direct plaintext FEC
frames with no link establishment, station/peer setup, HELLO, ACK wait, retry,
authentication, encryption, or alternate modem mode. Newlines are preserved.
The addressed ARQ modules remain only as legacy/core compatibility code.

## Verification

- `source\build.bat`: completed successfully.
- CTest: `fectty-tests` passed, 1/1 tests, approximately 18 seconds.
- Qt build: `source\build-gui.bat` completed successfully and deployed the Qt,
  MinGW, and PortAudio runtime files.
- The fixed-profile regression coverage passed the ordinary modem loopback,
  streaming multi-frame receive, direct plaintext decoding, and line-break
  preservation test. Legacy ARQ regression coverage also remains green.
- The GUI help text now describes only the original fixed RTTY/FEC waveform;
  weak-signal timing/AFC telemetry is absent from the active diagnostics.
- Packaged VB-Audio loopback passed in both message directions. The forward
  `HELLO` run decoded 5/5 bytes with 1 valid frame, 0 CRC failures, 0 sequence
  gaps, 1 acquisition, and 0 dropped samples. The reverse `REVERSE RTTY FEC`
  run decoded the exact 16-byte text in 2 valid frames with 0 CRC failures,
  0 sequence gaps, 1 acquisition, and 0 dropped samples. Both used RX
  `winmm:0` (`CABLE Output`) and TX `winmm:1` (`CABLE Input`) at 48 kHz.
- The packaged GUI launches as `FEC-RTTY - M0NXD`. A TX-only packaged GUI
  launch with `--rx none --tx winmm:1 --send ... --quit-after-send` exits `0`
  without waiting for a peer. A multiline GUI transmission decoded through
  VB-Audio with 1 acquisition, 6 valid frames, 0 CRC failures, 0 sequence
  gaps, and 0 dropped samples; the embedded and blank line breaks were present
  in the decoded byte stream.

## Package

The rebuilt package is:

`releases\fectty-v0.43-rtty-fec-20261004\`

The distributable archive is
`releases\fectty-v0.43-rtty-fec-20261004.zip`.

The GUI executable is in its `gui` directory. CAT/PTT remains disabled and the
bench radio selection is `NullRig / bench mode`.

## GUI display cleanup — 2026-10-04

The GUI was also corrected so the operator conversation view no longer appends
raw low-level frame/control bytes. It now displays only decoded plaintext and
uses direct text insertion, so embedded and trailing line breaks do not gain
extra paragraphs.

After the change, the core build and CTest passed again, the Qt GUI rebuilt and
deployed, and the packaged executable launched successfully as
`FEC-RTTY - M0NXD`.
