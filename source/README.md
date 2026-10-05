# FEC-RTTY source

This directory contains the C++20 modem, Windows audio adapters, Qt GUI and
regression tests for application version 0.45.1. Start with the
[project README](../README.md) and [build guide](../docs/BUILDING.md).
The project began as a 24-hour digital-mode experiment; current limitations
are documented in the [project guide](../docs/PROJECT_GUIDE.md).

## Build from the repository root

On Windows, with CMake and a compatible compiler available:

~~~powershell
& .\source\build.bat
~~~

This builds the core tools and runs two CTest suites. It does not build a GUI
or install development dependencies. For a Qt GUI build, first configure the
SDK/toolchain as described in [Building](../docs/BUILDING.md), then run:

~~~powershell
& .\source\build-gui.bat
.\source\build-gui\fectty-gui.exe
~~~

The GUI helper deploys runtime DLLs/plugins beside its output and runs four
CTest suites. Keep those files together. `build.bat clean` deletes the generated
`source/build` directory before rebuilding; do not use it to preserve build outputs.

For explicit CMake options and multi-configuration output paths, use the
[build guide](../docs/BUILDING.md). Other platforms/toolchains have not received
the same deployment or real-audio acceptance as the recorded Windows build.

## Code and tools

- `include/fectty/` and `src/`: modem interfaces and implementations.
- `src/gui_main.cpp`: desktop console; radio keying remains disabled.
- `tests/`: deterministic regression tests and explicitly invoked audio diagnostics.
- `CMakeLists.txt`: targets/version and optional Qt, PortAudio and Hamlib switches.
- `build.bat`, `build-gui.bat`: Windows core and GUI build/deploy helpers.

The internal `fectty` namespace and executable names are retained for build
compatibility; the displayed application name is `FEC-RTTY - M0NXD`.
See the [source map](../docs/PROJECT_GUIDE.md#6-architecture-and-source-map)
for component responsibilities.

The active modem uses 50-baud 4-FSK, 48 kHz audio, rate-1/2 convolutional FEC,
CRC-16 and up to eight UTF-8 payload bytes per frame. It is unencrypted and
is not conventional Baudot RTTY. The [protocol specification](../docs/PROTOCOL.md)
defines compatibility. Legacy ARQ/timing helpers do not add selectable GUI modes.

## Verification

Use [Testing](../docs/TESTING.md) for simulation, CTest and real-audio checks.
Hardware diagnostics transmit audio and must target verified bench endpoints.
Record exact decoded text and counters; successful playback does not prove
delivery. Published [v0.45.1 results](../evidence/BUG_FIXES_20261004_R11.md)
are dated host observations, not RF certification or a clean-machine guarantee.
