# 0.9.5 candidate QA — not released

This is a Windows x86/x64 VST2 **instrument** bridge. It is not a converter for
arbitrary DLLs or a certification of every Mac, plugin, or FL Studio release.
The public download remains 0.9.4 while final acceptance is incomplete.

## Bugs reproduced and corrected

- The 0.9.4 queue-full path permanently muted a healthy runner in four baseline
  cases. Queue backpressure no longer marks a live transport dead. MIDI state
  reconciliation is bounded and does not allocate in the audio callback.
- The first candidate recovery implementation introduced a second problem:
  dropping empty audio packets repeatedly retriggered held keys. SQ8L ignores
  CC120. A controlled probe measured one note at 1,022 microseconds per block,
  versus five replays at 4,988 microseconds. Panic alone did not silence it.
  Traces connected six/seven submitted note-ons with slow rendering and silence;
  macOS thread counters then showed policy/priority changing from 2/97 to 1/0.
  This is consistent with the realtime CPU failsafe in
  [Apple's scheduler implementation](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/kern/priority.c).
  Empty audio loss now leaves MIDI alone. Actual MIDI loss explicitly releases
  observed keys, then replays held keys in a later processing quantum. Rejected
  recovery packets retry their current phase instead of repeating accepted work.
  Shared-memory catch-up publishes audio before yielding after four quanta or
  approximately four milliseconds of a sustained backlog. No kernel protections
  or global scheduling settings are disabled.
- The pipe fallback did not opt its DSP thread into the macOS scheduling helper.
  A Core Audio recovery baseline passed 15/16, with one silent 256-frame quantum.
  The fallback now opts only its named realtime DSP worker into the same policy.
  A following 16-case run passed with transport and policy assertions. A later
  held-key timer failure exposed the separate replay problem described above;
  that failed candidate has not been published.
- A later pipe recovery run exposed delayed native I/O despite fast Windows DSP.
  The pipe I/O worker now opts into a verified, bounded macOS realtime policy;
  the shared-memory control worker remains unchanged. All 12 targeted pipe
  Core Audio recovery cases passed with both worker policies verified.
- J8X, Pha and U-NO-LX discarded their first note before their first processing
  call initialized DSP. One empty processing block before MIDI fixes cold,
  restored-state and selected-program first notes: nine checks passed.
- A nonstandard Drum current-program report is normalized on capture, and bounded
  legacy state is accepted without permitting arbitrary invalid program indices.
  Native/Rosetta state tests and real Drum state restoration passed.
- Slow editor commands are queued on the Windows message thread. An eight-second
  editor fixture passed on native/Rosetta: immediate acknowledgement, a state
  request waiting beyond the old timeout, continuous Core Audio while loading,
  a real control/audio change, and zero bridge underruns.

## Live legacy-editor repaint correction

A subsequent manual FL check exposed a gap in the earlier editor tests:
NoiseMaker's sound/parameter state changed, but its knob image updated only
after reopening. This is a real failed acceptance case, not the automation
focus problem described below. A direct same-window pixel test reproduced it:
SQ8L changed 579 knob-region bytes; NoiseMaker changed **zero**, despite a
changed plugin state. A separate native Mac-view repaint test passed before any
production UI change, directing investigation toward Windows repaint scheduling.

NoiseMaker imports the older `CreateDXGIFactory` entry point. The refresh pacing
hook previously covered only `CreateDXGIFactory2`; software-rendered editors can
also depend on DXGI vblank notifications. The generic adapter now instruments
factory versions 0, 1 and 2, retaining the existing bounded pacing and preserving
real device errors. An attempted WM_PRINT capture workaround did not solve the
case and was removed; there is no production capture-forcing change.

The corrected same-window tests pass for **both** legacy instruments: parameter
state changes, knob pixels change without reopening, and unrelated artwork stays
unchanged. NoiseMaker now changes 821 knob-region bytes. The test waits for real,
stable initial artwork and checks the SQ8L logo rather than its changing numeric
display. Earlier test-fixture rectangle/startup mistakes are retained in local
logs and are not counted as product regressions. Both Windows graphics contracts,
both native Mac repaint harness architectures and all three modern original
artwork/edited-state cases passed. The corrected build also passed **44/44** exact Core Audio comparisons, **9/9**
editor/concurrent-client checks and **9/9** fresh standalone import/render checks.
Its signed app and engine were installed after backing up the previous versions,
preserving the existing imports, wrappers, runtime profile and saved projects.
The saved two-plugin FL checkpoint loaded with the correction. The user subsequently confirmed **the knob moves immediately and the sound
changes in the corrected FL session**, without reopening. Separate per-instrument
solo exports remain pending.

## Original interface

The generic single-surface DirectComposition adapter lets the plugin draw its
own Direct2D artwork into a real texture and forwards the pixels to the Mac
view. It does not replace or redesign controls. Dirty rectangles preserve
untouched artwork; frame synchronization permits idle rendering and teardown.
Unsupported complex surfaces, transformations and scroll operations return
errors rather than pretending to render.

Fifteen official TAL DLL builds passed nonblank editor capture, parameter API
changes, state restoration and graphics-resource release. Coverage: current x64
J8X, Pha, J-8, U-NO-LX, BassLine-101, Drum, Sampler, Mod and NoiseMaker; legacy
x86/x64 BassLine, U-No-62 and Elek7ro-II. Vendor binaries are not distributed.

J8X, Pha and U-NO-LX also passed actual mouse volume changes, corresponding audio
changes, preservation of untouched original artwork, and fresh-instance edited
state/audio recall after the plugin's own serialization normalization. All six
one-minute Core Audio editor cases passed across native/Rosetta, with zero
bridge underruns. Graphics synchronization contracts passed in both Windows
architectures. These graphics results preceded the final overload-only changes;
no graphics implementation was changed afterward.

Drum and Sampler default to empty media slots. Sample loading and resulting audio
are not established. Parameter API checks are not physical knob tests.

## Audio regression ledger

The final wrapper passed **44/44** exact Core Audio live/offline comparisons
across native/Rosetta, SQ8L/NoiseMaker, 44.1–192 kHz and 64–4,096-frame blocks;
**9/9** original-editor/concurrent-client checks; **16/16** timer-driven new/held
note recovery cases; and **12/12** targeted pipe Core Audio recovery cases.
First-note coverage passed **9/9**. Three independent instances with state
save/recreation passed in both architectures; both original plugins passed
Steinberg validation (**4/4** combined).

The corrected shared-memory path passed **30/30** repetitions of the previously
failing Rosetta SQ8L held-key case. Each rejected about 1,900 empty audio packets
but submitted exactly one note-on and retained audible playback. Four further
thread-monitor trials completed without realtime-policy demotion.

A deterministic queue-loss fixture passed **8/8** Core Audio cases: actual dropped
note-ons and dropped note-offs, both transports and both Mac architectures.
Dropped note-offs had a positive audible control before the fault, explicit
release after it, complete received recovery quanta and subsequent silence.
The fixture deliberately ignores CC120/123, so these checks verify explicit
note-off recovery. An earlier fast-fixture run reached 3/4 because one intended
lost event was accepted; it did not exercise the fault and is superseded by the
deterministic test. These results do not replace manual FL project acceptance.

Earlier failed runs are retained: the timer matrix passed 57/58; sustained timer
comparisons 2/4; lifetime exact audio 14/16. Those exact-PCM failures reported host
lateness of approximately 43–128 ms, at/beyond the roughly 43 ms scheduling
allowance, with compiler/UI work during parts of the run. There were also the
subsequent low-lateness replay failures that led to the correction above.
Passing follow-ups do not erase these observations or promise inaudibility under
arbitrary host/CPU stalls. Repeated lifetimes kept six helper processes without
growth; this does not claim that a production Wine-memory leak was fixed.

## Clean installation and FL Studio

The final standalone packaged app, run from outside the source tree with a
genuinely empty disposable home, passed **9/9** checks: fresh SQ8L/NoiseMaker/
U-NO-LX imports and real WAV audio, duplicate import, invalid-DLL rejection,
distinct identities, one shared runtime and an intact registry. First setup
and import in the latest corrected package took 39.71 s; later imports took
approximately 1.5 s. The preceding package measured 34.44 s. The initial
command-sandbox run denied copying the runtime's .DS_Store; the complete isolated
run outside that sandbox passed and its Wine profile shut down successfully.
[Sanitized results and packaged binary hashes](test-results/qa-0.9.5.json) record
the tested candidate. This is CLI acceptance, not graphical import acceptance.

The final tested standalone app was subsequently installed normally on the
Desktop. The prior BridgeLab app, settings/runtime, three wrappers and its own
FL database entries were moved into restorable backups (17 items). FL was saved
to a separate project backup and quit before replacement. Original DLLs, projects
and unrelated plugins were preserved. Installed app/engine hashes match the
standalone test; all three wrapper executable payloads match after excluding
their expected bundle-specific ad-hoc signatures.

SQ8L was imported through Finder Copy → Paste DLLs in the graphical app. This
fresh graphical runtime setup took **134 seconds**, considerably longer than
the isolated CLI run. NoiseMaker and U-NO-LX were then imported through the
installed app CLI. All three produced non-silent installed-app WAV renders.
A user-triggered fresh FL Studio **24.2.2.4259** scan found all three as separate
Apple + Intel VST3 synths with status **ok**. SQ8L's original interface visibly
opened inside FL with the final candidate.

The user confirmed FL responds normally after automation timeouts, and that the
original NoiseMaker Volume knob responds and retains its change across closing
and reopening the editor. An edited FL project was saved, an empty project loaded
to destroy the plugin instances, then the saved project reopened. Capturing both
plugin states into a second FL project produced **exactly matching opaque state
hashes** for SQ8L and NoiseMaker. The edited NoiseMaker Volume value remained
0.2600000202655792. This establishes actual FL project state recall, beyond
editor-window reopening. The final QA pattern has four notes per bridged channel.

FL exported the two-plugin pattern as **44.1 kHz, 16-bit stereo, 88,200 frames**
(two seconds), peak 0.186859 and RMS 0.059174. This establishes a non-silent
combined export; individual plugin solo exports and an explicit live playback
check remain pending so one audible instrument cannot conceal a silent second
instrument. The user was asked to restore main-window focus for those checks.

Native mouse automation intermittently returns **noWindowsAvailable**, including
in Plugin Manager before a plugin editor opens. Accessibility may time out after
closing an editor, save panel or render dialog. Stack samples showed FL's main
event loop waiting for and processing events; the user confirmed normal manual
interaction. These automation failures are not evidence of an established plugin
deadlock, nor do they certify uncompleted checks. No forced quit or project
discard was used.

The macOS picker could be highlighted via accessibility, but automated selection
did not enable Open; direct coordinate input reported no available window.
Removing type restrictions did not resolve it and was reverted. No picker bug
or picker fix is claimed. The packaged importer test does not certify this
mouse-driven step.

## Resource/performance limits

New imports share one Wine profile while retaining one isolated worker per
active instrument. Existing environments are preserved, not merged. A controlled
four-instance comparison measured Wine servers 2 to 1, helpers 18 to 10, and
summed helper footprints approximately 286 to 165 MiB. These sums are not unique
total RAM and do not include a low-RAM-hardware benchmark. Four starts took
7.23 to 5.62 s in that single comparison; DSP memory per instance remains.

Cold Direct2D editor frames took approximately 9–16 s. The original UI now
renders and controls work in the tested paths; instant startup is not claimed.
Minimum bridge allowance is 2,048 samples at 48 kHz, 4,096 at 96 kHz and 8,192
at 192 kHz; large callbacks may require more, and device latency is additional.

Builds contain arm64/x86_64 slices with macOS 11 deployment metadata. Tests ran on
Apple Silicon, macOS 15.6.1; Rosetta is not physical Intel hardware certification.
Brandon's macOS 15.1 / FL 26.1 combination still requires its own confirmation.
Windows VST3, effects/audio-input buses, wider MIDI, host tempo/automation,
complex popups/file dialogs, external media workflows and protected-plugin
compatibility remain gaps; see [COMPATIBILITY.md](COMPATIBILITY.md).

## Typing-piano release after a knob click

The user reported a held Mac typing-keyboard note remaining on after clicking
NoiseMaker's Volume knob. An isolated native host reproduced the editor
swallowing the host's key release after the click changed first responder.
The candidate now remembers the previous host responder and returns releases
for keys whose presses did not begin in the editor. Editor-owned key events
still go to Windows. Host-release, editor-key ownership and live repaint tests
pass on arm64 and Rosetta. Actual FL confirmation with the corrected UI loaded
is pending; this is not yet an end-to-end resolution claim.

### Actual FL keyboard retest failed

After installing the candidate UI and restarting FL, the user repeated the
typing-keyboard / Volume sequence and the note still remained on. The isolated
responder-level test therefore does not establish a fix for FL window-level
keyboard routing. Keyboard acceptance remains failed. The user also reported
NoiseMaker dropdowns not opening; exact control and popup handling are under
investigation. Neither behavior is classified as a vendor plugin quirk.

The isolated NoiseMaker test also reproduces the FILTER LP 24dB arrow opening
no visible menu in the embedded frame. Popup composition/input remains under
investigation. A broader keyboard test now checks host NSWindow.sendEvent
interception: the previous candidate fails it; a new source correction passes
it on arm64 and Rosetta. That second correction is not installed or verified
in FL. Neither newly reported issue is resolved.

## Expanded original-control audit

See [QA-CONTROLS-0.9.5.md](QA-CONTROLS-0.9.5.md). The broader audit confirms
working control classes but leaves menu/popup and keyboard acceptance failed.
U-NO-LX partial control results were excluded due to trial-notice/layout
interference. No complete-interface compatibility or regression-free release
is claimed. Experimental popup changes were reverted from production source.

## Shared automatic popup repair and generic monitoring

The latest candidate and remaining manual checks are recorded in
[QA-CONTROLS-0.9.5.md](QA-CONTROLS-0.9.5.md). Six original dropdown round trips,
three modern editor/audio/recall checks and nine Core Audio/editor/concurrency
checks passed after the repair. Earlier baseline failures remain documented
above. FL menu and typing-keyboard release retests are still pending.
