## 0.9.4: realtime audio transport and DSP scheduling

This preview moves realtime audio into a bounded shared-memory queue and schedules the Windows DSP worker through a bundled native helper. Original editors retain the existing runtime. The bridge retries the legacy pipe path if shared transport cannot start; offline rendering still uses pipes. Runners get time for normal cleanup, with bounded termination for hung plugins.

The scheduling allowance scales with sample rate. At 48 kHz the minimum remains 2,048 samples (about 43 ms), including under Rosetta; device latency is additional.

**Upgrading:** Save and quit FL Studio and BridgeLab. Replace the complete app, then **reimport every original DLL/folder** before rescanning. Installing just the new app does not update existing plugin wrappers. Turn on **Verify plugins** and **Rescan previously verified plugins** for the scan. Restarting your Mac once is recommended if older preview/test runtimes remain running.

**Test correction:** earlier stress runs were affected by stale Wine helpers left across short tool-driven test sessions. A controlled old/new comparison kept helper counts stable in both versions. We do not claim a reproduced production memory leak was fixed. See the QA report for the corrected evidence.

**Validation:** 44/44 Core Audio/offline comparisons passed for SQ8L and TAL-NoiseMaker in native and Rosetta hosts. Knob edits during device playback and four simultaneous audio clients passed with zero bridge underruns. One timer-host block mismatch remains an unresolved finding despite 12 passing follow-ups. This is a test preview, not a promise of flawless playback or every Mac/FL version. [Read the QA results](https://github.com/Dalynation/BridgeLab/blob/main/QA-0.9.4.md).

## Start here: download the Mac app

1. Click [**Download BridgeLab for Mac**](https://github.com/Dalynation/BridgeLab/releases/download/v0.9.4-preview/BridgeLab-0.9.4-mac-preview.zip).
2. In Finder → Downloads, double-click **BridgeLab-0.9.4-mac-preview.zip** to extract it. Download the named app ZIP, not GitHub’s “Source code” archives.
3. Move **BridgeLab.app** to your **Applications** folder or Desktop. Keep the complete app together.
4. Double-click **BridgeLab.app**. Its main window should open. On Apple Silicon (M-series), install Rosetta if macOS prompts for it during setup or first import.

**If macOS blocks opening:** this experimental preview is ad-hoc signed and has not been notarized by Apple. If you trust this preview, attempt to open it, then go to **System Settings → Privacy & Security → Open Anyway** for BridgeLab, and follow the confirmation. On macOS 11/12, use **System Preferences → Security & Privacy → General → Open Anyway** instead. See [Apple’s opening-app instructions](https://support.apple.com/en-us/102445). If macOS reports the app is damaged, stop and send us the exact message.

## Import SQ8L (or another supported instrument)

1. Obtain the Windows plugin from its developer or an authorized download. For SQ8L, start at its [product page](https://www.kvraudio.com/product/sq8l-by-siegfried-kullmann). Plugin DLLs are not included with BridgeLab.
2. Extract the plugin’s ZIP in Finder. Keep the DLL and any adjacent preset/resource files together in the extracted folder.
3. In BridgeLab, click **Import Windows DLL…**. For SQ8L, select the extracted folder containing **SQ8L.dll**. Folder selection allows BridgeLab to copy adjacent resources too. You can also select a DLL directly.
4. Wait for **Imported: SQ8L.dll** and the plugin’s library entry. First use prepares the bundled Windows runtime and can take a minute or more. Wait until import finishes before scanning FL Studio.
5. Optional: select the imported plugin and click **Render test note…** to save a test WAV. Open that WAV to hear it. **Check runtime** shows setup information if import fails.

You can import multiple instruments. Repeat these steps for each extracted DLL/folder; each gets its own FL Studio entry. Finder **Copy** followed by BridgeLab’s **Paste DLLs** is another import option.

## Find it in FL Studio

1. Open FL Studio. Open **Options → Manage plugins**; if your version places it elsewhere, use **Options → File settings → Manage plugins**.
2. Enable **Verify plugins**, then click **Find installed plugins**. Let the scan finish. If retrying a failed scan, enable **Rescan plugins with errors** first.
3. Search the results for **BridgeLab**. SQ8L should appear as **BridgeLab - SQ8L (x86)** with status **ok**, format **VST3**, and type **Synth**.
4. Close Plugin Manager. Open the **Channel Rack**, click **+ → More plugins…**, search **BridgeLab**, and double-click **BridgeLab - SQ8L (x86)**. You can also use **Add → More plugins…**.
5. Click the new channel to open its editor. The original **SQ8L interface should appear inside FL Studio’s plugin window**. Play notes with MIDI or add notes in the Piano roll and press Play.

These are instrument/generator entries: add them to the **Channel Rack**. They do not belong in a Mixer effect slot. See [Image-Line’s plugin installation guide](https://www.image-line.com/fl-studio-learning-content/fl-studio-online-manual/html/basics_externalplugins.htm).

## Quick checks and troubleshooting

- **Nothing in FL?** Confirm BridgeLab says the import succeeded. Scan again after it finishes. The wrapper location is **~/Library/Audio/Plug-Ins/VST3/** (your user Library). In Finder, press **Command–Shift–G** and enter that path to check for a BridgeLab wrapper.
- **Not in the short Add menu?** Use **More plugins…** and search for BridgeLab. Newly scanned plugins may not be favorites yet.
- **Status “error,” blank editor, crash, or no audio?** Record the exact plugin name, Mac chip, macOS version, FL Studio version, and any error message. Include a screenshot and report it in [GitHub Issues](https://github.com/Dalynation/BridgeLab/issues). Compatibility is checked per plugin and host.
- **Before using an important project:** play notes, change a preset/control, save a small FL project, close and reopen it, and export a short WAV to confirm sound and settings return.


## Tested scope and limits

See the [0.9.4 QA report](https://github.com/Dalynation/BridgeLab/blob/main/QA-0.9.4.md) for exact results. Tests use SQ8L x86 and Windows TAL-NoiseMaker x64 on Apple Silicon/macOS 15.6.1 with arm64 and Intel-under-Rosetta host processes. Physical Intel hardware, other macOS/FL versions, low-RAM Macs and the exact unidentified TAL variant remain unverified. The earlier FL Studio 24 baseline does not establish FL Studio 26.1 compatibility.

Our compatibility target is macOS 11 and later on Apple Silicon and Intel, using FL Studio’s standard VST3 support. Universal builds and the macOS 11 deployment target are engineering targets, not certification of every version. Windows x86/x64 VST2 instruments are the supported scope. Arbitrary Windows VST3 plugins, effects, installers, copy protection, GPU editors and all plugin dialogs are not generally supported. There is still one Windows runner per active instrument. At 48 kHz, minimum bridge latency is 2,048 samples (about 43 ms), including under Rosetta. Higher rates and larger callbacks can increase the sample count; device latency is additional. Initial loading and heavy system load can still interrupt audio.

MIDI CC, pitch bend, aftertouch, SysEx and general parameter automation are not implemented. This preview is ad-hoc signed and not notarized. Test your exact plugin/host combination on a small project before relying on it.
