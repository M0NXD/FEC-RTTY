# FEC-RTTY v0.45.0 — waterfall and final audit acceptance

> Dated development/test record. Statements below apply to the recorded version and test host, not every reader's setup.
> Device IDs are examples; generated artifacts named here are not public downloads.
> Use the [current testing guide](../docs/TESTING.md) and [documentation index](../PROJECT_INDEX.md) for current instructions.

The user's waterfall request was added while the R8 audit was being finished.
This release includes all R8 fixes plus the live waterfall and precise
independent audio tuning. Historical releases are unchanged. Local-only Git
and the disabled NullRig/CAT/PTT bench boundary are retained.

## Implemented behavior

- Real selected RX audio feeds a Hann-windowed 4096-point FFT on the receive
  worker, including idle input. There is no simulated signal in the live GUI.
- Display: 0–3500 Hz audio frequency, 11.71875 Hz bin spacing, 2048-sample hop
  (42.67 ms), 240 history rows (about 10.24 seconds), newest at the top.
- Teal RX center/tone-span and orange dashed TX center stay visible above the
  conversation. Left click/drag tunes RX; right/Ctrl click/drag tunes TX.
- Independent RX/TX fields support 300–3000 Hz at 0.1 Hz control precision.
  Link RX + TX moves both together. Arrow keys tune by 1 Hz, Shift by 10 Hz,
  and Ctrl selects TX. These are the centers of the four audio tones, not RF
  dial frequency and not 0.1 Hz spectral/physical-clock accuracy.
- RX retunes safely at a worker boundary while listening and resets lock,
  decoder state, and receive counters; previous conversation text remains.
  A frame interrupted by retuning may be lost. TX captures one immutable
  frequency configuration per message. TX/link controls are locked during
  playback; linked RX is locked too, while unlinked RX remains tunable.
- Sensitivity changes display brightness only. It does not change RX audio,
  modem gain, TX volume, FEC, wire framing, or the single fixed modem profile.
- Audio/FFT/history/display queues are bounded. Painting uses Qt logical
  coordinates and font metrics; footer labels shorten rather than overlap
  at narrow widths/large text sizes. Spectrum bins align to the frequency
  scale at their centers, not half a bin away.
- Offset/linkage/brightness preferences persist and old single-center settings
  migrate to matching RX/TX centers. Bench runs leave operator settings alone.

## Additional device-selection finding

During testing, Windows/PortAudio endpoint numbering changed. Output
`portaudio:5`, formerly used successfully for the cable, became the speakers;
the cable moved to `portaudio:6`. The failed test correctly had zero decoded
frames, zero waterfall peak, and only silent captured audio. The transmitter
had completed playback, but to the wrong endpoint. This attempt is retained
as `audit-20261004/portaudio-20261004-210647-direction-0-{rx,tx}.json`.

The GUI now saves endpoint names as well as IDs, includes PortAudio host API
names in labels, and restores only uniquely named endpoints of the correct
direction. Missing/ambiguous endpoints require operator selection. Legacy
numeric-only preferences are not blindly restored, and refresh validates the
remembered names too. Five dedicated identity/direction/ambiguity regression
checks cover the restoration policy. Explicit bench numeric arguments still
mean exactly the current index requested; enumerate immediately before use.

Names are not hardware GUIDs. Two identically named devices on the same host
API are deliberately treated as ambiguous; the GUI does not claim to identify
them uniquely. Current VB-Audio IDs: WinMM RX 0/TX 1; PortAudio RX 1/TX 6 [MME].

## Automated and build results

- Core CTest: 2/2 passed, final canonical run 16.13 seconds. Expanded audit
  executable: 63 checks, failures 0.
- GUI CTest: 4/4 passed, final canonical run 17.11 seconds. Suites: original
  regression, expanded audit, Qt text-widget tests, and waterfall tests.
- Waterfall suite: 24 checks, failures 0. Tests include six tone peaks across
  the usable range, Hann dBFS normalization, 333-sample callback accumulation,
  NaN/silence handling, reversible frequency/pixel mapping, RX/TX/Ctrl clicks,
  keyboard tuning, TX lock, range clamp, bounded history, clear, and rendered
  normal/20-point-font views.
- Fresh core rebuild of the packaged source in
  `source/build-package-check/source with spaces`: successful, 2/2 CTest
  passed in 13.73 seconds initially and 15.63 seconds after the final decoder
  update. This verifies the actual source layout and quoted
  `build.bat` paths without depending on the canonical build cache.
  A final build-helper-only rerun passed 2/2 in 14.15 seconds. The helper
  also recognizes an explicitly supplied compiler's UNINITIALIZED cache type.
  The packaged audio-device listing helper successfully ran its bundled CLI.
- Packaged original/audit/Qt text/waterfall test executables all passed with
  their deployed DLLs. The GUI/CLI runtime copies remain separate because
  their Qt-compatible MSVCRT and standalone UCRT toolchains differ.
  The final packaged audit also reported 63 checks, failures 0, and 409 ms
  for three seconds of noise; the packaged waterfall suite passed all 24 checks.
- Strict core warnings-as-errors build passed. Strict syntax/warning checks
  for the actual GUI and waterfall painter passed with `-Wall -Wextra
  -Wpedantic -Wshadow -Wformat=2 -Werror`.
- Final Cppcheck warning/portability scan: exit 0, no findings; only its
  informational branch-limit notice is suppressed. Test code guards empty
  FFT results so a failed producer becomes a test failure, not an invalid
  `front()` access.
- The R8 simulator, benchmark, reduced-level CLI loopback, malformed argument,
  Unicode/WAV/settings, and long-message cancellation results remain valid.
  Final expanded audit runs processed three seconds of noise below real time
  on this machine (406 ms after the final sync-validation fix), with no locks
  or decoded text.

## Real modem audio / precise offsets

A late repetition exposed one more short-final-frame loss. The receiver could
commit to a partly received preamble before its sync arrived, leaving an
incorrect symbol grid. It now waits for a complete following sync and accepts
only a validated boundary. Nine callback-start alignments and a two-stage
preamble/sync regression verify this behavior. Failed run
`portaudio-20261004-212508-direction-1-rx.json` remains in the evidence.
A simultaneous WinMM/PortAudio comparison run (`pa-compare-1`) decoded the
same captured two-frame burst through both paths. PortAudio host-reported
input overflow/underflow events are now counted separately as capture
interruptions; they are not hidden behind a zero software-queue-drop counter.
The acceptance script also requires zero reported capture interruptions.

The executable tested is `releases/fectty-v0.45.0-waterfall-20261004/gui/fectty-gui.exe`.
Two windows stayed visible per direction. Distinct payloads contain a UTF-8
character crossing a frame boundary, embedded blank lines and trailing newlines.
Exactness is checked against the receive text widget, not console rendering.

```text
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/test-gui-direct.ps1 -GuiExe releases/fectty-v0.45.0-waterfall-20261004/gui/fectty-gui.exe -Backend winmm -CenterHz 1800.1 -SplitOffsets -RetuneRx
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/test-gui-direct.ps1 -GuiExe releases/fectty-v0.45.0-waterfall-20261004/gui/fectty-gui.exe -Backend portaudio -InputDevice 1 -OutputDevice 6 -CenterHz 1500 -SplitOffsets
```

| Backend | Direction | Exact text (escaped) | Bytes / frames | CRC / gaps / drops | Acquisitions | RX / TX exit |
|---|---|---|---|---|---:|---|
| WinMM | A to B | `1234567é\n\nA TO B\n` | 18 / 3 | 0 / 0 / 0 | 1 | 0 / 0 |
| WinMM | B to A | `B TO A é\n\n` | 11 / 2 | 0 / 0 / 0 | 1 | 0 / 0 |
| PortAudio | A to B | `1234567é\n\nA TO B\n` | 18 / 3 | 0 / 0 / 0 | 1 | 0 / 0 |
| PortAudio | B to A | `B TO A é\n\n` | 11 / 2 | 0 / 0 / 0 | 1 | 0 / 0 |

Final acceptance after the sync-validation/capture-counter update: three
consecutive PortAudio two-direction trials (six bursts), followed by one
WinMM two-direction trial (two bursts). All eight received the exact payload,
with one acquisition and zero CRC failures, gaps, software drops, or reported
capture interruptions. All eight TX reports indicate successful playback.
WinMM has no host overflow/underflow event API equivalent to PortAudio; its
zero interruption value is not a claim of such driver-level measurement.

Final package report prefixes (each has direction-0/1 RX/TX JSON and RX PNG):

- `portaudio-20261004-213923`
- `portaudio-20261004-213956`
- `portaudio-20261004-214028`
- `winmm-20261004-214431`

WinMM split/retune configuration: receiver initially 1400.1 Hz RX, retuned
while running to 1800.1 Hz after two seconds, with its TX unchanged at
2100.1 Hz. Sender RX stayed at 1500.1 Hz while actual TX was 1800.1 Hz.
This proves that the waveform uses TX, not the sender's independent RX field,
and that live receiver tuning configures the decoder. The measured last
spectral peak was 1828.125 Hz, in the expected four-tone region.

PortAudio split configuration: receiver RX 1500 Hz / TX 1800 Hz; sender
RX 1200 Hz / TX 1500 Hz. Last measured peak: 1523.4375 Hz, in the expected
tone region. Every receiving run generated hundreds of live waterfall rows.
The script asserts nonzero rows, a measured peak within 100 Hz of the expected
center, independent fields/linkage, exact text, valid frame count, and all
CRC/gap/drop counters. Sender reports have `tx_success=true`.
Final WinMM runs produced 302/255 waterfall rows; final PortAudio trials
produced hundreds of rows per receiving run. Simulator text `PROJECT BUG
AUDIT` decoded exactly with three valid frames and zero CRC failures/gaps.
The packaged benchmark passed at each noise value 0.02, 0.05, 0.1, 0.15,
and 0.2: seven valid frames, zero CRC failures and exact text at every point.

Both directions use the same installed cable sequentially, not independent
simultaneous physical paths. Modem gain 0.5 and volume 50% produce nominal
0.25 amplitude. CAT/PTT was not enabled and no radio was keyed.

## Visual review

Qt-rendered full GUI images at 2400 × 1470 pixels on the current high-DPI
desktop show readable labels, distinct markers, correctly aligned frequency
scale, unclipped offset fields, the actual received waveform, and decoded
Unicode/blank-line text. The normal and 20-point-font waterfall unit renders
were inspected too. No new pixel inspection on every monitor/Windows scaling
combination is claimed; previous general GUI scaling evidence remains available.
The group title's ampersand is escaped for Qt mnemonic handling.

JSON reports and matching PNGs are in `evidence/audit-20261004/`; filenames
identify source versus final package runs in the package manifest. Earlier
unsuccessful/intermediate attempts remain for diagnosis, not acceptance.

## Delivery and boundaries

Release: `releases/fectty-v0.45.0-waterfall-20261004/` and its ZIP.
Launch `gui/fectty-gui.exe`; keep its DLLs/plugins beside it. README,
protocol, GUI operation/build instructions, project plan, source/tests,
harnesses, and selected evidence are included. Hashes are recorded in
`BUILD_MANIFEST.md`. No remote repository or push was added.

This is still experimental bench software. Real RF fading/interference,
long independent sound-card drift, USB unplug recovery, radio CAT/PTT/audio
levels/ALC, optional Hamlib, independent simultaneous directions, and public
distribution/licensing review are not passed acceptance gates. Direct text
has no acknowledgements or guaranteed delivery. No alternate weak-signal mode
or encryption was introduced. The waterfall provides measurement and tuning,
not a change to those protocol/hardware boundaries.
