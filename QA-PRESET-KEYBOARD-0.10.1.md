# BridgeLab 0.10.1 candidate verification

The local candidate exposes the plugin's VST2 program names as a VST3 program list. SQ8L exports 512 named programs; these TAL versions export one each. The legacy parameter ID and 511-step scaling remain available for saved automation. Import probes enumerate a disposable instance, not a user's active patch. Host scans read cached metadata without starting Wine.

Keyboard releases are returned to the window/responder that received the press, including when clicking the editor moves focus to a different window. Editor-owned keys remain with Windows. The monitor is removed when the editor closes. Explicit frame drawing also preserves source behavior for hosts that do not call AppKit updateWindows; the installed baseline already repainted with proper macOS window access, so this is not counted as a newly reproduced user-facing repaint fix.

On Apple Silicon, a Rosetta host now invokes the native Hub authorization helper. The existing helper's arm64 invocation returned 0 while its x86_64 invocation returned 77 for the same saved login. The corrected Intel manager and VST3 audio test passed. This preserves online authorization; it does not remove Keychain checks.

## Passed

- Universal manager, plugin and editor builds; strict bundle signature verification.
- Five native import/render/invalid-binary tests.
- Preset metadata decoding, including Windows CRLF, blank names, UTF-8, Windows-1252, multiline names and malformed records.
- VST3 list/name/count, event-bus unit association, selection normalization and legacy state on arm64 and Rosetta.
- Real app imports of SQ8L, NoiseMaker and U-NO-LX into an isolated profile and then the local managed installation.
- SQ8L program selection through the VST3 parameter, audible-level output and state round trip on arm64 and Rosetta.
- Realtime NoiseMaker and U-NO-LX audio in the native test host; note-release tails near silence.
- Cross-window keyboard routing regression: installed baseline failed; candidate passed on both architectures, including editor-owned keys and monitor cleanup.
- 400 editor attachments/removals on each architecture.
- Six NoiseMaker popup selection/dismissal/audio checks; original SQ8L and NoiseMaker knob pixels changed with other artwork preserved.
- Actual FL Studio 24.2.2 scan: all three instruments show **ok**, VST3, Apple + Intel, Synth. Fresh SQ8L and NoiseMaker instances opened their original editors.

## Still required before release

- Actual FL preset-menu selection and immediate sound/display update.
- Actual FL Mac typing-key note, Volume adjustment and key release.
- Saved-project recall after the refreshed scan; the preserved earlier project initially opened missing-wrapper placeholders.
- Brandon's freeze scenario is not reproduced. A thread sample after closing the editor showed FL's normal event loop, not a BridgeLab lock.
- Physical Intel hardware and other macOS/FL versions remain untested.

The local managed app is installed; its previous copy is preserved at the path in `test-results/live-install-backup.txt`. Original DLLs and projects were not removed. No release has been published. Evidence is recorded in `test-results/verification.json` and `test-results/fl-scan.png`.

Run `sh tools/check-preset-catalog.sh` after building to repeat the metadata checks on Apple Silicon with Rosetta. The actual FL observations are recorded separately; automated checks do not replace them.
