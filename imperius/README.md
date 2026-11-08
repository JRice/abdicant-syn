# Imperius — version 0.4.0

A separate five-voice wavefolding instrument, inspired by the broad synthesis approach and visual language of the Majella Implexus. This is an original behavioral design, not a circuit emulation. Shiboleth's source and build are unchanged.

Imperius is a self-contained Windows x64 CLAP plug-in with five voices, a native silver/amber editor, host automation/modulation and project recall. The approved visual proposal is now implemented as a working panel with all 36 parameters. The synthesis engine and state format are unchanged from 0.3.0.

## Play in Bitwig

Copy `build/Imperius.clap` into the CLAP folder you use for Shiboleth, then rescan plug-ins and load **Imperius** on an instrument track. Its ID is `local.abdicant.imperius`, separate from Shiboleth. Arm/monitor the track and play MIDI notes. Open its plug-in window for the native panel.

## Native editor

![Native Imperius editor](build/editor.png)

- Drag knobs vertically. Hold Shift for fine movement. Mouse wheel and arrow keys also adjust values. Envelope time knobs use logarithmic travel.
- Double-click a control or press Home to restore its default. Hover or drag for a temporary name/value readout in the header.
- Octave and Basic Wave are three-position switches: click their center to cycle, or their left/right end to select the corresponding endpoint. Right-click any switch for explicit choices. LP/HP and Sync buttons toggle with a click.
- Each LFO has one Sync button. When lit amber, its Rate knob, wheel and arrow keys snap to host-tempo divisions. Turn Sync off to return to a continuous Hz rate.
- Tab/Shift+Tab traverses the panel. Space/Enter activates the focused switch. Escape, loss of mouse capture, hiding or closing the window ends an active drag cleanly.
- Native size is 1300 x 780. Host DPI scaling from 0.5x to 2x is supported; interactive resizing is disabled.
- Amber arcs, pointers and switches display base parameter values. Host modulation affects the sound without moving the base-value indicators. Host automation and project recall update the panel directly.

The faceplate is embedded in the CLAP binary. Knobs, amber arcs, switches, buttons, focus rings and value readouts are separately rendered in Win32/GDI+. Metal lighting remains fixed as pointers rotate. A bounded single-producer/single-consumer queue delivers begin/value/end automation gestures to the host and retains rejected events for retry. No artwork files or browser runtime are required after installation. Artwork provenance and the built-in image-tool prompt are in [assets/README.md](assets/README.md).

The host provides six remote-control pages: **Tone**, **Generators**, **Filter and Amp**, **Modulation**, **LFOs**, and **Waves and Audio Mod**. All 36 parameters are also available through the host's parameter list. The Tone page places Harmonics, Fold, Bias, Phase Modulation, Cutoff, Resonance, Fold Envelope and Volume together for immediate auditioning.

The **Waves and Audio Mod** page adds `OSC: Shape`: square at -100% (counterclockwise), sine at 0% (noon), saw at +100% (clockwise), with continuous morphing on either side. This selects the complex generator's base waveform before folding; the existing Harmonics control adds the previously auditioned phase shaping. To hear the pure waveform choices, set Harmonics, Fold, bias and all modulation amounts to zero. `BASIC: Wave` selects Square, Sine or Saw. The selected basic wave supplies both the mixer and the audio-rate modulation paths. The sub remains a sine.

Version 0.3.0 preserves the original parameter IDs and adds these two controls at the end. Original version-1 states load with both sources set to sine, preserving the earlier sound. New states include both wave controls. The original five remote pages keep their mappings; the sixth page exposes the new choices.

Start with Harmonics and Fold; add Phase Modulation or audio-rate fold modulation for metallic tones. The Generators page provides the three-position octave selector, relative basic tuning and complex/basic/sub levels. On the LFOs page, each LFO has one Sync switch: its same Rate control then selects tempo divisions. Sync follows host tempo, with 120 BPM as the default before tempo is supplied. LFOs remain free-running; transport start does not reset their phase.

MIDI sustain (CC64), per-channel pitch bend (+/-2 semitones), all-notes-off (CC123), all-sound-off (CC120), and controller reset (CC121) are handled. CC120 clears that channel's voices immediately; the shared FIR can retain up to 32 host samples of previous output. Native CLAP per-note expression/MPE and aftertouch are not implemented. Stereo output currently carries the same mono signal on both channels.

Continuous host parameters use a 5 ms smoothing time constant. Octave, filter mode and Sync are stepped. Host project state stores the base parameters; temporary host modulation is not baked into saved state. State is versioned and rejects malformed/truncated data without applying a partial patch.

The compiled DLL passes automated local-host and native-editor tests. The user auditioned version 0.3.0 successfully; the 0.4.0 editor still needs verification in Bitwig. No system installation or DAW settings are changed by the build script. Close loaded instances before replacing the installed binary, then rescan/reopen.

## Listen

Run `./build.ps1` from this directory, or `./imperius/build.ps1` from the repository root. Requires the same MinGW compiler and adjacent windres used by Shiboleth, plus the existing vendored CLAP headers. It builds `build/Imperius.clap`, runs DSP, binary-host and native-editor checks, captures actual editor screenshots, renders four eight-second mono 48 kHz / 16-bit WAV files, and writes `build/benchmark.txt` plus `build/plugin-benchmark.txt`.

- `build/01-fold-sweep.wav`: a held C, progressively adding folding and harmonic shaping over five seconds. Exposes the core timbre without basic/sub layering.
- `build/02-fold-plucks.wav`: short notes with envelope-driven folding.
- `build/03-audio-rate-metal.wav`: basic oscillator a fifth above the carrier, modulating carrier phase and fold depth.
- `build/04-five-voice-pad.wav`: five-note chord with slow harmonic/fold motion and a filter envelope.

Each listening file is peak-normalized to -3 dBFS after rendering. This preserves its internal dynamics but does not represent relative patch loudness. Console peak/RMS figures describe the original engine output. No delay, reverb, chorus or other post effects are added.

## Current core

- Exactly five voice slots. Idle voices cost no oscillator/filter work. Released voices are stolen before held voices; otherwise the oldest held voice is replaced. A short decaying output tail softens stealing. Repeated note-ons pair with note-offs in arrival order. Sustain and pitch bend are per MIDI channel. The CLAP note port advertises MIDI input.
- Each voice has a square–sine–saw morphing complex generator, selectable square/sine/saw basic generator, and sine sub at half the carrier frequency. Square/saw edges use polynomial BLEP correction in addition to oversampling. Shared three-position octave control is -1 / 0 / +1 and transposes all three. Basic tuning is relative in semitones.
- Continuous phase shaping adds harmonics ahead of a triangular folder, with fold gain and bipolar bias. Basic-generator audio-rate modulation independently targets carrier phase, harmonic shaping and folding. The `fm` field currently implements phase modulation, not linear frequency modulation.
- Separate amplifier/modulation ADSRs, two shared sine/triangle LFOs, and direct harmonic/fold modulation amounts. Per-LFO Sync makes its rate select a beat duration; BPM is supplied through the engine. Available durations: 8, 4, 3, 2, 1.5, 1, 0.75, 0.5, 1/3 and 1/4 quarter-note beats.
- Three-source saturating mixer, per-voice DC removal, resonant 12 dB/octave TPT low/high-pass filter and velocity-scaled amplifier.
- First-order antiderivative antialiasing at fold corners, 4x synthesis/filter processing, one shared 129-tap FIR decimator after mixing, and an output sample-peak limiter at 0.84 (about -1.5 dBFS). FIR latency is 16 host samples; the folder also introduces approximately half an internal sample of delay in its processed path.
- No audio-thread allocation or locks in sample processing. Coefficients shared by voices are calculated outside the voice loop. CPU figures are offline single-thread wall-time measurements, not DAW deadline guarantees.

## Verification and remaining work

Tests exercise the fold primitive across multiple corners, fixed-input behavior, five-voice allocation/stealing, repeated notes, channel sustain, complete release to silence, octave equivalence, tempo divisions, audible modulation routes, and finite/bounded output under extreme controls at 44.1/48/96 kHz in both filter modes.

Binary-host checks cover discovery and the separate identity, all parameter metadata, five remote pages, partial-stream state round trips and invalid state rejection, sample-positioned MIDI, sustain release, six-note stealing, sample-timed automation/modulation across different block boundaries, float32/float64 output, audible host-tempo sync, and isolation between two instances. The adapter advertises the 16-sample FIR latency. Supported activation rates are 8–192 kHz.

Version 0.3.0 also checks morph endpoints/midpoints, audible basic-wave changes in the modulation path, high/low-pass stress for all three waveform endpoints, both new host controls in the compiled binary, new-state recall, original-state migration and all six remote pages. Direct measurements of the exact two-integrator filter section give -11.96 dB/octave (LP, 2 to 4 kHz at 250 Hz cutoff) and -11.94 dB/octave (HP, 125 to 62.5 Hz at 1 kHz cutoff), at zero resonance and a 192 kHz internal rate. Results are written to `build/filter-response.txt`. The existing two-pole topology and resonance mapping are retained; no four-pole stage was present. The shared anti-alias decimator separately rolls off near output Nyquist and is not the musical cutoff filter.

Oversampling and folder ADAA reduce aliasing; strong phase modulation/shaping can still alias. Spectral/reference comparisons and listening review remain necessary before choosing final fold drive, modulation ranges and oversampling quality. The filter is currently a clean digital topology with a saturating input mixer, not an analog feedback circuit model. The core currently sums to mono.

Native editor tests exercise all 36 control mappings, automation gestures and event rejection/retry, pointer capture loss, hiding during a drag, switches, Shift adjustment, tempo-rate snapping, keyboard traversal/defaults, host automation readback, fixed metal lighting/moving pointers, 1.25x DPI rendering, twelve full close/reopen cycles while audio remains active, and parent-first destruction. Screenshots `build/editor.png`, `editor-sync.png`, `editor-automated.png`, `editor-125.png` and `editor-reopened.png` are captured from the compiled native editor, rather than the design mockup.

Listening-driven refinement remains. Envelopes are intentionally simple linear segments for this study. Sustain-loop/drone behavior, aftertouch, glide and stereo voice placement are not implemented. Parameter text entry is unavailable; use native/host controls or automation. Modulated values affect the sound while host readback/state retain base values. Pitch bend and stepped mode changes are immediate.

## Accepted design direction

Name: **Imperius**. Five voices are the baseline. Silver panel, black section tabs, rounded group outlines, black knobs with silver centers, restrained amber arcs, optional narrow wooden cheeks. Large controls emphasize harmonics, folding and cutoff. Octave selection uses a three-way switch rather than a large knob. One Sync button per LFO changes its rate knob to tempo divisions.

No arpeggiator, sequencer, preset browser/patch-management UI, delay, headphone controls, patch cables, clock input, tap tempo or MIDI clock indicator. Host project state recall is supported. The panel follows the approved concept after the user auditioned the sound engine.
