# BridgeLab 0.9.3 — friend-test preview

BridgeLab imports Windows VST2 instrument DLLs and creates a separate Mac VST3 instrument for each DLL. It runs the original Windows code through bundled Wine; it does not convert DLLs into native Mac code.

[Download the 0.9.3 preview and follow its setup steps](https://github.com/Dalynation/BridgeLab/releases/tag/v0.9.3-preview).

## Try it

1. Extract `BridgeLab-0.9.3-mac-preview.zip` and move the complete BridgeLab.app to Applications or your Desktop. This preview is ad-hoc signed and not notarized; a second Mac may require macOS approval before opening. Apple Silicon needs Rosetta for the Windows runtime.
2. Open BridgeLab and import an extracted Windows DLL or its extracted folder. Folder selection also permits copying adjacent presets and resources. Multiple DLLs remain in the library. Finder Copy → Paste DLLs is also supported. Drag/drop is implemented; the final clean-start run used the native picker and clipboard paths.
3. In FL Studio, open Options → Manage plugins → Find installed plugins. Each import gets a separate entry, such as **BridgeLab - SQ8L (x86)**. Add it from the Channel Rack as a generator/instrument. It is not a mixer effect.
4. Open that channel to use the original plugin editor through the Mac bridge view. Save, reopen, and export a small test project before relying on a new plugin.

No development checkout or Python is required. Original plugin DLLs are not included in the archive. Installed wrappers live in `~/Library/Audio/Plug-Ins/VST3/BridgeLab-*.vst3`; managed copies, runtimes, settings, and logs live in `~/Library/Application Support/BridgeLab`.

## 0.9.3 changes

Repeated activation preserves the current runner and held voices. MIDI notes received without an audio buffer are retained for the next audio callback. Prefetch processing waits for ordered worker completion rather than overflowing the realtime queue. Closing the editor now closes the Windows plugin UI, releases its frame mapping and stops idle callbacks. Active instances still use one isolated Windows runner each; the bundled runtime and per-architecture Wine profiles are shared. Lower-RAM machines and Brandon’s exact TAL variant remain unverified.

To upgrade, close FL Studio, replace BridgeLab.app, then reimport each original DLL/folder so its VST3 wrapper is rebuilt. Rescan updated wrappers in FL Studio with **Rescan previously verified plugins** enabled. Existing per-DLL IDs remain stable.

## Earlier FL baseline (0.9.1)

On Apple Silicon, macOS 15.6.1, FL Studio 24.2.2.4259:

- Clean reinstall and fresh GUI imports of SQ8L 0.91b x86 and TAL-NoiseMaker x64 completed. Both registered as separate verified VST3 synths in FL Studio.
- Both original editors displayed inside FL and accepted mouse input. The TAL blank editor and an editor-attachment crash were fixed. The packaged shared UI passed 400 attach/teardown cycles per Mac CPU architecture.
- A saved SQ8L FL project reopened after clean reinstall. Its final 0.9.1 WAV export matched the original PCM sample for sample: stereo, 44.1 kHz, 81,415 frames.
- Installed native host tests passed on arm64 and x86_64 under Rosetta with the development tree temporarily absent. Both plugins produced audio; live and offline PCM matched exactly. Three simultaneous instances (two SQ8L plus TAL) kept independent state and recreated identical audio, including relocation of another user's saved DLL path.
- Both wrappers passed Steinberg validation: 47 passed, zero failed each. Packaged WAV rendering, duplicate import, invalid DLL rejection, and both editor state-recreation checks passed.

See the baseline reports in `test-results/` and the current findings in [QA-0.9.3.md](QA-0.9.3.md). FL multi-instrument audio comparison was not completed; the simultaneous-instance evidence comes from the native host harness.

## Compatibility limits

This is ready for a controlled friend test, not a guarantee for all Macs, FL versions, or plugins. Physical Intel Macs, other macOS releases, FL Studio 20 end-to-end behavior, other FL versions, and installation on a clean second computer remain unverified. Universal arm64/x86_64 builds target macOS 11; that target is not a tested support promise.

Supported scope is Windows x86/x64 **VST2 instruments**. Windows VST3, audio-input effects, arbitrary installers, copy protection, external sample dependencies, GPU editors, popup menus, and file dialogs are not generally supported or verified. Dialogs may block synchronous controls. Notes and velocity work; MIDI CC, pitch bend, aftertouch, SysEx, transport/tempo, and general parameter automation are not implemented. Bridge latency has a 2048-sample minimum and increases for larger host callback sizes; device latency is additional.

## Development and backups

Build with `tools/build-runners.sh`, `tools/build-plugin.sh`, then `tools/build.sh`. The latter includes importer tests and packaged editor-attachment regression tests. Native installed commands include `doctor`, `inspect`, `import`, and `render`. The release matrix and installed editor protocol tests live under `tests/`.

Local backups, Wine installations, original DLLs, and user projects are excluded from the source repository. The downloadable application is provided separately as a release asset. Build scripts currently assume the original workspace dependency layout.
