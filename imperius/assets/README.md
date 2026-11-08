# Imperius artwork

The approved `imperius-ui-concept-v1.png` was generated with the built-in image-generation tool, using the user's Implexus photograph as a visual-language reference. Its generation prompt is [UI-PROMPT-v1.txt](../UI-PROMPT-v1.txt).

`faceplate-v1.png` is a second built-in image-tool edit of that approved concept. It removes physical knobs, controls, shadows and amber arcs while retaining the silver faceplate, wood cheeks, labels and printed scales. Its exact prompt is [FACEPLATE-PROMPT-v1.txt](FACEPLATE-PROMPT-v1.txt).

The faceplate is embedded as RCDATA in `Imperius.clap`. No path under the user's Codex directory is used at runtime. The editor separately draws interactive knobs, value arcs, switches, Sync buttons and temporary readouts. Knob pixels are rendered and cached on the main/UI thread with fixed metal illumination; the audio thread never touches windows or graphics.

`../build/editor.png` is the implementation screenshot from the compiled editor, not the original generated concept. `../build/editor-125.png` shows host DPI scaling. Both the concept and production faceplate remain in this workspace for future revisions.
