#include "NeonLookAndFeel.h"

namespace tep
{
using namespace juce;

Colour col::layer (int l)
{
    static const Colour c[] = {
        Colour (0xffb26bff), // chords  violet
        Colour (0xffff3d7f), // sub     hot pink
        Colour (0xffff8a1f), // bass a  orange
        Colour (0xffffd400), // bass b  yellow
        Colour (0xff3dffb0), // lead    mint
        Colour (0xff29f3ff), // arp     cyan
        Colour (0xff8ca3ff), // drums   periwinkle
        Colour (0xffff4bf0), // fx      magenta
    };
    return c[jlimit (0, 7, l)];
}

Font uiFont (float size, bool bold)
{
    return Font (FontOptions (size, bold ? Font::bold : Font::plain));
}

void drawGlass (Graphics& g, Rectangle<float> r, Colour accent, float radius, float glow)
{
    // soft drop shadow so the panel floats over the board
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (0.0f, 4.0f), radius);

    // clear tinted body: the circuit board shows through
    ColourGradient body (col::glassTop.withAlpha (0.56f), r.getX(), r.getY(), col::glassBot.withAlpha (0.70f), r.getX(), r.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (r, radius);

    // accent tint from below, like light inside the resin
    ColourGradient tint (accent.withAlpha (0.0f), r.getX(), r.getY() + r.getHeight() * 0.4f, accent.withAlpha (0.07f * glow),
                         r.getX(), r.getBottom(), false);
    g.setGradientFill (tint);
    g.fillRoundedRectangle (r, radius);

    // specular top highlight
    auto top = r.reduced (1.5f).withHeight (jmin (r.getHeight() * 0.42f, 60.0f));
    ColourGradient spec (Colours::white.withAlpha (0.11f), top.getX(), top.getY(), Colours::white.withAlpha (0.0f), top.getX(), top.getBottom(), false);
    g.setGradientFill (spec);
    g.fillRoundedRectangle (top, radius - 1.5f);

    // diagonal sheen streak (polished resin)
    {
        Graphics::ScopedSaveState ss (g);
        Path clip;
        clip.addRoundedRectangle (r, radius);
        g.reduceClipRegion (clip);
        Path streak;
        const float x0 = r.getX() + r.getWidth() * 0.62f;
        streak.startNewSubPath (x0, r.getY());
        streak.lineTo (x0 + r.getHeight() * 0.35f + 26.0f, r.getY());
        streak.lineTo (x0 + 26.0f - r.getHeight() * 0.25f, r.getBottom());
        streak.lineTo (x0 - r.getHeight() * 0.6f, r.getBottom());
        streak.closeSubPath();
        g.setColour (Colours::white.withAlpha (0.025f));
        g.fillPath (streak);
    }

    // neon edge
    g.setColour (accent.withAlpha (0.10f * glow));
    g.drawRoundedRectangle (r.expanded (2.5f), radius + 2.5f, 5.0f);
    g.setColour (accent.withAlpha (0.22f * glow));
    g.drawRoundedRectangle (r.expanded (1.0f), radius + 1.0f, 2.0f);
    g.setColour (accent.withAlpha (0.55f));
    g.drawRoundedRectangle (r, radius, 1.1f);
    g.setColour (Colours::white.withAlpha (0.20f));
    g.drawLine (r.getX() + radius, r.getY() + 1.4f, r.getRight() - radius, r.getY() + 1.4f, 1.0f);
}

void drawNeonText (Graphics& g, const String& t, Rectangle<float> r, Colour c, float size, Justification j, bool bold)
{
    g.setFont (uiFont (size, bold));
    g.setColour (c.withAlpha (0.25f));
    for (auto d : { Point<float> (-0.8f, 0.0f), Point<float> (0.8f, 0.0f), Point<float> (0.0f, -0.8f), Point<float> (0.0f, 0.8f) })
        g.drawText (t, r.translated (d.x, d.y), j, false);
    g.setColour (c);
    g.drawText (t, r, j, false);
}

// ---- circuit board ---------------------------------------------------------------------------------
Image makeCircuitImage (int w, int h, float scale)
{
    const int W = jmax (1, roundToInt (w * scale)), H = jmax (1, roundToInt (h * scale));
    Image img (Image::ARGB, W, H, true);
    Graphics g (img);
    g.addTransform (AffineTransform::scale (scale));
    Random rng (0x7E4A0u);

    ColourGradient base (Colour (0xff0d0b24), w * 0.5f, h * 0.42f, Colour (0xff030208), 0.0f, 0.0f, true);
    g.setGradientFill (base);
    g.fillAll();

    const float grid = 18.0f;
    // faint grid dots
    g.setColour (Colour (0xff1b2146).withAlpha (0.55f));
    for (float y = grid; y < h; y += grid)
        for (float x = grid; x < w; x += grid)
            g.fillRect (x - 0.6f, y - 0.6f, 1.2f, 1.2f);

    // chips
    for (int i = 0; i < 7; ++i)
    {
        float cw = grid * (float) rng.nextInt ({ 4, 9 }), ch = grid * (float) rng.nextInt ({ 3, 6 });
        float cx = grid * (float) rng.nextInt (jmax (1, (int) ((w - cw) / grid))), cy = grid * (float) rng.nextInt (jmax (1, (int) ((h - ch) / grid)));
        Rectangle<float> c (cx, cy, cw, ch);
        g.setColour (Colour (0xff090c1c));
        g.fillRoundedRectangle (c, 3.0f);
        g.setColour (Colour (0xff22305a).withAlpha (0.8f));
        g.drawRoundedRectangle (c, 3.0f, 1.0f);
        g.setColour (Colour (0xff2a3a66).withAlpha (0.7f));
        for (float x = c.getX() + 6; x < c.getRight() - 4; x += 6)
        {
            g.fillRect (x, c.getY() - 4, 2.0f, 4.0f);
            g.fillRect (x, c.getBottom(), 2.0f, 4.0f);
        }
        g.setColour (Colour (0xff3a4a7a).withAlpha (0.5f));
        g.fillEllipse (c.getX() + 5, c.getY() + 5, 4, 4);
    }

    // traces: manhattan / 45-degree walks ending in vias
    const Colour neon[] = { Colour (0xff29f3ff), Colour (0xffff2bd6), Colour (0xff9b5cff), Colour (0xffb6ff3b) };
    const int traces = (int) (w * h / 5200.0f);
    for (int i = 0; i < traces; ++i)
    {
        Path p;
        float x = grid * (float) rng.nextInt (jmax (1, (int) (w / grid))), y = grid * (float) rng.nextInt (jmax (1, (int) (h / grid)));
        p.startNewSubPath (x, y);
        int dir = rng.nextInt (4) * 2; // 0..7, even = straight
        const int segs = rng.nextInt ({ 2, 6 });
        for (int s = 0; s < segs; ++s)
        {
            const int len = rng.nextInt ({ 2, 9 });
            static const float dx[] = { 1, 1, 0, -1, -1, -1, 0, 1 };
            static const float dy[] = { 0, 1, 1, 1, 0, -1, -1, -1 };
            x += dx[dir] * len * grid;
            y += dy[dir] * len * grid;
            p.lineTo (x, y);
            dir = (dir + (rng.nextBool() ? 1 : 7)) % 8;
            if (rng.nextFloat() < 0.6f && dir % 2 == 1) dir = (dir + 1) % 8;
        }
        const bool lit = rng.nextFloat() < 0.16f;
        const Colour nc = neon[rng.nextInt (4)];
        if (lit)
        {
            g.setColour (nc.withAlpha (0.07f));
            g.strokePath (p, PathStrokeType (7.0f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (nc.withAlpha (0.16f));
            g.strokePath (p, PathStrokeType (3.2f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (nc.withAlpha (0.55f));
            g.strokePath (p, PathStrokeType (1.2f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        else
        {
            g.setColour (Colour (0xff10384a).withAlpha (0.65f));
            g.strokePath (p, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        // vias
        auto via = [&] (Point<float> c) {
            g.setColour (lit ? nc.withAlpha (0.65f) : Colour (0xff1d5266).withAlpha (0.8f));
            g.drawEllipse (c.x - 3.2f, c.y - 3.2f, 6.4f, 6.4f, 1.3f);
            g.setColour (Colour (0xff04030a));
            g.fillEllipse (c.x - 1.4f, c.y - 1.4f, 2.8f, 2.8f);
        };
        via (p.getPointAlongPath (0.0f));
        via (Point<float> (x, y));
    }

    // CRT scanlines + vignette
    g.setColour (Colours::black.withAlpha (0.10f));
    for (float y = 0; y < h; y += 3.0f) g.fillRect (0.0f, y, (float) w, 1.0f);
    ColourGradient vig (Colours::transparentBlack, w * 0.5f, h * 0.5f, Colours::black.withAlpha (0.55f), 0.0f, 0.0f, true);
    g.setGradientFill (vig);
    g.fillAll();
    return img;
}

// ---- pixel logo ----------------------------------------------------------------------------------------
static const char* glyph (char c)
{
    switch (c)
    {
        case 'T': return "######" "######" "..##.." "..##.." "..##.." "..##.." "..##..";
        case 'E': return "######" "##...." "##...." "#####." "##...." "##...." "######";
        case 'A': return ".####." "##..##" "##..##" "######" "##..##" "##..##" "##..##";
        case 'R': return "#####." "##..##" "##..##" "#####." "##.##." "##..##" "##..##";
        case 'O': return ".####." "##..##" "##..##" "##..##" "##..##" "##..##" ".####.";
        case 'U': return "##..##" "##..##" "##..##" "##..##" "##..##" "##..##" ".####.";
        case 'N': return "##..##" "###.##" "######" "##.###" "##..##" "##..##" "##..##";
        case 'G': return ".####." "##..##" "##...." "##.###" "##..##" "##..##" ".####.";
        case 'I': return "######" "..##.." "..##.." "..##.." "..##.." "..##.." "######";
        default:  return "......" "......" "......" "......" "......" "......" "......";
    }
}

static void pixelWord (Graphics& g, const char* word, float x0, float y0, float p, float gap,
                       const std::function<Colour (int row)>& fill, Colour extrude, int depth, Colour outline,
                       Colour glow, float glowAlpha)
{
    std::vector<Rectangle<float>> px;
    std::vector<int> rows;
    std::vector<bool> topEdge;
    float x = x0;
    for (const char* c = word; *c; ++c)
    {
        const char* gl = glyph (*c);
        for (int r = 0; r < 7; ++r)
            for (int k = 0; k < 6; ++k)
                if (gl[r * 6 + k] == '#')
                {
                    px.push_back ({ x + k * p, y0 + r * p, p, p });
                    rows.push_back (r);
                    topEdge.push_back (r == 0 || gl[(r - 1) * 6 + k] != '#');
                }
        x += 6 * p + gap;
    }
    Path all;
    for (auto& r : px) all.addRectangle (r);

    // glow behind everything
    if (glowAlpha > 0)
    {
        DropShadow (glow.withAlpha (glowAlpha), roundToInt (p * 3.2f), {}).drawForPath (g, all);
        DropShadow (glow.withAlpha (glowAlpha * 0.6f), roundToInt (p * 1.2f), {}).drawForPath (g, all);
    }
    // 3D extrusion
    for (int d = depth; d >= 1; --d)
    {
        const float off = d * p * 0.32f;
        g.setColour (extrude.darker (0.25f * (float) d / (float) depth));
        for (auto& r : px) g.fillRect (r.translated (off, off).expanded (p * 0.06f));
    }
    // outline
    g.setColour (outline);
    for (auto& r : px) g.fillRect (r.expanded (p * 0.16f));
    // face
    for (size_t i = 0; i < px.size(); ++i)
    {
        g.setColour (fill (rows[i]));
        g.fillRect (px[i].expanded (0.35f));
    }
    // bevel highlight on top edges + CRT shade at the bottom of each pixel
    for (size_t i = 0; i < px.size(); ++i)
    {
        if (topEdge[i])
        {
            g.setColour (Colours::white.withAlpha (0.75f));
            g.fillRect (px[i].withHeight (jmax (1.0f, p * 0.22f)));
        }
        g.setColour (Colours::black.withAlpha (0.13f));
        g.fillRect (px[i].withTrimmedTop (p * 0.7f));
    }
}

void drawPixelLogo (Graphics& g, Rectangle<float> r, float glowPhase)
{
    // TEAROUT: 7 glyphs of 6 px + gaps; ENGINE underneath, smaller, with neon rails
    const float units = 7 * 6 + 6 * 1.0f + 2.0f;
    const float p = jmin (r.getWidth() / units, r.getHeight() * 0.64f / 7.0f);
    const float wordW = 7 * 6 * p + 6 * p;
    const float x0 = r.getX() + 2.0f;
    const float y0 = r.getY() + 2.0f;
    const float pulse = 0.5f + 0.5f * std::sin (glowPhase);

    static const Colour rowCols[] = { Colour (0xfffffbd1), Colour (0xffffe24a), Colour (0xffffb020), Colour (0xffff7a2e),
                                      Colour (0xffff3d7f), Colour (0xffff2bd6), Colour (0xffc23bff) };
    pixelWord (g, "TEAROUT", x0, y0, p, p, [] (int row) { return rowCols[jlimit (0, 6, row)]; },
               Colour (0xff4a137f), 3, Colour (0xff0a0418), col::cyan, 0.30f + 0.20f * pulse);

    // ENGINE subtitle
    const float q = p * 0.42f;
    const float subW = 6 * 6 * q + 5 * q * 2.0f;
    const float sy = y0 + 7 * p + p * 1.25f;
    const float sx = x0 + (wordW - subW) * 0.5f;
    pixelWord (g, "ENGINE", sx, sy, q, q * 2.0f,
               [] (int row) { return row < 2 ? Colour (0xffe9feff) : (row < 5 ? col::cyan : Colour (0xff4fa8ff)); },
               Colour (0xff0d2a55), 1, Colour (0xff04081a), col::cyan, 0.35f + 0.25f * pulse);

    // rails either side of ENGINE
    const float ry = sy + 3.5f * q;
    auto rail = [&] (float a, float b) {
        if (b - a < 6) return;
        g.setColour (col::magenta.withAlpha (0.25f));
        g.fillRect (a, ry - 2.0f, b - a, 4.0f);
        g.setColour (col::magenta);
        g.fillRect (a, ry - 0.75f, b - a, 1.5f);
        g.setColour (Colours::white);
        g.fillRect (b - 3.0f, ry - 1.5f, 3.0f, 3.0f);
        g.fillRect (a, ry - 1.5f, 3.0f, 3.0f);
    };
    rail (x0 + p * 0.5f, sx - q * 2.5f);
    rail (sx + subW + q * 1.5f, x0 + wordW - p * 0.5f);
}

// ---- look and feel ---------------------------------------------------------------------------------------
NeonLookAndFeel::NeonLookAndFeel()
{
    setColour (ComboBox::textColourId, col::text);
    setColour (ComboBox::arrowColourId, col::cyan);
    setColour (PopupMenu::backgroundColourId, Colour (0xf0090b1a));
    setColour (PopupMenu::textColourId, col::text);
    setColour (PopupMenu::highlightedBackgroundColourId, col::cyan.withAlpha (0.18f));
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (Label::textColourId, col::text);
    setColour (TextButton::textColourOffId, col::text);
    setColour (TextButton::textColourOnId, Colours::white);
    setColour (TooltipWindow::textColourId, col::text);
    setColour (Slider::textBoxTextColourId, col::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
}

void NeonLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const auto accent = s.findColour (Slider::rotarySliderFillColourId);
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float size = jmin (b.getWidth(), b.getHeight());
    b = b.withSizeKeepingCentre (size, size);
    const auto c = b.getCentre();
    const float rOuter = size * 0.5f;
    const float angle = start + pos * (end - start);

    // track
    Path track;
    track.addCentredArc (c.x, c.y, rOuter - 4, rOuter - 4, 0, start, end, true);
    g.setColour (Colour (0xff1a1f3d));
    g.strokePath (track, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));
    // value arc with glow (bipolar knobs fill from the centre)
    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float from = bipolar ? (start + end) * 0.5f : start;
    Path arc;
    arc.addCentredArc (c.x, c.y, rOuter - 4, rOuter - 4, 0, jmin (from, angle), jmax (from, angle), true);
    g.setColour (accent.withAlpha (0.18f));
    g.strokePath (arc, PathStrokeType (11.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (accent.withAlpha (0.35f));
    g.strokePath (arc, PathStrokeType (7.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (accent);
    g.strokePath (arc, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));

    // glossy resin cap
    const float rc = rOuter - 11;
    ColourGradient cap (Colour (0xff2a2f5c), c.x, c.y - rc, Colour (0xff0a0c1c), c.x, c.y + rc, false);
    g.setGradientFill (cap);
    g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
    g.setColour (Colours::white.withAlpha (0.16f));
    g.fillEllipse (c.x - rc * 0.7f, c.y - rc * 0.92f, rc * 1.4f, rc * 0.8f);
    g.setColour (accent.withAlpha (0.6f));
    g.drawEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2, 1.0f);
    // pointer
    Path ptr;
    ptr.addRoundedRectangle (-1.6f, -rc + 2.0f, 3.2f, rc * 0.55f, 1.6f);
    g.setColour (Colours::white);
    g.fillPath (ptr, AffineTransform::rotation (angle).translated (c.x, c.y));
}

void NeonLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool down, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
    const auto accent = box.findColour (ComboBox::outlineColourId).isTransparent() ? col::cyan : box.findColour (ComboBox::outlineColourId);
    ColourGradient body (Colour (0xff1d2148).withAlpha (0.85f), 0, 0, Colour (0xff0b0d20).withAlpha (0.92f), 0, (float) h, false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.45f).reduced (1.0f), 7.0f);
    g.setColour (accent.withAlpha (box.isMouseOver (true) || down ? 0.9f : 0.5f));
    g.drawRoundedRectangle (r, 8.0f, 1.1f);
    // chevron
    Path chev;
    const float cx = (float) w - 15.0f, cy = (float) h * 0.5f;
    chev.startNewSubPath (cx - 4.5f, cy - 2.0f);
    chev.lineTo (cx, cy + 2.5f);
    chev.lineTo (cx + 4.5f, cy - 2.0f);
    g.setColour (accent);
    g.strokePath (chev, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
}

void NeonLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

PopupMenu::Options NeonLookAndFeel::getOptionsForComboBoxPopupMenu (ComboBox& box, Label& label)
{
    return LookAndFeel_V4::getOptionsForComboBoxPopupMenu (box, label).withStandardItemHeight (26);
}

void NeonLookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (Colour (0xff080a18));
    g.setColour (col::cyan.withAlpha (0.45f));
    g.drawRect (0, 0, w, h, 1);
}

void NeonLookAndFeel::drawPopupMenuItem (Graphics& g, const Rectangle<int>& area, bool isSeparator, bool isActive,
                                         bool isHighlighted, bool isTicked, bool, const String& text, const String&,
                                         const Drawable*, const Colour*)
{
    if (isSeparator)
    {
        g.setColour (col::dim.withAlpha (0.3f));
        g.fillRect (area.reduced (8, area.getHeight() / 2).withHeight (1));
        return;
    }
    auto r = area.toFloat().reduced (3.0f, 1.0f);
    if (isHighlighted && isActive)
    {
        g.setColour (col::cyan.withAlpha (0.16f));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (col::cyan.withAlpha (0.6f));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
    }
    if (isTicked)
    {
        g.setColour (col::magenta);
        g.fillEllipse (r.getX() + 6, r.getCentreY() - 3, 6, 6);
    }
    g.setColour (isActive ? (isTicked ? Colours::white : col::text) : col::dim);
    g.setFont (uiFont (14.5f, isTicked));
    g.drawText (text, r.withTrimmedLeft (20.0f), Justification::centredLeft, true);
}

void NeonLookAndFeel::drawPopupMenuSectionHeader (Graphics& g, const Rectangle<int>& area, const String& name)
{
    g.setColour (col::magenta);
    g.setFont (uiFont (12.0f, true));
    g.drawText (name.toUpperCase(), area.reduced (10, 0), Justification::bottomLeft, true);
}

void NeonLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const auto accent = b.findColour (TextButton::buttonColourId);
    const bool on = b.getToggleState();
    const float radius = jmin (r.getHeight() * 0.5f, 12.0f);
    const bool solid = b.getProperties()["solid"];
    if (solid || on)
    {
        ColourGradient fill (accent.brighter (0.25f), r.getX(), r.getY(), accent.darker (0.55f), r.getX(), r.getBottom(), false);
        g.setGradientFill (fill);
        g.setOpacity (down ? 0.75f : 1.0f);
        g.fillRoundedRectangle (r, radius);
        g.setOpacity (1.0f);
        g.setColour (Colours::white.withAlpha (0.22f));
        g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.45f).reduced (2.0f, 1.0f), radius - 1);
    }
    else
    {
        g.setColour (Colour (0xff0d1024).withAlpha (0.8f));
        g.fillRoundedRectangle (r, radius);
    }
    g.setColour (accent.withAlpha (over ? 0.35f : 0.18f));
    g.drawRoundedRectangle (r.expanded (1.5f), radius + 1.5f, 4.0f);
    g.setColour (accent.withAlpha (over || on || solid ? 1.0f : 0.7f));
    g.drawRoundedRectangle (r, radius, 1.3f);
}

void NeonLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    const bool solid = b.getProperties()["solid"];
    const float size = b.getProperties().contains ("fontSize") ? (float) b.getProperties()["fontSize"] : 13.0f;
    auto c = (solid || b.getToggleState()) ? Colours::white : b.findColour (TextButton::buttonColourId).brighter (0.4f);
    g.setFont (uiFont (size, true));
    g.setColour (c);
    g.drawText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), Justification::centred, false);
}

void NeonLookAndFeel::drawTooltip (Graphics& g, const String& text, int w, int h)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h);
    g.fillAll (Colour (0xf4070916));
    g.setColour (col::cyan.withAlpha (0.7f));
    g.drawRect (r, 1.0f);
    g.setColour (col::text);
    g.setFont (uiFont (13.5f));
    g.drawFittedText (text, r.reduced (8, 5).toNearestInt(), Justification::centredLeft, 6);
}

Rectangle<int> NeonLookAndFeel::getTooltipBounds (const String& tipText, Point<int> pos, Rectangle<int> parentArea)
{
    const int w = jmin (360, 24 + (int) GlyphArrangement::getStringWidth (uiFont (13.5f), tipText));
    const int lines = jmax (1, (int) std::ceil (GlyphArrangement::getStringWidth (uiFont (13.5f), tipText) / 330.0f));
    const int h = 12 + 18 * lines;
    return Rectangle<int> (pos.x > parentArea.getCentreX() ? pos.x - (w + 12) : pos.x + 18,
                           pos.y > parentArea.getCentreY() ? pos.y - (h + 6) : pos.y + 6, w, h)
        .constrainedWithin (parentArea);
}

void NeonLookAndFeel::drawLabel (Graphics& g, Label& l)
{
    g.setColour (l.findColour (Label::textColourId));
    g.setFont (l.getFont());
    g.drawFittedText (l.getText(), l.getLocalBounds(), l.getJustificationType(), 2, 0.9f);
}
} // namespace tep
