# FEC-RTTY - M0NXD: complete project guide

Documentation baseline: application v0.46.1,
4 October 2026, with public-reader instructions updated 5 October 2026.
Get the current Windows installer/portable ZIP from the
[latest release](https://github.com/M0NXD/FEC-RTTY/releases/latest).
For source builds, start with [Building](BUILDING.md) and [Testing](TESTING.md).
Historical results below are not tests newly run for this editorial update.

## Preamble: an experiment to build a digital mode in 24 hours

FEC-RTTY began as an experiment to build an amateur-radio digital mode in
24 hours: develop a text modem with forward error correction, make it usable
in a desktop GUI, and demonstrate actual modem audio being transmitted and
decoded between two running instances.

The challenge explains the project's motivation, not a verified elapsed-time
record or a declaration that all engineering/radio validation was completed
within 24 hours. The project also contains later iterations, reversions,
bug audits, and acceptance records. It remains experimental, with explicit
evidence and limitations rather than a finished-product guarantee.

All message formats are deliberately unencrypted. UTF-8 bytes are plaintext
before framing and recoverable by a compatible receiver. FEC, interleaving,
CRC, and tone mapping are public error-protection/transport operations, not
privacy mechanisms. There is no authentication, key exchange, or encrypted
mode. Operators remain responsible for their station operation and applicable
requirements; a bench test does not establish permission or readiness for RF.

## Contents

1. [Purpose and active scope](#1-purpose-and-active-scope)
2. [Repository layout and documentation](#2-repository-layout-and-documentation)
3. [Run and test the program](#3-run-and-test-the-program)
4. [GUI operation](#4-gui-operation)
5. [Protocol overview](#5-protocol-overview)
6. [Architecture and source map](#6-architecture-and-source-map)
7. [Build and dependencies](#7-build-and-dependencies)
8. [Command-line tools and test scripts](#8-command-line-tools-and-test-scripts)
9. [Settings and data](#9-settings-and-data)
10. [Testing and evidence](#10-testing-and-evidence)
11. [Troubleshooting](#11-troubleshooting)
12. [History and remaining work](#12-history-and-remaining-work)
13. [Maintenance, packaging, and Git](#13-maintenance-packaging-and-git)

## 1. Purpose and active scope

The current Windows Qt application is titled exactly `FEC-RTTY - M0NXD`.
It is an experimental keyboard-to-keyboard text modem with one fixed mode:
48 kHz mono, 50 symbols/s, Gray-mapped 4-FSK, rate-1/2 convolutional FEC,
soft Viterbi decoding, 16-column interleaving, and CRC-gated frames with
up to eight UTF-8 bytes each. It is **not** conventional two-tone Baudot RTTY
and requires a compatible FEC-RTTY decoder.

Operation is receive-first and linkless: no station/peer negotiation,
HELLO/ACK wait, or automatic retransmission. One acquisition field precedes
consecutive text frames. Unicode, tabs, newlines, blank lines, and trailing
newlines are preserved; binary/control frames are not rendered as chat text.
`TX complete` means local audio playback completed, not that another station
received every byte. There is no automatic identity/callsign exchange.

The GUI supports direct Hamlib, rigctld TCP and OmniRig Rig 1/2 with explicit
PTT arming and dial/mode apply. Arming is off on every launch. See
[Radio control](RADIO_CONTROL.md). The CLI live tool remains audio-only.
Historical addressed ARQ and weak-signal helpers are not current GUI features
or alternate selectable modes. Macro/logging APIs are not a full GUI logbook.

## 2. Repository layout and documentation

The checkout root is the canonical working directory; `<project-root>` in
redacted historical evidence stands for whichever checkout ran that test.

| Location | Purpose |
|---|---|
| `README.md`, `PROJECT_INDEX.md` | Introduction and full document/evidence navigation |
| `source/include/fectty/`, `source/src/` | Interfaces and canonical C++ implementation |
| `source/tests/` | Regression tests and explicitly invoked hardware noise emitter |
| `source/CMakeLists.txt`, `source/build*.bat` | Version/targets/options and Windows build/deploy/test helpers |
| `docs/` | Maintained guide, specification, GUI instructions/design, and plan |
| `tools/` | Repeatable real-audio/GUI tests and replay diagnostic source |
| `evidence/` | Audit notes, selected JSON/images/logs; some large captures ignored |
| `releases/` (local only) | Ignored packages/old snapshots; public binaries are release attachments |
| `source/build*/`, `third_party/`, preview folders | Generated outputs/local SDKs, not canonical source |

The full wire contract is [PROTOCOL.md](PROTOCOL.md); GUI deployment/usage
is [GUI_PACKAGE.md](GUI_PACKAGE.md); requirements versus implementation are
in [GUI_DESIGN.md](GUI_DESIGN.md); the maintained backlog/gates are in
[DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md). The
[project index](../PROJECT_INDEX.md) links the historical records.
[DEVELOPMENT_PLAN_SOURCE.txt](DEVELOPMENT_PLAN_SOURCE.txt) is an archived
planning brief, not current setup instructions or an active feature list.
Older dated ledgers are evidence, not additional published source versions.

## 3. Run and test the program

### Packaged launch

For a fresh GitHub checkout, build the GUI using [BUILDING.md](BUILDING.md),
then launch `source/build-gui/fectty-gui.exe`. Alternatively, download the
installer or portable ZIP from the official
[release page](https://github.com/M0NXD/FEC-RTTY/releases/latest).

Run `FEC-RTTY-0.46.1-Setup-x64.exe`, which bundles runtime prerequisites
and registers normal Windows uninstall. See [INSTALLER.md](INSTALLER.md).
No modem behavior or radio-keying scope is changed by installer packaging.

For a complete portable runtime package, open `gui\fectty-gui.exe` inside it.
Keep its entire `gui` directory with DLLs and plugin folders. No compiler
or Qt SDK is needed to run the deployed package. The `bin` tools have their
own separate runtime DLLs; do not mix those with the GUI DLLs.

### Two visible windows over VB-Audio

1. Launch the same GUI executable twice; call the windows A and B.
2. In both, select the same available backend: WinMM or PortAudio.
3. Choose RX=`CABLE Output`, TX=`CABLE Input`. These are Windows endpoint
   names: playback into the cable becomes capture from its output.
4. Keep Radio=`NullRig / bench mode` and start both audio sessions.
5. Begin with linked RX/TX at 1500 Hz. With split offsets, A TX must match
   B RX and B TX must match A RX.
6. Send a short distinctive message from A and check exact text/frames in B.
   Wait for playback to finish, then send a different message B to A.
   Include blank/trailing newlines when checking text handling.

This transports generated audio, not text between processes. Both receivers
can hear the shared cable, including their own sends. It is one shared bus,
not crossed/independently isolated paths. Send sequentially: simultaneous
transmissions can collide. Independent directions require a second cable,
router/mixer, or separate hardware.

Device numbers change. Select GUI endpoint **names** and enumerate before
scripted tests. R11 used WinMM RX 0/TX 1, PortAudio RX 1/TX 5 [MME]; these
are run-specific observations, not permanent IDs. Do not substitute speakers
or a radio-connected output when a script intends the virtual cable.

### Transmission time

Acquisition is 0.5 s; a full eight-byte frame is 2.36 s. For nonempty text
with `B` UTF-8 bytes and `F = ceil(B/8)` frames, modem audio takes:

```text
0.5 + 0.02 * (54 * F + 8 * B) seconds
```

`HELLO` takes 2.38 s, excluding startup/host delay. GUI playback adds 0.1 s
of silence at the end. This remains a low-rate 50-baud experiment; removing
link establishment did not remove framing/FEC overhead or increase baud rate.

## 4. GUI operation

The central area holds the real RX waterfall, offset controls, received-text
view, and transmit editor. Setup/Diagnostics are dockable. Qt layouts,
logical coordinates, font-aware painting, and an explicit high-contrast dark
palette support resizing/high DPI. Recorded visual/scaling checks are not
certification for every monitor or accessibility setting.

- **Audio:** choose independent endpoints for the selected backend. `No RX`
  permits TX-only operation. Refresh/reselection is done while stopped;
  device/backend controls lock while a session is open.
- **Start/Stop/Send:** Send requires an open session, a nonempty draft, and
  no current send. Stop cancels playback. Failure/cancellation retains the
  draft; success clears it only if it has not changed during transmission.
- **Modem gain/output volume:** gain 0.05–1.0, default 0.5; volume 0–100%,
  default 100% unless restored/overridden. Final scale is their product.
  Both affect new queued blocks during a send; volume is saved but current
  modem gain is not. Volume 0 sends silence. The slider does not measure
  radio ALC/drive or establish safe RF output.
- **Radio:** NullRig is audio-only. Direct Hamlib, rigctld and OmniRig offer
  live status, explicit dial/mode apply and armed PTT. Lead/tail timing is
  functional only when armed. See [Radio control](RADIO_CONTROL.md).
- **Mode:** one fixed `FEC-RTTY 4-FSK / FEC` entry, not a weak-signal selector.

### Waterfall and precise offsets

New waterfall rows appear at the top, covering 0–3500 Hz. Teal is RX;
dashed orange is TX. Tune the **center** of the four-tone region, not one
individual tone. The default region is 1425–1575 Hz around 1500 Hz.

Left click/drag tunes RX; right or Ctrl click/drag tunes TX. Arrow keys move
RX 1 Hz, Shift makes the step 10 Hz, Ctrl targets TX. Numeric fields cover
300–3000 Hz in 0.1 Hz increments. Link RX + TX moves both; unlink for split
offsets. These are audio offsets, not RF dial commands.

RX can retune while listening; that resets acquisition/decoder counters,
retains displayed text, and waits for a new complete burst. Mid-frame
retuning can lose data. TX/linkage lock during a send; linked RX also locks.
Split RX may still retune. Sensitivity changes brightness only, not RX gain
or output volume. TX-only sessions have no RX signal display.

The display uses a 4096-point Hann FFT, 2048-sample hop, 11.71875 Hz bins,
42.67 ms row interval, and 240 rows (about 10.24 s). Numeric 0.1 Hz precision
does not imply that FFT resolution or physical sound-clock accuracy.

### Diagnostics

Valid frames passed header/FEC/CRC; CRC failures reject candidates; sequence
gaps identify unexpected modulo-16 order within a burst; acquisitions count
validated preambles. Frequency offset is residual error against RX tones,
not radio RF. Captured, processed, buffered, dropped, and host interruption
counts are distinct. Zero queue drops does not prove shutdown processed
every captured sample. PortAudio reports some host overflow/underflow events;
WinMM has no equivalent flag, so its zero interruption count is not a host
loss measurement. The stop/backlog risk remains open (section 12).

## 5. Protocol overview

[PROTOCOL.md](PROTOCOL.md) specifies exact fields, acquisition pattern,
polynomials, mappings, vectors, length selection, and recovery. The chain is:

```text
UTF-8 -> <=8-byte payloads -> header/payload/CRC -> convolutional FEC
      -> interleaver -> sync/coded data -> 4-FSK -> audio

Audio -> acquisition/time/frequency estimate -> sync -> soft tone decisions
      -> deinterleave -> Viterbi -> header/length/CRC -> text bytes
```

The burst starts with 25 known symbols. Each frame has MSB-first sync
`0xD391C5A7`, then coded/interleaved data. Raw data has two header bytes,
0–8 payload bytes, and two big-endian CRC bytes. Byte 0 is version/type/flags;
byte 1 is sequence/length. Active GUI text is version 0, type Text, flags 0.
CRC-16/CCITT-FALSE uses `0x1021`, initial `0xFFFF`, no reflection/final XOR.
FEC is rate-1/2 K=7, octal `171/133`, terminated by six zero input bits;
interleaving is row-to-column with 16 columns.

Default increasing tones are 1425/1475/1525/1575 Hz, Gray-mapped from
`00/01/11/10`. Each symbol is 960 samples (20 ms). Center tuning shifts all
four equally without changing spacing or format. Oscillator phase is
continuous within generated segments; the current implementation does not
guarantee phase continuity across every preamble/frame boundary.

`ReliableReceiver` accepts arbitrary chunks, retains preamble **plus sync**
history, searches newly eligible positions incrementally, refines timing/CFO,
and decodes consecutive frames. Candidate payload lengths are tried because
length is itself FEC protected; header and CRC determine validity. Every
validated new preamble resets that burst's sequence continuity, even without
silence. In-burst recovery still reports missing frames. There is no production
continuous timing PLL or periodic AFC.

UTF-8 characters can cross frames, so the GUI decoder retains incomplete
sequences. Tabs/newlines stay visible; binary controls are filtered. FEC/CRC
do not authenticate a sender, and a lost frame can lose text or break Unicode.
There is no active GUI delivery proof, repair request, or end-message receipt.
The legacy addressed ARQ/HELLO formats in the specification are retained for
old/core tests, not required/generated by current direct GUI operation.

## 6. Architecture and source map

`fectty_core` is the reusable C++20 DSP/protocol/support library; applications
share it. Current GUI orchestration is `MainWindow` in `gui_main.cpp`.
Service class names proposed in the design document describe a future split,
not existing classes. The `fectty` namespace/header/executable/CMake naming
is historical; the displayed product name is FEC-RTTY.

Adapters queue bounded capture chunks. An RX worker computes waterfall rows
and runs `ReliableReceiver`. Its initial idle-energy gate avoids unnecessary
search work; after activity starts, subsequent chunks preserve the timeline.
UI polling takes pending results under a mutex and updates widgets; callbacks
do not touch the GUI. A TX worker owns `ModemTransmitStream`, keeping message
bytes and at most one frame of waveform, feeding short blocks into one output
session, reading live gain/volume, and observing cancellation. TX tones are
captured once per send. Shutdown cancels/joins workers before destroying
audio, but does not guarantee draining every captured chunk. Conversation
text still grows; bounded audio is not constant total application memory.

Files below are under `source/`; most `.*` entries have matching headers in
`include/fectty/` and implementations in `src/`.

| Files/components | Responsibility |
|---|---|
| `protocol.hpp`, `frame.*`, `crc16.*` | Protocol constants, header serialization/parsing, integrity |
| `convolutional.*`, `viterbi.*`, `interleaver.*` | FEC encoding, soft trellis decoding, permutation/inverse |
| `fsk4.*`, `acquisition.*`, `sync.*` | Tone mapping/modulation/demodulation, preamble search, sync checks |
| `frame_receiver.*`, `reliable_receiver.*` | Candidate decoding, bounded streaming acquisition/recovery, counters/text |
| `session.*` | Whole-burst modem API and bounded-waveform transmit stream |
| `stream_receiver.*`, `timing.*`, `spectrum.*` | Lower-level receive/timing/spectrum helpers, not a production timing PLL |
| `waterfall_spectrum.*`, `waterfall_widget.*` | Independent FFT analysis and Qt rendering/input tuning |
| `audio_io.hpp`, `winmm_io.*`, `portaudio_io.*` | Audio interface, endpoint enumeration/restoration, capture queues, cancellable streaming TX |
| `gui_text.hpp`, `bench_exit.hpp`, `parse.hpp`, `paths.hpp` | Incremental visible UTF-8, exit policy, strict numeric parsing, UTF-8 paths |
| `settings.*` | Validated settings load/save, default migration, temporary-file replacement |
| `rig_control.hpp`, `net_rigctl.*`, `omnirig.*`, `hamlib_rig.*` | NullRig/control interface, direct rigctld TCP, COM, optional libhamlib |
| `tx_controller.hpp`, `tx_watchdog.hpp` | Reusable PTT timing/state/watchdog scaffolding, not real-radio acceptance |
| `arq.*` | Historical addressed envelope, ACK/retry/duplicate handling, inactive in GUI |
| `channel.*`, `clock_drift.*`, `timing_tracker.*`, `interop.*`, `link_metrics.*` | Impairment/resampling/search/metric helpers and two-station/legacy ARQ simulation |
| `wav.*`, `level_meter.*`, `diagnostics.*` | Mono PCM capture/replay, peak/RMS/clipping, diagnostic support |
| `macros.*`, `qso_log.*` | Macro expansion and ADIF APIs, not a complete GUI logbook |
| `main.cpp`, `benchmark.cpp`, `live.cpp`, `gui_main.cpp` | Simulator, deterministic benchmark, WinMM bench, desktop application |
| `audio_smoke.cpp`, `portaudio_smoke.cpp` | Explicit hardware/cancellation diagnostics |
| `tests/tests.cpp`, `audit_tests.cpp`, `gui_text_tests.cpp`, `waterfall_tests.cpp` | Original/audit/Unicode/FFT-widget CTest regressions |
| `tests/noise_prefix_bench.cpp` | Explicitly invoked cable noise/burst emitter; not an automatic CTest |

## 7. Build and dependencies

Use [BUILDING.md](BUILDING.md) for prerequisites and a fresh-checkout workflow.
The concise reference below records helper behavior and the tested toolchain.

Requires CMake >=3.20 and a C++20 compiler. Accepted Windows builds used
CMake 4.2, MinGW GCC 15.2.0: UCRT for standalone core tools,
MSVCRT-compatible MSYS2 MinGW for Qt 6.8.3. Helpers do not install/download SDKs.

From the project root in PowerShell:

```powershell
& .\source\build.bat
& .\source\build-gui.bat
```

The core helper builds relative to its own location, quotes paths, uses
Release, disables Qt/PortAudio/Hamlib, respects a cached generator, deploys
available cached-compiler DLLs, runs CTest, and propagates failure.
`build.bat clean` **deletes generated `source/build`**, then reconfigures.
With MinGW, outputs are directly under `source/build`; multi-configuration
generators may use `Release`. Current simple bench scripts expect MinGW
layout. MSVC fallback is not proof all scripts/deployment were tested there.

For a separate generic core build:

```powershell
cmake -S source -B source/build-manual -DCMAKE_BUILD_TYPE=Release
cmake --build source/build-manual --config Release --parallel
ctest --test-dir source/build-manual --build-config Release --output-on-failure
```

The GUI helper defaults to `third_party/qt/6.8.3/mingw_64`, prefers the
conventional MSYS2 `mingw64` installation, and honors `QT_ROOT`/`TOOLCHAIN_ROOT`
overrides. It builds
GUI/tests/noise emitter, deploys Qt using `windeployqt`, copies matching
compiler/PortAudio/Hamlib/libusb DLLs, and runs five CTests. PortAudio auto-enables
when its pkg-config file is in that toolchain; the GUI helper requires Hamlib. CMake options are
`FECTTY_WITH_QT`, `FECTTY_WITH_PORTAUDIO`, `FECTTY_WITH_HAMLIB`; each optional
feature needs its development dependencies, not just DLLs. Runtime packages
do not include the SDK/compiler. See [GUI_PACKAGE.md](GUI_PACKAGE.md).

Do not mix the official MSVCRT-compatible Qt package with UCRT-built GUI code:
that caused recorded startup corruption. Do not exchange same-named runtime
DLLs between `gui` and `bin` or reuse an incompatible compiler cache. Exact
build/runtime/hashes are in the release manifest. This is Windows acceptance,
not a claim of tested portability to all systems/toolchains.

## 8. Command-line tools and test scripts

### Executables

| Program | Function/side effects |
|---|---|
| `fectty-gui.exe` | Visible GUI; opens audio on request; bench reports/snapshots |
| `fectty-live.exe` | WinMM enumerate/send/receive; sends real audio, writes WAVs in working directory |
| `fectty-sim.exe` | Optional message argument; deterministic channel and exact-text check |
| `fectty-benchmark.exe` | Fixed-message, known-frame-size noise sweep, not full live/RF acceptance |
| `fectty-tests.exe`, `fectty-audit-tests.exe` | Core regressions; some generate test settings/logs |
| `fectty-gui-text-tests.exe`, `fectty-waterfall-tests.exe` | Unicode/FFT/widget regressions; waterfall review images |
| `fectty-audio-smoke.exe` | Explicit WinMM capture/cancel test; opens selected devices |
| `fectty-portaudio-smoke.exe` | `--list` enumerates; normal invocation sends real `PORTAUDIO SMOKE` audio and checks capture |
| `fectty-noise-prefix-bench.exe` | `BACKEND:N clean` or `continuous`; noise plus `PREFIX TEST`; refuses output names not containing `CABLE Input` |

`tools/replay-probe.cpp` is diagnostic source, not a packaged GUI function or
CMake target. Compiled against the core, it accepts mono 16-bit 48 kHz WAV
or `--synthetic`, reporting samples/receiver events/decoded byte hex. WAV
support is a library API; `fectty-live` does not have a `--replay` flag.

### GUI flags

| Flag | Meaning |
|---|---|
| `--help`, `--autostart` | Print options; or start selected audio after showing GUI |
| `--rx BACKEND:N`, `--tx BACKEND:N` | `winmm:N`/`portaudio:N`; RX can be `none`; requires autostart/send |
| `--send TEXT`, `--quit-after-send` | Nonempty send implies autostart; quit requires an automatic send |
| `--center-hz N` | Linked center 300–3000 Hz |
| `--rx-center-hz N`, `--tx-center-hz N` | Independent 300–3000 Hz centers; unlink offsets |
| `--retune-rx-hz N`, `--retune-after N` | RX retune target; integer 1–3600 s delay, default 3 |
| `--volume N`, `--run-seconds N` | Integer 0–100% output; integer 1–3600 s closing timer |
| `--report PATH` | Atomic JSON: displayed text, draft/TX outcome, RX/audio metrics, endpoints, tuning/waterfall |
| `--snapshot PATH` | Qt-rendered end-of-run GUI image |

Invalid/incomplete arguments, unavailable devices, startup/report/snapshot
errors fail. Unfinished, failed, cancelled, or never-started automatic sends
return 2. Successful sends and ordinary receive-only/idle closes return 0
unless another error occurs. Receive-only exit 0 does not assert decoding;
inspect JSON. Provide writable existing parent directories for outputs.
Bench parameters do not save over operator preferences.

Example in the package `gui` directory, **after verifying these cable IDs**:

```powershell
.\fectty-gui.exe --autostart --rx winmm:0 --tx winmm:1 --center-hz 1500 --volume 50
.\fectty-gui.exe --rx none --tx winmm:1 --send "HELLO" --quit-after-send --volume 50
```

### Standalone WinMM

From package `bin` or `source/build`, not `gui`:

```powershell
.\fectty-live.exe --list
.\fectty-live.exe --receive --input 0 --seconds 15
# Another terminal, after the receiver starts:
.\fectty-live.exe --send "HELLO" --output 1 --gain 0.5
```

IDs are direction-specific WinMM numbers; explicitly select verified devices
instead of relying on Windows mapper defaults. Gain 0–1; receive seconds
1–86400; choose receive or a nonempty send, not both. No GUI report/center/
PortAudio flags exist here. RX prints payload pieces/counters and exits
nonzero without a valid frame. Captures are `fectty-live-rx.wav` and
`fectty-live-tx.wav`; repeated use of the same working directory overwrites
them. Prefer GUI JSON for exact Unicode/multiline display acceptance.

Unlike bounded GUI waveform generation, the standalone live tool builds the
whole TX waveform and retains captured RX audio for its WAV; memory grows
with duration. Its TX WAV is the generated waveform **before** playback gain,
not a recording of the final driver/radio output level.

### PowerShell bench harnesses

These transmit real audio: confirm endpoint names first. GUI harness windows
stay visible. Run from the project root; use a fresh evidence directory:

```powershell
$guiExe = (Resolve-Path '.\source\build-gui\fectty-gui.exe').Path
& .\tools\test-gui-direct.ps1 -GuiExe $guiExe -Backend winmm -InputDevice 0 -OutputDevice 1 -SplitOffsets -RetuneRx -EvidenceDir '.\evidence\manual-direct'
& .\tools\test-gui-noise-prefix.ps1 -GuiExe $guiExe -Backend winmm -InputDevice 0 -OutputDevice 1 -EvidenceDir '.\evidence\manual-noise'
& .\tools\test-gui-exit-status.ps1 -GuiExe $guiExe -OutputDevice 'winmm:1' -EvidenceDir '.\evidence\manual-exit'
& .\tools\test-vb-audio.ps1 -Profile short -InputDevice 0 -OutputDevice 1
```

Direct tests compare both directions including split Unicode/blank/trailing
lines; optional `-CenterHz`/`-Volume` configure them. Noise tests have optional
`-EmitterExe` for a matching emitter. Exit tests cover idle/not-started/
cancelled/completed cases. For PortAudio, use its `--list` output and current
IDs with the GUI scripts, not WinMM numbers. The standalone script resolves
root `bin/fectty-live.exe` or `source/build/fectty-live.exe`; profiles `short`,
`20`, `50`, `long` (512 bytes/64 frames), optional `-Gain`/`-Seconds`, logs/WAVs.
`list-audio-devices.bat` and `run-short-vb-audio.bat` are convenience wrappers.

## 9. Settings and data

Normal settings use Qt's AppConfigLocation plus `fectty.ini`; on Windows:

```text
%LOCALAPPDATA%\FEC-RTTY\FEC-RTTY\fectty.ini
```

Old FEC-TTY preferences can migrate when no new file exists. Recognized
numbers are finite/range checked; a malformed known field rejects the
candidate instead of partly applying it. Missing TX center copies RX;
linked offsets match. Embedded CR/LF in settings values are replaced with
spaces before saving so they cannot inject extra records; this sanitization
is not reversible text escaping. Saving writes a temporary file then replaces
the target, avoiding a partial in-place write.

Saved fields include backend/endpoint IDs **and names**, radio host/port,
volume, offsets/linkage, waterfall floor, and legacy timing/identity values.
Restore requires a unique endpoint name in the correct direction; PortAudio
names include host API. Missing/ambiguous/numeric-only choices need reselection.
Normal instances share preferences; there is no independent profile manager
or write-lock merge. Bench launches isolate preference writes.

Core callsign/station-ID/macro/ADIF helpers are not automatic GUI identity or
logging. Only typed text is sent. The historical ADIF default identifier is
`FECTTY`; no standardized external mode identifier is established here.
Conversation auto-archival and a full logbook/export workflow are not promised;
explicitly preserve needed text/reports. JSON, WAV, logs, and ADIF are
unencrypted and may contain text/station/device information: review before
sharing. JSON is evidence of local behavior, not an authenticated receipt.

## 10. Testing and evidence

Recorded v0.45.1 acceptance, not tests newly executed for this documentation:

- Core CTest 2/2 (35.57 s), GUI 4/4 (37.19 s), fresh packaged source in a
  path with spaces 2/2 (33.51 s), and deployed packaged regressions passed.
- Audit: 82 checks, zero failures; waterfall: 24 checks, zero failures;
  original core/Unicode tests passed. Strict warnings/static analysis passed
  within the exact R11 scope and suppression boundary.
- Three seconds of noise took 755/746/757 ms in core/GUI/packaged audit runs
  on the recorded test host, not a universal hardware performance guarantee.
- Four noisy-prefix RX cases (two backends, clean/continuous-noise burst)
  decoded `PREFIX TEST` exactly, two frames/one acquisition each.
- Four ordinary RX cases (two sequential directions per backend) decoded
  exact 18-byte/three-frame A-to-B and 11-byte/two-frame B-to-A Unicode/
  multiline payloads. WinMM included live RX retuning.
- All eight real-audio cases had zero CRC/gaps/software queue drops/reported
  interruptions and TX/RX exits 0, with WinMM/stop-backlog caveats intact.
- Eight packaged exit-policy cases returned expected statuses and preserved
  cancelled drafts.

See the [R11 ledger](../evidence/BUG_FIXES_20261004_R11.md)
for exact recorded results. Reports/images are in `evidence/audit-20261004-r11/`.
Current package hashes/manifests are attached to the latest release.
[R10](../evidence/BUG_AUDIT_20261004_R10.md) retains original failures;
[R9](../evidence/WATERFALL_20261004_R9.md) covers waterfall/tuning;
[R8](../evidence/BUG_AUDIT_20261004_R8.md) records the preceding full audit.
[Visual](../evidence/gui-visual-audit-20261004.md) and
[scaling](../evidence/gui-scale-matrix-20261004.md) records identify reviewed
configurations. Historical ARQ records do not describe current GUI behavior.

For changes, run core/GUI CTest first and add real-audio suites when relevant.
Compare exact UTF-8 text, not merely a nonempty result. Record frame/CRC/gap/
acquisition counts, captured versus processed samples, queue/host events,
both exits, version/hash, device names/API/indices, tuning, gain/volume,
expected bytes/text, JSON/log/screenshot, and WAV where available. Note
cancellation/timeouts and other applications sharing the cable. Preserve
failed evidence, add a reproducible regression, fix, then rerun it and related
tests. Known-frame-size simulation and deterministic noise complement live
acquisition tests; they are not calibrated RF sensitivity/fading/SNR claims.

## 11. Troubleshooting

| Symptom | Check/action |
|---|---|
| DLL/plugin error or launch corruption | Keep deployed folder/plugins intact, including `platforms/qwindows.dll`; do not mix GUI/bin CRT DLLs; use compatible Qt/compiler deployment, not random replacement DLL downloads. |
| SDK/compiler missing | Verify CMake and compatible `QT_ROOT`/`TOOLCHAIN_ROOT`; optional-free core works without GUI SDK. Runtime DLLs are not development SDKs. |
| Generator/toolchain cache conflict | Use a new generated build tree or deliberately clean the relevant generated tree; preserve source/settings. |
| Send unavailable | Start audio, choose valid TX for backend, enter nonempty text, wait for active send. No remote link is needed. |
| No RX waterfall | Check named cable endpoints/direction/backend, started session and nonzero TX volume; TX-only has no input display. |
| Signal visible, no text | Match receiver RX to sender TX center; start before preamble; wait for complete frame; avoid collisions/retuning; inspect acquisition/CRC/gaps. Waterfall alone is not decode proof. |
| Noise before burst fails on old release | Use v0.45.1; R10/R11 document the fixed search-history bug, not every possible noise/RF condition. |
| Text/Unicode missing | Compare exact JSON and CRC/gaps; lost frames cannot be repaired by display. Legacy/control frames should not appear as conversation bytes. |
| Timed send unfinished | Allow byte-count duration plus startup overhead; unfinished automatic sends fail and cancelled drafts remain. |
| Zero drops, captured/processed differ | Inspect shutdown/backlog, not just overflow; allowing processing time is a workaround, not a verified drain fix. |
| Device number changed | Re-enumerate by endpoint name/API; WinMM/PortAudio indices differ and are ephemeral. |
| Radio not keying/dial not moving | Verify exact model/port, live readback, explicit Apply and PTT arming; inspect faults. Waterfall controls change audio offsets only. See [Radio control](RADIO_CONTROL.md). |

## 12. History and remaining work

The supplied starting source was `fectty-v0.32-source.zip`. v0.33/v0.34
preserved the Windows real-audio baseline and improved live multi-frame RX.
v0.38 added the scalable GUI/device controls. Addressed ARQ/plaintext sessions
(v0.41/v0.42) and weak-signal/timing experiments were explored, then the fixed
original FEC-RTTY waveform was restored (v0.43). v0.44 removed GUI link setup
and improved direct text/newlines; v0.44.1 audited Unicode/streaming/parsing/
devices/settings. v0.45.0 added waterfall/split tuning; v0.45.1 fixed preceding-
noise acquisition and unfinished automatic-send status. Milestone records,
not an accumulated feature list, define each version's scope.

There is a working bench release, but not zero remaining limitations:

1. **Stop/backlog:** Stop can abandon queued captured samples. Current cases
   decoded fully, but a deliberately backlogged drain/text-loss regression
   and verified correction remain needed. Zero overflow cannot close this.
2. **Real radio:** independent computers/radios, RF fading/interference,
   USB transceiver audio, CAT/PTT lead/tail, drive/ALC, and frozen-vector
   independent interoperability need a separate acceptance gate.
3. **Audio devices/timing:** long independent sound-clock drift, production
   continuous timing tracking, unplug/replug, and all host/device combinations
   are not covered by one same-machine cable.
4. **Shared bus:** sequential reverse testing is not isolated/simultaneous
   bidirectional acceptance. No active scheduler, delivery ACK, or retry.
5. **Product/UI:** exhaustive monitor/accessibility validation, GUI logbook/
   macros/identity, and independent preference profiles remain incomplete.
6. **Distribution:** clean-machine deployment, physical-radio/OmniRig acceptance,
   explicit project source licensing and code signing remain incomplete.
   Public Windows packages include dependency notices and Qt source/replacement
   instructions; this does not select a general application source license.

Use [DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md) for the maintained route.
Future work must preserve the intended single mode, plaintext, and unarmed-
by-default radio behavior unless explicitly changed. Waveform changes need an updated
specification and interoperability vectors, not just a local successful send.

## 13. Maintenance, packaging, and Git

Edit canonical source, not frozen release copies. Keep spec/usage/design
consistent with code, separate new measurements from historical results,
and preserve failed reproductions. A documentation-only update does not bump
the application version or rewrite a tested release. Do not accidentally
restore inactive link/weak-signal features or imply radio keying from scaffolding.

A new delivery includes source/build helpers, current docs/scripts, selected
evidence, separate compatible GUI/core runtime folders, and manifest/hash.
Test actual deployed binaries, compare packaged source to canonical source,
fresh-build a copy including a path with spaces, and record compiler/runtime/
features, tests, binary/archive hashes, and limitations. Checksums identify
accidental changes, not signatures or assurances of RF safety. Existing
hashed releases remain intact; include updated canonical docs in the next one.

The source remote is [M0NXD/FEC-RTTY](https://github.com/M0NXD/FEC-RTTY).
Review `git status`, `git diff`,
and `git log`; preserve unrelated user changes. Commit source/docs/harnesses
and selected evidence, not all generated material. `.gitignore` excludes
build trees, SDKs, runtime folders, ZIPs, raw WAV, and generated test files.
Git therefore does not back up every runnable artifact; separately preserve
needed archives/SDKs/captures or regenerate from documented inputs. Do not
force-add caches or publish new artifacts without authorization. Follow
[the publishing/path-privacy policy](PUBLISHING.md): published records redact
private machine locations while retaining original measurements and hashes.
