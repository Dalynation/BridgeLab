# BridgeLab 0.9.3 QA findings

This update follows reports of one Wine process per instrument, opening delays and intermittent silent MIDI. Reproductions use the native VST3 host harness, not the reporter's exact FL Studio 26.1/macOS 15.1 session.

## Confirmed software failures and changes

| Reproduction | Before | 0.9.3 |
| --- | --- | --- |
| Note-on in a zero-sample/no-audio-buffer callback | Silent SQ8L | Notes retained for the next audio callback; SQ8L and TAL pass on both Mac host architectures |
| Repeated activation while holding a note | Runner restarted; held voice lost | Repeated activation is idempotent; voice continues |
| Faster-than-realtime prefetch callbacks | Realtime queue fills; permanent silence | Prefetch waits for ordered worker completion without restarting the runner |
| Continuous state/control traffic during realtime notes | SQ8L 0/479 later blocks audible | Bounded audio batches prevent permanent starvation; isolated trial 478/479 audible |
| Close plugin editor | Windows GUI only hidden; idle work retained | Plugin close called, mapping released, idle count stops; repeated reopening preserves state |

The sustained-control regression checks audio liveness, not gap-free delivery. Full-run audible counts were SQ8L arm64 456/479, SQ8L Intel-under-Rosetta 479/479, TAL arm64 479/479 and TAL Intel-under-Rosetta 476/479. Remaining lost blocks are a real performance limit, not a fully resolved result.

## Test results and remaining failure

- Final audio matrix: **53 of 54 checks passed**. The failed check was exact live/offline PCM for SQ8L with a 64-frame callback. Three follow-up trials had one exact match and two mismatches. This is an unresolved intermittent dropout, not an all-green release.
- Other tested live/offline comparisons matched exactly: arm64 256/1024/4096-frame callbacks for SQ8L, all four tested callback sizes for TAL, and 256-frame Intel-under-Rosetta checks for both.
- Four simultaneous cold-start clients produced audio. That check asserts liveness only; it does not establish uninterrupted playback while starting several Windows processes. Some SQ8L samples were absent during the load stress.
- Three-instance independence and exact saved-state/audio recall passed on both Mac host architectures.
- Steinberg validator: 47 passed, zero failed for each wrapper.
- Both original editors rendered, accepted knob input and restored GUI-edited state exactly in a new runner. Both closed editors stopped idle callbacks, released frame mappings and reopened three times with state unchanged.
- Three explicitly tracked runner PIDs exited after their owning test sessions were destroyed. Shared Wine services can remain; this does not claim that every Wine-related process exits.
- Packaged UI regression: 400 attachments per Mac architecture passed. Five harness tests and six native permission cases passed.

## Runtime/RAM and latency

There is still **one isolated Windows runner per active instrument**. It is not a full extra Wine installation: runtime files and the per-architecture Wine prefix are shared. Repeated activation no longer starts a replacement unnecessarily, and closed editor resources are released. A shared-process host or reusable runner pool is not implemented.

The three runner RSS readings were 7,988, 47,396 and 15,636 KiB in one session. RSS includes shared pages and may represent launcher processes; it is not total Wine memory or unique per-instrument RAM. No low-RAM hardware certification or RAM-reduction percentage is claimed.

Latency remains at least 2,048 samples (42.7 ms at 48 kHz), plus device latency. Larger configured host blocks receive two callback blocks plus scheduling headroom. A lower minimum caused dropouts in stress tests and was rejected. Initial Windows process startup and overload can still interrupt audio. Host logs record configured block size, rate, process mode, latency and termination underruns for further investigation.

## Scope and upgrade

Test machine: Apple Silicon/macOS 15.6.1. SQ8L x86 and Windows TAL-NoiseMaker x64 were tested with arm64 and x86_64-under-Rosetta host processes. Physical Intel hardware, low-RAM Macs, the reporter's macOS/FL combination and the exact plugin called “U TAL X” are unverified. Windows alone does not identify a TAL product.

Close FL Studio, replace BridgeLab.app, then **reimport every original DLL/folder** to regenerate its native VST3 wrapper. Rescan with Verify plugins and Rescan previously verified plugins enabled. Simply replacing the app does not update existing wrapper binaries. Per-DLL class IDs remain stable.

This is an experimental improvement preview, not a claim that every reported timing symptom is fixed. Test small projects and report remaining timing failures with exact plugin, Mac chip/RAM, macOS/FL version and buffer size.

Raw evidence is in [test-results/0.9.3/](test-results/0.9.3/).
