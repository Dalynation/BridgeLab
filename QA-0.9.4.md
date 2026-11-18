# BridgeLab 0.9.4 QA findings

## Test-environment correction

Repeated tool-driven sessions accumulated 1,050 Windows helpers in the isolated runtime and another 197 from earlier tests of the installed runtime. The isolated helpers' reported RSS total was about 2.1 GiB (including shared pages), and their aggregate CPU use exceeded two cores in one snapshot. The older installed group used another 58% of one core. This contaminated the timing measurements.

We initially attributed this to the transport's immediate SIGTERM. A controlled comparison **did not support that attribution**: both published 0.9.3 and the candidate held at six helpers across three normal playback sessions, with no Wine-server crash messages. Further observation showed the Wine server alive after the audio client exited, then absent after the tool session ended, leaving its helpers behind. Keeping the tool session alive for ten seconds allowed normal idle shutdown: server and helper counts reached zero after five seconds. Short test-session boundaries and their cleanup must therefore be accounted for. This is not proof of a production app memory leak or a claim that such a leak is fixed.

The QA session wrapper now runs timing checks together, then explicitly stops and waits for only its marked, disposable Wine profiles before the tool session ends. Old confirmed test helpers were stopped without changing user app files, DLLs, settings or FL projects.

## Product changes verified independently

- **Graceful runner teardown:** close stdin and permit normal plugin/editor cleanup before forced termination. The deterministic regression fails against the old immediate-kill behavior. The new version passes 12 normal cycles on each Mac architecture and reaps both a hung child and a worker blocked on a response. This verifies cleanup/cancellation behavior, not elimination of every Wine failure.
- **Shared realtime transport and native DSP scheduling:** a bounded cross-process queue carries realtime audio directly. Only the bridge DSP thread opts into a macOS time-constraint policy through the bundled helper. State replacement pauses/resumes DSP through an acknowledged barrier; editor messages remain separate. Startup mapping failure retries the legacy pipe path.
- **Bounded pipe pipelining:** up to eight audio requests can be in flight; replies are drained while sending, with partial I/O and backpressure handled. Control jobs run between completed batches. The regression requires eight requests before returning fragmented 37-byte replies; the serial worker cannot pass that barrier. Realtime callbacks do not wait for completion. Prefetch/offline processing preserve ordered completion.
- **Sample-rate headroom:** preserve approximately 43 ms of scheduling allowance, rounded to 256-sample quanta, rather than allowing the fixed 2,048 samples to shrink to 10.7 ms at 192 kHz. At 48 kHz the minimum remains 2,048 samples for native and Rosetta hosts. Larger callbacks can require more. Latency is reported through VST3 for host compensation.
- **Test host timing:** recording memory is preallocated/touched before realtime processing, and files are written after processing. Disk I/O inside the earlier fake-host audio loop could itself manufacture stalls.

The bundled Crossover runtime remains in use to preserve working original editors. A newer-runtime experiment passed audio checks but failed SQ8L/TAL editor checks, so it was rejected. A Rosetta-only latency increase was also rejected; both native and Rosetta hosts use the same allowance.
## Final validation

Results are recorded in `test-results/0.9.4/`:

| Check | Result |
| --- | --- |
| Core Audio callback vs offline PCM, both plugins × both host architectures | **44/44 exact comparisons**, 44.1/48/96/192 kHz, host blocks 64–4096; includes 20-second runs at 48 and 192 kHz |
| Original editor knob changes during 10-second Core Audio playback | **4/4**, state changed, zero bridge underruns |
| Four simultaneous Core Audio clients, mixed native/Rosetta and SQ8L/TAL | **4/4 exact PCM**, zero bridge underruns |
| Zero-sample MIDI, repeated activation, prefetch, strict state-capture pressure, timer PCM, multi-instance recall and validators | **53/54 aggregate checks**; see mismatch below |
| Steinberg validator (included in the aggregate row) | **47 passed / 0 failed per plugin** |
| Higher-rate timer PCM | **8/8** |
| Sustained 20-second timer PCM | **4/4** |
| Repeated session PCM and helper counts | **16/16**, seven helpers before/after; disposable profiles cleaned at session end |
| Rebuilt app upgrade/reimport | Both plugins imported, IDs preserved, installed engine files match the app payload, wrapper signatures verified |
| Original editor input/state recreation and close/reopen lifecycle | Both plugins passed |
| Mac editor view attachment/teardown | **400 per architecture**, 800 total |
| Deterministic teardown, pipe pipeline, mapped audio/state protocols | All passed on both Mac CPU architectures |
| Importer and folder-permission regression tests | Five importer tests and six permission cases passed |
| Static packaged binary compatibility | App, VST3, editor and scheduling helper each contain arm64/x86_64 slices, minimum macOS 11.0, no development-directory dylib dependencies |

**Remaining timing finding:** the aggregate timer-host suite had one SQ8L/Rosetta live/offline mismatch: one 256-sample block was silent at 48 kHz (about 5.3 ms). Twelve focused follow-ups using the final app-installed payload passed exactly; timer lateness was instrumented for those follow-ups. The original failure was not instrumented for lateness, so its cause is unconfirmed. The full 44-case Core Audio matrix and the live-editor/concurrent-device checks passed. This evidence supports a preview, not a claim that every dropout is resolved. The initial failure is retained in the published results.

## Compatibility and remaining limits

Tests use SQ8L x86 and Windows TAL-NoiseMaker x64 on Apple Silicon/macOS 15.6.1 with arm64 and x86_64-under-Rosetta host processes. Rosetta testing is not physical Intel hardware testing. Other macOS versions, physical Intel Macs, low-RAM computers, the exact unidentified TAL variant and the reporter's FL Studio 26.1/macOS 15.1 combination remain unverified. The earlier FL Studio 24.2.2.4259 baseline is described in the README; the new timing checks use the native VST3 harness.

The universal app and VST3 wrapper target macOS 11; that is a build target, not certification of every later macOS/FL combination. Supported plugin scope remains Windows x86/x64 VST2 instruments, with the existing MIDI/editor limitations. One Windows runner per active instrument remains expected. Initial loading still takes time. Minimum bridge latency is 2,048 samples at 48 kHz, plus device latency; higher rates and larger callbacks receive more samples. Heavy system load or a slow plugin can still exceed the allowance. Finite green tests cannot guarantee uninterrupted audio under arbitrary load.

## Upgrade

Save and quit FL Studio and BridgeLab. Replace the complete app and reimport each original DLL/folder to rebuild its VST3 wrapper. Rescan with Verify plugins and Rescan previously verified plugins enabled. Existing per-DLL IDs remain stable. Replacing only the app does not update installed wrapper binaries. Restarting the Mac once is recommended if old preview/test runtimes remain running.
