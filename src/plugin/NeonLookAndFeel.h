// TEAROUT ENGINE - neon / clear-polyurethane look: glass panels over a circuit board, pixel logo
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace tep
{
namespace col
{
const juce::Colour bg        { 0xff05040b };
const juce::Colour text      { 0xffe8f7ff };
const juce::Colour dim       { 0xff8090b8 };
const juce::Colour cyan      { 0xff29f3ff };
const juce::Colour magenta   { 0xffff2bd6 };
const juce::Colour violet    { 0xff9b5cff };
const juce::Colour lime      { 0xffb6ff3b };
const juce::Colour hot       { 0xffff3d7f };
const juce::Colour amber     { 0xffffb020 };
const juce::Colour glassTop  { 0xff1c2042 };
const juce::Colour glassBot  { 0xff0b0d1f };

// one colour per layer: chords, sub, bass a, bass b, lead, arp, drums, fx
juce::Colour layer (int l);
} // namespace col

juce::Font uiFont (float size, bool bold = false);

// Clear, glossy resin panel: tinted transparent body, specular top, sheen, neon edge.
void drawGlass (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour accent, float radius = 14.0f, float glow = 1.0f);
void drawNeonText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Colour c, float size,
                   juce::Justification j = juce::Justification::centredLeft, bool bold = true);

// Procedural circuit-board background (deterministic).
juce::Image makeCircuitImage (int width, int height, float scale);

// The arcade-style pixel logo. Fits itself into r.
void drawPixelLogo (juce::Graphics& g, juce::Rectangle<float> r, float glowPhase);

class NeonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NeonLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end,
                           juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return uiFont (14.0f, true); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon, const juce::Colour* textColour) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>& area, const juce::String& name) override;
    juce::Font getPopupMenuFont() override { return uiFont (14.5f); }
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
};
} // namespace tep
