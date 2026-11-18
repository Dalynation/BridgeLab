# BridgeLab 0.9.4 — friend-test preview

BridgeLab imports Windows VST2 instrument DLLs and creates a separate Mac VST3 instrument for each DLL. It runs the original Windows code through bundled Wine; it does not convert DLLs into native Mac code.

[Download the 0.9.4 preview and follow its setup steps](https://github.com/Dalynation/BridgeLab/releases/tag/v0.9.4-preview).

## 0.9.5 development candidate

The source includes queue-overload MIDI recovery, first-note DSP initialization,
a shared Wine profile for new imports, and a single-surface Direct2D editor
bridge. The original plugin draws its own interface and receives mouse input;
BridgeLab does not replace its artwork or knobs. Slow editor opening is queued
without blocking shared-memory audio. MIDI recovery explicitly releases keys
without duplicating held voices, and both pipe workers use verified realtime
scheduling. Original software-rendered popup windows are discovered and forwarded
with their input automatically; the engine contains no plugin-name branches.
Input delivery, frame publication and unsupported popup modes are observed in
local health reports available through **Check runtime**. These observations
do not certify control semantics. Generic MIDI/audio observations also distinguish quiet held-note
output, missing note input, stale processing, invalid samples and excessive
levels. Keep a pattern playing and use **Check runtime** for per-instrument
guidance; the bridge preserves the patch. See [QA-CONTROLS-0.9.5.md](QA-CONTROLS-0.9.5.md).
Live repaint compatibility also covers the older DXGI factory
entry points used by software-rendered editors. Automated audio, recovery and
multi-instance checks pass; final
manual FL project acceptance remains pending. See [QA-0.9.5.md](QA-0.9.5.md) for the
measured results, cold editor delays, retained stress failures and remaining
release checks. This candidate is not a universal-compatibility certification;
the public download above remains 0.9.4.

## Try it

1. Extract `BridgeLab-0.9.4-mac-preview.zip` and move the complete BridgeLab.app to Applications or your Desktop. This preview is ad-hoc signed and not notarized; a second Mac may require macOS approval before opening. Apple Silicon needs Rosetta for the Windows runtime.
2. Open BridgeLab and import an extracted Windows DLL or its extracted folder. Folder selection also permits copying adjacent presets and resources. Multiple DLLs remain in the library. Finder Copy → Paste DLLs is also supported. Drag/drop is implemented; the final clean-start run used the native picker and clipboard paths.
3. In FL Studio, open Options → Manage plugins → Find installed plugins. Each import gets a separate entry, such as **BridgeLab - SQ8L (x86)**. Add it from the Channel Rack as a generator/instrument. It is not a mixer effect.
4. Open that channel to use the original plugin editor through the Mac bridge view. Save, reopen, and export a small test project before relying on a new plugin.

No development checkout or Python is required. Original plugin DLLs are not included in the archive. Installed wrappers live in `~/Library/Audio/Plug-Ins/VST3/BridgeLab-*.vst3`; managed copies, runtimes, settings, and logs live in `~/Library/Application Support/BridgeLab`.

## 0.9.4 changes

Realtime audio now uses a bounded shared-memory queue between the Mac VST3 wrapper and the Windows DSP worker, avoiding pipe scheduling on the audio path. A bundled native helper gives only that worker a macOS realtime scheduling policy; UI and control threads retain their normal scheduling. If shared transport cannot initialize, the bridge retries the legacy pipe path. Offline rendering continues through pipes; the pipe worker can pipeline up to eight requests and handles partial I/O.

Windows runners receive EOF and can finish normal plugin/editor cleanup before bounded forced termination. The approximately 43 ms minimum scheduling allowance scales with sample rate: 2,048 samples at 48 kHz, 4,096 at 96 kHz and 8,192 at 192 kHz, including under Rosetta. Larger callbacks can require more. Device latency is additional.
The current Core Audio suite passed 44/44 exact audio comparisons. Both original editors accepted knob edits during playback, and four simultaneous native/Rosetta clients produced exact audio without bridge underruns. The broader timer-host suite passed 53/54 checks; one silent-block mismatch remains unconfirmed despite 12 passing follow-ups. See the QA report before relying on this preview.

Some earlier stress results ran with stale Wine helpers left across short tool-driven test sessions. A controlled old/new comparison kept helper counts stable in both versions. We do **not** claim a reproduced production Wine-memory leak was fixed. The test harness now explicitly closes its disposable Wine profiles at the end of each session. See [QA-0.9.4.md](QA-0.9.4.md) for evidence and limits.

To upgrade, save and quit FL Studio and BridgeLab, replace the complete BridgeLab.app, then reimport each original DLL/folder so its VST3 wrapper is rebuilt. Restarting the Mac once is recommended if old preview/test runtimes are still running. Rescan with **Rescan previously verified plugins** enabled. Existing per-DLL IDs remain stable.

The 0.9.3 MIDI, prefetch, editor-close and folder-permission fixes remain included. Active instruments still use one isolated Windows runner each; runtime files and per-architecture Wine profiles are shared.

## Earlier FL baseline (0.9.1)

On Apple Silicon, macOS 15.6.1, FL Studio 24.2.2.4259:

- Clean reinstall and fresh GUI imports of SQ8L 0.91b x86 and TAL-NoiseMaker x64 completed. Both registered as separate verified VST3 synths in FL Studio.
- Both original editors displayed inside FL and accepted mouse input. The TAL blank editor and an editor-attachment crash were fixed. The packaged shared UI passed 400 attach/teardown cycles per Mac CPU architecture.
- A saved SQ8L FL project reopened after clean reinstall. Its final 0.9.1 WAV export matched the original PCM sample for sample: stereo, 44.1 kHz, 81,415 frames.
- Installed native host tests passed on arm64 and x86_64 under Rosetta with the development tree temporarily absent. Both plugins produced audio; live and offline PCM matched exactly. Three simultaneous instances (two SQ8L plus TAL) kept independent state and recreated identical audio, including relocation of another user's saved DLL path.
- Both wrappers passed Steinberg validation: 47 passed, zero failed each. Packaged WAV rendering, duplicate import, invalid DLL rejection, and both editor state-recreation checks passed.

See the baseline reports in `test-results/` and the current findings in [QA-0.9.4.md](QA-0.9.4.md). FL multi-instrument audio comparison was not completed; the simultaneous-instance evidence comes from the native host harness.

## Compatibility limits

The compatibility target is macOS 11 and later, on Apple Silicon and Intel, through the standard VST3 interface used by FL Studio. The target is broader than the tested coverage:

| Combination | Evidence |
| --- | --- |
| Apple Silicon, macOS 15.6.1 | Current audio, state and editor regression host tests |
| Intel host process under Rosetta on that Mac | Current host tests; not physical Intel hardware |
| FL Studio 24.2.2.4259 on that Mac | Earlier 0.9.1 end-to-end baseline below |
| Physical Intel, macOS 11–14 / other macOS releases, FL Studio 20 / 21 / 2025 / 2026 | Not yet certified by end-to-end tests |
| Brandon’s macOS 15.1 / FL Studio 26.1 | Requires confirmation on his computer |

Universal arm64/x86_64 builds and a macOS 11 deployment target help reach this range; they do not prove every combination works. This remains a controlled friend-test preview.
Supported scope is Windows x86/x64 **VST2 instruments**. Windows VST3, audio-input effects, arbitrary installers, copy protection, external sample dependencies, GPU editors, popup menus, and file dialogs are not generally supported or verified. Dialogs may block synchronous controls. Notes and velocity work; MIDI CC, pitch bend, aftertouch, SysEx, transport/tempo, and general parameter automation are not implemented. Bridge latency has a 2048-sample minimum; higher sample rates and larger callbacks receive more scheduling headroom. Device latency is additional.

## Development and backups

Build with `tools/build-runners.sh`, `tools/build-plugin.sh`, then `tools/build.sh`. The latter includes importer tests and packaged editor-attachment regression tests. Native installed commands include `doctor`, `inspect`, `import`, and `render`. The release matrix and installed editor protocol tests live under `tests/`.

Local backups, Wine installations, original DLLs, and user projects are excluded from the source repository. The downloadable application is provided separately as a release asset. Build scripts currently assume the original workspace dependency layout.
