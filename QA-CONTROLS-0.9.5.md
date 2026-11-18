# Original plugin control audit — 0.9.5 candidate

Baseline before shared popup repair: **incomplete; release acceptance failed**. Tested on November 18, 2026.

This audit exercises actual original Windows plugin controls in disposable Wine profiles. It records original pixels and serialized state before/after input. It is not a claim that every control, DLL, Mac, or FL version works.

| Original UI | Confirmed responses | Failed or unverified |
|---|---|---|
| NoiseMaker 5.0.6 | Volume drag, AMP attack slider, envelope point, oscillator sync, chorus I, one-shot, LFO trigger, preset-next; MIDI-learn/lock visual response | Seven dropdowns and knob context menu show no menu. Wheel and double-click probes unverified. Waveform click and text entry not certified. |
| SQ8L 0.91b | Parameter knob, preset-next, mono/sync toggles; oscillator, LFO, envelope and matrix page redraw | File menu not forwarded; bank selector menu behavior not verified. |
| U-NO-LX 5.1.5 demo | No new broad pass claimed | Trial notice and saved expanded-keyboard layout interfered. Initial results excluded. Checked Cancel does not establish a clean fixture, so current audit stops. |

NoiseMaker: 22 probes, 10 meet the recorded response criterion; SQ8L: 10 probes, 8 meet it. Visual-only results do not prove complete feature behavior. Raw per-control observations are in [control-audit-0.9.5.json](test-results/control-audit-0.9.5.json).

Dropdown diagnostics found separate original Windows popup windows placed outside the forwarded editor. Experimental message-position and virtual-monitor changes were insufficient and were reverted; no popup fix is shipped.

The Mac typing-keyboard stuck note still fails the actual FL retest. A second source candidate routes releases through the host window and passes isolated tests, and is installed, but is not FL verified.

Acceptance must cover menu opening, hovering, selection, outside-click/Escape dismissal, reopening, submenus, correct parameter/audio changes, state recall, and uninterrupted playback. Text fields must support typing/editing/commit/cancel; preset file dialogs and full MIDI learn/lock behavior remain unverified. Do not treat these controls as working merely because the main editor appears.

Local screenshots remain under `work/graphics-bridge/control-audit/`. Earlier U-NO-LX attempts are retained as excluded diagnostic evidence. Original DLLs, unrelated plugins, and user projects were not changed by this audit.

## Shared popup repair candidate

The newer source automatically discovers same-thread original popup windows,
forwards their original GDI pixels, translates hit testing/input and virtual
monitor coordinates, and keeps their backing windows offscreen. Production
code uses no vendor or control-name branches. The six NoiseMaker dropdowns
FILTER, VOICES, LFO destination, AMP destination, portamento and modulation
passed opening, real state-changing selection, reopen, Escape dismissal,
outside-click dismissal and continuous audio. Health records verify original
popup capture, input delivery and offscreen placement.

Regression checks passed: popup ownership/mapping/limits on both Windows
architectures, nine Core Audio/editor/concurrent-client checks, and three modern
TAL original-artwork/audio/edited-state-recall checks in a clean GUI profile.
The expanded NoiseMaker audit also observed restored wheel adjustment and
double-click reset; SQ8L's File menu now appears. Nested menu selection,
context-menu semantics, text entry and arbitrary file dialogs are not certified.
Earlier U-NO-LX control-matrix attempts remain excluded because of fixture
layout/trial-notice interference; the clean-profile modern artwork test passed.

General input delivery and frame publication are recorded for all bridged editor
events. Check runtime reads live local health observations and reports rejected
input, stalled status/frame publication, and unsupported GPU or size/window
limits. Published buffers can still contain unchanged artwork, and delivered
events can still be ignored by a control; these observations are not proof of
intended behavior. Behavioral fixtures and manual host tests remain necessary.

The user confirmed the repaired filter dropdown works in actual FL. The Mac
typing-keyboard note-release retest remains pending for this new candidate. The prior installed keyboard correction failed. No universal
compatibility, zero-regression, or completed release claim is made.

See [popup-repair-0.9.5.json](test-results/popup-repair-0.9.5.json) for scoped results.

## Filter-silence investigation

The user's current FL settings were captured in a separate QA checkpoint.
NoiseMaker was set to HP 12dB with normalized cutoff 1.0 and volume 0.412.
Rendering that exact patch at 44.1 kHz measured RMS 0.000910, compared with
0.16135 for the same patch changed only to LP 24dB: approximately 45 dB lower.
Keeping HP 12dB and lowering cutoff to 0.25 restored RMS to 0.06159.

All twelve original filter selections were then exercised through their GUI.
Each changed only the filtertype field; held and fresh notes continued through
the audio transport, and returning to LP 24dB recovered sound without reloading.
An immediate release measurement slightly exceeded an initially over-strict
1e-6 threshold; the settled tail was 1.324e-7, matching the measured idle floor.
The test retains the initial failed measurement and now checks the settled
tail separately. This is not a passing Mac typing-keyboard focus test.

The evidence explains this saved patch's very quiet output as filter behavior.
The bridge preserves legitimate filtering and does not silently change cutoff
or filter type. See [filter-audio-0.9.5.json](test-results/filter-audio-0.9.5.json).

## Generic audio diagnostics

The candidate now observes MIDI input and stereo output without modifying
plugin parameters or samples. Below -55 dBFS for 0.75 seconds of held-note
audio produces informational advice about volume, filters, envelopes and
oscillator mix. Missing note input, stale audio processing, invalid samples
and over-full-scale output receive distinct guidance, labeled by instrument
metadata rather than vendor-specific logic. Quiet patches are not classified
as failed plugins. Normal filter behavior is preserved.

Eight real NoiseMaker patch cases passed, including low-pass/high-pass cutoff,
zero volume, oscillator/ring mix off, envelope sustain off, no note input and
high gain. Each preserved serialized state and recovered after a deliberate
test-only patch restoration. Six pre-monitor/new-monitor PCM comparisons were
bit-for-bit identical at 44.1/48 kHz. Nine Core Audio/editor/concurrent-client
checks passed. Generic contracts passed on arm64/Rosetta and both Windows
architectures at 44.1, 48 and 192 kHz. See
[audio-diagnostics-0.9.5.json](test-results/audio-diagnostics-0.9.5.json).

Use: keep the original editor open, play a sustained note or keep an FL pattern
running, then open BridgeLab and choose Check runtime. The message reports
observed signal behavior; it does not choose a new sound or prove all controls
correct. The installed update passed its live FL 24.2.2 message check: the user started
the QA pattern, live NoiseMaker observations reported held notes and quiet output,
and the installed app returned the expected quiet-patch guidance. Display frames
continued updating, with zero rejected inputs and invalid audio samples in the
observation. Mac typing-keyboard focus/note release remains a separate pending
acceptance item.
