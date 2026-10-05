# FEC-RTTY 0.46.0 CAT release acceptance — 5 October 2026

> Dated development/test record. These are observations on one Windows host,
> not a guarantee for every radio, driver or computer. No physical radio was
> connected/keyed by these tests and no RF was transmitted.

## Software and real-audio results

- GUI build: five CTest suites passed, including 149 CAT assertions with
  official Hamlib 4.7.2 Dummy, loopback rigctld and fake OmniRig dispatch.
  Direct Hamlib disables its frontend cache and refuses models without live
  getters, including PTT, before port open. The added missing-getter checks
  prevent frontend transmit state being accepted as radio feedback.
- Dependency-free core: three suites passed after a clean, serial rebuild.
  Hamlib is intentionally absent there; Windows TCP/OmniRig tests remain active.
  Its separate CAT executable passed 136 checks on the final rerun.
- TCP GUI: all seven cases passed: normal, rejected ON, lost ON reply,
  external TX, failed OFF, cancellation and overlong-message rejection.
  Failed/lost keying generated zero modem samples and confirmed release.
  External TX was not commandeered (no ON/OFF); failed OFF retained uncertain
  ownership/fault and the manual-unkey warning. Cancelled/oversized drafts survived.
- Actual shared-cable PortAudio MME test with direct Hamlib Dummy PTT:
  A→B exact 18 UTF-8 bytes, 3 frames; B→A exact 11 bytes, 2 frames.
  Both had zero CRC failures, sequence gaps, software drops and capture
  interruptions; TX/RX exits were zero and PTT was confirmed off.
  Messages included Unicode and blank/trailing newlines, split offsets,
  1500 Hz sender TX/receiver RX, gain 0.5 and 50% output volume.
- Endpoint names were checked before playback: PortAudio MME CABLE Output
  ID 1 / CABLE Input ID 6; WinMM IDs 0/1. These numbers are host-specific.

The first TCP regression setup incorrectly retained simulated TX from a
negative-response case; the controller correctly refused to key it. The
test state was reset before rerunning. An initial GUI test script used a
relative report path with the executable's working directory; the script
now resolves its evidence directory before launch.

Memory exhaustion interrupted an early parallel build and CTest process.
Build helpers now default to one compile job and reject all nonzero process
exits, including negative Windows crash statuses. The affected core was
rebuilt cleanly rather than accepting a stale compilation begun before edits.

## Packaging acceptance

The first portable compression ran out of disk space after setup compilation.
Checksum-identical library/source duplicates in obsolete generated staging and
diagnostic folders were removed; retained masters and all previous complete
packages/source/history remained available. The incomplete ZIP was preserved
privately and compression was retried from a hash-verified payload.

An initial isolated-shortcut test exposed that DisableProgramGroupPage=yes
ignored the requested /GROUP. Cleanup removed three pre-existing Start-menu
shortcuts; equivalent links to the unchanged original installation were restored.
Folder selection is now enabled, and the test additionally keeps byte-identical
shortcut backups for recovery. The recorded final rerun uses a separate group
and leaves the desktop shortcut unmodified; desktop creation is outside its scope.

The Radio tab was visually checked at an effective 200% UI scale on the
host's 4K display (3200×1960 window capture). Radio controls, status and the
fixed force-OFF control remained readable; this is not exhaustive display QA.

Installer/portable verification and final runtime hashes are recorded in the
companion public receipts for this release. Windows packages include Hamlib
4.7.2 and libusb 1.0.30 rather than Hamlib's older bundled libusb, matching
license/source/replacement material, Qt/PortAudio and compiler runtimes.
OmniRig and radio-manufacturer USB drivers remain optional external inputs.

Final installer acceptance passed **40/40 checks**: all 558 installed file
hashes, Windows-only-PATH startup, five installed regression executables,
two responsive visible windows, both audio backends, repair, registered
Windows uninstall, user-file/settings/device preservation and unchanged
pre-existing shortcut bytes on the final isolated-group run.

| Installed audio path | Exact UTF-8 bytes / frames | CRC / gaps / drops / interruptions | Dummy PTT at finish |
|---|---|---|---|
| WinMM A→B | 18 / 3 | 0 / 0 / 0 / 0 | OFF, unowned, no fault |
| WinMM B→A | 11 / 2 | 0 / 0 / 0 / 0 | OFF, unowned, no fault |
| PortAudio MME A→B | 18 / 3 | 0 / 0 / 0 / 0 | OFF, unowned, no fault |
| PortAudio MME B→A | 11 / 2 | 0 / 0 / 0 / 0 | OFF, unowned, no fault |

All eight sender/receiver exits were zero. The final seven GUI fault cases
also passed; cancellation generated 63,488 samples before cancellation and
confirmed release (this timing-dependent count is not a fixed protocol vector).

Tested runtime/source-tree identities:

~~~text
GUI SHA256: 36A5CB105EC050E757FAEFF1021A3065089D3014AD64F813306FE000EBFB6420
CLI SHA256: 70024D45AF93C627E9C0D2A41434BB6DDB980C91E099D699B27F8F08323DACC6
Setup SHA256: 5CB5C9DC01018FBD05AFC90041C836BA285537BD077990B8664BC38F0798224A
Portable SHA256: D98572DDD9810017FFF9245F7D48BEAFC9857EA997B21B5AC05B1D43BB1A4AC3
Git source/ tree: 45a378655baffab0eddf534b9d3b984d0049b307
~~~

Receipts: [installer](cat-20261005/installer-acceptance.json),
[GUI CAT cases](cat-20261005/gui-tcp/results.json),
[WinMM log](cat-20261005/direct-winmm.log),
[PortAudio log](cat-20261005/direct-portaudio.log),
[installed CAT regression](cat-20261005/fectty-cat-tests.log) and
[read-only installed OmniRig](cat-20261005/omnirig-installed-read.json).
Per-direction JSON and screenshots accompany the audio logs. Package manifests
freeze the payload built before these final companion receipts; published
source/documentation include the receipts. The application source-tree identity
is the same. Final SHA256SUMS also cover the separately generated source ZIP.

## Limits

A read-only check of the installed OmniRig server returned Status 2 (port
busy), frequency 0 and unknown mode/PTT. The GUI rejected connection with
zero generated samples, unarmed/unowned PTT and a nonzero test exit. No
profile, dial, PTT or port configuration was changed. Installed-radio OmniRig
acceptance therefore remains pending external configuration/port availability.
An initial launch hit Windows commitment-limit error 0x5af; the repeated
read-only launch completed with the expected offline fault, not a loader failure.

Dummy state and a fake COM dispatch are not physical-radio or installed-
OmniRig acceptance. Exact models/ports, transceiver input routing, hardware
PTT timing, driver failures, ALC/drive and RF must be supervised and verified.
A blocked backend call or lost connection can prevent software release;
use a hardware TX timeout/manual override. No universal model guarantee is made.

The shared cable is not isolated simultaneous bidirectional routing or
independent sound clocks. RX Stop can abandon queued samples. Prior
intermittent WinMM capture requeue errors are not claimed fixed by CAT work.
No clean Windows VM, signing/reputation, every display/device/model or
exhaustive accessibility test is claimed. Source visibility is not a general
application license grant; no encryption was added.
