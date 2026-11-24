# BridgeLab 0.10.3 verification — November 24, 2026

This preview publishes the current Hub-integrated source and the shared original-editor input fixes. It is not a universal compatibility or zero-regression certification. The installed 0.10.2 app is deliberately left in place so the user can test the next update manually through the Hub.

## Passing evidence

- The user's physical FL check on 0.10.2 passed: open SQ8L WRITE, change the selection, then play a Mac typing-keyboard note without moving the window; sound plays immediately. The same native keyboard implementation is included in 0.10.3.
- The first 32 SQ8L checks passed in `test-results/sq8l-controls-0.10.3-r3/results.json` before the additional oversized-menu coverage exposed its limitation. These cover WRITE, COMPARE, list click/arrow navigation, text focus/Return, 21 page buttons, audible MIDI and a real edited-name WRITE verified by a fresh runner.
- `test-results/release-0.10.3/sq8l-extended/results.json`: 21 passing observations and one explicitly retained display limitation. Passing checks include value-menu selection and Escape, no residual knob drag on the next click, INIT, PANIC with zero residual audio and a new audible note, About click-dismissal, original library open/save/reload, four nested emulation settings, voice stealing and GUI checkboxes. Saved INI values are verified. Escape recovers from the oversized page context menu; closing with a native menu open releases all editor/frame resources. These tests back up and restore only a disposable plugin library.
- `test-results/release-0.10.3/sq8l-matrix/results.json`: 11/11 representative controls pass, including AM, MONO, SYNC, bank cycling, preset/knob changes and page navigation. FILE coverage here certifies opening; the extended suite tests real file actions.
- `test-results/release-0.10.3/noisemaker-popups/results.json`: all six dropdown round trips pass selection, state change, Escape, outside dismissal and audio continuity.
- `test-results/cross-plugin-pixels/results.json`: SQ8L and NoiseMaker control state changes and original-artwork preservation passed before the final oversized-menu fallback. That fallback does not change the image-capture implementation.
- `test-results/cross-plugin-artwork/original-artwork/results.json`: TAL-U-NO-LX volume input changes actual audio/state, preserves the unedited original logo, releases graphics resources and recalls normalized state in a fresh runner. This limited check does not certify every control's immediate repaint.
- Both Windows runner architectures pass native menu selection, submenu placement/z-order, Escape, outside dismissal, oversized-menu Escape, focused-child popup routing and capture-change notifications. Results: `test-results/release-0.10.3/checks.json` and the adjacent per-test logs.
- The universal Mac build passed `tools/build.sh`: 400 editor attachments per CPU slice, MIDI recovery, plugin-state compatibility, import/render and permission tests, shutdown/pipeline/shared transports, health parsing and audio diagnostics.
- The packaged editor separately passed keyboard/repaint checks on arm64 and x86_64: first key without prior host input, fresh typing notes after clicks, matching key release, text-owned keys, repeats across focus changes, repaint and monitor cleanup.
- Archive integrity, deep code-signature verification, universal slices, app/engine version 0.10.3, Hub authentication metadata and exact packaged-runner bytes were checked. These checks do not install the release on the user's machine.

## Retained limitations and failures

- SQ8L's large page context menu exceeds the fixed editor canvas. It is diagnosed and Escape recovers, but full pixels/selection are not certified. The report retains `passed: false` for this display limitation.
- TAL-U-NO-LX's extra 12-control audit passed 8 controls and failed 4 observations: immediate master-slider repaint, LFO dropdown, voice-mode dropdown and filter context menu. A partially black frame occurred during the audit. The exact installed 0.10.2 runner was also tested: master repaint, LFO dropdown and context-menu failures reproduced, while voice mode passed in that run. The differing voice-mode result is unresolved; it is not evidence of no regressions. See `test-results/cross-plugin-u-no-lx-controls-4/results.json` and `test-results/release-0.10.3/u-no-lx-baseline/results.json`. The matrix now exits nonzero on failed observations. Vendor trial dismissal waits for a visible redraw, and accepts the observed 1195-pixel fixture as well as the older 1198-pixel width.
- Hardware SEND/REQ, arbitrary custom text widgets, all GPU/popout implementations, physical Intel hardware, every macOS version and every FL version are not certified.
- The final 0.10.3 Hub installation and physical FL acceptance are intentionally reserved for the user's manual update test. Static architecture/deployment checks and software integration tests do not replace that acceptance.

No vendor DLLs, licenses, projects, runtime prefixes or user logs are committed to the source repository. Local evidence paths are retained for this workspace; they are not shipped as source assets.
