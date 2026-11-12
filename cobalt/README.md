# Cobalt Editor — Windows hardware controller

**0.1.0 alpha is built as CLAP and VST3.** See [INSTALL.md](INSTALL.md) for
Bitwig routing, controls, first-use steps and current limitations.

Run `./cobalt/build.ps1` from the repository root to build, test, validate and
package `cobalt/build/CobaltEditor-0.1.0-Windows-x64.zip`. The ZIP contains both
formats, installation notes and licenses; it contains no firmware, MODALapp,
original plugin binary, analysis tools, or captured audio.

The native fixed editor is 1440 × 1080 pixels. Its 113 parameters have stable
IDs derived from their internal hardware parameter addresses. Values, MIDI
channel, selected port name and enabled-connection state are versioned in the
plugin state. New instances start disconnected. State restore can reconnect
and replay all **managed** parameters without an open UI or audio processing.
This does not yet establish complete hardware-patch recall: LFO sync toggles,
matrix-enable flags and some secondary settings remain unmapped.

Validation: custom CLAP and VST3 host tests, guarded mock-MIDI transport tests,
Python byte-encoding tests, native UI rendering/reopening, and Steinberg's
validator (47 passed / 0 failed). Physical hardware and Bitwig validation are
still pending. Logs and the real native-editor capture are in `cobalt/build`.

## Design and protocol notes

Target: Windows 11 / Bitwig; COBALT8X OS 2.1; USB MIDI. Argon is out of scope.

Original editor target: approximately 1500 × 1000 logical pixels, to be checked
against Windows display scaling. All three envelopes, all LFOs, the flexible
modulation matrix, fixed routes, and three effects remain visible. No arp,
sequencer editor, or hardware preset librarian. Oscillator graphics are
illustrations driven by plugin parameter state, not measured waveforms.

Accepted layout refinements: knobs for the filter/voice panel, effect parameters
and LFO rates; thin horizontal sliders elsewhere; vertical ADSR plus depth
faders on all three envelopes. Filter/modulation envelope depths are bipolar.
Knob values are hidden at rest and shown in a compact transient popup during
pointer or keyboard adjustment. Knob labels sit to the left, except on the
filter/voice panel where labels sit below the knobs. Effects use a compact
three-column/two-row control arrangement.
The web study was approximately 1230 pixels tall. The native implementation
uses a tighter 1440 × 1080 layout with every managed parameter visible.

Envelope type selectors must include all eight OS 2.1 types: Expo, Snappy,
Soft, Linear, and a Long variant of each. Long variants double the maximum
stage times; maximum Long release is 10 seconds (manual p.32). The preview
uses different illustrative attack/decay/release curves, scaled to each
graph's width, and labels the Long time behavior. Curve coefficients and
parameter-to-seconds scaling are not firmware-verified. Graphs reflect ADSR,
type and depth, including inverted filter/modulation envelope polarity.

The DAW owns the patch: stable automation IDs, versioned plugin state, and
paced replay of parameter state on project/preset restore. Provide Send All
for reconnects. Restore effect types before effect parameters, and sources /
destinations before final modulation depths. Do not transmit global MIDI
filters, firmware commands, preset writes, resets, or calibration operations.

## Routing design

First investigate output-only USB ownership: the plugin opens the COBALT8
output for notes and parameters; Bitwig keeps the COBALT8 input for keyboard,
wheel, sustain, and pressure. This may avoid taking the keyboard input away
from Bitwig, without a MIDI-input relay. Bitwig must not simultaneously open
the same hardware output. Needs real-host validation; no ports were opened
during initial inventory. Direct MIDI output needs a worker queue with
bounded/coalesced automation, complete non-interleaved extended transactions,
note-off priority and an explicit unavailable/busy state. Audio still enters
through the user's audio interface.

## Current evidence

- Original installer: Cobalt8VST 0.4.9, AU/VST3/app, universal macOS binaries.
- Cobalt8VST contains JUCE 8.0.10 and named parameter-MIDI routines.
- Supplied MODALapp is a Windows PE VST3 binary.
- Read-only Windows WinMM inventory found `COBALT8` input 1 and output 2.
  Device indices are session-specific; persist names/identifiers, not indices.
- An isolated MODALapp controller reports zero parameters without a discovered
  synth. An offline inventory alone cannot reveal its dynamic parameter list.
  The probe guards MIDI enumeration, port opening and sending before InitDll.

## Documented controls

COBALT8 OS 2.1 manual:
https://cmi.com.au/Files/Files/Docs/MODAL/cobalt8_MANUAL_v2.pdf

The CC chart covers envelope ADSRs/types, oscillator algorithms/parameters,
filter controls, LFOs and matrix slot depths (88–95), sources (100–107), and
destinations (108–115). Address coverage does not establish source/destination
enum values, scaling, reserved values, or bipolar center conventions.
The manual describes eight flexible slots plus four fixed routes. Reconcile
these with the owner's requested two fixed-source slots before final layout.
The three effects' complete parameter sets are absent from that CC chart.

## Extended protocol lead — inferred, NOT hardware verified

Static inspection of `c8::param_midi(int,int,int)` at Intel image address
0x5ea70 in Cobalt8VST 0.4.9 suggests four CC messages per extended write:

```
Bn 63 group
Bn 62 section
Bn 26 value
Bn 06 field
```

All numbers in this block are hexadecimal. `n` is the zero-based channel.
The value is sent on CC38 before the final CC6 field selector. This is NOT
ordinary 14-bit NRPN data-entry ordering; a generic NRPN helper is unsuitable.

Effect candidates: group 0x22; section 1/2/3 selects effect slot; field 0..6.
Internal IDs 128..148 split into three consecutive seven-field groups. The
editor's effect-type control uses the start of each group. Field 0 appears
to select the algorithm; fields 1..6 appear to be algorithm-specific controls.
Parameter ranges, exact effect IDs and per-algorithm labels remain unverified.

OS 2.1 firmware labels resolve group 0x20, section 0x0c, fields 0..7 as source
depths (Note, Velo, Expr, AftT, X+, X-, Y+, Y-). The four fixed modulation
routes are section 0x0d, fields 0..3: Note>Cutoff, Y+>LFO1 Depth,
AftT>Cutoff, Velo>AEG Amount. The earlier tentative interpretation was corrected.
Other global/configuration groups were observed but are intentionally not
implemented for transmission.

`protocol.py` generates candidate bytes entirely offline. It has no MIDI I/O.
Run `python -m unittest discover -s cobalt/tests -v` from the repository root.
The tests validate byte encoding, transaction order, channel bounds and data
validation; they do not establish firmware compatibility.

## Next hardware validation

1. Preserve a working hardware sound before testing any parameter writes.
2. Capture MODALapp changing one effect type, then one FX value at a time;
   compare with the candidate four-CC transactions and determine enum ranges.
3. Capture each fixed route's destination/depth and verify source/destination
   enum tables for the flexible matrix. Test neutral and both depth extremes.
4. Check that output-only plugin access leaves Bitwig's USB keyboard input
   working and that project recall works with playback stopped and the GUI shut.
5. Test paced full recall, disconnect/reconnect, busy ports and two instances
   competing for the same physical synth. Define explicit ownership behavior.

## Source and reproducibility

`src/model.hpp` defines the managed parameter catalog and message encoders.
`src/transport.hpp` owns only WinMM MIDI output on a worker; it never opens
the keyboard input. `src/editor.hpp` implements the native vector interface.
`src/plugin.cpp` supplies CLAP integration and versioned state. The MIT CLAP
wrapper supplies VST3, using this workspace's pinned dependencies and existing
guarded sample-rate fix. The root Shibboleth and Imperius sources are unchanged.

`tools/generate_tables.py` reproduces the interoperability label header from
the supplied OS 2.1 panel image, with a checked SHA-256. Firmware resides only
under the ignored build directory. `tools/decode_firmware.py` decodes the
original update container without executing it or communicating with hardware.
Source/destination enum ordering, seven LFO shapes, mode ordering and effect
transactions were also compared with the supplied Cobalt8VST's named routines.
Those binary observations are recorded as interoperability facts; no original
plugin implementation or firmware code is linked into this plugin.

The native oscillator previews use the existing research in
`research/synth-character/cobalt8.md` and its 40-algorithm map. They distinguish
spread, PWM, sync, fractal/reverse, metal, ring modulation, FM, noise-AM, chaos,
folding and filtered-noise families. These are intentionally inexpensive
illustrations rather than emulations of the firmware's antialiased DSP.
