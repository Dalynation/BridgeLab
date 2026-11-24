# BridgeLab 0.10.4 packaging verification

The Hub rejected the published 0.10.3 ZIP before extraction because the `ditto --sequesterRsrc` packaging command added a second top-level `__MACOSX` metadata tree. The archive contained 11,398 app entries and 12,433 metadata entries. The unchanged current Hub `PackageArchive.validate` reproduced the rejection. Earlier ZIP integrity, signature, metadata and update-feed checks did not exercise that validator; they were insufficient to establish installability.

0.10.4 changes the release packaging and version metadata, with no plugin/editor behavior changes. `tools/package-release.py` writes a single `BridgeLab.app` root, omits Finder metadata, preserves executable modes and stores verified relative runtime links. It requires the current Hub source checkout and compiles `tests/hub_release_install.swift` with the actual Hub Products, Archive and Installer implementations. The final release ZIP/checksum is only moved into place after this gate succeeds. The source hashes used by the gate are included in `package-verification-0.10.4.json`.

## Verification

- Reproduced 0.10.3 rejection with the actual current Hub archive validator.
- The corrected package passed the complete Hub installer in a disposable home: checksum, archive checks, extraction, code signature, CPU/OS compatibility, version 0.10.4, Hub authentication metadata, managed install, preservation of a test data marker and backup of a copied prior 0.10.2 app.
- Two release-writer unit tests pass: one-app layout/executable/relative-link preservation and rejection of an escaping link.
- The unchanged Hub's 18 installer regression cases pass: fresh install, backup, traversal/escaping links/case aliases/cycles, identity/signature/OS/CPU/checksum/version rejection, cancellation, running-host checks, rollback and interrupted-install recovery.
- The universal 0.10.4 build passed its existing native attachment, MIDI/state/import, permissions, transport and diagnostic checks.
- Package: `BridgeLab-0.10.4-mac-preview.zip`, 705,208,558 bytes, SHA-256 `2bf710adda0e07676413e0e17cadd439a19c5657c4dda1138ddd3e319678ff71`.

The user's real installation was not changed. The user explicitly reserved the final Hub download/install for their own manual test; no automated network-install test is claimed. Known plugin/editor limits in QA-0.10.3.md still apply.

## Release command

After building the app, use:

```
python3 tools/package-release.py --hub-source ../StudioHub --output build/release-0.10.4
```

The Hub source must be the version whose installer is being targeted. Do not publish a ZIP made with an unrelated archive command. Archive rejection remains enabled in the Hub.
