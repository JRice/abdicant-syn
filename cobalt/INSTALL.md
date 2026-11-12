# Cobalt Editor 0.1.0 alpha — Windows x64

This is a hardware controller for COBALT8 / COBALT8X / COBALT8M, targeting OS
2.1. It does not generate the Cobalt's sound. The ARGON family is not supported.

## Install

Choose a format for your Bitwig project:

- CLAP: copy `CobaltEditor.clap` into a CLAP folder that Bitwig scans. The
  standard per-user folder is `%LOCALAPPDATA%\Programs\Common\CLAP`.
- VST3: copy the entire `CobaltEditor.vst3` directory into a VST3 folder that
  Bitwig scans, such as `C:\Program Files\Common Files\VST3`. Preserve its
  `Contents\x86_64-win` directory.

Rescan plugins in Bitwig, then insert **Cobalt Editor** on an instrument track.
CLAP and VST3 are separate plugin types; a saved instance stays with its format.

## Connect the hardware

1. Connect the Cobalt over USB. Close MODALapp.
2. Keep **COBALT8 MIDI input** enabled in Bitwig for your keyboard/controller.
   Release the **COBALT8 MIDI output** in Bitwig: this plugin owns that output.
   Do not also route an HW Instrument device to the same USB output.
3. Open Cobalt Editor. New instances start disconnected. Select the COBALT8
   output and match the plugin's channel to the synth's MIDI channel.
4. Preserve any hardware sound you want to keep before clicking **Connect +
   Send All**: connecting replaces the current edit-buffer parameters with the
   plugin's values. It does not write a hardware preset slot.
5. Route your Cobalt's audio outputs into your audio interface and Bitwig. This
   plugin is silent without an audio return. It also offers stereo audio
   passthrough if your host setup routes a return through its input.

Notes, velocity, modwheel, sustain, pitch bend and pressure entering the plugin
are sent to its selected hardware channel. If keyboard monitoring produces
double notes, turn **Local Off** on the Cobalt while monitoring through Bitwig.
The plugin does not change that global hardware setting for you.

One editor instance at a time owns a given output. Another instance waits for
it to be released. Device names are saved; Windows' temporary port indices
are not. An enabled connection retries a missing/busy device once per second.

## Edit, automate and save

- Drag knobs up/down. Labels are to the left; Filter & Voice labels are below.
  Knob values appear briefly during editing.
- Drag horizontal sliders left/right and envelope faders up/down.
- Hold Shift for fine adjustment. Mouse wheel and arrow keys change one step.
- Tab/Shift+Tab navigates controls. Enter/Space opens a selected menu. Home
  restores a continuous control's initial value.
- Use Bitwig's normal parameter automation and plugin preset/project saving.
  All 113 managed parameters are exposed and saved.
- The connection choice and MIDI channel are saved too. A project saved with
  the connection enabled reconnects and replays its managed parameter state
  when restored, even with the editor closed and transport stopped.
- **Send All** replays that state; **Panic** releases sustain and sounding notes.

## Alpha status

CLAP and VST3 host tests pass, and Steinberg's VST3 validator passes 47/47
checks. MIDI routing/replay tests use a mocked output. This build has not yet
been validated on the physical synth or inside Bitwig.

The 113 managed controls include oscillator algorithms/tuning/A/B, filters,
three ADSRs and their eight types, LFO controls, eight matrix routes, four
fixed routes, eight source depths, three effects, and output controls.

Full synth-state recall is not yet established: dedicated LFO tempo-sync
toggles and modulation-slot enable flags still need protocol mapping. Voice
mode is currently a compound numeric control; its popup shows the voice
family and the low allocation/legato bits. Parameter units remain MIDI values.
Waveforms and envelope curves are illustrations, not firmware-exact plots.

There is no hardware readback. Hardware knob or preset changes do not update
the plugin state. MIDI output uses a paced worker, so automation and notes are
not guaranteed to arrive with sample-accurate timing. This build does not edit
the arp/sequencer, manage hardware presets, update firmware, or send SysEx.

Start with a new Bitwig project and test cutoff, an effect control, keyboard
input, automation, and save/reopen before using it in an existing project.
