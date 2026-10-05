# FEC-RTTY installer sources

Current Windows downloads are on the
[latest release](https://github.com/M0NXD/FEC-RTTY/releases/latest).
The Git checkout contains installer source, not its executable/DLL payload.
[Installation documentation](../docs/INSTALLER.md) covers prerequisites,
portable use, standard Windows uninstall, rebuilding and acceptance limits.

- `fec-rtty.iss`: stable per-user identity, shortcuts, uninstall and folder guard.
- `build-installer.ps1`: pinned runtime/source checks, notices, offline HTML,
  payload manifest, installer and portable ZIP.
- `test-installer.ps1`: isolated install/repair/uninstall and optional audio tests.
- `resources/`: introduction, getting-started page and Qt/CAT library replacement information.
- `accepted-release.json`: tested version and runtime/archive hash contract.
- `THIRD_PARTY_NOTICES.md`: dependency notices/source and driver boundary.

Builder inputs (accepted runtime archive, SDK/notices, Qt source archives and
Inno compiler) must be staged separately; a fresh clone alone is insufficient.
The builder performs no downloads. See [Building](../docs/BUILDING.md) for
compiling the application, and [Publishing](../docs/PUBLISHING.md) for privacy.
