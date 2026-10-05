# FEC-RTTY documentation and test-record index

FEC-RTTY began as a 24-hour digital-mode experiment. Application **0.45.1**
is the current baseline. [Latest Windows downloads](https://github.com/M0NXD/FEC-RTTY/releases/latest)
are release attachments; this checkout contains the canonical source.

## Start here

- [README](README.md): downloads, purpose and getting started.
- [Installation](docs/INSTALLER.md): installer, portable build, repair and uninstall.
- [Building](docs/BUILDING.md): dependencies and source-to-executable steps.
- [Testing](docs/TESTING.md): regressions and virtual-cable tests.
- [GUI operation](docs/GUI_PACKAGE.md): devices, levels, waterfall and tuning.
- [Project guide](docs/PROJECT_GUIDE.md): architecture, source map and limitations.
- [Protocol](docs/PROTOCOL.md): wire format and modem specification.

## Development and distribution

- [Source overview](source/README.md).
- [GUI design](docs/GUI_DESIGN.md): implemented behavior versus future requirements.
- [Roadmap](docs/DEVELOPMENT_PLAN.md).
- [Installer sources](installer/README.md).
- [Publishing and path privacy](docs/PUBLISHING.md).
- [Third-party notices](installer/THIRD_PARTY_NOTICES.md).

SDKs, executable/DLL payloads, installers, ZIPs and raw audio captures are not
stored in Git. Older release source snapshots have been removed from the public
tree; only the latest canonical source is published.

## Recorded results

These are dated observations, not guarantees for every computer or device.
Tests used actual modem audio; radio CAT/PTT stayed disabled.

- [Documentation and fresh-source build review](evidence/DOCS_PUBLIC_AUDIT_20261005.md).
- [Public Windows package acceptance](evidence/PUBLIC_RELEASE_20261005.md):
  current hashes, install/repair/uninstall, exact audio and unresolved warnings.
- [v0.45.1 modem acceptance](evidence/BUG_FIXES_20261004_R11.md).
- [Original installer acceptance](evidence/INSTALLER_20261005_R12.md):
  34 checks on the test host; the current public package is a later packaging revision.
- [Waterfall acceptance](evidence/WATERFALL_20261004_R9.md).
- [Noise/backlog findings](evidence/BUG_AUDIT_20261004_R10.md).
- [Whole-project audit](evidence/BUG_AUDIT_20261004_R8.md).
- [Visual review](evidence/gui-visual-audit-20261004.md) and
  [scaling review](evidence/gui-scale-matrix-20261004.md).
- [Evidence index](evidence/README.md) for additional records.

Selected JSON, logs and screenshots are retained under `evidence/`.
Private paths are redacted. Historical hashes identify the original tested
artifacts, not subsequently edited documents or the current public packages.

Older notes can mention retired ARQ/HELLO features, earlier names, host device
numbers and unpublished package names. They are test history, not current
setup instructions or alternate selectable modes.
