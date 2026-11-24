# BridgeLab 0.10.2 candidate

The user confirmed that FL's named SQ8L presets change the display and sound immediately in 0.10.1. The remaining report concerns Mac typing-keyboard notes after clicking WRITE or changing controls: moving the window restores the ability to play.

The native canvas previously consumed every new key press after taking focus. The candidate publishes Windows text/dialog keyboard ownership through a separate four-byte shared-memory field, routes ordinary typing keys to the remembered host window, and keeps text entry and dialog navigation in the editor. Note releases and repeats retain the original owner. Editors opened before any host note preserve their owning/main window or use the normal responder chain. The Windows popup router now retains the focused child control instead of sending every key to the popup parent.

## Evidence

- `test-results/sq8l-dialogs-fixed/results.json`: 32 checks pass. WRITE opening, COMPARE, list clicks and arrow selection, Cancel, text focus and Return, all 21 page buttons, continuing MIDI audio, and a real name edit followed by WRITE. A fresh runner reports `VOYAGERXY` in test slot 3. The disposable library is backed up and restored, including on failure.
- `test-results/sq8l-controls-0.10.2/results.json`: 10 checks pass. Knob and preset changes, MONO, SYNC, representative page selection, bank cycling, and FILE menu opening. Page checks certify visual navigation; menu opening does not certify all menu actions.
- `test-results/popups-0.10.2/results.json`: all six NoiseMaker dropdown round trips pass selection, state change, Escape, outside dismissal and audio continuity.
- `test-results/popup-contract-{i686,x86_64}-0.10.2.log`: both Windows architectures pass focused-child list navigation, coordinate mapping, thread isolation, oversized-popup detection and cleanup.
- `test-results/editor-keyboard-{arm64,x86_64}-0.10.2.log`: both native slices pass first-note ownership without prior keyboard input, subsequent host notes after a click, text-owned keys, cross-window releases, repeat ownership across focus changes, live repaint and monitor removal.
- Full `tools/build.sh` passes: 400 editor attachments per native slice, MIDI recovery, state compatibility, five import/render tests, permission recovery, shutdown/pipeline/shared transports, health parsing and audio diagnostics. Static deployment checks target macOS 11 and both slices; they do not certify physical Intel hardware or every macOS/FL release.

## Local installation and FL acceptance

The local app, engine and three imported wrappers are version 0.10.2. The native editor library was refreshed once more after the startup-path regression was added. `test-results/live-install-focus-backup.txt` identifies the previous-app backup. Existing preset sidecars were preserved during reimport.

The current project was saved separately as `~/Documents/Image-Line/FL Studio/Projects/BridgeLab-before-dialog-focus-20261124/BridgeLab-before-dialog-focus-20261124.flp` and reloaded in FL. It contains the SQ8L channel named SLAP HOLD.

On November 24, 2026, the user confirmed that opening WRITE, changing its selection, then playing a physical Mac typing-keyboard note works immediately without moving the window. This passes the reported FL reproduction case. Automated key presses did not provide a calibrated positive MIDI result; the FL acceptance is based on the user's physical-key observation. FL's typing-to-piano setting was toggled once for diagnosis and toggled back to its original state.

This is not complete certification of every interactable element. Nested OPTIONS actions, external file dialogs, INIT/PANIC, hardware SEND/REQ, all custom-drawn text widgets, and the other plugins' entire surfaces still need separate acceptance coverage. No claim of universal DLL compatibility or absence of regressions is made.

The candidate has not been published. The user requested that the next published update be installed manually from the Hub; leave that installation to the user.
