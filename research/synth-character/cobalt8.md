# Cobalt8VST and COBALT8 2.1: oscillator research

Inspected November 10, 2026. Read-only static analysis of the supplied macOS plugin and three firmware files. No plugin or firmware was executed and no hardware was accessed. This entry identifies representative oscillator mechanisms; it is not a complete derivation of all 40 algorithms or an audio-quality measurement.

## Where the algorithms actually live

Cobalt8VST 0.4.9 is a universal Mach-O binary containing both x86_64 and arm64 slices. The supplied executable's SHA-256 matches the previously extracted workspace copy exactly. Architecture is therefore not a barrier to inspection; both slices retain extensive C++ symbols.

This plugin emulates the instrument's processors and peripherals and executes external firmware. It is not simply a collection of native macOS oscillator implementations. The traced path is:

```text
JUCE processBlock -> MIDI and parameter queues / buffered audio read
    -> engine worker -> emulated COBALT8 execution
    -> master + two slave processor instances + panel processor
    -> emulated audio interface -> captured audio -> host buffers
```

Evidence includes the Intel `Engine::start` calling `load_firmware_dir` at `0x5b366`, then constructing `Cobalt8` at `0x5b5d0`. `Cpu` construction calls Unicorn's `uc_open`, installs memory-mapped peripheral regions, and registers execution hooks. The engine loop calls `Cobalt8::run` at `0x60bb7` and `pull_audio` at `0x60d77`.

The Cobalt8 constructor creates a master SamE70 and two additional SamE70 instances named slave0 and slave1. It loads the same slave firmware into both. A Sam4e instance handles the panel. The firmware load address is `0x410000`. Corresponding construction, firmware-loading, and audio-read calls were also checked in the arm64 slice; these findings do not depend on treating the Intel slice as a different product.

The supplied `.syx` files are firmware update containers, not just patch presets or collections of waveform samples. The plugin's decoder scans SysEx packets, checks their header and packet sequence, and unpacks the seven-bit MIDI representation into program bytes. Following that decoder produced:

| Image | Packets | Decoded size | Role observed in emulator |
|---|---:|---:|---|
| Cobalt8_master_2-1.syx | 3,275 | 419,200 bytes | Master program |
| Cobalt8_slave_2-1.syx | 2,294 | 293,632 bytes | Same program loaded into both slave instances; contains oscillator renderers |
| Cobalt8_panel_2-1.syx | 1,917 | 245,376 bytes | Panel program, including algorithm display names |

The recovered vector tables and valid Thumb/VFP instructions agree with that load address. Input hashes and decoded-image hashes are in [cobalt8-manifest.json](cobalt8-manifest.json). Firmware addresses below refer to the slave's own emulated address space, not Mach-O addresses.

## Forty dedicated algorithm cases

At slave address `0x439490`, a dispatcher accepts an algorithm index from 0 through 39 and selects a renderer. Its branch table and literal function pointers were resolved and matched by index to the panel's 40 fixed-width algorithm names. The full mapping is in [cobalt8-algorithm-map.csv](cobalt8-algorithm-map.csv).

This is stronger evidence than finding UI labels alone: there are distinct executable render paths. Some share building blocks and patterns, so 40 cases should not be interpreted as 40 unrelated synthesis inventions.

| Representative mode | Index | Renderer | Observed oscillator-state count |
|---|---:|---|---:|
| VA Sweep | 0 | `0x416b44` | 2 |
| Spread Saw | 2 | `0x418ac0` | 4 |
| H-Sync Saw | 10 | `0x41c498` | 2 |
| FM Saw Squ | 29 | `0x4388f0` | 2 |
| Chaos Saw | 35 | `0x435ca0` | 2 |
| Fold Tri | 38 | `0x41be90` | 1 |
| Filt Noise | 39 | `0x43161c` | 1 |

A byte table at `0x456998` supplies oscillator-state counts used by the renderers. Four-phase processing is directly visible in Spread Saw. These counts concern internal oscillator states within a group, not polyphonic voice count. A renderer argument selects which of the two groups' state to use.

Modal's [official product documentation](https://modalelectronics.com/cobalt8/) independently describes two oscillator groups, up to four oscillators per group, and 40 modes. Its descriptions of each A/B control are useful context; they are not substitutes for tracing the corresponding calculations.

## Wave generation combines arithmetic, a sine table, and event corrections

The selected renderers use persistent floating-point phases and phase increments. They advance phase, detect crossings, and preserve state between calls. Their main loops write blocks of 128 samples.

VA Sweep is not just playback of an arbitrary bank of complete sampled waves. It includes algebraic waveform expressions and piecewise morph regions, plus sine lookup. At `0x416ef4–0x416f44` and `0x417024–0x41706e`, phase is scaled to a 1024-entry grid; adjacent indices wrap modulo 1024 and their samples are linearly interpolated. The addressed table at `0x4501bc` numerically matches one cycle of sine, with maximum error about `5.2e-7` against `sin(2*pi*k/1024)`.

This establishes a hybrid approach: inexpensive lookup for a smooth shape, arithmetic for other shapes, and additional calculations where those shapes have edges or corners. It does not establish a large mipmapped wavetable bank.

### Fractional events receive polynomial corrections

The renderers do considerably more than wrap phase and emit a naive ramp or square. For example, Spread Saw at `0x418d9c` detects a wrap, subtracts a cycle, and multiplies the residual phase by the reciprocal phase increment. That obtains an event's fractional position within the sample interval. It evaluates several polynomials of that fractional variable and adjusts persistent correction terms before forming the output.

VA Sweep and the hard-sync/FM paths contain related event-sensitive polynomial calculations. Coefficients include familiar fractional constants, but the constants alone are not sufficient to identify a named implementation. The data flow—fractional crossing time, event corrections, and carried correction state—is the important evidence.

**Interpretation:** this is strongly consistent with polynomial bandlimited step/ramp correction, broadly in the BLEP/BLAMP family. The exact kernel, support, delay, and alias-rejection properties have not been fully derived. It should not be labeled definitively as the common two-sample polyBLEP algorithm. These polynomials are involved in waveform construction and transition correction; their presence is not evidence of analogue saturation.

For original designs, the lesson is to treat a wrap, pulse edge, slope change, or sync reset as a timed event with a correction appropriate to the discontinuity. This is especially relevant when modulation makes event times move between samples.

## Spread Saw: four separate phases and a designed blend

The renderer at `0x418ac0` obtains four sets of phase/increment/correction state, then explicitly processes all four phase accumulators in its sample loop. It does not generate one saw and make delayed copies of its output to obtain the four components.

At `0x419108–0x419164`, a normalized control addresses two 128-step gain mappings with interpolation. One mapping controls the central component; the other weights the three companions. The endpoints move from a central-only blend toward roughly equal contributions. The central component remains present. The curves shape the transition rather than applying one linear gain to the whole group.

The weighted sum is visible at `0x419034–0x419090`. Separate persistent increments allow relative phase motion between components. However, this inspection did not completely trace the pitch/spread recipe that populates every increment, so exact cent offsets, interval transitions, and any slow drift should not be inferred from the four phases alone.

**Design idea:** independently design the detuning/interval map and the central-versus-companion gain map. Four oscillators plus a carefully chosen blend can provide a more controllable change in density than increasing a generic unison detune amount.

## FM Saw:Square: phase integration changes inside the sample

The renderer at `0x4388f0` keeps separate carrier and modulator phase state. It computes two carrier increments around the base increment. When the square modulator crosses a transition inside a sample, the code uses a fractional transition position to divide the carrier's advance between the old and new increments. See `0x438bd4–0x438c18`.

There are explicit branches for negative phase motion and boundary crossings, followed by fractional-event correction calculations. Thus this is actual phase-advance modulation with detailed event handling, not merely an output waveshaper that visually resembles FM. The complete control range and all sideband behavior were not measured.

**Design idea:** when an audio-rate modulator changes a carrier's velocity mid-sample, account for both portions of that sample. Using the modulator's value only at a sample boundary discards that timing information.

## Fold Tri: an antiderivative-style antialiasing calculation

Fold Tri at `0x41be90` generates its underlying waveform and applies a nonlinear folding path. Particularly informative is `0x41c210–0x41c28c`:

- It forms a current-versus-previous input difference.
- It evaluates a piecewise function using periodic reduction, absolute value, and quadratic arithmetic.
- It subtracts that function's previous value and divides by the input difference.
- For input differences below roughly `1e-4`, it takes a separate fallback path rather than dividing by a nearly zero value.
- It saves the current input and function value for the following sample.

**Interpretation, high confidence:** this is the characteristic first-order antiderivative antialiasing (ADAA) structure, conceptually `(F(x[n])-F(x[n-1]))/(x[n]-x[n-1])`. The derivative of the folded piecewise-quadratic function is a piecewise-linear folding law. This differs from applying that folding law independently to each sampled input. The exact end-to-end alias reduction and delay have not been measured.

For the general method, see Parker, Zavalishin, and Le Bivic's [DAFx 2016 paper](https://www.dafx.de/paper-archive/2016/dafxpapers/20-DAFx-16_paper_41-PN.pdf). That paper supplies public background for original implementations; it is not evidence of this firmware's authorship or provenance.

## Chaos Saw: randomization reaches phase advance

At `0x436024–0x436078`, Chaos Saw advances a pseudorandom integer state, converts part of it into a normalized floating-point value, combines that with a control-dependent expression, and uses the result in a secondary oscillator's phase increment. The subsequent phase advance and wrap handling are explicit.

This establishes randomized oscillator timing within this particular algorithm. It should not be conflated with the instrument's general Drift feature, ordinary vibrato, or a claim that all 40 algorithms constantly randomize tuning. The full stochastic distribution and update cadence have not been characterized.

## What this adds to the library

The strongest reusable findings are fractional-event handling, hybrid arithmetic/lookup waveform construction, independent phase states with curved blend weights, and antialiased nonlinear folding. These mechanisms help explain how a compact set of basic waveforms can support many distinct and controllable synthesis behaviors.

Unresolved: a complete mathematical reduction of every mode; global drift and phase-reset policy; the entire pitch/spread map; behavior at algorithm changes; filter/effects contributions; and measured spectra or perceptual comparisons. No claim that one observed technique is the dominant reason the instrument sounds good follows from this static analysis.
