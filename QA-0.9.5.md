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
and import took 34.44 s; later imports took approximately 1.5 s. The initial
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

Native mouse automation returned **noWindowsAvailable**, including in Plugin
Manager before a plugin editor was open. After Escape/F6 from the SQ8L editor,
accessibility automation timed out again. A fresh two-second process sample
showed FL's main event loop waiting for and processing events; it did not prove
a BridgeLab deadlock or a passed close/reopen test. User confirmation of normal
FL interaction is pending. No forced quit or project discard was used. In-FL
knob input, multi-plugin project export and save/reopen recall remain pending.

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
