# Tearout MIDI study: what producers do, and how TEAROUT ENGINE 0.5 encodes it

This is a summary of production guides and genre descriptions (sources at the end). It's about **how parts are written and keyed in**. Sound design is out of scope except where it shapes the MIDI.

## 1. The grid: half-time at 140

- Tempo is about 140–150 BPM, felt in half-time: **kick on beat 1, snare on beat 3**. Hats and percussion often stay in normal time, with triplet fills.
- Drops run in 8-, 16- or 32-bar cycles. Pre-drop silence or a stab gives impact. A second drop escalates with denser edits and fills.
- **Engine:** the Tearout FLOW assumes the snare on beat 3 (step 8 of each bar). In 8- and 16-bar phrases, the second half is flagged "big", so fills get heavier there.

## 2. The phrase: call, answer, heavier call, fill

- A 4-bar shape that keeps coming up:
  - bar 1: main sustain
  - bar 2: gun or screech answer
  - bar 3: heavier sustain variation
  - bar 4: stop / fill / fakeout / reverse / pitch-drop reset
- In short: *"Heavy sound. Space. Heavy sound. Fill. Reset."*
- Call-and-response between **different patches** keeps riffs whiplash-like. Producers copy one line onto several synths and delete notes so the bass hops between sounds.
- Silence is part of the pattern. Remove everything on the last two beats of every fourth bar, then add single elements back into that gap.
- **Engine:**
  - FLOW = Tearout builds every 4 bars from the roles call / answer / call2 / fill.
  - The answer bar picks a rhythm cell that contrasts with the call (hits where the call is silent).
  - In answer bars, Under drops to the downbeat while Over takes short "gun" hits.
  - Fill types are **stop, gun, roll, drop, fakeout**, weighted by the Space / Aggression / Horror / Mutation knobs and ROLLS.
  - LINK puts each layer on its own track (its own patch), which is the copy-and-delete trick done for you.

## 3. Room for the drums

- Leave silence before the snare so it hits heavier: shorten the bass tail, duck it, or leave a gap. Keep space for the kick and the next answer.
- Short notes leave room for the drums. Riddim keeps notes short even when the movement is smooth.
- **Engine:**
  - In Tearout FLOW, any sustain running into beat 3 ends one 1/16 early (the "snare pocket").
  - Under rarely starts on the 1/16 just before the snare.
  - Riddim FLOW uses 1–3-step notes.

## 4. The sub and the mids

- The sub follows the root and stays mostly static and mono. Movement belongs to the mid-bass.
- Don't hold a sub note for a whole bar. Chop it rhythmically with the lead, but hold at least a beat for weight.
- Harmony is minimal: minor modes, chromatic tension, pedal tones.
- **Engine:**
  - Under stays root-centred, with octave / fifth / ♭7 / ♭2 only on turnarounds and heavier bars.
  - Over carries the motif and the octave jumps.
  - Scales include Phrygian, Phrygian Dominant and Hungarian Minor for the dark modes.

## 5. Keying in movement: glides, rolls, octaves

- **Glides:** turn on glide / portamento and let the notes that should slide **overlap** the next note. Notes that don't overlap retrigger without sliding (mono legato behaviour).
- **Rolls and triplets:** triplet fills before a reset, and "su-per-man" triplet rolling feels. Dotted-eighth against eighth gives a "riding" syncopation (3-3-2).
- **Octave jumps** for phrase changes. Sharp pitch moves on downbeats at phrase ends. Pitch drops as resets.
- **Engine:**
  - GLIDE overlaps a note one 1/16 into the next different note, and always on pitch-drop fills.
  - ROLLS adds 1/16T, 1/32 and 1/8T rolls in fills and 1/32 ratchets on pickups. At 0, everything stays on the strict 1/16 grid.
  - Riddim cells include 3-3-2 shapes.
  - Drop fills descend octave → fifth → root.

## 6. Resampled chops

- Tearout leans on serial resampling. Basses are rendered, chopped and re-sequenced, often as one-shots in a sampler.
- Gun hits are short one-shots where the transient matters most. Sustains need internal movement.
- **Engine:** MODE = **Bass Chops** sequences pads from C1 (Drum Rack / sliced Simpler):
  - sustain pads on calls
  - answer / gun pads on answers
  - gun / roll / screech / impact pads in fills and on phrase starts
  - pad choices repeat per role, so the pattern stays recognisable

## 7. Wobble and modulation

- The classic wobble is an LFO on **filter cutoff**, synced to tempo. Other common targets are wavetable position, FM amount, warp, distortion mix and volume.
- **The wobble rate is part of the riff.** One documented technique puts rate control on low MIDI notes, each key selecting a division: 1/2, 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T. The key range is limited so those notes don't sound.
- Typical choices:
  - quarter-note pulses for simple head-nods
  - 1/8 chops for bounce
  - triplets for unstable phrasing
  - fast ramps for screechy tearout stabs
  - stepped movement for robotic phrases
  - tiny filter dips before the snare
- **Engine (MOD LANES):** this is that technique, built in.
  - Mod notes below C0 choose the rate per hit (12 keys, 1/1 … 1/32T, including dotted 1/4 and 1/8).
  - Each one retriggers the LFO, like a synth LFO in trigger mode.
  - The device filters them out before the synth.
  - **Auto Wobble** picks steadier rates on calls, faster on answers and fastest in fills. **Auto Sweep** gives slow movement for a second target.
  - The modulation runs through Live 11's own modulation path (`live.modulate~`), so it sits over the top of the knob.

## Why the modulation isn't clip automation

Live 11's Max for Live API can clear clip envelopes, but it **can't create or write them**. `automation_envelope`, `create_automation_envelope` and `insert_step` are absent from Live 11's Max for Live object list (`_MxDCore/LomTypes.py`). The Python remote-script API has them, but M4L doesn't.

So the movement is carried as notes in the clip and turned into modulation by the device in real time. A bonus: the wobble pattern is visible and editable next to the bass, and it travels with the clip.

## Sources

- [How To Make Tearout Dubstep Bass In Serum 2 (EDM Templates)](https://edmtemplates.net/blogs/edm-templates-blog/how-to-make-tearout-dubstep-bass-in-serum-2)
- [How To Make Riddim Bass In Serum 2 (EDM Templates)](https://edmtemplates.net/blogs/edm-templates-blog/how-to-make-riddim-bass-in-serum-2)
- [How To Make Dubstep Growls In Serum 2 (EDM Templates)](https://edmtemplates.net/blogs/edm-templates-blog/how-to-make-dubstep-growls-in-serum-2)
- [How to Create a Dubstep Drop from Scratch (Point Blank)](https://www.pointblankmusicschool.com/blog/how-to-create-a-dubstep-drop-from-scratch/)
- [Rhythm: How Dubstep Works (Musical U)](https://www.musical-u.com/learn/rhythm-how-dubstep-works)
- [Dubstep: 5 Production Essentials (ModeAudio)](https://modeaudio.com/magazine/dubstep-5-production-essentials)
- [Tearout Brostep genre profile (Melodigging)](https://www.melodigging.com/genre/tearout-brostep)
- [10 Tips for More Powerful Drops (Envato Tuts+)](https://music.tutsplus.com/10-tips-for-more-powerful-drops--cms-29673t)
- [Control a Wobble Bass Using a MIDI Note (Ask.Audio)](https://ask.audio/articles/control-wobble-bass-midi-note-logic-pro)
- [How to build an LFO wobble bass (MusicRadar)](https://musicradar.com/how-to/lfo-wobble-bass)
- [Sliding into a bass note (Ableton forum)](https://forum.ableton.com/viewtopic.php?p=1131642)
- [Mono Legato Retrigger Problem (Xfer forum)](https://www.xferrecords.com/forums/posts/84279)
- [live.modulate~ reference (Cycling '74)](https://docs.cycling74.com/legacy/max8/refpages/live.modulate~)
- [JavaScript and threading in Max (Cycling '74)](https://docs.cycling74.com/legacy/max8/vignettes/jsthreading)
- [Live 11 MIDI Remote Scripts, `_MxDCore/LomTypes.py` (Max for Live exposure list)](https://github.com/gluon/AbletonLive11_MIDIRemoteScripts)
