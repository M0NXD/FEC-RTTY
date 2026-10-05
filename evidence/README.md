# Test and development records

These files record dated checks of particular source versions, binaries and
test environments. They are evidence of those runs, not a promise that every
device, platform or radio works. The [test guide](../docs/TESTING.md) explains
how to run new checks; the [project guide](../docs/PROJECT_GUIDE.md) describes
current behavior and unresolved limitations.

## Current baseline records

- [Current public-package acceptance](PUBLIC_RELEASE_20261005.md): latest
  hashes, 37 installer checks, exact audio and unresolved WinMM/Stop warnings.

- [Public documentation and fresh-source build check](DOCS_PUBLIC_AUDIT_20261005.md).
- [v0.45.1 modem acceptance](BUG_FIXES_20261004_R11.md).
- [Original installer acceptance](INSTALLER_20261005_R12.md): a local packaging
  revision preceding the current public installer; its hashes are historical.
- [Waterfall/tuning acceptance](WATERFALL_20261004_R9.md).
- [Noise and shutdown/backlog findings](BUG_AUDIT_20261004_R10.md).

Older files describe earlier features and test-host observations. ARQ/HELLO/
ACK session experiments are historical, not part of the active GUI. Device
indices belong to each run; enumerate your own endpoints before transmitting.
References to a generated package or raw WAV do not mean it is in the repository.

Selected logs, JSON and screenshots are retained, including failed diagnostic
runs. Private paths are redacted; result values and recorded artifact hashes
are preserved. Later privacy/editorial changes to a document do not change
the historical artifact identified by its checksum. See
[Publishing](../docs/PUBLISHING.md) before sharing new evidence.
