# Publication and path privacy

The [public repository](https://github.com/M0NXD/FEC-RTTY) contains one canonical
latest source tree, documentation and selected test records. Current Windows
binaries are attachments on the [latest release](https://github.com/M0NXD/FEC-RTTY/releases/latest),
not executable/DLL files committed to Git. Earlier package/source snapshots
are retained privately, not published as additional versions.

## Public packages

The current installer/portable ZIP share one verified payload with matching
runtime DLLs, source, offline documentation, notices and complete Qt Base/SVG
6.8.3, Hamlib 4.7.2 and libusb 1.0.30 source archives, license notices and
library replacement/build instructions. The Qt archives are also available from the same release
page for recipients who only need the library sources. Include checksums and
test scope; do not describe bench acceptance as RF or clean-machine acceptance.

Do not bundle VB-CABLE without resolving its vendor's integration terms.
The public package links to the official separate driver download instead.
Do not add a project license without the owner's decision. LGPL library rights,
notices, corresponding sources and replacement instructions remain applicable
independently of the application's unselected general source license.

## Private paths and test records

Examples use paths relative to the checkout and generic environment locations
such as `%LOCALAPPDATA%`. Never publish a developer's username, checkout,
temporary directory or conversation-export path.

Dated logs/JSON are path-redacted copies. `<project-root>` means the test checkout.
Counters, decoded text, outcomes and historical artifact hashes are preserved;
redaction changes document bytes, not the original artifact's recorded checksum.
Private originals/backups and generated SDKs/packages/captures stay outside Git.

Before publication, run from the checkout root:

~~~powershell
pwsh -NoProfile -File tools/check-public-paths.ps1 -SelfTest -History
pwsh -NoProfile -File tools/check-docs.ps1
~~~

The guard inspects tracked bytes and main's text history; it is not OCR or a
complete secrets scanner. Inspect screenshots separately and scan package
contents, including UTF-8/UTF-16 binary strings. Upstream Qt DLLs contain Qt's
own build-host paths; these are not the project owner's workstation paths.

Installer testing retains raw local outputs and writes a redacted
`acceptance.public.json`. Redact raw Inno logs before sharing. Mark evidence
with its tested version, artifact checksum and limits.

## History

The owner authorized replacing the public branch with one clean latest commit;
the original Git history is backed up locally. Use an exact expected remote
revision when replacing history. Old clones, forks, object URLs and GitHub
caches can retain earlier data despite a rewrite; support-assisted cached-object
removal may be needed. Do not claim a force-push guarantees erasure.

Keep operator guides, developer references, future design requirements and
dated evidence clearly separated. Link checks use the public Git inventory,
not ignored artifacts that happen to exist on a developer's disk.
