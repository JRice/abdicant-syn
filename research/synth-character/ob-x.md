# GForce OB-X: character mechanisms

Inspected November 10, 2026. Static inspection of the installed Windows VST3; no audio renders or listening comparisons were performed. These are architectural research notes, not recovered source or a substitute implementation.

## Binary and evidence trail

- File: `C:/Program Files/Common Files/VST3/GForce/OB-X.vst3/Contents/x86_64-win/OB-X.vst3`
- Size: 10,572,288 bytes.
- SHA-256: `34caf97e30f9f1d2c2c7902dfbe7be5ba7852ab0bab69b3aa51b8b527ab759cb`.
- File/product version and moduleinfo.json report 1.0. The modification date alone is not a release/version identifier.
- Preferred image base: `0x180000000`. Addresses below are static preferred virtual addresses, not live ASLR addresses.

The native code retains class names including OBX8_Voice, OscillatorStage, FilterStage, OBX_FilterEnv, OBX_VcaEnv, and CEM3310. The internal OBX8 name does not establish that this is a model of the physical OB-X8 rather than the OB-X product named by its metadata.

The file also contains embedded object modules with names such as Filter2PoleType1 and setDrive. Their existence alone does not establish that their algorithms run in the synth's main filter. The conclusions here follow native constructor and audio-processing call paths.

## 1. Vintage creates coordinated voice individuality

**Strong evidence: fixed per-voice differences scaled by the Vintage control.**

The voice constructor at `0x1803ab810` initializes a table of sixteen signed coefficients at voice-relative offset `0x40118`. Examples are approximately +0.0632, -0.0844, and +0.0875. These are constants in the constructor, rather than a fresh random choice on each audio sample.

The Vintage callback is identifiable, not merely guessed from its effects:

1. Initializer `0x18000f640` associates the string `Vintage` at `0x18051f380` with identifier storage at `0x18099ba68`.
2. Constructor code around `0x1803ac9ad` binds that identifier to the callback whose dispatch is `0x1803a4b80`.
3. That callback invokes `0x1803a7170`.

The implementation multiplies the incoming Vintage amount by the coefficient selected by voice index. It then propagates related deviations through multiple subsystems. Many scale factors have the form `1 + deviation * scaledVintage`.

Confirmed destinations include:

- Oscillator frequency scaling, and additional oscillator-control scaling.
- Filter control scaling, including quantities that feed cutoff and resonance calculations.
- Filter-envelope and amplitude-envelope time/level scale fields, followed by calls that recalculate their settings.

The oscillator stage slots receive distinct deviation coefficients; the initialization table includes both positive and negative frequency-deviation coefficients. Thus a shared voice-level character value need not move all oscillator properties in the same direction.

The useful design principle is **consistent, correlated tolerances**: a voice can retain a recognizable tuning/filter/envelope personality while one control increases the overall extent. This is different from applying unrelated random modulation to every parameter.

**Limit:** this establishes a static tolerance mechanism. It does not prove an autonomous slow pitch-wander process, nor exclude additional time-varying mechanisms elsewhere.

## 2. Tuning is exponential, with separate offsets and scaling

**Strong evidence: the oscillator's pitch calculation is explicitly visible.**

The native unsynchronized oscillator sample routine is `0x1801a5ca0`; its block caller is `0x1803ee6b0`. Around `0x1801a5df8–0x1801a5e92`, it:

- Combines the current note/pitch value with additional pitch offsets.
- Subtracts MIDI reference note 69 and divides by 12.
- Uses a power-of-two calculation and a reference-frequency field initialized to 440.
- Applies additional exponential modulation and a separate frequency multiplier, including the multiplier altered by the Vintage path.

This supports distinguishing semitone offsets, exponential modulation, and multiplicative calibration. They are not interchangeable: a fixed number of hertz behaves differently across the keyboard from a fixed pitch ratio.

The routine also supports an incrementally changing pitch value before conversion to frequency. The surrounding voice code contains glide/portamento calculations. A random-number call occurs in a portamento-related path (`0x1803a7520`); its result influences the transition length shared by the pitch interpolators. That observation should not be relabeled as continuously wandering oscillator tuning.

**Limit:** the exact perceptual depth of each deviation has not been measured. Full behavior depends on controls, voice index, and modulation configuration.

## 3. Phase persists; sync has its own processing path

**Strong evidence: persistent floating-point phase state.**

The oscillator advances a double-precision phase value and wraps it as cycles complete, retaining it between sample calls. Its saw/pulse processing includes small polynomial corrections around phase discontinuities, consistent with BLEP-style edge correction. That is deterministic edge reconstruction, not evidence of the random edge-jitter mechanism found in Model 84.

A separate sample routine at `0x1801a6160` handles the synchronized case. It reads information from a linked oscillator and tracks additional phase/wrap information. This supports a dedicated oscillator-sync mechanism rather than merely assigning both oscillators the same nominal frequency.

**Limits:** persistent sample-to-sample phase does not prove that phase is always free-running across note-on, note stealing, or every voice mode. A full note-reset policy has not been established. No comparable randomized fractional-edge displacement has been confirmed in these traced OB-X oscillator routines.

## 4. The filter has two integrator states and deliberate cutoff/Q mapping

**Strong evidence: a state-variable-style two-state core.**

The active native filter sample routine is `0x180157e40`, called from the voice audio path at `0x1803a84af` and from a filter-stage processing path at `0x1803f0e32`.

Its control calculation includes:

- A bounded, squared control mapping into cutoff.
- An initialized nominal lower cutoff around 35 Hz and a hard upper cap of 22 kHz in this routine.
- A rational polynomial approximation to the tangent-like frequency transform.
- Two persistent integrator states, each advanced through two calculations per input sample. The first calculation uses the average of the previous and current input; the second uses the current input.

This is consistent with a trapezoidal/state-variable filter advanced in two internal steps. It is stronger evidence than a class-name guess, but does not by itself establish a complete circuit schematic or a whole-plugin oversampling factor.

**Resonance changes with cutoff.** Around `0x18015804c–0x1801580b5`, the routine maps resonance into a Q-like quantity and applies an additional reduction below approximately 750 Hz. The correction grows quadratically as cutoff approaches the lower end of its range. The quantity then enters the state solve through its reciprocal, consistent with damping control.

The sonic implication is a hypothesis to measure: bass-region resonance can have a deliberately different strength/shape from the same knob setting higher in the spectrum. A constant-Q textbook filter would not automatically provide this behavior.

**An important contrast with Model 84:** at fixed controls, the traced OB-X core is linear in its audio input/state. Its control mappings are nonlinear, but this routine does not contain the bounded quadratic/cubic audio-state saturation found in Model 84. Distortion elsewhere in the instrument remains possible; bundled symbols named setDrive do not settle that question.

## 5. What to carry into an original design

| Mechanism | Independent design direction | What has not been established |
|---|---|---|
| Fixed voice coefficient table | Give each voice a lasting personality | Audibility and ideal coefficient ranges |
| Shared Vintage strength | Scale related pitch, filter, and envelope tolerances together | A complete physical aging model |
| Different signed oscillator coefficients | Make paired oscillators deviate in distinct, correlated ways | Exact beating trajectories for each patch |
| Separate pitch offsets and ratios | Control keyboard scaling of tuning imperfections | A measured autonomous slow-drift spectrum |
| Persistent phase and dedicated sync | Design note-reset policy separately from tuning spread | All note-on/voice-stealing reset conditions |
| Cutoff-dependent damping | Tune resonance musically across the keyboard | Measured Q, gain, and stability across the full range |
| Two-step state-variable processing | Explore internal integration quality separately from saturation | Full-plugin sample-rate/oversampling policy |

The most distinctive lesson from this pass is that compelling variation can be **structured and repeatable**. Randomness is one possible ingredient; a calibrated voice identity and deliberate response mapping are separate tools.

## Useful verification with audio

To determine which mechanisms dominate the perceived character: disable effects and intentional modulation, cycle repeated notes through the voices at several Vintage settings, and estimate pitch and envelope trajectories. Long held notes would distinguish fixed offsets from slow wander. Filter sweeps at several resonance settings would test the low-frequency damping change. Input-level sweeps would establish where any nonlinear audio behavior actually occurs.

The analysis above has not performed those measurements, so it does not rank the mechanisms by perceptual importance.
