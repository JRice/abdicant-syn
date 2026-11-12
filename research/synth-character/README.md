# Synth character research library

Architectural notes from static inspection of locally installed synthesizers, intended to inform original sound designs. Each entry distinguishes directly observed calculations, engineering interpretations, and questions that require audio measurements. No claim that an identified mechanism is audible should be inferred from its presence alone.

| Instrument | Strong findings | Unresolved |
|---|---|---|
| [Softube Model 84](model84.md) | Four coupled filter states; asymmetric nonlinear state update; randomized fractional oscillator-edge timing; 2x processing in the inspected 48 kHz path | Autonomous slow tuning drift, exact circuit/component identities, perceptual contribution |
| [GForce OB-X](ob-x.md) | Fixed voice identities scaled by Vintage across pitch/filter/envelopes; exponential tuning; dedicated sync path; two-state filter with cutoff-dependent damping | Autonomous slow tuning drift, complete note-reset policy, location/extent of audio saturation |
| [Softube Model 82](model82.md) | Stateful pre-filter transfer curves; asymmetric nonlinear four-state filter; later stereo-path transfer curves; randomized oscillator-edge timing; 96 kHz timestep in the 48 kHz path | Exact circuit identities, complete Drive-control routing, slow tuning drift, perceptual contribution |
| [Cobalt8VST + COBALT8 2.1 firmware](cobalt8.md) | Firmware emulator; 40 oscillator dispatch cases; four-phase Spread Saw; interpolated sine lookup and waveform morphing; fractional-event polynomial corrections; ADAA-style folding; within-sample FM event handling | Complete derivation of every mode, global drift implementation, full pitch/spread maps, measured alias rejection |

## Reusable ideas to investigate

1. **Voice identity:** persistent differences in tuning, envelopes, and filter calibration.
2. **Correlation:** allow a common tolerance value to affect several properties, with distinct signs and sensitivities.
3. **Time scale:** distinguish static offsets, slow drift, vibrato, fractional-edge jitter, and deterministic edge correction.
4. **Nonlinearity location:** distinguish nonlinear audio-state behavior from nonlinear control curves and output saturation.
5. **Resonance mapping:** examine how damping and gain change across cutoff, input level, and voice identity.
6. **Phase policy:** treat continuity, note-on reset, and oscillator sync as independent design choices.
7. **Nonlinear memory:** compare a static transfer curve with parallel curves driven by signals smoothed on different time scales.
8. **Gain staging:** change level before the filter independently of output loudness; compare pre-filter, internal-state, and post-filter nonlinearities.
9. **Oscillator event timing:** handle wraps, waveform corners, sync resets, and modulator transitions within the sample interval, with matching correction terms.
10. **Antialias nonlinear synthesis:** investigate antiderivative methods for wavefolding, alongside correction methods for discontinuities in oscillator waveforms.
11. **Curated macro controls:** combine phase relationships, interval choices, and curved blend weights into a small number of musically useful controls.

Evidence applies to the exact installed builds recorded in the entries. The notes contain no replacement DSP implementation.
