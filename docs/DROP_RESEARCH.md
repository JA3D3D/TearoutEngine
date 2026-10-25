# What a drop is made of: research behind TEAROUT ENGINE 1.0

This is the knowledge the generator is built on. It covers **notes, keys, rhythm and structure**, not sound design. Every rule here maps to code in `src/core/` and to a test in `tests/`.

## 1. Where this comes from, and what it isn't

- Public production guides, genre analyses, chord-usage statistics (Hooktheory) and artist interviews. Links are at the end.
- **No stems were analysed.** I looked for official stems from Crankdat, Subtronics, Level Up, Ray Volpe and SVDDEN DEATH.
  - The only official release I found was the Subtronics "Eyes Cut Deeper" remix stems, from a SoundCloud campaign.
  - This environment can't download audio from those sites or transcribe audio.
  - So the artist points below come from what they and the press have said about their writing. They were never measured.
- **To study a stem yourself:** in Live 11 Suite, right-click the audio clip → *Convert Harmony / Melody to New MIDI Track*. Then feed that clip to TEAROUT ENGINE as a **seed** (CAPTURE, or drop the exported `.mid`). The engine reads its key, chords and rhythm and builds the layers around them.
- **Artists are knowledge, not presets.** Nothing in the engine is named after an artist. Their shared habits became the default rules. Their individual habits became pattern options that any genre can use.

## 2. The layers of a drop

Across dubstep, riddim, trap, hardstyle and pop-EDM, the same jobs keep showing up. Some genres leave a few of them empty.

| # | Layer | Job | Register |
|---|---|---|---|
| 1 | **CHORDS** | The progression, sustained or rhythmic. Supersaws, pads, stabs, offbeat chords. | C3–G5 |
| 2 | **SUB / 808** | Chord root, mono, sine. Long holds, or locked to the kick. | ~D1–C#2 |
| 3 | **BASS A** | The main mid-bass: growl, reese, "yoi". **Carries the character.** Sustain or a sustain pattern on the chord root. | ~D2–C#3 |
| 4 | **BASS B** | The second mid-bass sound. Either it **doubles** A, stacking sounds, or it **takes turns** with A (call-and-response, sound switching). A + B together stay one continuous line. | same as A |
| 5 | **LEAD** | The hook. Short, singable, repeated. Chord tones on the strong beats. | C4–C6 |
| 6 | **ARP / PLUCK** | Chord tones in order (up / down / up-down) at 1/16, 1/8 or 1/16T. Restarts on every chord. | C5–E6 |
| 7 | **DRUMS** | The skeleton: kick, snare / clap, hats, crash. Decides where the bass can breathe. | Drum Rack C1+ |
| 8 | **FX** | Impacts on the downbeat, risers / reverse crashes into phrase boundaries, sweeps. | Pads C2+ |

**Why eight separate layers.** Producers put each of these on its own track with its own patch. In TEAROUT ENGINE each layer keeps its own pattern, octave, movement and length. Each one drags out as its own MIDI clip, so nothing gets mixed into one part.

## 3. Rules every layer shares

1. **Harmony first.** The progression is decided first, and every layer reads it. Sub and bass play the **chord root**. Chords play the **chord**. Lead and arp use **chord tones on strong beats** and scale tones in between. Nothing picks notes at random.
2. **One chord per bar** is the default harmonic rhythm. Half-bar and two-bar chords are options.
3. **Phrases are 4 bars, sections are 8, drops are 16–32.**
   - The 4th bar is the *turnaround*: fills, pickups, slides.
   - The 8th bar is the *phrase end*: a bigger fill, then a crash and an impact on the next downbeat.
   - In long drops the second half lifts slightly, for forward momentum.
4. **Repetition is the hook.** A 2-bar idea is repeated. Bars 3–4 answer bars 1–2, and bars 5–8 repeat 1–4. Variation happens *inside* the repetition.
5. **Bass and sub are sustains with no gaps.** A long hold, or a gapless *sustain pattern* that re-articulates the same root. Gaps only happen where the genre puts the kick (hardstyle / house offbeat bass).
6. **Rhythm-matching rule.** Rhythmic chords pair with a sustained bass, and sustained chords with a rhythmic bass. The genre defaults follow it.
7. **Glide is overlap.** A mono / legato synth slides only when a note overlaps the next one. A 1/32 overlap is enough. Slides go at turnarounds, as seasoning, not everywhere.
8. **Kick and bass interlock.** The trap 808 starts on every kick. A half-time sub re-strikes with the kick. Hardstyle and house bass sits on the offbeats, between kicks.

## 4. Genres

| | Melodic Dubstep | Tearout | Riddim | Trap | Hardstyle Techno | Common |
|---|---|---|---|---|---|---|
| Tempo / feel | 150, half-time | 150, half-time | 150, half-time | 140, half-time feel, straight | 150, four-on-the-floor | 128, backbeat |
| Drums | K on 1, S on 3 | K on 1 + syncopated kicks, S on 3, heavy | K on 1, S on 3 | K steps 1 & 11, clap on 9; hats 1/8 with rolls | K every beat, clap 2 & 4, offbeat hats | K 1 & 3, S 2 & 4 |
| Harmony | Emotional minor loops: i–VI–III–VII, VI–VII–i, pop vi–IV–I–V | Pedal tonic, i–♭II Phrygian push, i–VI | Pedal tonic, i–♭II | i–VI, i–isus2, i–VII–VI–V, i–VI–III–v | i–♭VI–♭III–♭VII, i–♭VII–♭VI–V, i–iv–♭VI–♭VII | I–V–vi–IV, I–vi–IV–V, IV–I–V–vi |
| Sub | Long hold on the root | Long hold | Re-strikes with the kick | **808 starts on every kick**, holds until the next | Offbeat 1/8 (reverse bass) | Hold |
| Mid bass | Sustains with re-strikes; B doubles A | 3-3-2 sustain ride; B answers on bars 2 & 4 | Triplet bounce; A/B swap every hit | none (the 808 is the bass) | Offbeat 1/8 following the root | 1/8 pump |
| Chords | Long supersaw holds (add9 colour, root doubled below) | none or sparse stabs | none | Sustained 7th chords | Offbeat 1/8 triads, root an octave below | Holds |
| Lead | Hook on chord tones | none or sparse | none | Hook | 1/8 riff on chord tones | Hook |
| Arp | 1/16 up | none | none | Sparse 1/8 | 1/16 up (builds) | 1/16 up-down |

**Common chord statistics** (Hooktheory, minor keys): i ≈ 14 %, VI ≈ 9 %, VII ≈ 9 %, III ≈ 6 %, iv ≈ 5 %, v ≈ 3 %, ii° rare. In major songs, I, IV, V and vi carry most of the harmony. The catalogue is built from these.

## 5. Bass writing: the details that make it feel finished

- **Start on the root**, in a steady rhythm that locks to the drums.
- **Passing / approach notes** go on the last 1/8 before a chord change and lead into the next root.
- **Octave jumps** replace passing notes to mark a change. A short +12 "flick" inside a hold is the classic dubstep movement.
- **Sustain patterns** stay gapless at the 1/8 level:
  - **3-3-2** ("riding" dotted feel) and **4-2-2**
  - **hold + re-strike on the snare**
  - **1/8 pump**
  - **triplet bounce** (riddim)
- **Call and response between sounds.** Copy the line onto two synths and delete alternate parts, so the bass hops between patches. In the engine this is BASS B's *Answers / Swap / Restrikes* modes.
- **Silence is placed, not random.** The only intended gaps are the hardstyle offbeat pattern and short stab patterns.

## 6. What "Variation" and "Movement" mean

Your rule was: *long holds stay long holds; variation adds movement and groove, not new grooves.* The engine does exactly that:

- **MOVEMENT (per layer)** may only:
  1. re-strike a note that's already sounding, at the song's shared **groove points**
  2. add a 1/16–1/8 octave flick on the same pitch class
  3. add one approach note in the last 1/8 before a chord change
  4. add a 1/32 slide overlap at a turnaround
  5. change velocity accents

  It never removes a base note, never moves the chord changes and never adds a new harmony. The tests check this for every genre and pattern.
- **Groove points are shared**, so the sub, the basses and the chords re-strike in the same places. That's why it sounds like one groove.
- **VARIATION (button)** re-rolls only where the movement goes. The harmony, the patterns and the hook stay.
- **GENERATE (button)** rolls a new idea: the progression (when it's on Auto), the hook contour and the arp choices.

## 7. Artist study, as knowledge (not styles)

**What they share**
- half-time 140–150
- one main bass idea per drop, repeated with variation
- root-centred minor-key bass
- call-and-response between patches
- dense layering
- space before the snare
- 4/8-bar phrases that end in a fill
- second drops that vary the first

These are the defaults for every bass genre.

**What's distinctive, and where it went**

| Artist (public sources) | Habit | Where it lives in the engine |
|---|---|---|
| Subtronics | "Solid quarter-note" grooves; riddim roots with more mid-range; arpeggios brought into the drop; heavy layering; drop order reworked many times | *Hold + Restrike* and 3-3-2 sustain patterns; ARP available in every genre; BASS B *Double* |
| Ray Volpe | Short, catchy, singable, simple ideas | The 2-bar hook motif, repeated, with chord tones on strong beats |
| SVDDEN DEATH | Epic / ravey contrast into heavy; four-to-the-floor riddim elements | The *Four-on-Floor* drum pattern works under any genre; melodic progressions under heavy bass |
| Crankdat | Melodic writing; sometimes starts from the drop; resampled bass shots | Melodic progressions with lush voicings; one-shot-friendly BASS B *Swap* |
| Level Up | Heavier side, wonky rhythms, dark and spooky | *Triplet Bounce*; i–♭II Phrygian push; high, sparse lead notes in dark keys |

## Sources

- Hooktheory, chord statistics in minor keys and the 1300-song analysis: <https://www.hooktheory.com/theorytab/common-chord-progressions>, <https://www.hooktheory.com/blog/i-analyzed-the-chords-of-1300-popular-songs-for-patterns-this-is-what-i-found/>
- Plugin Boutique, *How to Write a Great Bassline for your Chord Progression*: <https://www.pluginboutique.com/articles/2048-How-to-Write-a-Great-Bassline-for-your-Chord-Progression>
- Violet Recording, *How to Make 808 Slides*: <https://violetrecording.com/how-to-make-808-slides/>
- BassGorilla, *What is Riddim Dubstep?*: <https://bassgorilla.com/what-is-riddim-dubstep/>
- Future Proof Music School genre explorer, *Melodic Dubstep*: <https://genreexplorer.futureproofmusicschool.com/genre/melodic-dubstep>
- Point Blank, *How to Create a Dubstep Drop from Scratch*: <https://www.pointblankmusicschool.com/blog/how-to-create-a-dubstep-drop-from-scratch/>
- EDM Templates, tearout and riddim bass guides: <https://edmtemplates.net/blogs/edm-templates-blog/how-to-make-tearout-dubstep-bass-in-serum-2>, <https://edmtemplates.net/blogs/edm-templates-blog/how-to-make-riddim-bass-in-serum-2>
- Musical U, *Rhythm: How Dubstep Works*: <https://www.musical-u.com/learn/rhythm-how-dubstep-works>
- Dancing Astronaut, Subtronics *Scream Saver* interview: <https://dancingastronaut.com/2020/04/subtronics-breaks-down-his-hardest-hitting-project-yet-scream-saver-ep-interview/>
- Splice, *How to make a trap beat*: <https://splice.com/blog/how-to-make-trap-beat-fl-studio/>
- Scaler forum, why DAW clips can't be dragged straight into a plugin: <https://forum.scalermusic.com/t/drag-a-clip-from-the-daw-directly-into-scaler/24495>
- Earlier tearout study (v0.5): see `docs/TEAROUT_STUDY_v05.md`
