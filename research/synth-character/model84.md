# Model 84: DSP inspection notes

Inspected November 10, 2026. Static, read-only inspection of the installed Windows binaries; no plugin execution, audio rendering, modification, or license-system changes. These notes describe observed calculations and architectural interpretations, not recovered source code. Audibility has not been measured.

## What was inspected

The supplied VST3 is an approximately 11.3 MB shell. Its PluginInfo.spi identifies version 2.6.41. The shell contains the installed-program search paths and dynamic-library loading infrastructure rather than the relevant synth DSP names.

Following that installed module layout located the actual engine at:

`C:/Program Files/Common Files/Softube/SSX/Model 84 Polyphonic Synthesizer.ssx/Contents/x86_64-win/Model 84 Polyphonic Synthesizer.ssx`

The neighboring Model 84 Filter and Model 84 Oscillator SSX modules provide smaller, easier-to-follow versions of related DSP. Crucial observations below were checked against corresponding routines inside the full synth, rather than assuming that standalone modules prove the synth's implementation.

File sizes and SHA-256 hashes are recorded in model84-manifest.json. The supplied VST3's SHA-256 is `bd2a6530792ad17a4735cf3d4c57211b0fef88c5f59364b20fde82606accf381`.

The binaries retain RTTI names for six sample-rate variants: n44a, n48a, n88a, n96a, n176a, and n192a. Class tables and processing wrappers provide the route into the DSP. Addresses below are preferred virtual addresses with image base 0x180000000; they are not runtime addresses under ASLR.

## Filter: observations with strong static evidence

**Four coupled states.** The full synth's n48 filter kernel at `0x1801764e8` reads four floating-point state values, computes a coupled cutoff/resonance-dependent update, and writes four state values back. This is consistent with a fourth-order resonant filter. It is not enough to identify an exact original circuit or individual transistor model.

**Nonlinearity participates in the state update.** At `0x180176814–0x180176870`, an intermediate difference between the computed solution and previous state is scaled, bounded, passed through a polynomial, rescaled, and added to the previous state. The result becomes the next persistent state. The same structural operation occurs at `0x18009e3c5–0x18009e40e` in the standalone Filter module.

The local polynomial has the form `u + a*u^2 + b*u^3`, applied after bounding the local variable. This equation describes the local numerical operation; u is an internal normalized variable, not a documented voltage at a hardware test point.

**Asymmetry is explicit.** The standalone Filter's initialized state data contains unequal bounds of approximately -1.70 and +1.21 and nonzero quadratic and cubic coefficients of approximately -0.0119 and -0.1615. The matching coefficient sequence also exists in the full synth's initialization data, including the n48 state-data stream at `0x1821fa920`, decompressed offset `0x1cf0`. These are ordinary zlib-compressed numerical initialization tables referenced by constructors.

The nonzero quadratic term and unequal bounds support asymmetric internal nonlinear behavior. They do not establish a particular DC bias voltage or the identity of a modeled circuit component. A plausible sonic consequence is a mixture of even- and odd-order distortion, with resonance and level interacting through the persistent state. Quantifying the audible result requires measurements.

**Cutoff and resonance enter a joint rational calculation.** The n48 kernel constructs a fourth-degree denominator. Calling its local scaled cutoff variable g and its third floating argument r, the observed expression is:

`(1 + 10*r)*g^4 + 16*g^3 + 72*g^2 + 128*g + 80`.

The r=0 polynomial factors as `(g + 10)*(g + 2)^3`. This is a useful mathematical fingerprint of the calculation. It supports looking beyond four identical textbook one-pole stages. It does not by itself map those factors to physical pole frequencies, resistors, or component tolerances. The caller's normalization and the state representation matter.

**The inspected 48 kHz path uses a 96 kHz timestep.** The synth passes approximately 1/96000 into both the oscillator and filter kernels, for example at `0x18017a621` and `0x18017af1b`. Together with the surrounding buffer expansion, this supports 2x internal processing in this sample-rate path. Do not generalize that oversampling factor to every supported sample rate without checking its path.

The practical design lesson is to investigate nonlinear state/feedback behavior, internal headroom, and coefficient mapping together. An output waveshaper alone would not reproduce the observed internal mechanism.

## Oscillator tuning, timing, and phase

**Persistent integer phase.** The n48 oscillator kernel at `0x1801768f8` loads an integer phase state, computes a quantized increment from the supplied frequency and timestep, advances the accumulator, handles pulse/wrap events, and stores state for the next invocation. Comparable code is at `0x18009dfb8` in the standalone Oscillator module. This is evidence for a clock-like, numerically controlled timing core; it is not evidence that a physical hardware clock is being simulated cycle by cycle.

**Small randomized edge timing is directly visible.** At `0x180176c37–0x180176c99`, a per-instance pseudorandom state advances, its result is reduced modulo five and centered by subtracting two. The resulting offset, in {-2,-1,0,1,2}, is added to two fractional event positions. Those positions are bounded to 0–1023 and used in the subsequent event-correction table machinery.

This supports describing a small randomized displacement of waveform edge timing. The displacement occurs in fractional event/correction positions, rather than as an obvious random addition to the main phase increment at this point. It could contribute to oscillator texture, but static inspection cannot establish its perceptual importance or the developer's intended physical interpretation. In particular, do not equate it with slow thermal pitch drift.

**Phase continuity and note reset are separate questions.** The kernel maintains phase between invocations and also contains an incoming control-edge branch that can realign the oscillator. Identifying that mechanism does not prove how often the voice-control code triggers it in every mode.

Softube's manual independently documents that UNISON Mode can select synchronized note-on resets or free operation; UNISON Spread controls the relative tuning of voices. This provides a supported explanation for changes in attack and beating without assuming constant random detuning. The global Tune control is documented as an overall offset.

**Slow pitch wander remains unresolved.** The surrounding engine contains random generators, smoothing/filtering, lookup-table mappings, and six repeated voice paths. I have not assigned all those intermediate buffers to named pitch controls or established the rate and depth of an autonomous slow tuning process. Neither the existence of random generators nor a search hit for the word drift would establish that. Some drift strings elsewhere in these files belong to unrelated infrastructure.

## What these findings suggest for an original synth

- Keep slow tuning movement, static voice detuning, initial phase, and fractional edge jitter as independently controllable mechanisms.
- Explore a nonlinear four-state filter whose saturation affects state evolution, and listen to how that changes resonance with signal level.
- Explore controlled asymmetry in the internal nonlinear law rather than treating vintage character as a single symmetric distortion curve.
- Evaluate oversampling around the nonlinear stages and oscillator event generation.
- Treat chorus, PWM, and note-reset behavior as additional sources of movement; their presence does not demonstrate pitch drift in the oscillator itself.

These are independent design directions informed by observed architecture, not a recommendation to transplant Model 84's coefficients.

## Limits and useful measurement follow-up

This inspection identifies mechanisms, not how much each contributes to the sound you like. It does not yet establish transistor-level simulation, an exact circuit schematic, physical bias voltages, full cutoff calibration, all voice-to-voice offsets, or a complete tuning/drift model.

A useful next measurement pass would render long single notes with chorus/PWM/vibrato disabled, repeat notes across voice allocation cycles, compare free and synchronized unison, and sweep input level and resonance. Frequency trajectories would separate slow wander from stationary timing noise; repeated attacks would expose reset and voice differences; harmonic/intermodulation measurements would test the filter asymmetry and level-dependent resonance interpretation.

Public context used: [Softube Model 84 manual](https://www.softube.com/se/user-manuals/model-84-polyphonic-synthesizer). The manual supports the control descriptions, not the internal polynomial or jitter findings, which come from this binary inspection.
