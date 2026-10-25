// TEAROUT ENGINE - editor widgets
#pragma once

#include "../core/Engine.h"
#include "NeonLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace tep
{
class CircuitBackground : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void resized() override { cache = {}; }
    float renderScale = 1.0f;

private:
    juce::Image cache;
};

class PixelLogo : public juce::Component, private juce::Timer
{
public:
    PixelLogo() { startTimerHz (15); }
    void paint (juce::Graphics& g) override { drawPixelLogo (g, getLocalBounds().toFloat(), phase); }

private:
    void timerCallback() override { phase += 0.18f; repaint(); }
    float phase = 0.0f;
};

// A glass panel with an optional title in the top-left corner.
class GlassPanel : public juce::Component
{
public:
    GlassPanel (juce::String t = {}, juce::Colour a = col::cyan) : title (std::move (t)), accent (a)
    {
        setInterceptsMouseClicks (false, true);
    }
    void paint (juce::Graphics& g) override;
    juce::String title;
    juce::Colour accent;
    juce::String rightText;
};

class Knob : public juce::Component
{
public:
    Knob (const juce::String& name, juce::Colour accent);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
    juce::String caption;
    std::function<juce::String (double)> format;
};

// Chord chips for the loop, with the one under the playhead lit up.
class ChordStrip : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    std::shared_ptr<const te::Song> song;
    double playTick = -1;
};

// Piano roll: one layer in detail, or every layer as stacked lanes (layer = -1).
class PianoRoll : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    std::shared_ptr<const te::Song> song;
    int layer = 1;
    double playTick = -1;
    uint32_t audible = 0xFF;
};

// Drag this to drop a MIDI clip into the DAW.
class DragHandle : public juce::Component, public juce::SettableTooltipClient
{
public:
    DragHandle (juce::String label, juce::Colour accent) : text (std::move (label)), colour (accent)
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { dragging = false; }
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; repaint(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    std::function<juce::File()> makeFile;
    std::function<void (const juce::String&)> onStatus;
    juce::String text;
    juce::Colour colour;
    bool big = false;

private:
    bool dragging = false;
};

class LayerRow : public juce::Component
{
public:
    explicit LayerRow (int layerIndex);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    int layer;
    bool selected = false;
    bool hasNotes = false;
    juce::String patternText;
    juce::TextButton mute { "M" }, solo { "S" };
    DragHandle handle;
    std::function<void (int)> onSelect;
};

// Seed MIDI drop zone (the editor forwards file drags here).
class SeedZone : public juce::Component, public juce::SettableTooltipClient
{
public:
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }
    bool hovering = false;
    bool capturing = false;
    int capturedNotes = 0;
    float blink = 0.0f;
    juce::String summary;
    juce::String source;
    std::function<void()> onClick;
};
} // namespace tep
