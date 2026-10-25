#include "Widgets.h"

namespace tep
{
using namespace juce;

void CircuitBackground::paint (Graphics& g)
{
    if (! cache.isValid() || cache.getWidth() != roundToInt (getWidth() * renderScale))
        cache = makeCircuitImage (getWidth(), getHeight(), renderScale);
    g.drawImage (cache, getLocalBounds().toFloat());
}

void GlassPanel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    drawGlass (g, r, accent, 14.0f);
    if (title.isNotEmpty())
    {
        auto t = r.reduced (14.0f, 6.0f).withHeight (18.0f);
        g.setColour (accent);
        g.fillRect (t.getX(), t.getCentreY() - 4.0f, 3.0f, 8.0f);
        drawNeonText (g, title, t.withTrimmedLeft (9.0f), accent.brighter (0.3f), 12.5f);
        if (rightText.isNotEmpty())
        {
            g.setColour (col::dim);
            g.setFont (uiFont (12.0f));
            g.drawText (rightText, t, Justification::centredRight, true);
        }
    }
}

// ---- knob ---------------------------------------------------------------------------------------------
Knob::Knob (const String& name, Colour accent) : caption (name)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setColour (Slider::rotarySliderFillColourId, accent);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.22f, MathConstants<float>::pi * 2.78f, true);
    slider.setVelocityBasedMode (false);
    slider.setMouseDragSensitivity (180);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (16);
    r.removeFromBottom (16);
    slider.setBounds (r);
}

void Knob::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::dim);
    g.setFont (uiFont (11.5f, true));
    g.drawText (caption.toUpperCase(), r.removeFromTop (16.0f), Justification::centred, false);
    g.setColour (col::text);
    g.setFont (uiFont (13.0f, true));
    const String v = format ? format (slider.getValue()) : slider.getTextFromValue (slider.getValue());
    g.drawText (v, r.removeFromBottom (16.0f), Justification::centred, false);
}

// ---- chord strip -----------------------------------------------------------------------------------------
void ChordStrip::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (! song || song->chords.empty()) return;
    const int loopTicks = jmin (song->totalTicks, song->loopBars * te::BAR);
    StringArray numerals;
    if (song->progressionLabel != "From seed")
        numerals.addTokens (String::fromUTF8 (song->progressionLabel.c_str()), String::fromUTF8 ("\xE2\x80\x93"), {});

    // key badge
    auto keyBox = r.removeFromLeft (92.0f).reduced (2.0f, 3.0f);
    g.setColour (col::magenta.withAlpha (0.14f));
    g.fillRoundedRectangle (keyBox, 7.0f);
    g.setColour (col::magenta.withAlpha (0.7f));
    g.drawRoundedRectangle (keyBox, 7.0f, 1.0f);
    const String keyText = String::fromUTF8 (te::keyName (song->keyRoot, song->mode).c_str()).toUpperCase();
    drawNeonText (g, keyText, keyBox, Colours::white, 13.0f, Justification::centred);
    r.removeFromLeft (6.0f);

    const double playInLoop = playTick >= 0 ? std::fmod (playTick, (double) loopTicks) : -1.0;
    int idx = 0;
    for (auto& e : song->chords)
    {
        if (e.start >= loopTicks) break;
        const float x0 = r.getX() + r.getWidth() * (float) e.start / (float) loopTicks;
        const float x1 = r.getX() + r.getWidth() * (float) jmin (e.end(), loopTicks) / (float) loopTicks;
        auto chip = Rectangle<float> (x0, r.getY(), x1 - x0, r.getHeight()).reduced (2.0f, 3.0f);
        const bool lit = playInLoop >= e.start && playInLoop < e.end();
        const auto accent = lit ? col::cyan : col::violet;
        g.setColour (accent.withAlpha (lit ? 0.24f : 0.10f));
        g.fillRoundedRectangle (chip, 7.0f);
        g.setColour (accent.withAlpha (lit ? 1.0f : 0.5f));
        g.drawRoundedRectangle (chip, 7.0f, lit ? 1.6f : 1.0f);
        const String name = String::fromUTF8 (e.chord.name (song->flats).c_str());
        auto top = chip.withTrimmedBottom (chip.getHeight() * 0.38f);
        drawNeonText (g, name, top, lit ? Colours::white : col::text, jmin (18.0f, chip.getHeight() * 0.42f), Justification::centredBottom);
        if (idx < numerals.size())
        {
            g.setColour (col::dim);
            g.setFont (uiFont (11.0f, true));
            g.drawText (numerals[idx], chip.withTrimmedTop (chip.getHeight() * 0.6f), Justification::centredTop, false);
        }
        ++idx;
    }
}

// ---- piano roll ---------------------------------------------------------------------------------------------
static String drumName (int p)
{
    switch (p)
    {
        case te::drum::Kick: return "KICK";
        case te::drum::Snare: return "SNARE";
        case te::drum::Clap: return "CLAP";
        case te::drum::ClosedHat: return "HAT";
        case te::drum::OpenHat: return "OPEN";
        case te::drum::Crash: return "CRASH";
        case te::drum::Rim: return "RIM";
        case te::drum::Tom: return "TOM";
        case te::fx::Impact: return "IMPACT";
        case te::fx::Downlifter: return "DOWN";
        case te::fx::Riser: return "RISER";
        case te::fx::ReverseCrash: return "REVERSE";
        case te::fx::NoiseSweep: return "SWEEP";
        default: return String (p);
    }
}

static const char* laneNames[] = { "CHORDS", "SUB", "BASS A", "BASS B", "LEAD", "ARP", "DRUMS", "FX" };

void PianoRoll::paint (Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    g.setColour (Colour (0xff04050e).withAlpha (0.55f));
    g.fillRoundedRectangle (full, 8.0f);
    if (! song || song->totalTicks <= 0) return;

    const float gutter = 50.0f;
    auto area = full.withTrimmedLeft (gutter).reduced (2.0f, 4.0f);
    const float total = (float) song->totalTicks;
    auto xOf = [&] (int tick) { return area.getX() + area.getWidth() * (float) tick / total; };

    // chord bands
    int ci = 0;
    for (auto& e : song->chords)
    {
        auto band = Rectangle<float> (xOf (e.start), area.getY(), xOf (e.end()) - xOf (e.start), area.getHeight());
        g.setColour (Colours::white.withAlpha ((ci++ % 2) ? 0.035f : 0.012f));
        g.fillRect (band);
    }
    // grid
    for (int t = 0; t <= song->totalTicks; t += te::BEAT)
    {
        const bool bar = t % te::BAR == 0;
        g.setColour (Colours::white.withAlpha (bar ? (t % (4 * te::BAR) == 0 ? 0.24f : 0.13f) : 0.045f));
        g.fillRect (xOf (t) - 0.5f, area.getY(), 1.0f, area.getHeight());
        if (bar && t < song->totalTicks && (song->bars <= 16 || t % (2 * te::BAR) == 0))
        {
            g.setColour (col::dim);
            g.setFont (uiFont (10.0f, true));
            g.drawText (String (t / te::BAR + 1), Rectangle<float> (xOf (t) + 3, area.getY() + 1, 24, 12), Justification::left, false);
        }
    }

    auto drawNote = [&] (Rectangle<float> nr, Colour c, int vel, bool dimmed) {
        const float a = (dimmed ? 0.25f : 1.0f) * (0.45f + 0.55f * (float) vel / 127.0f);
        nr = nr.withWidth (jmax (2.0f, nr.getWidth() - 1.0f));
        g.setColour (c.withAlpha (0.22f * a));
        g.fillRoundedRectangle (nr.expanded (2.0f), 3.0f);
        ColourGradient fill (c.brighter (0.35f).withAlpha (a), nr.getX(), nr.getY(), c.darker (0.2f).withAlpha (a), nr.getX(), nr.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (nr, jmin (3.0f, nr.getHeight() * 0.4f));
        g.setColour (Colours::white.withAlpha (0.35f * a));
        g.fillRect (nr.getX() + 1, nr.getY(), nr.getWidth() - 2, jmin (1.5f, nr.getHeight() * 0.3f));
    };

    if (layer < 0)
    {
        // all layers as lanes
        const float laneH = area.getHeight() / (float) te::kNumLayers;
        for (int l = 0; l < te::kNumLayers; ++l)
        {
            auto lane = Rectangle<float> (area.getX(), area.getY() + l * laneH, area.getWidth(), laneH);
            const auto c = col::layer (l);
            const bool dimmed = ! ((audible >> l) & 1u);
            g.setColour (c.withAlpha (0.04f));
            g.fillRect (lane);
            g.setColour (Colours::white.withAlpha (0.06f));
            g.fillRect (lane.getX(), lane.getBottom() - 0.5f, lane.getWidth(), 1.0f);
            g.setColour (dimmed ? col::dim.withAlpha (0.5f) : c);
            g.setFont (uiFont (10.5f, true));
            g.drawText (laneNames[l], Rectangle<float> (full.getX() + 4, lane.getY(), gutter - 6, laneH), Justification::centredLeft, false);
            const auto& notes = song->layers[(size_t) l];
            if (notes.empty()) continue;
            int lo = 127, hi = 0;
            for (auto& n : notes) { lo = jmin (lo, n.pitch); hi = jmax (hi, n.pitch); }
            const float range = (float) jmax (6, hi - lo + 1);
            const float rowH = jmax (1.5f, (laneH - 6.0f) / range);
            for (auto& n : notes)
            {
                const float y = lane.getBottom() - 3.0f - (float) (n.pitch - lo + 1) * (laneH - 6.0f) / range;
                drawNote ({ xOf (n.start), y, xOf (n.end()) - xOf (n.start), jmin (rowH, 6.0f) }, c, n.velocity, dimmed);
            }
        }
    }
    else
    {
        const auto& notes = song->layers[(size_t) layer];
        const auto c = col::layer (layer);
        const bool drumsLike = layer == (int) te::Layer::Drums || layer == (int) te::Layer::FX;
        if (notes.empty())
        {
            drawNeonText (g, "OFF", area, col::dim, 22.0f, Justification::centred);
        }
        else if (drumsLike)
        {
            std::vector<int> pitches;
            for (auto& n : notes)
                if (std::find (pitches.begin(), pitches.end(), n.pitch) == pitches.end()) pitches.push_back (n.pitch);
            std::sort (pitches.begin(), pitches.end(), std::greater<int>());
            const float rowH = area.getHeight() / (float) pitches.size();
            for (size_t i = 0; i < pitches.size(); ++i)
            {
                auto row = Rectangle<float> (area.getX(), area.getY() + i * rowH, area.getWidth(), rowH);
                g.setColour (Colours::white.withAlpha (i % 2 ? 0.02f : 0.0f));
                g.fillRect (row);
                g.setColour (col::dim);
                g.setFont (uiFont (10.5f, true));
                g.drawText (drumName (pitches[i]), Rectangle<float> (full.getX() + 4, row.getY(), gutter - 6, rowH), Justification::centredLeft, false);
            }
            for (auto& n : notes)
            {
                auto it = std::find (pitches.begin(), pitches.end(), n.pitch);
                const float y = area.getY() + (float) (it - pitches.begin()) * rowH;
                drawNote ({ xOf (n.start), y + rowH * 0.2f, jmax (3.0f, xOf (n.end()) - xOf (n.start)), rowH * 0.6f }, c, n.velocity, false);
            }
        }
        else
        {
            int lo = 127, hi = 0;
            for (auto& n : notes) { lo = jmin (lo, n.pitch); hi = jmax (hi, n.pitch); }
            lo -= 2;
            hi += 2;
            while (hi - lo + 1 < 15) { --lo; ++hi; }
            const float rowH = area.getHeight() / (float) (hi - lo + 1);
            for (int p = lo; p <= hi; ++p)
            {
                const float y = area.getY() + (float) (hi - p) * rowH;
                const int k = te::pc (p);
                const bool black = k == 1 || k == 3 || k == 6 || k == 8 || k == 10;
                if (black)
                {
                    g.setColour (Colours::black.withAlpha (0.18f));
                    g.fillRect (area.getX(), y, area.getWidth(), rowH);
                }
                if (k == 0 || rowH >= 11.0f)
                {
                    g.setColour (k == 0 ? col::text.withAlpha (0.8f) : col::dim.withAlpha (0.8f));
                    g.setFont (uiFont (jmin (11.0f, rowH * 0.9f), k == 0));
                    g.drawText (String::fromUTF8 (te::pitchName (p, song->flats).c_str()),
                                Rectangle<float> (full.getX() + 4, y, gutter - 8, rowH), Justification::centredRight, false);
                }
            }
            for (auto& n : notes)
            {
                const float y = area.getY() + (float) (hi - n.pitch) * rowH;
                drawNote ({ xOf (n.start), y + 0.5f, xOf (n.end()) - xOf (n.start), jmax (2.0f, rowH - 1.0f) }, c, n.velocity, false);
            }
        }
    }

    // playhead
    if (playTick >= 0)
    {
        const float x = xOf ((int) std::fmod (playTick, total));
        g.setColour (col::cyan.withAlpha (0.18f));
        g.fillRect (x - 3.0f, area.getY(), 6.0f, area.getHeight());
        g.setColour (Colours::white);
        g.fillRect (x - 0.75f, area.getY(), 1.5f, area.getHeight());
    }
}

// ---- drag handle -------------------------------------------------------------------------------------------
void DragHandle::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    const bool over = isMouseOver();
    const float radius = jmin (r.getHeight() * 0.5f, 10.0f);
    ColourGradient fill (colour.withAlpha (over ? 0.42f : (big ? 0.30f : 0.16f)), r.getX(), r.getY(),
                         colour.withAlpha (over ? 0.20f : 0.06f), r.getX(), r.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (r, radius);
    g.setColour (colour.withAlpha (over ? 0.35f : 0.15f));
    g.drawRoundedRectangle (r.expanded (1.5f), radius + 1.5f, 3.5f);
    g.setColour (colour.withAlpha (over ? 1.0f : 0.75f));
    g.drawRoundedRectangle (r, radius, 1.2f);
    // grip: three dots + a down arrow
    auto icon = r.removeFromLeft (jmin (r.getHeight(), 34.0f)).reduced (jmin (r.getHeight(), 34.0f) * 0.3f);
    Path arrow;
    arrow.startNewSubPath (icon.getCentreX(), icon.getY());
    arrow.lineTo (icon.getCentreX(), icon.getBottom());
    arrow.startNewSubPath (icon.getX() + 1, icon.getBottom() - icon.getHeight() * 0.42f);
    arrow.lineTo (icon.getCentreX(), icon.getBottom());
    arrow.lineTo (icon.getRight() - 1, icon.getBottom() - icon.getHeight() * 0.42f);
    g.setColour (Colours::white);
    g.strokePath (arrow, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setFont (uiFont (big ? 14.0f : 11.5f, true));
    g.drawText (text, r.withTrimmedRight (6.0f), Justification::centredLeft, false);
}

void DragHandle::mouseDrag (const MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 4) return;
    dragging = true;
    File f = makeFile ? makeFile() : File();
    if (! f.existsAsFile())
    {
        if (onStatus) onStatus ("Nothing to drag: that layer is Off.");
        return;
    }
    if (onStatus) onStatus ("Drop it on a MIDI track: " + f.getFileName());
    DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
}

// ---- layer row -------------------------------------------------------------------------------------------------
LayerRow::LayerRow (int l) : layer (l), handle ("MIDI", col::layer (l))
{
    for (auto* b : { &mute, &solo })
    {
        b->setClickingTogglesState (true);
        b->getProperties().set ("fontSize", 11.0f);
        addAndMakeVisible (*b);
    }
    mute.setColour (TextButton::buttonColourId, col::amber);
    solo.setColour (TextButton::buttonColourId, col::lime);
    mute.setTooltip ("Mute this layer in the preview (it still exports).");
    solo.setTooltip ("Solo this layer in the preview.");
    handle.setTooltip ("Drag this layer's MIDI clip onto a track in Ableton.");
    addAndMakeVisible (handle);
}

void LayerRow::resized()
{
    auto r = getLocalBounds().reduced (6, 7);
    handle.setBounds (r.removeFromRight (74));
    r.removeFromRight (6);
    solo.setBounds (r.removeFromRight (26).withSizeKeepingCentre (26, 24));
    r.removeFromRight (3);
    mute.setBounds (r.removeFromRight (26).withSizeKeepingCentre (26, 24));
}

void LayerRow::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f, 2.0f);
    const auto c = col::layer (layer);
    g.setColour (selected ? c.withAlpha (0.16f) : Colours::white.withAlpha (isMouseOver (true) ? 0.04f : 0.015f));
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (selected ? c.withAlpha (0.9f) : Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (r, 9.0f, selected ? 1.4f : 1.0f);
    if (selected)
    {
        g.setColour (c.withAlpha (0.2f));
        g.drawRoundedRectangle (r.expanded (1.5f), 10.5f, 3.0f);
    }
    // LED
    auto led = Rectangle<float> (r.getX() + 10, r.getCentreY() - 5, 10, 10);
    if (hasNotes)
    {
        g.setColour (c.withAlpha (0.3f));
        g.fillEllipse (led.expanded (4));
        g.setColour (c);
    }
    else
        g.setColour (Colour (0xff1b1f38));
    g.fillEllipse (led);
    g.setColour (Colours::white.withAlpha (hasNotes ? 0.6f : 0.15f));
    g.fillEllipse (led.getX() + 2.5f, led.getY() + 1.5f, 4, 3);

    auto t = r.withTrimmedLeft (30).withRight ((float) mute.getX() - 4);
    drawNeonText (g, laneNames[layer], t.withTrimmedBottom (t.getHeight() * 0.45f), hasNotes ? c : col::dim, 15.0f, Justification::bottomLeft);
    g.setColour (col::dim);
    g.setFont (uiFont (11.5f));
    g.drawText (patternText, t.withTrimmedTop (t.getHeight() * 0.56f), Justification::topLeft, true);
}

void LayerRow::mouseUp (const MouseEvent& e)
{
    if (e.mouseWasClicked() && onSelect) onSelect (layer);
}

// ---- seed zone ---------------------------------------------------------------------------------------------------
void SeedZone::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const auto accent = capturing ? col::hot : (hovering ? col::magenta : col::violet);
    g.setColour (accent.withAlpha (hovering ? 0.18f : 0.07f));
    g.fillRoundedRectangle (r, 10.0f);
    Path border;
    border.addRoundedRectangle (r, 10.0f);
    Path dashed;
    const float dashes[] = { 6.0f, 4.0f };
    PathStrokeType (1.3f).createDashedStroke (dashed, border, dashes, 2);
    g.setColour (accent.withAlpha (0.85f));
    g.fillPath (dashed);

    auto icon = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.26f);
    // little piano-roll icon
    g.setColour (accent);
    g.drawRoundedRectangle (icon, 3.0f, 1.3f);
    for (int i = 0; i < 3; ++i)
        g.fillRoundedRectangle (icon.getX() + 3 + i * icon.getWidth() * 0.27f, icon.getY() + 3 + (i % 2) * icon.getHeight() * 0.3f,
                                icon.getWidth() * 0.3f, icon.getHeight() * 0.18f, 1.5f);

    auto text = r.reduced (4.0f, 4.0f);
    if (capturing)
    {
        const float a = 0.55f + 0.45f * std::sin (blink);
        g.setColour (col::hot.withAlpha (a));
        g.fillEllipse (text.getX(), text.getY() + 6, 9, 9);
        drawNeonText (g, "CAPTURING  " + String (capturedNotes) + " notes", text.withTrimmedLeft (15).removeFromTop (text.getHeight() * 0.5f),
                      Colours::white, 14.0f, Justification::centredLeft);
        g.setColour (col::dim);
        g.setFont (uiFont (12.0f));
        g.drawText ("Play your clip on this track, then stop the transport or press CAPTURE again.",
                    text.withTrimmedTop (text.getHeight() * 0.5f), Justification::centredLeft, true);
        return;
    }
    if (summary.isEmpty())
    {
        drawNeonText (g, String::fromUTF8 ("SEED  \xc2\xb7  drop a .mid here"), text.removeFromTop (text.getHeight() * 0.5f), col::text, 14.0f, Justification::centredLeft);
        g.setColour (col::dim);
        g.setFont (uiFont (12.0f));
        g.drawText (String::fromUTF8 ("or CAPTURE: put the clip on this track, arm, press play  \xc2\xb7  click to browse"),
                    text, Justification::centredLeft, true);
    }
    else
    {
        drawNeonText (g, "SEED  " + summary, text.removeFromTop (text.getHeight() * 0.5f), Colours::white, 14.0f, Justification::centredLeft);
        g.setColour (col::dim);
        g.setFont (uiFont (12.0f));
        g.drawText ("from " + source + String::fromUTF8 ("  \xc2\xb7  PROGRESSION = From Seed uses its chords"), text, Justification::centredLeft, true);
    }
}
} // namespace tep
