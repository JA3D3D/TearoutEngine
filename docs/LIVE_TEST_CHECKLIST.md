# First run in Live 11: TEAROUT ENGINE 1.2 (about 5 minutes)

Go down the list. At the first step that doesn't work, send back the step number and a screenshot.

| # | Do this | You should see / hear |
|---|---|---|
| 1 | Quit Live, install the new .pkg, reopen Live, open a Set that already had TEAROUT ENGINE | Same settings and the same drop as before the update |
| 2 | Drop **TEAROUT ENGINE** on a new MIDI track | The neon window opens. **PLAY** is off, so it's silent even while Live plays |
| 3 | Press **PLAY**, then press it again (**STOP**) | The drop plays, then stops dead |
| 4 | **PLAY** on, start Live's transport | It locks to Live (**FOLLOW LIVE** on). Turn FOLLOW LIVE off and it loops on its own |
| 5 | Switch a few layers **OFF** in the layer list | Those layers go quiet right away |
| 6 | Press **GENERATE** a few times | Each one is clearly a new drop: chords and patterns change. **VARIATION** keeps the notes and only moves the groove |
| 7 | Drag a WAV (e.g. a bass sustain) onto the **BASS A** row | The row gets a speaker badge, the panel shows the sound and its root, and the preview plays your sound |
| 8 | Drag **INSTRUMENT** onto an empty spot below your tracks, then drag **MIDI** onto the new track | A new MIDI track holding a Simpler named **TEAROUT BASS A** with your sound. The clip plays in tune and sounds like the preview: one voice, overlapping notes slide, long notes hold |
| 8b | Pick **CHORDS** (no sound loaded) and do the same | A Simpler with the built-in chord voice, playing full chords |
| 9 | VST3 version: **MIDI OUT** = Bass A. On a Serum track, set MIDI From = the TEAROUT track / TEAROUT ENGINE, Monitor = In | Serum plays the Bass A part while Live plays |
| 10 | Save the Set, close it, reopen it | The sound slot, settings and drop are all still there |

**If step 8 fails** (Live says it can't open the preset, or nothing happens):
1. Send the step number, a screenshot and Live's exact version (Live → About Live).
2. To help fix it: drop any sample into a new Simpler, then save it with the floppy-disk icon in the device title bar. Send that `.adv` file.
3. Until it's fixed: hold **Alt / Option** while dragging **INSTRUMENT** to get the WAV. Drop it on a MIDI track. In Simpler, set **Transpose** to `60 − root`. For example, root C0 = 24, so Transpose = +36.
