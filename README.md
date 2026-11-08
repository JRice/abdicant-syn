# Shiboleth - version 0.4.0

A personal two-oscillator mono instrument for Windows x64 CLAP and VST3. Design details and research are in [DESIGN.md](DESIGN.md).

## Shareable VST3 + CLAP package

`build/Shiboleth-0.4.0-Windows-x64.zip` contains both formats, installation instructions and third-party notices. The VST3 is self-contained: copy the entire `Shiboleth.vst3` folder into `C:/Program Files/Common Files/VST3/`, then rescan the DAW. No CLAP installation is needed. This package is Windows x64 only.

VST3 and CLAP share the synth and native editor implementation. The VST3 uses the MIT-licensed [CLAP wrapper](https://github.com/free-audio/clap-wrapper), with its note events translated into the same engine. Modwheel, pitch bend, channel/poly pressure, automation, and state recall are supported. Host-specific CLAP modulation does not automatically transfer to VST3. Existing CLAP projects retain their identity and state format; a VST3 instance is a separate plug-in type.

Run `./build-vst3.ps1` to build/test both formats, run Steinberg's validator and create the ZIP. `./build.ps1` remains the lighter CLAP-only build. Dependency revisions and the small sample-rate validation patch are documented in [third_party/SHIBOLETH-DEPENDENCIES.md](third_party/SHIBOLETH-DEPENDENCIES.md).

VST3 verification: Steinberg validator **47 passed, 0 failed**; custom host tests cover discovery, all synth parameters, sample-accurate notes/audio parity, MIDI expression, automation, saved state, UI gestures, and 12 editor reopen cycles. The final binary depends only on Windows system libraries. Other DAWs have not been individually tested. Validation output is in `build/vst3-validation.log`; the native VST3 editor capture is `build/editor-vst3.png`.

## Try the build

`build/Shiboleth.clap` is a self-contained binary with the native graphical editor and embedded artwork. Close instances before replacing the previous file in your plug-in location, then rescan/reopen. If upgrading from `MonoStudy.clap`, replace that file rather than keeping both names: the internal plug-in ID is unchanged. Host remote pages are Tone, Oscillator 1, Oscillator 2, and Performance. Local DSP and CLAP host tests pass; verification of this revision in Bitwig is pending.

## Editor

The approved midnight-blue layout has moving sliders, fixed-light knob frames, live scopes, a small aftertouch post, circular resonance and octave buttons, and a lower-right Options button.

- Drag sliders vertically; click their tracks to move directly. Drag knobs vertically.
- Drag a Crush, Alt or Noise AM symbol upward/downward to set its bipolar filter-envelope amount. A white 2-pixel track line shows the range from the base value, clipped at the travel limits. Double-click the symbol to clear its amount. All six amounts are independent, automatable and saved with the patch.
- Hold Shift for fine adjustment. Mouse wheel adjusts the hovered control. Double-click restores its default.
- Click waveform displays or their round selectors to cycle waves; right-click for the full list.
- The three-pip button below Osc I's wave selector cycles -2, -1, 0 and +1 octaves, shifting both oscillators together. Zero through three filled pips represent those positions; the default is 0 octaves (two pips). Osc II tuning remains relative.
- Click Resonance to cycle 0, 0.1, 0.5, 0.95, indicated by zero through three lit pips.
- Click the aftertouch selector or display to cycle targets; right-click for the list.
- Hover or drag for a temporary name/value readout. Pulse Crush displays detune in cents.
- Options opens the shared noise-cutoff control.
- Tab/Shift+Tab moves between controls. Arrow keys adjust; Shift makes finer steps. Space/Enter activates selectors; Home resets; Escape closes Options or ends a drag.
- Fixed size: 1300 x 765 pixels, with host DPI scaling support. Interactive resizing is disabled.
- Both waveform windows show pitch-synchronized oscillator traces after Crush/Alt/Noise AM/Squish, before oscillator-2 level and the shared filter/amplifier. They update when the host processes audio and freeze when processing stops.

GUI edits emit CLAP begin/value/end automation gestures. Host automation and recalled state update the same parameter storage. Scope transfer uses atomic snapshots; the audio thread never accesses graphics or windows. Artwork provenance is in [assets/README.md](assets/README.md).

## Implemented sound and performance

- Seven waveforms per oscillator; oscillator 2 level and center-fine/outer-stepped tuning, relative to the shared octave shift.
- Crush: symmetric amplitude quantization from effectively 16-bit resolution to three levels (-1, 0, +1); zero bypasses. No sample-rate reduction or pitch tracking. Quantization preserves both zero and -1 Squish tail levels.
- Pulse exception: Crush blends in an equally weighted hidden square, detuned upward by 0-100 cents. A short blend-in near zero avoids an abrupt switch. Both copies receive Alt and Squish. Peak gain is normalized; beating still varies perceived level.
- Classified host parameter names, such as `OSC1: Wave`, `FILTER: Cutoff`, and `ENV: Attack`.
- Doubled noise AM excursion relative to the original implementation. Shared `OPTIONS: Noise cutoff` ranges from 10 to 1000 Hz, default 80 Hz; modulation variance is compensated for bandwidth/sample-rate changes.
- Squish fills the trailing portion with -1 for triangle, trapezoid, folded triangle, square + triangle and pulse (including the hidden detuned pulse). Saw and half-triangle keep a zero tail. Subsequent Noise AM still modulates the resulting waveform.
- Internal hard sync (Alt), low-pass noise AM, independent wave-squish LFOs, and six bipolar filter-envelope routes to the oscillator treatments. Envelope routes include velocity sensitivity and are independent of the filter's envelope amount.
- Four resonance base states: 0, 0.1, 0.5, 0.95.
- One ADSR set: amp decay is twice filter decay; amp sustain is (1+S)/2 above S=1/6. Below that, multiplying by smoothstep(6S) closes amp sustain smoothly to zero. Velocity scales both envelope outputs.
- Last-note priority, legato-only glide up to 100 ms, retrigger from the current value on detached notes. Port at exactly zero retriggers both envelopes on every note-on, including overlapping and repeated notes; any positive setting preserves legato envelopes. Note-off fallback does not retrigger. Oscillators (including the hidden pulse), LFOs, noise and drift are free-running during processing. Note events do not reset them. Velocity changes while sounding use a 4 ms exponential smoothing time constant shared by amp, filter and treatment-envelope modulation; silent attacks start at the requested velocity.
- CC1 modwheel vibrato up to +/-1 semitone; 2-6 Hz rate. Pitch bend +/-2 semitones.
- Channel aftertouch and poly aftertouch for the current key. Six targets with bipolar depth. Resonance pressure acts continuously around its base state.
- Host automation, global CLAP parameter modulation, four remote pages, and versioned state saving.
- Slow independent drift, saturating mixer/filter states/VCA, DC removal, 4x processing, FIR downsampling.
- Always-on -1.5 dBFS sample-peak limiter with instantaneous gain reduction and nominal 60 ms exponential release. Reported FIR latency: 12 host samples.

## Compatibility and limitations

- State version 4 preserves existing parameter IDs. Versions 1-3 still load: missing octave defaults to 0, missing envelope depths to zero, and missing noise cutoff to 80 Hz. Original Reduction automation now controls Crush, as requested.
- The editor is Windows/Win32 embedded only. Host modulation changes sound while controls display base values.
- The nonlinear state-variable filter approximates the desired character; it is not a Dreadbox circuit model.
- Oversampling reduces aliasing, but hard sync, squish and some waveform edges still need dedicated bandlimiting. Crushing deliberately adds high harmonics.
- MIDI note/control input and global CLAP modulation are supported; native CLAP per-note expression/MPE and sustain pedal are not implemented.
- Knob text entry is unavailable. Tuning changes and waveform switches may need additional smoothing after listening.
- The limiter is not a true-peak limiter. Sound/state compatibility is not frozen during prototype development.

## Build and verification

Run `./build.ps1` in PowerShell. Default compiler: `C:/msys64/ucrt64/bin/g++.exe`; override with `-Compiler`. The adjacent `windres.exe` embeds the faceplate. GDI+, GDI, User32 and Ole32 provide the editor; no web runtime is needed. The script builds the plug-in and both test executables, runs them, renders `build/first-study.wav`, and captures editor screenshots. No global installation or system settings are changed.

Official CLAP headers are vendored in `third_party/clap` at commit `a47f6badb49d948fd009998f28309cdab78979c9` (MIT; retain LICENSE). To fetch them in a fresh checkout:

```powershell
git clone https://github.com/free-audio/clap.git third_party/clap
git -C third_party/clap checkout a47f6badb49d948fd009998f28309cdab78979c9
```

Verification covers DSP contracts, envelope relationships, oscillator routes, octave transposition, note handling, limiter stress across sample rates, binary loading, automation, partial state streams and legacy states. Native editor tests cover gestures, rejected-event retry, selectors, Options, fixed dimensions/DPI, scopes, and teardown.

The 0.3.1 crash fix replaces GDI+'s cached generic font family with a locally owned family. Tests now allow full graphics shutdown between sessions instead of keeping a host-owned graphics token alive. They close, recreate and repaint the editor 12 times while the plugin remains active, processing audio between cycles, and exercise host-parent destruction before GUI destruction.

`build/first-study.wav` is an eight-second generated DSP preview, not a matched Dreadbox reference or a finished sound-design demo.

Version 0.3.2 tests verify phase/source continuity through legato, last-note fallback and detached retriggers; velocity transition timing at 44.1/48/96 kHz; hidden-pulse continuity when Crush is bypassed; and a rendered-audio velocity-step regression. State format and parameter IDs are unchanged.
