# Model 82: DSP inspection notes

Inspected November 10, 2026. Static, read-only inspection of the installed Windows binaries. No plugin execution, audio rendering, modification, or license-system changes. Findings below describe observed numerical operations and their architectural implications; they do not establish which mechanism contributes most to the perceived sound.

## Engine and scope

The supplied VST3 is an 11,337,000-byte shell. The installed engine is the 88,705,840-byte module at:

`C:/Program Files/Common Files/Softube/SSX/Model 82 Sequencing Mono Synth.ssx/Contents/x86_64-win/Model 82 Sequencing Mono Synth.ssx`

Its Resources/PluginInfo.spi contains version 2.6.41. File sizes and SHA-256 hashes of the shell, engine, and supporting Model 82 VCF module are recorded in [model82-manifest.json](model82-manifest.json).

The engine retains RTTI for six sample-rate variants: n44a, n48a, n88a, n96a, n176a, and n192a. The detailed trace here follows n48a. Addresses are preferred virtual addresses at image base `0x180000000`, not runtime addresses under ASLR.

The n48a processing wrapper at `0x18006e0d0` calls `0x180172554`, a thunk into the main DSP routine at `0x1801689c4`. The preparation wrapper at `0x18006bff0` reaches the parameter preparation routine at `0x18016e590`. This separates audio calculations from parameter remapping and smoothing.

## Main finding: distributed nonlinearity with memory

The inspected audio route includes the following ordered operations. This is a partial computational map, not an identification of individual hardware components:

```text
oscillator / source processing and mixing
    -> level scaling
    -> four-branch stateful transfer-curve stage
    -> coupled four-state resonant filter, with nonlinear state updates
    -> further filtering, gain processing, and delay/doubling operations
    -> later transfer-curve stage, with separate left/right state
    -> further output processing
```

The pre-filter stage and filter-state nonlinearity give direct support to investigating saturation throughout the signal path. Their presence does not establish that every stage is appreciably distorted at normal patch levels, or that they dominate the instrument's appeal.

### Before the resonant filter: transfer curves fed by different histories

At `0x18016d92f–0x18016d947`, the signal buffer is scaled by approximately 0.0443048. At `0x18016d955–0x18016d98f`, each sample then goes through helper `0x180162d20`, with four branches. Its output replaces the sample in the same buffer. The following loop passes that buffer into the resonant-filter kernel at `0x18016da15`.

The helper first bounds its input near -1 and +1 in its own normalized coordinates. For each branch it updates a persistent smoothed value, uses that value to address a branch-specific transfer curve with linear interpolation, and sums the four curve outputs. The smoothing update has the familiar form:

`s_i[n] = a_i*s_i[n-1] + (1-a_i)*x[n]`

The state update is visible at `0x180162df7–0x180162e12`; interpolation and summation follow. Thus this is not simply a lookup table addressed by the current sample alone. The numerical initialization banks contain differing smoothing coefficients and nonlinear, asymmetric transfer curves. Matching long runs of curve data also occur in the standalone VCF module, providing a useful cross-check.

**Interpretation:** this construction can produce frequency-dependent nonlinear behavior and history-dependent transient response. Two signals with the same instantaneous amplitude can encounter different branch states because their preceding samples differ. This is a useful original-design idea: combine nonlinear paths with different response speeds rather than putting one static soft clipper on the output.

It does not prove magnetic hysteresis, transistor charge storage, or any particular physical component. The table coordinates and the preceding scaling factor are internal numerical units, not documented circuit voltages. Exact table-to-initialization-offset binding was not completed, so the branch time constants are deliberately not reported as established values.

### Inside the filter: four coupled states and an asymmetric bounded polynomial

The sample kernel at `0x180167bc4–0x180167fd1` reads and updates four persistent floating-point states. Cutoff, resonance, and timestep enter a coupled rational calculation. This supports a fourth-order resonant-filter interpretation; it does not prove four identical cascaded one-poles or identify an exact transistor-level circuit.

At `0x180167ee4–0x180167f4c`, a difference between the computed solution and old state is normalized, bounded, passed through a polynomial, rescaled, and added to the old state. This happens to the persistent states, not merely to the final audio sample.

The local polynomial is of the form `u + a*u² + b*u³`. The corresponding initialized coefficient sequence has unequal bounds of approximately -0.622 and +0.536, a quadratic coefficient around -0.13, and a cubic coefficient of -1. The sequence was checked in both the standalone VCF's initialized state and the full synth's decompressed initialization data; in the full n48a state stream at `0x1820f2ba0` it occurs at decompressed offset `0x1b88`.

**Interpretation:** the nonzero quadratic term and unequal bounds establish asymmetry in this local state-update operation. Nonlinear behavior can interact with the filter's ongoing resonant motion. That is materially different from a linear resonant filter followed by output clipping. Even/odd harmonic balance, resonance compression, and intermodulation are sensible measurement targets, but their magnitude was not measured here.

The polynomial coefficients cannot be compared directly with Model 84's to say one instrument is more distorted. Their internal state normalization, input levels, and operating trajectories differ. The reusable feature is the architecture, not the coefficient magnitudes.

### Later in the path: another stateful nonlinear stage

After the resonant-filter loop, the main routine contains additional filtering and gain operations, followed by circular-buffer/fractional-delay processing consistent with the instrument's doubling section. Later, calls at `0x18016e08a` and `0x18016e103` use the same four-branch transfer helper on two audio buffers with separate state vectors. Each buffer is first scaled by approximately 0.025. This establishes a further nonlinear processing location after the filter, with independently maintained channel histories.

The Drive parameter is present in the engine and is remapped/smoothed in the preparation routine, including `0x1801706d6–0x180170745`. That control mapping is not itself proof of audio distortion. A complete dependency trace from the knob to a particular transfer bank, gain, or blend was not completed; the later table stage should not be labeled exclusively as the Drive effect on this evidence alone.

Softube separately documents Drive as an added output overdrive with a fuzz character, and Doubling as an added stereo effect. Those product features should be distinguished from the pre-filter and internal-filter mechanisms above. See the [official Model 82 manual](https://www.softube.com/user-manuals/model-82-sequencing-mono-synth), Detailed Controls and Overview.

## Oscillator variation: small edge-timing perturbations

The oscillator sample kernel is at `0x180167fd4–0x1801689c3`, called from the main routine at `0x18016cefb`. It contains integer phase/timing calculations and pseudorandom perturbations of fractional waveform-edge positions.

At `0x18016831e–0x18016837a`, a pseudorandom integer update is reduced modulo five and shifted to yield one of -2, -1, 0, +1, or +2. Those small offsets are added to fractional edge indices, with bounds of 0 through 1023. This is the same general timing mechanism identified in Model 84. The index grid belongs to edge placement; it should not be read as a 1024-step quantization of an entire oscillator cycle or converted directly into cents.

**Interpretation:** the oscillator has a confirmed source of fine timing variation even with only one voice. It is distinct from voice-to-voice detuning, a smooth slow pitch wander, and deliberate LFO modulation. Its audible strength has not been established.

This inspection did not establish an autonomous slow tuning-drift process, a complete note-on phase-reset policy, or a separate all-pass/phase-shifting character stage. Failure to establish those features is not evidence of their absence.

## Internal processing rate

The n48a path loads approximately `1/96000` at `0x18016b529` and `0x18016bdf7`, passing that timestep into the oscillator and filter sample kernels. This supports a 96 kHz internal timestep for the inspected 48 kHz path. It is consistent with 2x internal processing, but does not establish the factors used by every other sample-rate variant or every effect stage.

## Additions to the original-design idea library

1. **Give saturation memory.** Explore several smoothed signal histories feeding different nonlinear curves. Design independent curves and response times, then listen for changes in attack and sustained tone at matched loudness.
2. **Place nonlinearities deliberately.** Compare pre-filter saturation, nonlinear filter-state behavior, and post-filter saturation separately. They affect harmonics, resonance, and articulation at different points.
3. **Expose input level as a timbre control.** With nonlinear processing before and within a filter, changing source-mixer levels can change timbre even when output loudness is compensated.
4. **Use asymmetry as a controlled variable.** Investigate the harmonic consequences of asymmetric state updates and transfer curves without treating arbitrary numerical offsets as physical voltage measurements.
5. **Separate kinds of instability.** Tiny edge-timing variations, slow pitch drift, vibrato, and static tuning offsets are different mechanisms. A monophonic design can explore each independently.

Useful follow-up measurements would be level sweeps into a fixed filter, resonance sweeps at several mixer levels, transient-versus-steady-tone comparisons, and long isolated-oscillator pitch recordings. Drive and Doubling should be varied independently. Those experiments would quantify the mechanisms' audible contribution; the present static inspection establishes their numerical structure and ordering.
