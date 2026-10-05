# FEC-RTTY plaintext session completion — 2026-10-04

> Dated development/test record. Statements below apply to the recorded version and test host, not every reader's setup.
> Device IDs are examples; generated artifacts named here are not public downloads.
> Use the [current testing guide](../docs/TESTING.md) and [documentation index](../PROJECT_INDEX.md) for current instructions.

## Scope

This round removes the remaining practical software-side ARQ limits while
keeping the requested security posture explicit: FEC-RTTY sends plaintext and
does not encrypt, authenticate, exchange keys, or claim callsign identity.
The single weak-signal waveform and NullRig/bench CAT/PTT boundary remain
unchanged.

## Software changes

- A logical message can be up to `195,075` bytes. The sender divides it into
  sequential 765-byte ARQ segments and advances the one-byte transfer ID for
  each segment; each segment still uses the existing CRC/FEC/ACK retry path.
- Peer station ID `255` is a wildcard source selector. A destination of `255`
  is accepted as a broadcast destination by the receiver.
- HELLO is implemented as an optional plaintext capability advertisement. It
  carries protocol version and the fixed plaintext/FEC/interleaving/ARQ/
  wildcard capability bitmap. It is informational and never gates a session.
- The GUI records local modem energy and waits for approximately 450 ms of
  quiet input before each ARQ attempt. This is best-effort carrier sense; ARQ
  timeout, retry, and bounded deterministic backoff remain the loss-recovery
  mechanism.
- The GUI displays the actual maximum plaintext message bound and keeps the
  fixed single waveform; no encryption or selectable second mode was added.

## Automated results

| Check | Result |
|---|---|
| `source\\build.bat` | Passed; Release build and CTest 1/1 passed in 15.24 s |
| ARQ segmentation regression | Passed; 900-byte plaintext produced 300 chunks and rolled from transfer `0x70` to `0x71` with 45 chunks in the next segment |
| ARQ maximum bound | Passed; `195,076` bytes is rejected, `195,075` bytes is the supported upper bound |
| HELLO capability regression | Passed; plaintext, FEC, interleaving, ARQ, and wildcard bits decoded correctly |
| Wildcard destination/source filtering | Passed |
| `source\\build-gui.bat` | Passed; Qt GUI and PortAudio smoke target built and deployed |
| cppcheck warning pass | Passed; 38/38 files checked, no warnings (only normal branch-analysis information) |
| VB-Audio short smoke | Passed; input 0/output 1, exact `HELLO`, 1 valid frame, CRC 0, gaps 0, acquisitions 1, drops 0, receiver exit 0 |

## Visible GUI WinMM tests

The installed device list was:

- Input `0`: `CABLE Output (VB-Audio Virtual ...)`
- Input `1`: `Microphone (USB Microphone)`
- Output `0`: `Speaker (Synaptics HD Audio)`
- Output `1`: `CABLE Input (VB-Audio Virtual C...)`
- Output `2`: `CABLE In 16ch (VB-Audio Virtual ...)`

The rebuilt GUI was launched as two visible stations with both using input 0:

1. Station 1, output 1, peer 2 sent `HELLO ARQ CCA`; sender exit status was
   `0` after an addressed ACK and station 2 remained running as the receiver.
2. Station 2, output 2, peer 1 sent `REVERSE ARQ CCA`; sender exit status was
   `0` after an addressed ACK and station 1 remained running as the receiver.
3. An earlier visible long-message bench run sent `LONGER ARQ CCA TEST` in the
   same configuration and also returned sender exit status `0`.
4. Wildcard bench: station 1 sent `WILDCARD ARQ` with peer ID `255` to station
   2 configured with peer ID `255`; the sender exited `0` after the addressed
   broadcast ACK.

The ACK proves that the receiving modem decoded and accepted the addressed
data frame before responding. Both directions use the installed shared
VB-Audio bus; output names 1 and 2 do not create a second independent capture
endpoint.

## Remaining boundaries

- Encryption is intentionally absent, as requested. The wire data is
  plaintext; there is no authentication, key management, or cryptographic
  privacy.
- Callsign exchange and identity proof are not implemented; station IDs are
  routing selectors only.
- Local carrier sense cannot detect a hidden station and is not a network
  scheduler. The receiver holds one active transfer at a time, although peer
  wildcard mode permits sequential sources.
- The installed machine still exposes one VB-Audio capture endpoint. A truly
  independent reverse physical path needs a second virtual cable, router, or
  sound card.
- Live HF propagation, independent sound-card clock tests, and real-radio
  CAT/PTT remain hardware/on-air validation gates. Bench tests leave CAT/PTT
  off and use NullRig behavior.
