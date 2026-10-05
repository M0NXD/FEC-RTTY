# Public documentation review — 5 October 2026

> Dated development/test record. Statements below apply to the recorded version and test host, not every reader's setup.
> Device IDs are examples; generated artifacts named here are not public downloads.
> Use the [current testing guide](../docs/TESTING.md) and [documentation index](../PROJECT_INDEX.md) for current instructions.

Scope: make the published 0.45.1 source repository understandable without
access to the original development conversation, SDK folders or private builds.
No modem/GUI implementation or tested installer binary changed.

## Editorial changes

- Rewrote the entry-point README, index, source overview and roadmap for
  external readers; retained the 24-hour experiment preamble.
- Added fresh-checkout build/testing guides and documentation/evidence/archive
  landing pages. Clarified source-only availability and undeclared application
  licensing instead of directing readers to absent installer/ZIP files.
- Updated GUI/project/installer references to distinguish source-build paths,
  separately generated packages, current controls and future design requirements.
- Annotated all 124 milestone Markdown pages, 17 dated records and seven
  archived planning briefs. Historical results and binary checksums were not
  replaced with new claims.
- Added public-inventory link/anchor/render checks and archive-scope checks.
  Existing private-path checks still apply. Old Git history was not rewritten.

## Executed verification

A fresh export of the public source/index was built in a new directory with
spaces in its parent path, without pre-existing build caches or SDK folders
inside the export. Existing external developer tools supplied the prerequisites;
this was **not** a clean Windows VM or fresh dependency installation.

- Windows core `build.bat`: exit 0; CTest **2/2**, 34.41 seconds.
- GUI `build-gui.bat`, explicit Qt 6.8.3/MSVCRT-compatible MinGW GCC 15.2
  overrides, PortAudio enabled: exit 0; CTest **4/4**, 38.32 seconds.
- Fresh deployed GUI `--help`: exit 0 with Windows-only PATH and Qt environment
  overrides cleared. Runtime DLLs/plugins were supplied by the helper.
- Simulator `HELLO`: exact text, one valid frame, zero CRC failures/gaps.
- Benchmark noise 0.02/0.05/0.10/0.15/0.20: all exact, seven frames each,
  zero CRC failures. These are deterministic simulations, not RF sensitivity.
- Markdown render/local links and private-path checks passed before publication.

The GUI build reported optional Vulkan headers, unused `CMAKE_C_COMPILER`,
and absent optional DX compiler/SSL deployment notices, but completed tests
and the deployed help probe successfully. No new hardware audio or RF test
was run for this documentation change. MSYS2's deprecated MINGW64 status is
now distinguished from the recorded working toolchain in the build guide.
