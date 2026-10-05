# Public Windows release acceptance: v0.45.1 packaging revision 2

> Dated development/test record. These observations describe the artifacts
> and test host below, not every Windows computer or radio. Use the current
> [test guide](../docs/TESTING.md) for new checks.

## Delivery

The public tree contains one canonical latest source version, not archived
release source copies. Original Git history and older packages remain backed
up locally. The owner authorized a single clean public commit.

The application/modem binaries are unchanged from accepted v0.45.1. Packaging
revision 2 updates public documentation/notices, includes complete unmodified
Qt Base/SVG 6.8.3 sources and replacement instructions, and omits VB-CABLE,
optional Mesa software-OpenGL and Microsoft D3D compiler DLLs. The GUI uses
raster painting. No radio CAT/PTT or encryption was added.

| Artifact | SHA256 |
|---|---|
| `FEC-RTTY-0.45.1-Setup-x64.exe` | `9571FFF173C92A08A72F72324AD7D87526A0F8905D5B129C42A5ADB0CAC87961` |
| `FEC-RTTY-0.45.1-Windows-x64.zip` | `2364A550CED11DFF903FC503CDBB09BC9A126072E2CB8C5687DD49EB284A627E` |

Both packages contain the same **512-file, 124.63 MiB** payload and hash
inventory. The installer/ZIP contain matching application source, offline
documentation and dependency licenses. The source subtree object is
`d0be1232767e73061eb1cea7f656b397be20acaa`; this is a Git tree identifier,
not a commit ID. Documents are a packaging-time snapshot; this acceptance
record was written after testing and is published separately.

## Executed checks

- All 39 accepted runtime/plugin inputs verified against their pinned archive;
  37 shipped after the two optional graphics DLL omissions.
- 83 local links/anchors checked across eight primary offline pages.
- Latest portable ZIP: all 512 payload hashes verified against its manifest.
- Packaged GUI launched with Windows-only PATH; packaged simulator returned
  exact `HELLO`, one valid frame, zero CRC failures/gaps.
- 476 payload text/binary files scanned as UTF-8/UTF-16 for the owner's profile
  path and its slash/escaped variants. PNGs and official compressed Qt sources
  were excluded from that string scan; public tracked-image metadata passes
  the repository guard, and Qt archive hashes match upstream downloads.
- Installed core, audit, Qt text and waterfall regression executables passed.
- **37/37 installer harness assertions passed**: isolated spaces/Unicode
  install, nonempty-folder rejection, all hashes, Windows registration and
  shortcuts, clean-PATH starts, two visible responding GUI windows, audio
  payload checks, repair and registered uninstall. User-created files,
  actual operator settings and existing cable devices were preserved.
- No installed test copy or its shortcuts/registration remained afterward.

Raw/private outputs remain in ignored acceptance staging. Published receipt
and audio reports are under `public-release-20261005/`; owner paths are redacted.

## Exact installed audio results

Two sequential directions over the installed shared VB-CABLE bus, visible
GUI windows, gain 0.5/output volume 50%, split offsets: sender TX=1500 Hz,
receiver RX=1500 Hz. Both instances use NullRig; CAT/PTT is disabled.

| Backend | Direction | Exact UTF-8 result | Bytes | Frames | Acquisitions | CRC/gaps/software drops | TX/RX exits |
|---|---|---|---:|---:|---:|---|---|
| WinMM RX 0 / TX 1 | A→B | `1234567é\n\nA TO B\n` | 18 | 3 | 1 | 0/0/0 | 0/0 |
| WinMM RX 0 / TX 1 | B→A | `B TO A é\n\n` | 11 | 2 | 1 | 0/0/0 | 0/0 |
| PortAudio MME RX 1 / TX 5 | A→B | `1234567é\n\nA TO B\n` | 18 | 3 | 1 | 0/0/0 | 0/0 |
| PortAudio MME RX 1 / TX 5 | B→A | `B TO A é\n\n` | 11 | 2 | 1 | 0/0/0 | 0/0 |

All sender reports have `tx_success=true`. Each measured waterfall peak is
1523.4375 Hz. Device numbers are observations, not defaults for other computers.

## Failures retained and unresolved warnings

The initial candidate's short receive timer ended with only two of three
frames displayed (`1234567é\n\nA TO `). Captured/processed samples were
790528/536576, despite zero software overflow. The direct-GUI harness now uses
a bounded **20-second receive tail**, not eight seconds. This is a bench-timing
workaround, **not a fix for RX Stop discarding queued chunks**.

A separate short-tail portable retry captured only 86880 samples and reported
`RX buffer requeue failed: There is no driver installed on your system.`
The extended installed WinMM A→B case decoded all text but also reported that
error; captured/processed samples were 1138560/669696. Its B→A case did not
report the error and processed all 1208320 captured samples. Windows device
enumeration and device-manager status reported the cable present/OK between
runs; the underlying transient API/driver failure was **not resolved**.

The harness assertions check exact messages/counters/exits, not the absence of
every event-string error. Therefore 37 passed assertions are **not** a claim
of fault-free continuous WinMM capture. WinMM's zero interruption field also
does not measure host loss. PortAudio had no reported capture error in either
extended case. A→B processed all 1298432 captured samples; B→A processed
571392 of 1204224 captured samples, while still decoding the whole message.
That second case also demonstrates the unresolved Stop/backlog distinction.
Prefer PortAudio for this cable bench if WinMM reports a requeue failure;
stop/reopen and verify reception rather than trusting zero drop counters.

Still experimental and unsigned. No clean Windows VM, separate computers/
clocks, real radios/RF, simultaneous isolated paths, exhaustive scaling,
driver installation/restart, new-version migration or signing acceptance is
claimed. No general application source license has been selected.
