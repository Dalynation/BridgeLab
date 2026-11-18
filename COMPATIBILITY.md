# Plugin-agnostic compatibility requirements

BridgeLab's target is Windows audio plugins through their standard interfaces.
An arbitrary DLL may be a dependency or unrelated library rather than a plugin.
Import must identify its format and architecture and report unsupported
capabilities instead of creating a wrapper that merely appears successful.

## Original editor

Display and operate the plugin's own editor inside the DAW wrapper. Preserve its
artwork, knob designs, labels, layout, presets and behavior. Scaling the original
editor to fit a display must preserve pointer coordinates. A generic parameter
panel or a screenshot without working input does not count as original-editor
compatibility.

Audio, editor capture, physical control input, audible parameter changes,
multiple independent instances, state recall and project reopen are separate
acceptance checks. A successful DLL probe or FL Studio scan proves only that
stage. Failed stages must remain visible in compatibility reports.

## Shared host work still required

| Capability | Current status |
| --- | --- |
| Windows x86/x64 VST2 instruments | Implemented; compatibility varies |
| Windows VST3 backend | Not implemented |
| Audio-input effects and declared audio buses | Not implemented end to end |
| Notes and velocity | Implemented; wider MIDI remains below |
| CC, pitch bend, aftertouch and SysEx | Not implemented end to end |
| Tempo, transport and general host automation | Not implemented end to end |
| Original editor pixels and input | GDI and single-surface Direct2D paths implemented; certify individual controls and plugins |
| Software/GDI popup windows | Generic discovery, pixel and input forwarding implemented; six NoiseMaker dropdown round trips passed in candidate |
| Separate GPU popups, arbitrary dialogs, external file/sample drop | Incomplete; GPU/size limits are reported when observed |
| Generic MIDI/audio health | Reports observed quiet notes, missing MIDI, stale processing, invalid/over-range samples; never changes patch or gain |
| Generic input/frame health | Observes delivery, queue rejection, frame publication and popup limits; does not prove the intended parameter/audio change |
| External samples and adjacent resources | Adjacent resources can be copied; loading and relocation need broader checks |
| Dependency discovery and runtime compatibility | Architecture/probe errors exist; automatic resolution is incomplete |
| Licensing, hardware drivers and protected plugins | No general compatibility guarantee; never bypass licensing |
| macOS/DAW versions | macOS 11+ is a build target; tested combinations must be listed individually |

Use declared capabilities, probe results and interface behavior to select a
backend. Avoid plugin-name branches. If two runtimes support different editors,
selecting one must also pass audio, input and recall checks; audio alone is not
sufficient. Preserve old runtime environments when migrating dependencies.

This document states requirements and gaps, not release certification. See
the versioned QA reports for measured results and unresolved failures.
