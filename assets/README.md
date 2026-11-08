# Original panel references

Since version 0.5.0, Shiboleth renders its entire interface from vector paths and gradients in `src/vector_art.hpp`. These PNGs are retained as historical design references; none are embedded in or loaded by the plug-in. The original generation prompts are preserved below.

# Panel artwork

Generated with the built-in imagegen tool, based on the user's sketch and approved realistic concept. These were embedded in earlier builds.

- `approved-panel.png`: the approved visual reference; retained as a visual reference. Its knob materials informed the new vector rendering.
- `faceplate.png`: first clean-background iteration, retained as source material.
- `faceplate-v2.png`: final raster background reference, incorporating circular Resonance and lower-right circular Options.

The application draws slider caps, fixed-light knob frames, envelope range lines, scope traces, resonance pips, target text, hover readouts and Options over this background. `build/editor.png` is a capture from the actual native editor.

## Clean-background prompt

Edit this approved synth panel into a production background faceplate for a real software instrument. Preserve the exact image dimensions/aspect, framing, perspective, midnight blue texture, border, lighting, ALL printed labels and glyphs and ALL control locations. Remove ONLY all moving controls: remove every rotary knob entirely (including oversized cutoff knob) and its shadow, filling these circular areas seamlessly with matching uninterrupted blue painted panel texture; remove ALL horizontal slider caps and their shadows, leaving the recessed vertical slider tracks EMPTY and continuous. Remove all white waveform lines inside both waveform displays, leaving their dark recessed windows empty. Remove the word CUTOFF from the small bottom-right target display leaving that display empty. Remove the three circular pips from the resonance button leaving the black rectangular button blank. Preserve the tiny round waveform selector buttons, aftertouch target round button, all the empty slider slots, all printed symbols and labels including CUT RES ENV A D S R PORT VEL VIB ATOUCH VOL roman I and II, and the hamburger options button in the upper right. Absolutely no new objects or labels. This is not a new design: an exact clean background of the existing panel, ready for software-rendered knobs and moving slider caps.

## Final faceplate edit prompt

Edit this production synth faceplate with exactly TWO small changes only. 1. At the lower left next to CUT and above RES, replace the empty vertical black rectangular resonance button with a circular dark black pushbutton of approximately 80 pixels diameter, centered at the same location (roughly x364 y750 in the 1635x962 reference). Leave it blank: software will draw three pips. 2. Remove the hamburger button in the upper-right corner completely, seamlessly restoring matching blue textured panel there. Add a small circular black button with three thin ivory hamburger lines near the LOWER-RIGHT corner (roughly x1575 y890), below the aftertouch display, inside the rim. Preserve EXACTLY the image size, framing, blue metal texture, empty recessed slider tracks, empty waveform and aftertouch displays, every other label, tiny round selector buttons, blank areas where knobs will go, and every other detail. Do NOT add any rotary knobs or slider caps. This is a working application background and exact layout must remain unchanged.
