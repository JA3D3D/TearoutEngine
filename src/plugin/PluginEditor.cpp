#include "PluginEditor.h"

namespace tep
{
using namespace juce;

static const char* kTitles[] = { "CHORDS", "SUB / 808", "BASS A", "BASS B", "LEAD", "ARP / PLUCK", "DRUMS", "FX" };

static String u8 (const std::string& s) { return String::fromUTF8 (s.c_str()); }

TearoutEditor::TearoutEditor (TearoutProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    root.setLookAndFeel (&lnf);
    addAndMakeVisible (root);
    background.renderScale = 2.0f;
    for (auto* c : std::initializer_list<Component*> { &background, &songPanel, &layersPanel, &rollPanel, &layerPanel, &bottomPanel, &logo })
        root.addAndMakeVisible (c);

    auto& apvts = proc.apvts;
    auto fillCombo = [] (ComboBox& box, const std::vector<std::string>& items) {
        for (size_t i = 0; i < items.size(); ++i) box.addItem (u8 (items[i]), (int) i + 1);
    };
    fillCombo (genreBox, te::genreChoiceNames());
    fillCombo (keyBox, te::keyChoiceNames());
    {
        auto names = te::progressionChoiceNames();
        for (size_t i = 0; i < names.size(); ++i)
        {
            if ((int) i == te::kProgressionFirst) progBox.addSectionHeading ("Minor keys");
            if ((int) i == te::kProgressionFirst + 20) progBox.addSectionHeading ("Major keys");
            progBox.addItem (u8 (names[i]), (int) i + 1);
        }
    }
    chordLenBox.addItemList ({ "1/2 Bar", "1 Bar", "2 Bars" }, 1);
    barsBox.addItemList ({ "4 Bars", "8 Bars", "16 Bars", "32 Bars" }, 1);
    midiOutBox.addItemList ({ "Off", "Selected Layer", "All Layers" }, 1);
    midiOutBox.addSeparator();
    midiOutBox.addItemList ({ "Chords", "Sub / 808", "Bass A", "Bass B", "Lead", "Arp", "Drums", "FX" }, 4);

    for (auto* b : { &genreBox, &keyBox, &progBox, &chordLenBox, &barsBox, &midiOutBox, &patternBox })
        root.addAndMakeVisible (b);
    genreAtt = std::make_unique<ComboAtt> (apvts, ids::genre, genreBox);
    keyAtt = std::make_unique<ComboAtt> (apvts, ids::key, keyBox);
    progAtt = std::make_unique<ComboAtt> (apvts, ids::progression, progBox);
    chordLenAtt = std::make_unique<ComboAtt> (apvts, ids::chordLength, chordLenBox);
    barsAtt = std::make_unique<ComboAtt> (apvts, ids::bars, barsBox);
    midiOutAtt = std::make_unique<ComboAtt> (apvts, ids::midiOut, midiOutBox);
    volumeAtt = std::make_unique<SliderAtt> (apvts, ids::volume, volumeKnob.slider);
    volumeKnob.format = [] (double v) { return String (v, 1) + " dB"; };

    genreBox.setTooltip ("The drop's rules: drum skeleton, which layers play and how. Every layer set to 'Genre' follows it.");
    keyBox.setTooltip ("Root note. Minor or major comes from the progression.");
    progBox.setTooltip (String::fromUTF8 ("\xe2\x98\x85 = typical for this genre. Auto picks one (GENERATE rolls a new one). From Seed uses your seed clip's chords."));
    chordLenBox.setTooltip ("How long each chord lasts. One chord per bar is the standard.");
    barsBox.setTooltip ("Length of the clip. Drops are usually 16-32 bars; the progression loops inside it.");
    midiOutBox.setTooltip ("Send a layer out as live MIDI (VST3 only - Live can't take MIDI from Audio Units). On a Serum track set MIDI From = this track / TEAROUT ENGINE and Monitor = In. One layer per Serum track; All Layers uses channels 1-8 but Live merges channels between tracks.");

    // ---- buttons ----
    generateBtn.setColour (TextButton::buttonColourId, col::magenta);
    generateBtn.getProperties().set ("solid", true);
    generateBtn.getProperties().set ("fontSize", 19.0f);
    generateBtn.setTooltip ("A completely new drop: new progression (PROGRESSION on Auto), new patterns for every layer set to 'Genre', new hook and groove. Layers you set to a specific pattern keep it.");
    generateBtn.onClick = [this] { proc.generateNew(); setStatus ("New drop generated."); };
    variationBtn.setColour (TextButton::buttonColourId, col::cyan);
    variationBtn.getProperties().set ("fontSize", 16.0f);
    variationBtn.setTooltip ("Same song, new groove: only moves where MOVEMENT puts its re-strikes, flicks and slides. Chords, holds and patterns stay.");
    variationBtn.onClick = [this] { proc.newVariation(); setStatus ("New variation: same notes, new groove placement."); };
    playBtn.setColour (TextButton::buttonColourId, col::lime);
    playBtn.getProperties().set ("fontSize", 15.0f);
    playBtn.setClickingTogglesState (true);
    playBtn.setTooltip ("PLAY / STOP the preview. STOP silences the plugin completely, even while Live is playing.");
    playBtn.onClick = [this] { proc.setPreviewPlaying (playBtn.getToggleState()); };
    playBtn.onStateChange = [this] { playBtn.setButtonText (playBtn.getToggleState() ? "STOP" : "PLAY"); };
    playBtn.setButtonText ("PLAY");
    followBtn.setColour (TextButton::buttonColourId, col::cyan);
    followBtn.setClickingTogglesState (true);
    followBtn.getProperties().set ("fontSize", 10.5f);
    followBtn.setToggleState (proc.isFollowingLive(), dontSendNotification);
    followBtn.setTooltip ("On: while Live plays, the preview locks to Live's bars and tempo. Off: it loops on its own.");
    followBtn.onClick = [this] { proc.setFollowLive (followBtn.getToggleState()); };
    captureBtn.setColour (TextButton::buttonColourId, col::hot);
    captureBtn.setClickingTogglesState (true);
    captureBtn.getProperties().set ("fontSize", 14.0f);
    captureBtn.setTooltip ("Record the MIDI coming into this track as the seed: put your clip on this track, press CAPTURE, play. It stops when the transport stops.");
    captureBtn.onClick = [this] {
        proc.setCapture (captureBtn.getToggleState());
        setStatus (captureBtn.getToggleState() ? "Capturing: play your clip on this track." : "Capture finished.");
    };
    clearSeedBtn.setColour (TextButton::buttonColourId, col::violet);
    clearSeedBtn.setTooltip ("Forget the seed clip.");
    clearSeedBtn.onClick = [this] { proc.clearSeed(); setStatus ("Seed cleared."); };
    saveBtn.setColour (TextButton::buttonColourId, col::cyan);
    saveBtn.setTooltip ("Write every layer (plus an ALL file) as .mid files into a folder.");
    saveBtn.onClick = [this] { saveMidiFiles(); };
    allViewBtn.setColour (TextButton::buttonColourId, col::magenta);
    allViewBtn.setClickingTogglesState (true);
    allViewBtn.getProperties().set ("fontSize", 11.0f);
    allViewBtn.setTooltip ("Show every layer at once.");
    allViewBtn.setComponentID ("all");
    allViewBtn.onClick = [this] { viewAll = allViewBtn.getToggleState(); roll.layer = viewAll ? -1 : proc.selectedLayer.load(); roll.repaint(); };
    for (auto* b : { &generateBtn, &variationBtn, &playBtn, &captureBtn, &clearSeedBtn, &saveBtn, &allViewBtn, &followBtn })
        root.addAndMakeVisible (b);

    // ---- sound slot (selected layer) ----
    for (auto* b : { &loadSoundBtn, &clearSoundBtn, &rootDownBtn, &rootUpBtn })
    {
        b->setColour (TextButton::buttonColourId, col::cyan);
        b->getProperties().set ("fontSize", 12.0f);
        root.addAndMakeVisible (b);
    }
    loadSoundBtn.setTooltip ("Load your own sound (WAV / AIFF / FLAC / MP3) for this layer. You can also drop a file onto any pitched layer row.");
    loadSoundBtn.onClick = [this] { browseForSound (proc.selectedLayer.load()); };
    clearSoundBtn.setTooltip ("Back to the built-in preview sound.");
    clearSoundBtn.onClick = [this] { proc.clearLayerSound (proc.selectedLayer.load()); refreshSoundControls(); songChanged(); };
    rootDownBtn.setTooltip ("The sound's root note is wrong? Nudge it down a semitone.");
    rootUpBtn.setTooltip ("Nudge the sound's root note up a semitone.");
    rootDownBtn.onClick = [this] { proc.nudgeRoot (proc.selectedLayer.load(), -1); refreshSoundControls(); };
    rootUpBtn.onClick = [this] { proc.nudgeRoot (proc.selectedLayer.load(), 1); refreshSoundControls(); };
    simplerToggle.setColour (ToggleButton::textColourId, col::text);
    simplerToggle.setColour (ToggleButton::tickColourId, col::lime);
    simplerToggle.setColour (ToggleButton::tickDisabledColourId, col::dim);
    simplerToggle.setTooltip ("On: the dragged MIDI is shifted so it plays in tune in a Simpler that holds the dragged SOUND (Simpler's default root is C3). Off: true pitches, for Serum or anything else.");
    simplerToggle.onClick = [this] { proc.setSimplerMidi (proc.selectedLayer.load(), simplerToggle.getToggleState()); };
    root.addAndMakeVisible (simplerToggle);
    soundDrag.makeFile = [this] { return proc.soundFileForDrag (proc.selectedLayer.load()); };
    soundDrag.onStatus = [this] (const String& s) {
        setStatus (s.startsWith ("Nothing") ? String ("No sound loaded on this layer.") : "Drop it on an empty MIDI track: Live makes a Simpler. Then drag the MIDI onto the same track.");
    };
    soundDrag.big = true;
    soundDrag.setTooltip ("Drag this layer's sound onto an empty MIDI track: Live puts it in a Simpler. Then drag the MIDI onto the same track.");
    root.addAndMakeVisible (soundDrag);

    root.addAndMakeVisible (volumeKnob);
    volumeKnob.slider.setTooltip ("Preview volume (the MIDI itself has no volume).");
    root.addAndMakeVisible (chordStrip);
    root.addAndMakeVisible (roll);

    // ---- layer rack ----
    for (int l = 0; l < te::kNumLayers; ++l)
    {
        auto* row = rows.add (new LayerRow (l));
        row->onSelect = [this] (int layer) { selectLayer (layer); };
        row->mute.setToggleState (! proc.isMuted (l), dontSendNotification);
        row->mute.setButtonText (proc.isMuted (l) ? "OFF" : "ON");
        row->solo.setToggleState (proc.isSoloed (l), dontSendNotification);
        row->mute.onClick = [this, l, row] { proc.setMute (l, ! row->mute.getToggleState()); roll.repaint(); };
        row->solo.onClick = [this, l, row] { proc.setSolo (l, row->solo.getToggleState()); roll.repaint(); };
        row->handle.makeFile = [this, l] { return proc.writeLayerMidi (l); };
        row->handle.onStatus = [this] (const String& s) { setStatus (s); };
        root.addAndMakeVisible (row);
    }

    // ---- selected layer panel ----
    for (auto* k : { &octaveKnob, &movementKnob, &lengthKnob })
        root.addAndMakeVisible (k);
    octaveKnob.format = [] (double v) { int o = roundToInt (v); return o == 0 ? String ("0") : (o > 0 ? "+" : "") + String (o) + " oct"; };
    movementKnob.format = [] (double v) { return String (roundToInt (v * 100)) + " %"; };
    lengthKnob.format = [] (double v) { int x = roundToInt (v * 100); return x >= 100 ? String ("Legato") : String (x) + " %"; };
    octaveKnob.slider.setTooltip ("Shift this layer by octaves.");
    movementKnob.slider.setTooltip ("Groove only: re-strikes notes that are already sounding, octave flicks, approach notes into the next chord, slides at turnarounds. Never removes a note or changes the chords. 0 = the pure pattern.");
    lengthKnob.slider.setTooltip ("Note length inside each slot. Legato (100%) = no gaps; slides need it.");
    patternBox.setTooltip ("What this layer plays. 'Genre' follows the genre's typical pattern for this layer.");
    layerDrag.makeFile = [this] { return proc.writeLayerMidi (proc.selectedLayer.load()); };
    layerDrag.onStatus = [this] (const String& s) { setStatus (s); };
    layerDrag.big = true;
    layerDrag.setTooltip ("Drag the selected layer's MIDI clip onto a track in Ableton.");
    allDrag.makeFile = [this] { return proc.writeLayerMidi (-1); };
    allDrag.onStatus = [this] (const String& s) { setStatus (s); };
    allDrag.big = true;
    allDrag.setTooltip ("Drag every layer as one multi-track MIDI file (Live makes one track per layer when you drop it in an empty area).");
    root.addAndMakeVisible (layerDrag);
    root.addAndMakeVisible (allDrag);

    // ---- seed ----
    seedZone.onClick = [this] { browseForSeed(); };
    seedZone.setTooltip ("Drop a .mid file here (or click to browse). Its key and chords become PROGRESSION = From Seed; its rhythms feed the 'From Seed' patterns.");
    root.addAndMakeVisible (seedZone);

    root.addAndMakeVisible (overlay);
    tooltips = std::make_unique<TooltipWindow> (&root, 450);

    layoutRoot();
    proc.onSongChanged = [this] { songChanged(); };
    selectLayer (proc.selectedLayer.load());
    songChanged();

    setResizable (true, true);
    setResizeLimits (kW * 3 / 4, kH * 3 / 4, kW * 3 / 2, kH * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kW / (double) kH);
    setSize (kW, kH);
    startTimerHz (30);
}

TearoutEditor::~TearoutEditor()
{
    stopTimer();
    proc.onSongChanged = nullptr;
    patternAtt.reset();
    octaveAtt.reset();
    movementAtt.reset();
    lengthAtt.reset();
    root.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void TearoutEditor::paint (Graphics& g) { g.fillAll (col::bg); }

void TearoutEditor::resized()
{
    const float scale = (float) getWidth() / (float) kW;
    root.setBounds (0, 0, kW, kH);
    root.setTransform (AffineTransform::scale (scale));
}

// ---- layout (design coordinates, 1200 x 780) ------------------------------------------------------------------
static void place (Component& c, int x, int y, int w, int h) { c.setBounds (x, y, w, h); }

void TearoutEditor::layoutRoot()
{
    place (background, 0, 0, kW, kH);
    place (overlay, 0, 0, kW, kH);
    place (logo, 22, 14, 380, 94);

    // transport + main actions
    place (volumeKnob, 640, 14, 74, 90);
    place (playBtn, 724, 20, 108, 46);
    place (followBtn, 724, 70, 108, 26);
    place (generateBtn, 842, 28, 176, 62);
    place (variationBtn, 1026, 32, 156, 54);

    // song row
    place (songPanel, 16, 114, 1168, 78);
    place (genreBox, 34, 142, 200, 34);
    place (keyBox, 246, 142, 96, 34);
    place (progBox, 354, 142, 360, 34);
    place (chordLenBox, 726, 142, 128, 34);
    place (barsBox, 866, 142, 118, 34);
    place (midiOutBox, 996, 142, 170, 34);

    // layer rack
    place (layersPanel, 16, 200, 312, 478);
    place (allViewBtn, 258, 206, 58, 24);
    for (int l = 0; l < te::kNumLayers; ++l)
        place (*rows[l], 22, 234 + l * 54, 300, 54);

    // drop view
    place (rollPanel, 336, 200, 564, 478);
    place (chordStrip, 350, 230, 536, 50);
    place (roll, 350, 286, 536, 380);

    // selected layer
    place (layerPanel, 908, 200, 276, 478);
    place (patternBox, 924, 298, 244, 34);
    place (octaveKnob, 920, 366, 82, 94);
    place (movementKnob, 1005, 366, 82, 94);
    place (lengthKnob, 1090, 366, 82, 94);
    place (loadSoundBtn, 924, 520, 62, 26);
    place (clearSoundBtn, 990, 520, 62, 26);
    place (rootDownBtn, 1086, 520, 26, 26);
    place (rootUpBtn, 1142, 520, 26, 26);
    place (simplerToggle, 920, 550, 248, 22);
    place (layerDrag, 924, 612, 244, 46);
    place (soundDrag, 1048, 612, 120, 46);

    // seed + export
    place (bottomPanel, 16, 686, 1168, 82);
    place (seedZone, 30, 696, 618, 62);
    place (captureBtn, 658, 703, 114, 48);
    place (clearSeedBtn, 778, 703, 78, 48);
    place (allDrag, 866, 700, 196, 54);
    place (saveBtn, 1070, 703, 102, 48);
}

void TearoutEditor::selectLayer (int l)
{
    l = jlimit (0, te::kNumLayers - 1, l);
    proc.selectedLayer.store (l);
    for (auto* r : rows)
    {
        r->selected = r->layer == l;
        r->repaint();
    }
    if (viewAll)
    {
        viewAll = false;
        allViewBtn.setToggleState (false, dontSendNotification);
    }
    roll.layer = l;
    bindLayerControls();
    roll.repaint();
    overlay.repaint();
}

void TearoutEditor::bindLayerControls()
{
    const int l = proc.selectedLayer.load();
    const auto accent = col::layer (l);
    patternAtt.reset();
    octaveAtt.reset();
    movementAtt.reset();
    lengthAtt.reset();
    patternBox.clear (dontSendNotification);
    auto names = te::patternChoiceNames ((te::Layer) l);
    for (size_t i = 0; i < names.size(); ++i) patternBox.addItem (u8 (names[i]), (int) i + 1);
    auto& apvts = proc.apvts;
    patternAtt = std::make_unique<ComboAtt> (apvts, ids::layer (l, "pattern"), patternBox);
    octaveAtt = std::make_unique<SliderAtt> (apvts, ids::layer (l, "octave"), octaveKnob.slider);
    movementAtt = std::make_unique<SliderAtt> (apvts, ids::layer (l, "movement"), movementKnob.slider);
    lengthAtt = std::make_unique<SliderAtt> (apvts, ids::layer (l, "length"), lengthKnob.slider);
    patternBox.setColour (ComboBox::outlineColourId, accent);
    for (auto* k : { &octaveKnob, &movementKnob, &lengthKnob })
    {
        k->slider.setColour (Slider::rotarySliderFillColourId, accent);
        k->repaint();
    }
    const bool pitched = te::layerUsesOctave ((te::Layer) l);
    octaveKnob.setEnabled (pitched);
    lengthKnob.setEnabled (te::layerUsesLength ((te::Layer) l));
    octaveKnob.setAlpha (pitched ? 1.0f : 0.3f);
    lengthKnob.setAlpha (te::layerUsesLength ((te::Layer) l) ? 1.0f : 0.3f);
    layerPanel.accent = accent;
    layerPanel.repaint();
    layerDrag.colour = accent;
    layerDrag.text = String ("DRAG ") + te::layerName ((te::Layer) l) + " MIDI";
    layerDrag.repaint();
    soundDrag.colour = accent;
    refreshSoundControls();
    songChanged();
}

void TearoutEditor::refreshSoundControls()
{
    const int l = proc.selectedLayer.load();
    const bool takes = TearoutProcessor::layerTakesSound (l);
    const bool has = takes && proc.getSound (l).data != nullptr;
    loadSoundBtn.setVisible (takes);
    clearSoundBtn.setVisible (has);
    rootDownBtn.setVisible (has);
    rootUpBtn.setVisible (has);
    simplerToggle.setVisible (has);
    simplerToggle.setToggleState (has && proc.getSound (l).simplerMidi, dontSendNotification);
    soundDrag.setVisible (has);
    layerDrag.setBounds (924, 612, has ? 120 : 244, 46);
    soundDrag.setBounds (1048, 612, 120, 46);
    layerDrag.text = has ? String ("MIDI") : String ("DRAG ") + te::layerName ((te::Layer) l) + " MIDI";
    soundDrag.text = "SOUND";
    layerDrag.repaint();
    for (auto* r : rows)
    {
        const bool rowHas = proc.getSound (r->layer).data != nullptr;
        if (r->hasSound != rowHas) { r->hasSound = rowHas; r->repaint(); }
    }
    overlay.repaint();
}

void TearoutEditor::refreshProgressionStars()
{
    const auto& g = te::genreProfile (proc.currentSettings().genre);
    auto names = te::progressionChoiceNames();
    for (int i = te::kProgressionFirst; i < (int) names.size(); ++i)
    {
        bool pick = false;
        for (auto& pk : g.picks) if (pk.first == i - te::kProgressionFirst) pick = true;
        progBox.changeItemText (i + 1, (pick ? String::fromUTF8 ("\xe2\x98\x85  ") : String ("     ")) + u8 (names[(size_t) i]));
    }
    auto seed = proc.getSeed();
    progBox.changeItemText (te::kProgressionSeed + 1, seed.valid ? String::fromUTF8 ("From Seed  \xc2\xb7  ") + u8 (seed.summary)
                                                                  : String ("From Seed MIDI (no seed loaded)"));
}

void TearoutEditor::songChanged()
{
    song = proc.getSong();
    chordStrip.song = song;
    roll.song = song;
    if (! song) return;
    const auto settings = proc.currentSettings();
    for (auto* r : rows)
    {
        const int l = r->layer;
        const int pid = song->pattern[(size_t) l];
        r->hasNotes = ! song->layers[(size_t) l].empty();
        const String name = te::patternsFor ((te::Layer) l)[(size_t) pid].name;
        r->patternText = settings.layers[(size_t) l].pattern == 0 ? "Genre: " + name : name;
        r->repaint();
    }
    // the "Genre" item shows what it resolves to
    {
        const int l = proc.selectedLayer.load();
        const int pid = song->pattern[(size_t) l];
        patternBox.changeItemText (1, String::fromUTF8 ("Genre  \xc2\xb7  ") + te::patternsFor ((te::Layer) l)[(size_t) pid].name);
    }
    refreshProgressionStars();
    seedZone.summary = proc.getSeed().valid ? u8 (proc.getSeed().summary) : String();
    seedZone.source = proc.getSeedSource();
    chordStrip.repaint();
    roll.repaint();
    seedZone.repaint();
    overlay.repaint();
}

void TearoutEditor::setStatus (const String& s)
{
    status = s;
    statusTime = Time::getMillisecondCounter();
    overlay.repaint();
}

void TearoutEditor::timerCallback()
{
    const bool running = proc.isRunning();
    const double tick = running ? proc.playheadTicks() : -1.0;
    if (tick != roll.playTick)
    {
        roll.playTick = tick;
        chordStrip.playTick = tick;
        roll.repaint();
        chordStrip.repaint();
    }
    uint32_t audible = 0;
    for (int l = 0; l < te::kNumLayers; ++l)
    {
        bool solo = false;
        for (int k = 0; k < te::kNumLayers; ++k) solo = solo || proc.isSoloed (k);
        const bool on = solo ? proc.isSoloed (l) : ! proc.isMuted (l);
        if (on) audible |= 1u << l;
    }
    if (audible != roll.audible) { roll.audible = audible; roll.repaint(); }

    const bool cap = proc.isCapturing();
    if (captureBtn.getToggleState() != cap) captureBtn.setToggleState (cap, dontSendNotification);
    if (playBtn.getToggleState() != proc.isPreviewPlaying()) playBtn.setToggleState (proc.isPreviewPlaying(), dontSendNotification);
    if (followBtn.getToggleState() != proc.isFollowingLive()) followBtn.setToggleState (proc.isFollowingLive(), dontSendNotification);
    blink += 0.25f;
    if (cap || seedZone.capturing != cap)
    {
        seedZone.capturing = cap;
        seedZone.capturedNotes = proc.capturedNoteCount();
        seedZone.blink = blink;
        seedZone.repaint();
    }
    const String summary = proc.getSeed().valid ? u8 (proc.getSeed().summary) : String();
    if (summary != seedZone.summary) songChanged();
    if (status.isNotEmpty() && Time::getMillisecondCounter() - statusTime > 5000)
    {
        status.clear();
        overlay.repaint();
    }
}

// ---- files ---------------------------------------------------------------------------------------------------
static bool isMidiFile (const String& f) { return f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi"); }

bool TearoutEditor::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files) if (isMidiFile (f) || TearoutProcessor::isAudioFile (File (f))) return true;
    return false;
}

int TearoutEditor::rowAt (Point<int> editorPos) const
{
    const auto p = root.getLocalPoint (this, editorPos);
    for (auto* r : rows)
        if (r->getBounds().contains (p)) return r->layer;
    return -1;
}

bool TearoutEditor::overLayerPanel (Point<int> editorPos) const
{
    return layerPanel.getBounds().contains (root.getLocalPoint (this, editorPos));
}

void TearoutEditor::setDropHover (int layer)
{
    for (auto* r : rows)
    {
        const bool h = r->layer == layer;
        if (r->dropHover != h) { r->dropHover = h; r->repaint(); }
    }
}

static bool hasAudio (const StringArray& files)
{
    for (auto& f : files) if (TearoutProcessor::isAudioFile (File (f))) return true;
    return false;
}

void TearoutEditor::fileDragEnter (const StringArray& files, int x, int y) { fileDragMove (files, x, y); }

void TearoutEditor::fileDragMove (const StringArray& files, int x, int y)
{
    if (hasAudio (files))
    {
        int l = rowAt ({ x, y });
        if (l < 0 && overLayerPanel ({ x, y })) l = proc.selectedLayer.load();
        setDropHover (TearoutProcessor::layerTakesSound (l) ? l : -1);
        if (seedZone.hovering) { seedZone.hovering = false; seedZone.repaint(); }
        return;
    }
    if (! seedZone.hovering) { seedZone.hovering = true; seedZone.repaint(); }
}

void TearoutEditor::fileDragExit (const StringArray&)
{
    seedZone.hovering = false;
    seedZone.repaint();
    setDropHover (-1);
}

void TearoutEditor::filesDropped (const StringArray& files, int x, int y)
{
    seedZone.hovering = false;
    seedZone.repaint();
    setDropHover (-1);
    for (auto& f : files)
    {
        if (isMidiFile (f))
        {
            String msg;
            proc.loadSeedFile (File (f), msg);
            setStatus (msg);
            break;
        }
        if (TearoutProcessor::isAudioFile (File (f)))
        {
            int l = rowAt ({ x, y });
            if (l < 0) l = proc.selectedLayer.load();
            String msg;
            if (proc.loadLayerSound (l, File (f), msg)) selectLayer (l);
            setStatus (msg);
            break;
        }
    }
    refreshSoundControls();
    songChanged();
}

void TearoutEditor::browseForSound (int layer)
{
    chooser = std::make_unique<FileChooser> ("Choose a sound for " + String (te::layerName ((te::Layer) layer)),
                                             File::getSpecialLocation (File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this, layer] (const FileChooser& fc) {
        auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        String msg;
        proc.loadLayerSound (layer, f, msg);
        setStatus (msg);
        refreshSoundControls();
        songChanged();
    });
}

void TearoutEditor::browseForSeed()
{
    chooser = std::make_unique<FileChooser> ("Choose a seed MIDI clip", File::getSpecialLocation (File::userHomeDirectory), "*.mid;*.midi");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc) {
        auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        String msg;
        proc.loadSeedFile (f, msg);
        setStatus (msg);
        songChanged();
    });
}

void TearoutEditor::saveMidiFiles()
{
    chooser = std::make_unique<FileChooser> ("Choose a folder for the MIDI files", File::getSpecialLocation (File::userMusicDirectory));
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [this] (const FileChooser& fc) {
        auto dir = fc.getResult();
        if (dir == File()) return;
        String msg;
        proc.saveAllMidi (dir, msg);
        setStatus (msg);
    });
}

// ---- static text drawn above the glass ------------------------------------------------------------------------
void TearoutEditor::paintOverlay (Graphics& g)
{
    auto caption = [&] (const String& t, int x, int y, int w) {
        g.setColour (col::dim);
        g.setFont (uiFont (11.0f, true));
        g.drawText (t, x + 2, y, w, 14, Justification::centredLeft, false);
    };
    caption ("GENRE", 34, 126, 200);
    caption ("KEY", 246, 126, 96);
    caption ("PROGRESSION", 354, 126, 360);
    caption ("CHORD LENGTH", 726, 126, 128);
    caption ("BARS", 866, 126, 118);
    caption ("MIDI OUT", 996, 126, 170);

    // genre summary next to the logo
    const auto settings = proc.currentSettings();
    const auto& gp = te::genreProfile (settings.genre);
    g.setColour (col::text.withAlpha (0.85f));
    g.setFont (uiFont (13.0f, true));
    g.drawText (String (gp.name).toUpperCase(), 420, 22, 210, 18, Justification::centredLeft, false);
    g.setColour (col::dim);
    g.setFont (uiFont (12.0f));
    g.drawFittedText (gp.summary, 420, 40, 210, 60, Justification::topLeft, 4, 0.9f);

    // roll info
    if (song)
    {
        String info = String (song->bars) + " bars  " + String::fromUTF8 ("\xc2\xb7") + "  " + String (song->loopBars) + "-bar loop  "
                    + String::fromUTF8 ("\xc2\xb7") + "  " + String (proc.currentBpm(), 0) + " BPM";
        if (! song->hints.empty()) info = u8 (song->hints.front());
        if (status.isNotEmpty()) info = status;
        g.setColour (status.isNotEmpty() ? col::cyan : (song->hints.empty() ? col::dim : col::amber));
        g.setFont (uiFont (12.0f, status.isNotEmpty()));
        g.drawText (info, 420, 208, 462, 16, Justification::centredRight, true);
    }

    // selected layer panel text
    const int l = proc.selectedLayer.load();
    const auto accent = col::layer (l);
    drawNeonText (g, kTitles[l], Rectangle<float> (924, 214, 244, 26), accent, 20.0f);
    g.setColour (col::text.withAlpha (0.8f));
    g.setFont (uiFont (12.5f));
    g.drawFittedText (te::layerHelp ((te::Layer) l), 924, 242, 244, 34, Justification::topLeft, 3, 0.9f);
    caption ("PATTERN", 922, 282, 244);
    if (song)
    {
        const int pid = song->pattern[(size_t) l];
        g.setColour (col::dim);
        g.setFont (uiFont (12.0f));
        g.drawFittedText (te::patternsFor ((te::Layer) l)[(size_t) pid].help, 924, 336, 244, 30, Justification::topLeft, 2, 0.85f);
    }
    // sound slot
    caption ("SOUND", 922, 462, 244);
    if (TearoutProcessor::layerTakesSound (l))
    {
        const auto& snd = proc.getSound (l);
        if (snd.data)
        {
            drawNeonText (g, snd.file.getFileNameWithoutExtension(), Rectangle<float> (924, 478, 244, 18), Colours::white, 13.0f);
            g.setColour (col::dim);
            g.setFont (uiFont (11.5f));
            static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            g.drawFittedText ("root " + String (n[snd.root % 12]) + String (snd.root / 12 - 2) + "  " + String::fromUTF8 ("\xc2\xb7") + "  " + snd.rootInfo,
                              924, 496, 244, 18, Justification::centredLeft, 1, 0.8f);
            g.setColour (col::text);
            g.setFont (uiFont (13.0f, true));
            g.drawText (String (n[snd.root % 12]) + String (snd.root / 12 - 2), 1112, 520, 30, 26, Justification::centred, false);
            g.setColour (col::dim);
            g.setFont (uiFont (10.5f, true));
            g.drawText ("ROOT", 1056, 520, 28, 26, Justification::centredRight, false);
        }
        else
        {
            g.setColour (col::dim);
            g.setFont (uiFont (11.5f));
            g.drawFittedText ("Preview with your own sound: drop a WAV / AIFF on this layer's row, or LOAD.", 924, 478, 244, 36,
                              Justification::topLeft, 2, 0.9f);
        }
    }
    else
    {
        g.setColour (col::dim);
        g.setFont (uiFont (11.5f));
        g.drawFittedText ("Sound slots are for the pitched layers (chords, sub, bass, lead, arp).", 924, 478, 244, 36, Justification::topLeft, 2, 0.9f);
    }
    if (song && ! song->layers[(size_t) l].empty())
    {
        const auto& notes = song->layers[(size_t) l];
        int lo = 127, hi = 0;
        for (auto& n : notes) { lo = jmin (lo, n.pitch); hi = jmax (hi, n.pitch); }
        String stats = String ((int) notes.size()) + " notes";
        if (te::layerUsesOctave ((te::Layer) l))
            stats << "  " << String::fromUTF8 ("\xc2\xb7") << "  " << u8 (te::pitchName (lo, song->flats)) << " - " << u8 (te::pitchName (hi, song->flats));
        g.setColour (accent.withAlpha (0.9f));
        g.setFont (uiFont (12.0f, true));
        g.drawText (stats, 924, 582, 244, 18, Justification::centredLeft, false);
    }
}

} // namespace tep
