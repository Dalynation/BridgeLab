# BridgeLab 0.10.4 preview

BridgeLab imports supported Windows VST2 instrument DLLs and creates a separate Mac VST3 instrument for each DLL. The original Windows code runs through bundled Wine and draws its own interface. Multiple imported instruments retain their own identity and per-instance state.

[Download and release notes](https://github.com/Dalynation/BridgeLab/releases/tag/v0.10.4-preview). Use Zone Six Studio Hub to sign in, install BridgeLab, and check for updates. This package requires the Hub and an internet connection to verify access.

## Update and test

1. Save your project and quit FL Studio and BridgeLab.
2. In Zone Six Studio Hub, click **Check for updates**, then **Update BridgeLab**.
3. Open BridgeLab once to install its updated engine. To refresh an older imported wrapper, import the same DLL again. Folder imports can copy adjacent presets/resources, so use the copy containing the presets you want to retain.
4. In FL Studio, rescan if needed and add the instrument from the Channel Rack. Each import appears under its own BridgeLab name.
5. Check named presets, original controls, Mac typing-keyboard notes and key release. Save/reopen a small test project before relying on a new plugin.

No source checkout or Python is required to use the download. Vendor DLLs and licenses are not included. Apple Silicon needs Rosetta for the Windows runtime. This preview is ad-hoc signed, not notarized; macOS may require approval.

## Installation package correction

0.10.3 was rejected by the Hub because its ZIP included an extra macOS metadata tree. 0.10.4 fixes that packaging error. The release command now requires a full install through the current Hub installer in a disposable home before producing the final ZIP. See [QA-0.10.4.md](QA-0.10.4.md).

## Changes

- Original named presets are exposed in the FL wrapper.
- Printable piano keys return to the owning host after non-text editor controls take focus. Text entry stays in the Windows editor; key releases and repeats retain their original owner.
- Standard Windows menu input goes through its original menu-tracking loop, including submenu selection, Escape and outside dismissal.
- Virtual capture transfers notify the losing control, avoiding a knob remaining in drag mode after a value menu closes.
- Original file dialogs retain their focused filename field. Oversized native menus can be dismissed safely even when their full artwork cannot fit the editor canvas.
- The published source includes the Hub login integration already used by the installed 0.10.x builds, including native Hub authorization under Rosetta.

## Coverage and remaining limits

See [QA-0.10.3.md](QA-0.10.3.md) for measured checks and retained failures. The user physically confirmed the reported SQ8L WRITE/typing-keyboard case in FL; automated integration checks cover further SQ8L settings, NoiseMaker dropdown round trips, and native keyboard/repaint behavior on both CPU slices.

TAL-U-NO-LX volume/audio/state checks pass, but its full GPU interface is **not certified**: some dropdown/context-menu checks fail and partial black repaint was observed. SQ8L's oversized page context menu is diagnosed and can be escaped, but is not fully displayed. Hardware SEND/REQ and external synth communication are untested.

Supported scope is Windows x86/x64 **VST2 instruments**. This is not support for every Windows DLL. Windows VST3, arbitrary installers, audio-input effects, external dependencies, copy protection, all custom GPU surfaces, and all custom text widgets are not generally verified. MIDI CC, pitch bend, aftertouch, SysEx, transport/tempo, and general parameter automation remain outside the verified scope.

The Mac binaries contain arm64 and x86_64 slices with a macOS 11 deployment target. Intel processes are tested under Rosetta, not on physical Intel hardware. This does not certify every Mac, macOS release or FL version; Brandon's separate machine still needs acceptance testing. Shared audio scheduling has a 2048-sample minimum at 48 kHz and scales with higher rates/callback sizes; device latency is additional.

Managed wrappers live in `~/Library/Audio/Plug-Ins/VST3/BridgeLab-*.vst3`. Managed DLL copies, runtime, settings, local diagnostics and engine backups live in `~/Library/Application Support/BridgeLab`. The app does not change permissions on unrelated plugins.

## Development

Build with `tools/build-runners.sh`, `tools/build-plugin.sh`, then `tools/build.sh`. Build scripts currently assume the original workspace dependency layout (LLVM-MinGW, Steinberg VST3 SDK and the bundled Wine runtime). The last command runs native attachment, MIDI/state/import/permissions, transport and diagnostic checks. Package releases with `python3 tools/package-release.py --hub-source ../StudioHub`; this requires the current Hub checkout and exercises its complete installer before producing a release ZIP and checksum.

Vendor-plugin tests require a disposable `BRIDGELAB_HOME` with a `.bridgelab-qa` marker and explicitly supplied vendor files. They do not certify vendor controls simply because pixels are visible. Local runtimes, DLLs, projects, backups and generated test artifacts are excluded from this repository.

Historical reports: [0.10.2 keyboard acceptance](QA-DIALOG-KEYBOARD-0.10.2.md), [0.10.1 preset checks](QA-PRESET-KEYBOARD-0.10.1.md), [0.9.5](QA-0.9.5.md), [0.9.4](QA-0.9.4.md).
