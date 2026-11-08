# Shibboleth — working specification

Status: Windows DSP/CLAP instrument with native graphical editor and automated verification. User has approved the basic tone, waves, Squish, resonance, envelope relationship, glide, and the realistic midnight-blue panel based on their drawing. The actual editor is now built and tested in a local CLAP host; Bitwig-specific editor verification remains pending.

## Intent

A personal-use, two-oscillator monophonic instrument for Bitwig, inspired by aspects of Dreadbox rather than emulating a particular unit. Direct controls, strong character, and an elegant dark, desaturated blue skeuomorphic panel. No sub oscillator, arpeggiator, sequencer, preset browser, general effects bus, or modulation matrix. Host state saving remains essential for DAW recall.

Proposed first format: Windows CLAP. Expose meaningful parameters to host automation and modulation, and organize host remote controls. Keep synthesis independent of the format adapter so later instruments and a possible VST3 version can reuse it.

## Confirmed requirements

- Two oscillators; oscillator 1 has a waveform selector plus the requested waveform treatments. Oscillator 2 adds level and bipolar tuning.
- Basic waveforms: triangle, saw, pulse. Exotic shapes include a positive triangle occupying the first half-cycle followed by zero, and a square–triangle composite. Working interpretation of the composite: one complete bipolar square cycle in the first half of the nominal period, followed by one complete bipolar triangle cycle in the second half. Each half visits positive and negative amplitudes; the two different halves together form one repeating period.
- Two additional shapes: trapezoid (flat-topped bipolar triangle) and a fixed, asymmetrically folded triangle. User likes the implemented waveforms.
- Last-note priority; releasing the current note returns to the most recently held remaining note.
- Glide control with a maximum of 100 ms.
- Shared ADSR controls for filter and amplifier, with separate internal envelope trajectories: filter decay D and sustain S; amp decay 2D and sustain (1+S)/2 above S=1/6. In the bottom sixth, multiply that mapping by smoothstep(6S), reaching zero with a smooth join at 1/6. Proposed common attack and release times. Both envelopes release to zero on gate-off.
- Velocity sensitivity controls both amplifier envelope level and filter-envelope excursion. Proposed implementation scales envelope output after the sustain mapping, so low velocity can still produce quiet notes.
- Vibrato has a dedicated rate knob (2–6 Hz). Modwheel directly controls depth from zero to ±1 semitone (±100 cents). No depth knob. Use smooth sinusoidal modulation of both oscillators.
- Pitch wheel is hard-coded to ±1 whole step (±2 semitones / ±200 cents).
- Aftertouch has exactly six selectable targets: cutoff, resonance, oscillator 2 mix, oscillator 1 Alt, oscillator 2 Alt, both oscillators' Alt. Depth is implemented as bipolar (-100% to +100%, default zero). Channel pressure and poly pressure for the current MIDI key are supported. Pressure is smoothed. For resonance, it continuously offsets the selected base value, bounded to 0–0.95; it does not jump through button states. This continuous interpretation and bipolar depth are implementation choices available for audition.
- Each oscillator has amplitude Crush (with a detuned-square exception on Pulse), Alt, noise AM, and its own wave-squish LFO with independent rate and depth controls. This replaces the original pitch-tracking sample-rate reduction at the user's request.
- Resonance is a four-state cycling button. Three pips progressively fill: empty, one, two, three. The underlying normalized resonance settings are 0, 0.1, 0.5, 0.95. The small persistent filter accent is separate from this control.
- Confirmed performance: legato-only glide, no envelope retrigger on overlapping notes, retrigger from current envelope value on a new detached note.
- Master volume precedes an always-on output limiter: ceiling -1.5 dBFS, nominal release 60 ms. Any lookahead latency must be reported to the host. Sample-peak versus true-peak guarantee remains an implementation decision to document and test.

## Proposed treatment behavior

Crush: symmetric amplitude quantization, without pitch tracking or sample-rate reduction. Zero bypasses; maximum produces three levels (-1, 0, +1). On Pulse, the same control instead introduces a second square, detuning it up to one semitone. The pair is weighted equally after a short blend-in near zero; gains sum to one to retain peak headroom, but beating naturally changes loudness. Both square copies receive the oscillator's Alt and Squish treatments. Existing Reduction parameter IDs are retained.

Alt (confirmed): internal hard sync, increasing the slave cycle rate while resetting at the oscillator's nominal fundamental period. This changes harmonic structure while retaining the played fundamental period, rather than simply transposing the oscillator.

Noise AM: bounded unipolar gain modulation; double the original excursion about the mean, preserving base loudness rather than simply amplifying the oscillator. One shared noise low-pass cutoff in Options, 10–1000 Hz, default 80 Hz (original was fixed at 150 Hz). Compensate filtered-noise variance across bandwidth and sample rate so lowering the cutoff does not also weaken modulation.

Squish: compress a complete waveform into the beginning of its nominal cycle and leave the remainder at zero. At depth zero, use the full cycle. Use a smooth unipolar LFO for the compression amount; preserve the fundamental cycle duration. Exact placement relative to Alt/crush/noise AM should be auditioned.

Oscillator 2 tuning: center = unison; the inner half of either side moves continuously through 0–100 cents (one half-step); the outer half snaps in half-step increments through 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 semitones; the endpoint jumps to 24 semitones (two octaves). Negative side mirrors positive. The user's later clarification to "snap at half-notes" (subsequently corrected to half-steps) supersedes the earlier selected-interval proposal.

## Character and signal path proposal

Oscillators with independent slow pitch drift and free-running phases → per-oscillator treatments → gently saturating mixer → resonant low-pass with nonlinear feedback → envelope-controlled amplifier with mild saturation → volume → limiter.

Start with an Erebus-inspired 12 dB/octave low-pass and a small persistent resonance accent. Provide cutoff, resonance, and filter-envelope amount. This is a behavioral design direction, not a verified circuit model. Consider 24 dB/octave only after auditioning the first filter.

Use modest, deliberately tuned oscillator asymmetry, rounded transitions, and level-dependent saturation. Do not assume that arbitrary noise or widespread distortion reproduces analog hardware. Audition before adding a separate compressor; nonlinear stages may already supply the desired peak compression.

Handle aliasing at oscillator/sync discontinuities and nonlinear stages. Oversampling alone is not a complete anti-aliasing strategy. Remove DC introduced by asymmetric waveforms at an appropriate point before it wastes filter/limiter headroom. Audible waveform transforms remain intentional.

## Approved interface and implementation

The user's drawing supersedes the earlier three layout directions. Wide landscape midnight-indigo powder-coated panel with two oscillator groups along the top, sliders for Volume, oscillator-2 level, Crush, Alt and Noise AM; stacked Squish rate/depth knobs. Below are the large cutoff knob, a circular three-pip Resonance button, filter-envelope slider, four ADSR sliders, and the performance/aftertouch group. Aftertouch depth is a small plastic post aligned beneath Portamento. A circular hamburger Options button sits at the lower-right. No title or extra branding is printed on the faceplate. Symbols and short silkscreen labels match the approved concept.

The waveform windows are actual live scopes, synchronized to each oscillator's fundamental cycle and sampled after the waveform treatments, before oscillator-2 level and the shared signal chain. They update when the host processes the instrument. Waveform selection remains accessible by clicking a display or its small round selector, or right-clicking for a menu. Hover supplies an unobtrusive parameter/value readout. Shared noise cutoff lives in Options. Knobs and sliders support fine Shift adjustment, wheel input and double-click default reset. Host parameter names retain classification prefixes (OSC1, OSC2, FILTER, ENV, PERF, OUT, OPTIONS).

The native Win32/GDI+ editor embeds its assets in the CLAP binary. A bounded single-producer/single-consumer queue carries UI automation gestures to processing/flush; scope data crosses back through atomic samples. All native windows and rendering remain on the main thread. Existing control IDs remain stable. State version 4 appends a shared octave shift (-2, -1, 0, +1), defaults it to 0 for earlier patches, and preserves all prior IDs. Version 3 introduced six bipolar envelope depths; versions 1 and 2 load with zero depths.

Each oscillator treatment symbol is a vertical drag target for its own filter-envelope amount (-1 to +1). The filter-shaped envelope, including velocity sensitivity, adds to the treatment base value independently of the filter cutoff envelope amount; the result is bounded to 0�1. White 2-physical-pixel track lines show base-to-modulation endpoint, clipped to the available travel. Shift, wheel, keyboard and default reset also work on the symbols.

Knob rendering uses up to 129 cached raster frames per style, generated on the UI thread. Body geometry and lighting stay fixed in panel space; pointers and molded ribs rotate.

## Open decisions

No remaining questions block implementation. Common envelope attack/release, noise bandwidth, treatment order, aftertouch depth polarity and continuous resonance modulation, and detailed analog coloration remain auditionable implementation choices.

## Research

- Dreadbox Erebus v2 manual: 12 dB/octave low-pass with an always-active pre-resonance accent independent of its normal resonance control; self-oscillation at maximum resonance. https://dreadbox-fx.com/wp-content/uploads/2025/04/Erebus_v2_User_Manual.pdf
- Dreadbox Typhon manual: two analog VCOs, 24 dB/octave resonant low-pass, analog VCAs; waveform control combines oscillator waveforms and routing. https://dreadbox-fx.com/wp-content/uploads/2025/04/Typhon_User_Manual.pdf
- Resident Advisor's firsthand Erebus review describes a relatively gentle filter slope retaining edge from the pre-resonance accent. https://ra.co/reviews/16466
- Bitwig describes CLAP parameter modulation as temporary offsets that preserve the underlying parameter value. https://www.bitwig.com/stories/clap-the-new-audio-plug-in-standard-201/
- CLAP API provides state, parameter, latency, GUI, and remote-control extensions. https://github.com/free-audio/clap

## First implementation milestone

After resolving the waveform and performance semantics, build an auditionable core with host recall and a functional panel. Verify note handling, envelope relationships, tuning mapping, automation, finite output, and limiter ceiling across sample rates before judging sound in Bitwig. Develop visual polish alongside listening iterations. No claim of Dreadbox sonic accuracy until comparison and listening support it.

## Editor revision 0.3.1

Fixed panel size: 1300 x 765 pixels, with host DPI scaling. The circular three-pip octave button below Osc I shifts both oscillators together, preserving Osc II relative tuning. Zero through three pips represent -2, -1, 0 and +1 octaves; the default is 0 (two pips).

The host test now permits full graphics shutdown between sessions. This reproduced a crash from the cached font family returned by GDI+ GenericSansSerif after shutdown/restart. Font families are now locally owned and destroyed before graphics shutdown. Native window destruction also clears its stored handle and callback user data. Regression tests repeat close/reopen/repaint 12 times while the synth remains active, with audio processing between cycles, and cover parent-first destruction.

## Note-transition continuity (0.3.2)

Note-down already preserved main oscillator and LFO phase. The abrupt velocity reassignment could cause an immediate gain/filter/treatment-modulation step even during portamento. A 4 ms exponential velocity smoother now runs at the internal sample rate and feeds all three destinations. Silent attacks initialize velocity directly; legato, note fallback and retriggering over a release tail retain continuity. Hidden-pulse phase no longer realigns to the main oscillator when Crush is zero. All oscillators, LFOs, noise and drift advance during processing without note-triggered resets; explicit host reset/activation still initializes the engine. Tests cover these transitions and rendered-audio gain continuity.

## Squish tail correction (0.3.3)

The compressed-cycle trailing section is -1 for triangle, trapezoid, folded triangle, square + triangle and pulse; saw and half-triangle retain zero. This applies independently to the main and hidden pulse before blending. Crush retains the tail level; Noise AM remains downstream. Waveform definitions, phase continuity, parameter IDs and state format are unchanged.

## Port-zero retrigger mode (0.3.4)

Port at exactly zero retriggers both envelopes from their current levels on every note-on, even with held notes or a repeated key. Any positive Port value retains legato envelope behavior, including sub-sample glide times. The decision uses the current unsmoothed effective control value. Note-off fallback does not retrigger. Oscillator/LFO phase and velocity smoothing are unaffected. No new parameters or state format changes.

## VST3 distribution (0.4.0)

Added a self-contained Windows x64 VST3 folder bundle using pinned clap-wrapper 0.17.0 and VST3 SDK 3.8.0. Both formats compile src/plugin.cpp and share all synthesis/editor code and parameter/state definitions. Only the VST3 build enables wrapper note on/off and note-pressure event translation; the existing CLAP MIDI dialect remains unchanged. Engine held notes retain the source note ID for precise VST3 note-off matching. The wrapper setup rejects unsupported sample rates before altering its last valid setup.

Build-vst3.ps1 runs the existing CLAP checks, VST3 integration checks and official validator, then packages binaries, instructions and notices. Validation: 47 official tests passed; custom tests verify sound parity, events, MIDI expression, automation/state and 12 editor reopen cycles. No global installation or messages to the recipient are performed.

## Vector interface (0.5.0)

Shibboleth now renders panel, recesses, bezel, typography, symbols, buttons and knobs using GDI+ vector primitives, paths and gradients. Existing control coordinates and behavior are retained. Sparse deterministic vector stippling approximates the powder-coat finish. The static faceplate is raster-cached only at the actual host pixel size for repaint efficiency; no source bitmap assets or per-angle knob frames are used. Knob pointers and grooves rotate while lighting remains fixed. The prior PNGs remain as reference material; the RC resource now contains only version metadata. Sound, patch data and Imperius remain unchanged.

## Initial editor size correction (0.5.1)

The fixed panel size is 1300 x 765 physical Windows pixels. guiScale previously multiplied these dimensions by host DPI hints, but newly created editors reset to the unscaled size. Hosts sending a hint only on first creation could therefore show a large first window and a smaller reopened one. Host scaling hints are now explicitly declined per the CLAP GUI contract; the VST3 wrapper forwards that result. Regression tests cover a first-open 150% hint, reopening without another hint, repeated scale requests, native child bounds and a fresh instrument instance with a 200% hint.

## Rapid selector clicks (0.5.2)

Windows replaces the second button-down in a rapid pair with WM_LBUTTONDBLCLK. The prior generic handler reset all parameters to their defaults, causing resonance and octave to alternate instead of cycling. Discrete selectors now treat this message as another ordinary click; continuous controls retain double-click reset. This also covers waveform and aftertouch selectors and the Options button. Regression tests send the Windows rapid-click message sequence through two complete cycles of each parameter selector and verify both values and automation gesture boundaries.

## Live scope cycle boundary (0.5.3)

Scope capture no longer interpolates across phase wrap. The first new sample anchors the beginning of the new cycle and the prior sample finishes the previous one. A pending buffer publishes only complete cycles; the display draws an explicit last-to-first edge at its left boundary. This preserves live post-treatment capture while preventing square-wave wrap interpolation from causing flicker. Tests vary pitch and sample rate and verify that changed oscillator levels continue to appear. Rapid-click tests now include waveform displays and the aftertouch text display in addition to their buttons.


## Filter algorithms (0.6.0)

FilterAlgorithm is appended after Octave to retain existing parameter IDs. State ASYN0005 saves it; versions 1-4 migrate to Original. ALG uses the circular three-pip selector above-right of Cutoff.

Original retains its saturated two-pole state-variable DSP unchanged. Wasp-inspired uses two poles with an asymmetric zero-centered inverter transfer on input, integrator state updates, and output, with positive damping. Ladder and OTA-inspired modes use four trapezoidal low-pass stages, stage saturation, and current-sample global feedback solved with six Newton iterations. They differ in saturation headroom, drive, feedback gain, bass compensation and output coloration. These are character approximations, not component-calibrated emulations. The Sub 37 MultiDrive circuit is not reproduced; OTA corresponds conceptually to Pro 3 LP1, not all three of its filters.

All branches run continuously; convex output weights crossfade with a 5 ms time constant at the oversampled rate. Retargeting mid-transition is supported. Original's reference audition remains byte-identical. Hard all-sound-off clears every filter state. Existing cutoff, envelope, velocity, resonance and aftertouch controls apply to all modes.

Primary research: https://doepfer.de/a124.htm ; Moog Sub 37 User Manual v1.1; https://sequential.com/wp-content/uploads/2021/02/Pro-3-Users-Guide-1.2.pdf .


### Wasp low-cutoff correction (0.6.1)

The original nonlinear state updates were incorrect: applying the offset tanh transfer on every stored-state update produces spurious nonzero attractors as the frequency-dependent integration term gets small. Driven 60 Hz / resonance .5 reproduction at 192 kHz internally latched near (-.472,-.502) even after one second of zero input. The supplied recording's mean shifts from about -.02 to -.22 while AC RMS collapses from .11 to .002. This is DC lockup, not intentional resonance. Envelope sustain offsets effective cutoff, explaining the different panel cutoff threshold.

Use the ordinary trapezoidal state-variable integrator memory updates with positive damping, retain asymmetric input/output coloration, and apply a local 5 Hz DC blocker after Wasp coloration. This prevents the attractor and removes normal distortion rectification without modifying Original, Ladder or OTA outputs. The prior description of coloration on Wasp integrator state is superseded. Tests check driven-then-silent recovery at 20/60/120/500 Hz, three resonance values, four internal sample rates, and bidirectional continuous-drive sweeps.
