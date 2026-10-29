#include "PluginEditor.h"

using namespace ui;
using namespace ppm;

static constexpr float kPiF        = juce::MathConstants<float>::pi;
static constexpr float kStartAngle = kPiF * 1.25f;   // 7:30 position
static constexpr float kEndAngle   = kPiF * 2.75f;   // 4:30 position

static juce::FontOptions sans (float h, bool bold = true)
{
    return juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain);
}
static juce::FontOptions serif (float h, bool bold = true)
{
    return juce::FontOptions (juce::Font::getDefaultSerifFontName(), h, bold ? juce::Font::bold : juce::Font::plain);
}

// =============================================================================
//  Drawing helpers
// =============================================================================
juce::Image draw::makeBrushedMetal (int w, int h)
{
    juce::Image img (juce::Image::RGB, juce::jmax (1, w), juce::jmax (1, h), true);
    juce::Graphics g (img);
    g.fillAll (col::metal);
    juce::Random r (1977);
    for (int y = 0; y < h; ++y)
    {
        // a few long horizontal streaks per row = brushed finish
        for (int k = 0; k < 5; ++k)
        {
            const float x0 = r.nextFloat() * (float) w;
            const float len = (0.15f + r.nextFloat() * 0.6f) * (float) w;
            const bool light = r.nextBool();
            g.setColour ((light ? juce::Colours::white : juce::Colours::black).withAlpha (light ? 0.025f + r.nextFloat() * 0.03f
                                                                                                 : 0.10f + r.nextFloat() * 0.12f));
            g.drawHorizontalLine (y, x0, juce::jmin ((float) w, x0 + len));
        }
    }
    // soft top light
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.06f), 0, 0,
                                juce::Colours::transparentBlack, 0, (float) h * 0.6f, false);
    g.setGradientFill (sheen);
    g.fillAll();
    return img;
}

void draw::screw (juce::Graphics& g, juce::Point<float> c, float d, float slotAngle)
{
    auto r = juce::Rectangle<float> (d, d).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillEllipse (r.expanded (1.5f).translated (0.6f, 1.0f));
    juce::ColourGradient grad (juce::Colour (0xffeaeaea), r.getX(), r.getY(), juce::Colour (0xff4b4b4b), r.getRight(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (r);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawEllipse (r, 0.8f);
    // Phillips slot
    g.setColour (juce::Colour (0xff1a1a1a));
    for (float a : { slotAngle, slotAngle + kPiF * 0.5f })
    {
        const auto dx = std::cos (a) * d * 0.32f, dy = std::sin (a) * d * 0.32f;
        g.drawLine (c.x - dx, c.y - dy, c.x + dx, c.y + dy, juce::jmax (1.2f, d * 0.12f));
    }
}

void draw::chromeRing (juce::Graphics& g, juce::Rectangle<float> r, float t)
{
    juce::Path ring;
    ring.addRoundedRectangle (r, juce::jmin (r.getHeight(), r.getWidth()) * 0.12f);
    juce::ColourGradient grad (juce::Colour (0xfff2f2f2), r.getX(), r.getY(), juce::Colour (0xff3d3d3d), r.getRight(), r.getBottom(), false);
    grad.addColour (0.45, juce::Colour (0xff8c8c8c));
    grad.addColour (0.55, juce::Colour (0xffd0d0d0));
    g.setGradientFill (grad);
    g.strokePath (ring, juce::PathStrokeType (t));
}

void draw::plate (juce::Graphics& g, juce::Rectangle<float> r, const juce::Image& texture, bool screws, juce::Colour tint)
{
    const float corner = 6.0f;
    // drop shadow
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0, 3.0f).expanded (1.0f), corner);

    // brushed face, slightly lighter than the case
    {
        juce::Graphics::ScopedSaveState s (g);
        juce::Path clip; clip.addRoundedRectangle (r, corner);
        g.reduceClipRegion (clip);
        g.drawImage (texture, r, juce::RectanglePlacement::fillDestination);
        g.setColour (juce::Colours::white.withAlpha (0.035f));
        g.fillRect (r);
        if (! tint.isTransparent())
        {   // anodised colour over the brushed metal, then the grain again on top
            juce::ColourGradient anod (tint.brighter (0.12f), r.getX(), r.getY(), tint.darker (0.45f), r.getX(), r.getBottom(), false);
            g.setGradientFill (anod);
            g.fillRect (r);
            g.setOpacity (0.22f);
            g.drawImage (texture, r, juce::RectanglePlacement::fillDestination);
            g.setOpacity (1.0f);
            juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.10f), r.getX(), r.getY(), juce::Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.4f, false);
            g.setGradientFill (sheen);
            g.fillRect (r);
        }
    }
    // bevel
    juce::ColourGradient bevel (juce::Colours::white.withAlpha (0.35f), r.getX(), r.getY(),
                                juce::Colours::black.withAlpha (0.8f), r.getX(), r.getBottom(), false);
    g.setGradientFill (bevel);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.6f);
    g.setColour (juce::Colours::black.withAlpha (0.9f));
    g.drawRoundedRectangle (r.expanded (1.0f), corner + 1, 1.0f);

    if (screws)
    {
        const float in = 11.0f;
        int i = 0;
        for (auto p : { r.getTopLeft() + juce::Point<float> (in, in), r.getTopRight() + juce::Point<float> (-in, in),
                        r.getBottomLeft() + juce::Point<float> (in, -in), r.getBottomRight() + juce::Point<float> (-in, -in) })
            screw (g, p, 11.0f, 0.3f + 0.7f * (float) (i++));
    }
}

void draw::jewel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float b)
{
    b = juce::jlimit (0.0f, 1.0f, b);
    const auto centre = r.getCentre();
    // glow on the panel
    if (b > 0.05f)
    {
        juce::ColourGradient glow (c.withAlpha (0.45f * b), centre.x, centre.y, c.withAlpha (0.0f), centre.x + r.getWidth() * 1.1f, centre.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (r.expanded (r.getWidth() * 0.9f));
    }
    // chrome bezel
    auto bezel = r.expanded (r.getWidth() * 0.18f);
    juce::ColourGradient bz (juce::Colour (0xffe0e0e0), bezel.getX(), bezel.getY(), juce::Colour (0xff353535), bezel.getRight(), bezel.getBottom(), false);
    g.setGradientFill (bz);
    g.fillEllipse (bezel);
    g.setColour (juce::Colours::black);
    g.fillEllipse (r.expanded (1.0f));
    // dome
    const auto lit  = c.interpolatedWith (juce::Colours::white, 0.35f * b);
    const auto dark = c.darker (2.6f).interpolatedWith (juce::Colours::black, 0.3f);
    juce::ColourGradient dome (b > 0.05f ? lit : c.darker (1.6f), centre.x - r.getWidth() * 0.15f, centre.y - r.getHeight() * 0.2f,
                               b > 0.05f ? c.darker (0.6f + 0.6f * (1.0f - b)) : dark, centre.x + r.getWidth() * 0.5f, centre.y + r.getHeight() * 0.5f, true);
    g.setGradientFill (dome);
    g.fillEllipse (r);
    // specular highlight
    g.setColour (juce::Colours::white.withAlpha (0.25f + 0.35f * b));
    g.fillEllipse (r.getX() + r.getWidth() * 0.24f, r.getY() + r.getHeight() * 0.14f, r.getWidth() * 0.32f, r.getHeight() * 0.22f);
}

void draw::engraved (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, float h,
                     juce::Justification j, juce::Colour c, bool bold)
{
    g.setFont (sans (h, bold));
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.drawText (text, r.translated (0, 1.0f), j, false);
    g.setColour (c);
    g.drawText (text, r, j, false);
}

// =============================================================================
//  LookAndFeel
// =============================================================================
MasterdeskLookAndFeel::MasterdeskLookAndFeel()
{
    setColour (juce::TextButton::buttonColourId,  col::plate);
    setColour (juce::TextButton::textColourOffId, col::engrave);
    setColour (juce::TooltipWindow::textColourId, col::ink);
    setColour (juce::ComboBox::textColourId,       col::ink);
    setColour (juce::ComboBox::arrowColourId,      col::ink);
    setColour (juce::PopupMenu::backgroundColourId,            col::cream);
    setColour (juce::PopupMenu::textColourId,                  col::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff2a2a2a));
    setColour (juce::PopupMenu::highlightedTextColourId,       col::cream);
}

void MasterdeskLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> ((float) w, (float) h).reduced (1.5f);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (r, 4.0f);
    auto window = r.reduced (3.0f);
    juce::ColourGradient paper (col::cream, window.getX(), window.getY(), col::creamDark, window.getX(), window.getBottom(), false);
    g.setGradientFill (paper);
    g.fillRoundedRectangle (window, 2.5f);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.fillRect (window.withHeight (window.getHeight() * 0.4f).reduced (2.0f, 1.0f));
    draw::chromeRing (g, r, 1.6f);
    // arrow
    const float ax = window.getRight() - 14.0f, ay = window.getCentreY();
    juce::Path tri; tri.addTriangle (ax - 5.0f, ay - 2.5f, ax + 5.0f, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 0.85f : 0.4f));
    g.fillPath (tri);
}

juce::Font MasterdeskLookAndFeel::getComboBoxFont (juce::ComboBox&) { return juce::Font (sans (13.0f)); }

void MasterdeskLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 2, box.getWidth() - 30, box.getHeight() - 4);
    label.setFont (getComboBoxFont (box));
}

void MasterdeskLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    juce::ColourGradient paper (col::cream, 0, 0, col::creamDark, 0, (float) h, false);
    g.setGradientFill (paper);
    g.fillAll();
    g.setColour (juce::Colour (0xff5a4a2a));
    g.drawRect (0, 0, w, h, 1);
}

void MasterdeskLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                              float startAngle, float endAngle, juce::Slider& s)
{
    // Glossy black fluted knob (Bakelite style), white scale ticks printed on the
    // panel around it, and a cream pointer line from the centre to the rim.
    const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    if (s.getProperties()["skirt"])
    {   // parametric EQ knob: glossy black cap, silver knurled skirt, white pointer line
        const float R = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 1.0f;
        const auto c = area.getCentre();
        const float a = startAngle + pos * (endAngle - startAngle);
        auto circ = [&] (float rad) { return juce::Rectangle<float> (rad * 2, rad * 2).withCentre (c); };
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (circ (R).translated (1.5f, 3.0f));
        // skirt
        juce::ColourGradient sk (juce::Colour (0xfff2f2f2), c.x - R, c.y - R, juce::Colour (0xff5c5c5c), c.x + R * 0.6f, c.y + R, false);
        sk.addColour (0.45, juce::Colour (0xffb8b8b8));
        g.setGradientFill (sk); g.fillEllipse (circ (R));
        // knurling around the skirt edge
        const int teeth = 56;
        for (int i = 0; i < teeth; ++i)
        {
            const float t = juce::MathConstants<float>::twoPi * (float) i / (float) teeth + a;
            const juce::Point<float> p0 (c.x + R * 0.80f * std::sin (t), c.y - R * 0.80f * std::cos (t));
            const juce::Point<float> p1 (c.x + R * 0.98f * std::sin (t), c.y - R * 0.98f * std::cos (t));
            const float shade = 0.5f + 0.5f * std::cos (t + 0.8f);
            g.setColour (juce::Colours::black.withAlpha (0.18f + 0.2f * (1.0f - shade)));
            g.drawLine ({ p0, p1 }, 1.1f);
        }
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawEllipse (circ (R), 1.0f);
        // glossy black cap
        const float rc = R * 0.72f;
        g.setColour (juce::Colours::black); g.fillEllipse (circ (rc + 1.0f));
        juce::ColourGradient cap (juce::Colour (0xff3a3a3c), c.x, c.y - rc, juce::Colour (0xff050505), c.x, c.y + rc, false);
        g.setGradientFill (cap); g.fillEllipse (circ (rc));
        juce::ColourGradient gloss (juce::Colours::white.withAlpha (0.35f), c.x - rc * 0.3f, c.y - rc * 0.7f,
                                    juce::Colours::transparentWhite, c.x, c.y, true);
        g.setGradientFill (gloss);
        g.fillEllipse (juce::Rectangle<float> (rc * 1.3f, rc * 0.8f).withCentre ({ c.x - rc * 0.15f, c.y - rc * 0.45f }));
        // white pointer line
        const juce::Point<float> pe (c.x + rc * 0.92f * std::sin (a), c.y - rc * 0.92f * std::cos (a));
        const juce::Point<float> ps (c.x + rc * 0.05f * std::sin (a), c.y - rc * 0.05f * std::cos (a));
        g.setColour (s.isEnabled() ? juce::Colour (0xfff4f4f4) : juce::Colour (0xff8a8a8a));
        g.drawLine ({ ps, pe }, juce::jmax (2.0f, R * 0.07f));
        return;
    }
    const float R   = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 1.0f;   // tick radius
    const auto  c   = area.getCentre();
    const bool  on  = s.isEnabled();
    const float angle = startAngle + pos * (endAngle - startAngle);
    auto circle = [&] (float rad) { return juce::Rectangle<float> (rad * 2, rad * 2).withCentre (c); };

    // --- scale ticks on the panel -------------------------------------------------
    const double interval = s.getInterval() > 0 ? s.getInterval() : 1.0;
    const int steps   = juce::jlimit (2, 64, (int) std::lround ((s.getMaximum() - s.getMinimum()) / interval) + 1);
    const int current = (int) std::lround ((s.getValue() - s.getMinimum()) / interval);
    const bool dense  = steps > 30;
    const float tickAlpha = on ? 0.9f : 0.35f;
    for (int i = 0; i < steps; ++i)
    {
        if (dense && i % 2 != 0 && i != current) continue;
        const float a = startAngle + (endAngle - startAngle) * (float) i / (float) (steps - 1);
        const bool major = (i == 0 || i == steps - 1 || (steps % 2 == 1 && i == steps / 2));
        const auto p0 = c.getPointOnCircumference (R * (major ? 0.84f : 0.88f), a);
        const auto p1 = c.getPointOnCircumference (R * 0.99f, a);
        g.setColour (juce::Colours::black.withAlpha (0.6f * tickAlpha));
        g.drawLine ({ p0.translated (0, 1.0f), p1.translated (0, 1.0f) }, major ? 2.0f : 1.4f);
        g.setColour (i == current && on ? col::amber : juce::Colour (0xffeeeeee).withAlpha (tickAlpha));
        g.drawLine ({ p0, p1 }, i == current ? 2.2f : (major ? 1.8f : 1.2f));
    }

    // --- knob body ----------------------------------------------------------------
    const float rb = R * 0.80f;                 // outer radius of the fluted skirt
    const int   nFlutes = 10;

    // soft drop shadow
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillEllipse (circle (rb + (float) i * 1.5f).translated (rb * 0.05f, rb * 0.10f));
    }

    // fluted outline: rounded scallops that turn with the knob
    juce::Path skirt;
    {
        const int n = 240;
        for (int i = 0; i <= n; ++i)
        {
            const float th = juce::MathConstants<float>::twoPi * (float) i / (float) n;
            const float bump = std::pow (std::abs (std::cos ((th - angle) * (float) nFlutes * 0.5f)), 0.45f);
            const float r = rb * (0.84f + 0.16f * bump);
            const juce::Point<float> p (c.x + r * std::sin (th), c.y - r * std::cos (th));
            if (i == 0) skirt.startNewSubPath (p); else skirt.lineTo (p);
        }
        skirt.closeSubPath();
    }
    juce::ColourGradient body (juce::Colour (0xff3a3a3c), c.x - rb * 0.7f, c.y - rb * 0.8f,
                               juce::Colour (0xff020202), c.x + rb * 0.6f, c.y + rb * 0.9f, false);
    body.addColour (0.5, juce::Colour (0xff111112));
    g.setGradientFill (body);
    g.fillPath (skirt);

    // flute ridges catch the light from the upper left
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (skirt);
        for (int k = 0; k < nFlutes; ++k)
        {
            const float th = angle + juce::MathConstants<float>::twoPi * (float) k / (float) nFlutes;
            const float lit = 0.5f + 0.5f * std::cos (th - (-0.8f));          // light from ~10:30
            const auto p0 = c.getPointOnCircumference (rb * 0.70f, th);
            const auto p1 = c.getPointOnCircumference (rb * 1.02f, th);
            g.setColour (juce::Colours::white.withAlpha (0.06f + 0.38f * lit * lit));
            g.drawLine ({ p0, p1 }, juce::jmax (1.4f, rb * 0.07f));
            const float thv = th + juce::MathConstants<float>::pi / (float) nFlutes;   // valley between flutes
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawLine ({ c.getPointOnCircumference (rb * 0.70f, thv), c.getPointOnCircumference (rb * 1.02f, thv) },
                        juce::jmax (1.0f, rb * 0.04f));
        }
    }
    g.setColour (juce::Colours::black.withAlpha (0.9f));
    g.strokePath (skirt, juce::PathStrokeType (1.0f));

    // glossy domed top
    const float rt = rb * 0.70f;
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (circle (rt + 0.8f), 1.6f);
    juce::ColourGradient dome (juce::Colour (0xff2e2e30), c.x - rt * 0.5f, c.y - rt * 0.6f,
                               juce::Colour (0xff050505), c.x + rt * 0.7f, c.y + rt * 0.8f, true);
    g.setGradientFill (dome);
    g.fillEllipse (circle (rt));
    // bright specular highlight, upper left
    {
        const auto hc = c.translated (-rt * 0.32f, -rt * 0.38f);
        juce::ColourGradient spec (juce::Colours::white.withAlpha (on ? 0.55f : 0.25f), hc.x, hc.y,
                                   juce::Colours::white.withAlpha (0.0f), hc.x + rt * 0.55f, hc.y, true);
        g.setGradientFill (spec);
        g.fillEllipse (juce::Rectangle<float> (rt * 1.1f, rt * 0.8f).withCentre (hc));
    }
    // reflected rim light, lower right
    {
        juce::Path rim;
        rim.addCentredArc (c.x, c.y, rt * 0.9f, rt * 0.9f, 0.0f, 1.9f, 3.4f, true);
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.strokePath (rim, juce::PathStrokeType (juce::jmax (1.2f, rt * 0.08f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // --- cream pointer line from the centre out to the rim -----------------------------
    const auto pStart = c.getPointOnCircumference (rt * 0.05f, angle);
    const auto pEnd   = c.getPointOnCircumference (rb * 0.97f, angle);
    const float lw = juce::jmax (2.0f, R * 0.06f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawLine ({ pStart.translated (1.0f, 1.5f), pEnd.translated (1.0f, 1.5f) }, lw + 1.0f);
    juce::ColourGradient cream (juce::Colour (on ? 0xfff6edd2 : 0xff8a857a), pStart.x, pStart.y,
                                juce::Colour (on ? 0xffd8c79d : 0xff6a665e), pEnd.x, pEnd.y, false);
    g.setGradientFill (cream);
    juce::Path ptr;
    ptr.startNewSubPath (pStart); ptr.lineTo (pEnd);
    g.strokePath (ptr, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (! on)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillPath (skirt);
    }
}

void MasterdeskLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hi, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (r.translated (0, 2.0f), 4.0f);
    juce::ColourGradient face (juce::Colour (down ? 0xff1a1a1a : (hi ? 0xff4a4a4a : 0xff383838)), r.getX(), r.getY(),
                               juce::Colour (down ? 0xff2e2e2e : 0xff151515), r.getX(), r.getBottom(), false);
    g.setGradientFill (face);
    g.fillRoundedRectangle (r, 4.0f);
    draw::chromeRing (g, r, 1.6f);
    if (! b.isEnabled()) { g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (r, 4.0f); }
}

void MasterdeskLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    draw::engraved (g, b.getButtonText(), b.getLocalBounds().toFloat(), 12.5f, juce::Justification::centred,
                    b.isEnabled() ? col::engrave : col::engraveDim);
}

juce::Rectangle<int> MasterdeskLookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    juce::AttributedString s; s.append (text, sans (13.0f, false), col::ink);
    juce::TextLayout tl; tl.createLayoutWithBalancedLineLengths (s, 300.0f);
    const int w = (int) tl.getWidth() + 24, h = (int) tl.getHeight() + 18;
    return juce::Rectangle<int> (pos.x + 14, pos.y + 18, w, h).constrainedWithin (parent);
}

void MasterdeskLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    auto r = juce::Rectangle<float> ((float) w, (float) h);
    juce::ColourGradient paper (col::cream, 0, 0, col::creamDark, 0, (float) h, false);
    g.setGradientFill (paper);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colour (0xff5a4a2a));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.2f);
    juce::AttributedString s; s.append (text, sans (13.0f, false), col::ink);
    juce::TextLayout tl; tl.createLayoutWithBalancedLineLengths (s, 300.0f);
    tl.draw (g, r.reduced (12.0f, 9.0f));
}

// =============================================================================
//  SteppedKnob
// =============================================================================
SteppedKnob::SteppedKnob (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& cap,
                          const juce::String& minL, const juce::String& maxL)
    : caption (cap), minLabel (minL), maxLabel (maxL)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (kStartAngle, kEndAngle, true);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setMouseDragSensitivity (160);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, paramId, slider);
    if (auto* p = s.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));

    valueBox.setJustificationType (juce::Justification::centred);
    valueBox.setFont (sans (12.5f));
    valueBox.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (valueBox);

    slider.onValueChange = [this] { refresh(); };
    refresh();
}

void SteppedKnob::refresh()
{
    valueBox.setText (valueText ? valueText() : slider.getTextFromValue (slider.getValue()), juce::dontSendNotification);
    valueBox.setColour (juce::Label::textColourId, isEnabled() ? valueColour : valueColour.withAlpha (0.5f));
    repaint();
}

void SteppedKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (15);                                     // caption
    auto box = r.removeFromBottom (19);
    valueBox.setBounds (box.withSizeKeepingCentre (juce::jmin (box.getWidth() - 8, 76), 19));
    r.removeFromBottom (11);                                  // end labels
    slider.setBounds (r);
}

void SteppedKnob::paint (juce::Graphics& g)
{
    const auto on = isEnabled();
    draw::engraved (g, caption, getLocalBounds().removeFromTop (15).toFloat(), 11.0f,
                    juce::Justification::centred, on ? captionColour : col::engraveDim);

    // end labels in the strip just under the knob, below the 7:30 / 4:30 ticks
    const auto sb = slider.getBounds().toFloat();
    const float R = juce::jmin (sb.getWidth(), sb.getHeight()) * 0.5f;
    const float cx = sb.getCentreX();
    const float y  = sb.getBottom() - 1.0f;
    const float off = juce::jmin (R * 0.72f, sb.getWidth() * 0.5f - 15.0f);
    const auto lc = on ? endLabelColour : col::engraveDim.darker (0.4f);
    draw::engraved (g, minLabel, { cx - off - 16.0f, y, 32.0f, 11.0f }, 9.0f, juce::Justification::centred, lc);
    draw::engraved (g, maxLabel, { cx + off - 16.0f, y, 32.0f, 11.0f }, 9.0f, juce::Justification::centred, lc);

    // cream value window
    auto box = valueBox.getBounds().toFloat();
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (box.expanded (2.0f), 3.0f);
    juce::ColourGradient paper (on ? col::cream : col::cream.darker (0.9f), box.getX(), box.getY(),
                                on ? col::creamDark : col::creamDark.darker (0.9f), box.getX(), box.getBottom(), false);
    g.setGradientFill (paper);
    g.fillRoundedRectangle (box, 2.5f);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.fillRect (box.removeFromTop (box.getHeight() * 0.4f).reduced (2.0f, 1.0f));
}

// =============================================================================
//  ModeSwitch
// =============================================================================
ModeSwitch::ModeSwitch (juce::AudioProcessorValueTreeState& s, const juce::String& paramId,
                        juce::StringArray n, juce::Array<juce::Colour> l)
    : names (std::move (n)), lamps (std::move (l)), attachment (*s.getParameter (paramId), [this] (float v) { value = (int) std::lround (v); repaint(); })
{
    attachment.sendInitialUpdate();
}

void ModeSwitch::paint (juce::Graphics& g)
{
    const int n = juce::jmax (1, names.size());
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    // recessed slot
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (r, 5.0f);
    juce::ColourGradient inner (juce::Colour (0xff050505), r.getX(), r.getY(), juce::Colour (0xff202020), r.getX(), r.getBottom(), false);
    g.setGradientFill (inner);
    g.fillRoundedRectangle (r.reduced (2.0f), 4.0f);
    draw::chromeRing (g, r, 1.4f);

    const float w = (r.getWidth() - 4.0f) / (float) n;
    for (int i = 0; i < n; ++i)
    {
        auto seg = juce::Rectangle<float> (r.getX() + 2.0f + w * (float) i, r.getY() + 2.0f, w, r.getHeight() - 4.0f).reduced (1.5f);
        const bool sel = (i == value);
        if (sel)
        {
            // chrome lever block
            juce::ColourGradient lever (juce::Colour (0xffe9e9e9), seg.getX(), seg.getY(), juce::Colour (0xff6a6a6a), seg.getX(), seg.getBottom(), false);
            g.setGradientFill (lever);
            g.fillRoundedRectangle (seg, 3.0f);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawRoundedRectangle (seg, 3.0f, 1.0f);
            const auto lampCol = isEnabled() ? lamps[i] : lamps[i].withAlpha (0.4f);
            g.setColour (lampCol);
            g.fillRoundedRectangle (seg.getX() + 4.0f, seg.getBottom() - 4.0f, seg.getWidth() - 8.0f, 2.5f, 1.0f);  // lit indicator strip
            g.setColour (juce::Colours::black.withAlpha (isEnabled() ? 0.9f : 0.5f));
            g.setFont (sans (10.0f));
            g.drawText (names[i], seg.withTrimmedBottom (2.0f), juce::Justification::centred, false);
        }
        else
        {
            draw::engraved (g, names[i], seg, 10.0f, juce::Justification::centred, isEnabled() ? col::engraveDim.brighter (0.3f) : col::engraveDim.darker (0.5f));
        }
    }
}

void ModeSwitch::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled()) return;
    const int n = juce::jmax (1, names.size());
    attachment.setValueAsCompleteGesture ((float) juce::jlimit (0, n - 1, (int) ((float) n * (float) e.x / (float) getWidth())));
}

// =============================================================================
//  JewelButton / JewelLamp
// =============================================================================
JewelButton::JewelButton (const juce::String& on, const juce::String& off, juce::Colour l)
    : onText (on), offText (off), lamp (l)
{
    setClickingTogglesState (true);
}

void JewelButton::paintButton (juce::Graphics& g, bool hi, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    const bool on = getToggleState();
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (r.translated (0, 2.0f), 4.0f);
    juce::ColourGradient face (juce::Colour (down ? 0xff1c1c1c : (hi ? 0xff464646 : 0xff353535)), r.getX(), r.getY(),
                               juce::Colour (0xff121212), r.getX(), r.getBottom(), false);
    g.setGradientFill (face);
    g.fillRoundedRectangle (r, 4.0f);
    draw::chromeRing (g, r, 1.6f);

    const float d = r.getHeight() * 0.46f;
    auto lampArea = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + r.getHeight() * 0.55f, r.getCentreY() });
    draw::jewel (g, lampArea, lamp, on ? (isEnabled() ? 1.0f : 0.4f) : 0.0f);

    draw::engraved (g, on ? onText : offText, r.withTrimmedLeft (r.getHeight() * 0.9f).withTrimmedRight (4.0f), 11.0f,
                    juce::Justification::centred, isEnabled() ? (on ? col::engrave : col::engraveDim.brighter (0.2f)) : col::engraveDim.darker (0.5f));
}

void JewelLamp::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto text = r.removeFromBottom (13.0f);
    const float d = juce::jmin (r.getWidth(), r.getHeight()) * 0.62f;
    draw::jewel (g, juce::Rectangle<float> (d, d).withCentre (r.getCentre()), colour, brightness);
    draw::engraved (g, label, text, 9.5f, juce::Justification::centred, col::engraveDim.brighter (0.5f));
}

// =============================================================================
//  AnalogMeter
// =============================================================================
AnalogMeter::AnalogMeter (Kind k, const juce::String& l, const juce::String& rLegend)
    : kind (k), leftLegend (l), rightLegend (rLegend)
{
    needle = target = (kind == Kind::VU ? -40.0f : (kind == Kind::Harmonics ? 0.001f : 0.0f));
}

void AnalogMeter::setTarget (float v, float lampB) { target = v; lamp = lampB; }

void AnalogMeter::tick (float dt)
{
    // VU ballistics: ~300 ms rise to 99 %. Harmonics: a little slower.
    const float tau = kind == Kind::Harmonics ? 0.15f : 0.065f;
    const float a = 1.0f - std::exp (-dt / tau);
    const float before = needle;
    needle += (target - needle) * a;
    if (std::abs (needle - before) > 1e-4f) repaint();
}

float AnalogMeter::valueToAngle (float v) const
{
    float frac;
    if (kind == Kind::VU)
    {
        // a real VU scale is linear in voltage: -20 VU near the left, +3 at the right
        const float lo = std::pow (10.0f, -20.0f / 20.0f), hi = std::pow (10.0f, 3.0f / 20.0f);
        frac = (std::pow (10.0f, juce::jlimit (-40.0f, 4.0f, v) / 20.0f) - lo) / (hi - lo);
    }
    else if (kind == Kind::GainReduction)
        frac = std::log10 (1.0f + juce::jlimit (0.0f, 22.0f, v)) / std::log10 (21.0f);   // 0 .. 20 dB
    else
        frac = std::log10 (juce::jlimit (0.005f, 15.0f, v) / 0.01f) / 3.0f;     // 0.01 % .. 10 %
    frac = juce::jlimit (-0.06f, 1.05f, frac);
    return juce::degreesToRadians (-48.0f + 96.0f * frac);
}

void AnalogMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (3.0f);
    const float rad = b.getHeight() * 0.42f;

    // case shadow + chrome bezel
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (b.translated (0, 4.0f), rad);
    juce::ColourGradient bez (juce::Colour (0xfff5f5f5), b.getX(), b.getY(), juce::Colour (0xff2c2c2c), b.getRight(), b.getBottom(), false);
    bez.addColour (0.3, juce::Colour (0xffb5b5b5));
    bez.addColour (0.5, juce::Colour (0xff6e6e6e));
    bez.addColour (0.7, juce::Colour (0xffdcdcdc));
    g.setGradientFill (bez);
    g.fillRoundedRectangle (b, rad);

    auto inner = b.reduced (b.getHeight() * 0.055f);
    const float innerRad = rad * 0.88f;
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (inner, innerRad);
    auto window = inner.reduced (3.0f);
    juce::Path windowPath; windowPath.addRoundedRectangle (window, innerRad * 0.95f);

    auto face = window.withHeight (window.getHeight() * 0.70f);
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (windowPath);

        // black face
        juce::ColourGradient paper (juce::Colour (0xff1e1e20), face.getCentreX(), face.getY() + face.getHeight() * 0.35f,
                                    juce::Colour (0xff050505), face.getX(), face.getBottom(), true);
        g.setGradientFill (paper);
        g.fillRect (face);

        const juce::Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.28f);
        const float rs = face.getHeight() * 0.98f;
        auto onArc = [&] (float angle, float radius) { return juce::Point<float> (pivot.x + radius * std::sin (angle), pivot.y - radius * std::cos (angle)); };

        // scale arcs
        auto arc = [&] (float a0, float a1, float radius, juce::Colour c, float t)
        {
            juce::Path p;
            for (int i = 0; i <= 40; ++i)
            {
                const auto pt = onArc (a0 + (a1 - a0) * (float) i / 40.0f, radius);
                if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            g.setColour (c); g.strokePath (p, juce::PathStrokeType (t));
        };

        struct Mark { float v; const char* text; bool major; };
        static const Mark vuMarks[] = { { -20, "20", true }, { -10, "10", true }, { -7, "7", true }, { -5, "5", true }, { -3, "3", true },
                                        { -2, "2", false }, { -1, "1", false }, { 0, "0", true }, { 1, "1", false }, { 2, "2", false }, { 3, "3", true } };
        static const Mark thdMarks[] = { { 0.01f, ".01", true }, { 0.03f, ".03", false }, { 0.1f, ".1", true }, { 0.3f, ".3", false },
                                         { 1.0f, "1", true }, { 3.0f, "3", false }, { 10.0f, "10", true } };
        static const Mark grMarks[] = { { 0, "0", true }, { 1, "1", false }, { 2, "2", true }, { 3, "3", false }, { 5, "5", true },
                                        { 7, "7", false }, { 10, "10", true }, { 15, "15", false }, { 20, "20", true } };
        const bool isVu = kind == Kind::VU, isGr = kind == Kind::GainReduction;
        const juce::Span<const Mark> marks = isVu ? juce::Span<const Mark> (vuMarks, std::size (vuMarks))
                                           : isGr ? juce::Span<const Mark> (grMarks, std::size (grMarks))
                                                  : juce::Span<const Mark> (thdMarks, std::size (thdMarks));
        const float redFrom = isVu ? 0.0f : (isGr ? 10.0f : 1.0f);
        const float aMin = valueToAngle (marks.front().v), aRed = valueToAngle (redFrom), aMax = valueToAngle (marks.back().v);
        arc (aMin, aRed, rs, col::meterInk, 1.6f);
        arc (aRed, aMax, rs, col::red, 1.6f);
        arc (aRed, aMax, rs + 4.0f, col::red.withAlpha (0.85f), 6.0f);

        for (auto& m : marks)
        {
            const float a = valueToAngle (m.v);
            const bool red = m.v >= redFrom && ! (kind == Kind::VU && std::abs (m.v) < 0.01f);
            g.setColour (red ? col::red : col::meterInk);
            g.drawLine ({ onArc (a, rs - (m.major ? 9.0f : 5.0f)), onArc (a, rs + 1.0f) }, m.major ? 1.6f : 1.0f);
            const auto lp = onArc (a, rs + 15.0f);
            g.setFont (serif (m.major ? 13.0f : 11.0f));
            g.drawText (m.text, juce::Rectangle<float> (30, 14).withCentre (lp), juce::Justification::centred);
        }
        if (kind == Kind::VU)
        {
            g.setColour (col::meterInk);
            g.setFont (serif (11.0f, false));
            g.drawText ("-", juce::Rectangle<float> (14, 12).withCentre (onArc (aMin - 0.12f, rs + 15.0f)), juce::Justification::centred);
            g.setColour (col::red);
            g.drawText ("+", juce::Rectangle<float> (14, 12).withCentre (onArc (aMax + 0.12f, rs + 15.0f)), juce::Justification::centred);
        }

        // centre legend
        g.setColour (col::meterInk);
        g.setFont (serif (kind == Kind::VU ? 22.0f : (isGr ? 13.5f : 15.0f)));
        g.drawText (kind == Kind::VU ? "VU" : (isGr ? "GAIN REDUCTION" : "HARMONICS"), juce::Rectangle<float> (face.getWidth(), 24).withCentre ({ face.getCentreX(), face.getY() + face.getHeight() * 0.66f }),
                    juce::Justification::centred);
        g.setFont (sans (8.5f, false));
        g.drawText (subtitle.isNotEmpty() ? subtitle : (kind == Kind::VU ? "0 VU = -18 dBFS" : (isGr ? "dB  -  VARIABLE MU" : "TUBE MAKE-UP STAGE")),
                    juce::Rectangle<float> (face.getWidth(), 12).withCentre ({ face.getCentreX(), face.getY() + face.getHeight() * 0.83f }),
                    juce::Justification::centred);

        // needle (shadow first)
        const float na = valueToAngle (needle);
        const auto tip = onArc (na, rs + 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));          // soft glow around the white needle
        g.drawLine ({ pivot, tip }, 4.0f);
        g.setColour (juce::Colour (0xfff7f7f7));
        g.drawLine ({ pivot, tip }, 1.6f);

        // black lower housing covers the pivot
        auto housing = window.withTrimmedTop (face.getHeight());
        juce::ColourGradient hz (juce::Colour (0xff232323), housing.getX(), housing.getY(), juce::Colour (0xff050505), housing.getX(), housing.getBottom(), false);
        g.setGradientFill (hz);
        juce::Path hump;
        hump.startNewSubPath (window.getX(), housing.getY() + 6.0f);
        hump.quadraticTo (face.getCentreX(), housing.getY() - face.getHeight() * 0.18f, window.getRight(), housing.getY() + 6.0f);
        hump.lineTo (window.getRight(), window.getBottom());
        hump.lineTo (window.getX(), window.getBottom());
        hump.closeSubPath();
        g.fillPath (hump);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.strokePath (hump, juce::PathStrokeType (1.0f));

        // corner legends (like "COMP" / "MAX" on the reference)
        g.setColour (col::meterInk);
        g.setFont (sans (11.0f));
        g.drawText (leftLegend,  juce::Rectangle<float> (face.getX() + face.getWidth() * 0.09f, housing.getY() - 22.0f, 70, 14), juce::Justification::centredLeft);
        g.drawText (rightLegend, juce::Rectangle<float> (face.getRight() - face.getWidth() * 0.09f - 70.0f, housing.getY() - 22.0f, 70, 14), juce::Justification::centredRight);

        // housing lamp
        const float d = housing.getHeight() * 0.42f;
        draw::jewel (g, juce::Rectangle<float> (d, d).withCentre ({ housing.getCentreX(), housing.getCentreY() + 2.0f }),
                     kind == Kind::Harmonics ? col::green : col::amber, lamp);
        for (float sx : { housing.getX() + housing.getWidth() * 0.22f, housing.getRight() - housing.getWidth() * 0.22f })
            draw::screw (g, { sx, housing.getCentreY() + 3.0f }, 7.0f, 0.6f);

        // glass reflection
        juce::ColourGradient glass (juce::Colours::white.withAlpha (0.09f), window.getX(), window.getY(),
                                    juce::Colours::white.withAlpha (0.0f), window.getX(), window.getY() + window.getHeight() * 0.45f, false);
        g.setGradientFill (glass);
        g.fillRect (window.withHeight (window.getHeight() * 0.45f));
    }
    g.setColour (juce::Colours::black);
    g.strokePath (windowPath, juce::PathStrokeType (2.0f));
}

// =============================================================================
//  TubeGlass
// =============================================================================
void TubeGlass::paintHalo (juce::Graphics& g, juce::Rectangle<float> r, float glow)
{
    if (glow <= 0.02f) return;
    r.removeFromBottom (r.getHeight() * 0.24f);                       // same geometry as paint()
    const float gw = r.getWidth() * 0.72f;
    auto glass = juce::Rectangle<float> (gw, r.getHeight() - 4.0f).withCentre ({ r.getCentreX(), r.getCentreY() + 2.0f });
    const auto c = glass.getCentre().translated (0, glass.getHeight() * 0.1f);
    // wide warm halo on the panel, then a hotter inner glow
    juce::ColourGradient halo (col::amber.withAlpha (0.85f * glow), c.x, c.y, col::amber.withAlpha (0.0f), c.x + gw * 1.9f, c.y, true);
    g.setGradientFill (halo);
    g.fillEllipse (juce::Rectangle<float> (gw * 3.8f, glass.getHeight() * 1.8f).withCentre (c));
    juce::ColourGradient hot (juce::Colour (0xffffc25a).withAlpha (0.75f * glow), c.x, c.y, juce::Colour (0xffff9a2e).withAlpha (0.0f), c.x + gw * 1.0f, c.y, true);
    g.setGradientFill (hot);
    g.fillEllipse (juce::Rectangle<float> (gw * 2.0f, glass.getHeight() * 1.2f).withCentre (c));
}

void TubeGlass::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto base = r.removeFromBottom (r.getHeight() * 0.24f);
    const float gw = r.getWidth() * 0.72f;
    auto glass = juce::Rectangle<float> (gw, r.getHeight() - 4.0f).withCentre ({ r.getCentreX(), r.getCentreY() + 2.0f });

    juce::Path env;
    env.addRoundedRectangle (glass.getX(), glass.getY(), glass.getWidth(), glass.getHeight() + 8.0f, gw * 0.5f, gw * 0.5f,
                             true, true, false, false);

    // glow behind the glass
    const auto c = glass.getCentre().translated (0, glass.getHeight() * 0.1f);

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (env);
        g.setColour (juce::Colour (0xff1a120a).withAlpha (0.85f));
        g.fillRect (glass.expanded (2.0f, 10.0f));
        // the whole bottle fills with warm light when it's hot
        juce::ColourGradient fill (juce::Colour (0xffff9a2e).withAlpha (0.55f * glow), c.x, c.y,
                                   juce::Colour (0xffff7a10).withAlpha (0.12f * glow), c.x, glass.getY(), true);
        g.setGradientFill (fill);
        g.fillRect (glass.expanded (2.0f, 10.0f));
        // plate structure
        auto plates = glass.reduced (gw * 0.22f, glass.getHeight() * 0.22f).translated (0, glass.getHeight() * 0.06f);
        g.setColour (juce::Colour (0xff3a3a3a));
        g.fillRect (plates.removeFromLeft (plates.getWidth() * 0.32f));
        g.fillRect (plates.removeFromRight (plates.getWidth() * 0.48f));
        // filament glow
        juce::ColourGradient fil (juce::Colour (0xffffe6b0).withAlpha (juce::jmin (1.0f, 0.3f + 0.9f * glow)), c.x, c.y,
                                  col::amber.withAlpha (0.0f), c.x + gw * 0.7f, c.y, true);
        g.setGradientFill (fil);
        g.fillEllipse (juce::Rectangle<float> (gw * 1.3f, glass.getHeight() * 0.95f).withCentre (c));
        // glowing filaments with a white-hot centre line
        for (float dx : { -gw * 0.06f, gw * 0.06f })
        {
            const float y0 = glass.getY() + glass.getHeight() * 0.28f, y1 = glass.getBottom() - glass.getHeight() * 0.08f;
            g.setColour (juce::Colour (0xffffa040).withAlpha (0.5f * glow));
            g.drawLine (c.x + dx, y0, c.x + dx, y1, 4.5f);
            g.setColour (juce::Colour (0xffffd890).withAlpha (0.45f + 0.55f * glow));
            g.drawLine (c.x + dx, y0, c.x + dx, y1, 1.8f);
            g.setColour (juce::Colours::white.withAlpha (0.7f * glow));
            g.drawLine (c.x + dx, y0 + 4.0f, c.x + dx, y1 - 4.0f, 0.7f);
        }
        // silver getter at the top
        juce::ColourGradient getter (juce::Colour (0xffa0a0a0).withAlpha (0.6f), glass.getX(), glass.getY(),
                                     juce::Colours::transparentBlack, glass.getX(), glass.getY() + glass.getHeight() * 0.25f, false);
        g.setGradientFill (getter);
        g.fillRect (glass.withHeight (glass.getHeight() * 0.25f));
        // glass highlights
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.fillRoundedRectangle (glass.getX() + gw * 0.14f, glass.getY() + gw * 0.25f, gw * 0.1f, glass.getHeight() * 0.7f, gw * 0.05f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.fillRoundedRectangle (glass.getRight() - gw * 0.24f, glass.getY() + gw * 0.4f, gw * 0.06f, glass.getHeight() * 0.5f, gw * 0.03f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.strokePath (env, juce::PathStrokeType (1.2f));

    // socket
    auto sock = base.withSizeKeepingCentre (r.getWidth() * 0.86f, base.getHeight());
    juce::ColourGradient sk (juce::Colour (0xff3c3c3c), sock.getX(), sock.getY(), juce::Colour (0xff050505), sock.getRight(), sock.getY(), false);
    sk.addColour (0.3, juce::Colour (0xff5a5a5a));
    g.setGradientFill (sk);
    g.fillRoundedRectangle (sock, 4.0f);
    draw::chromeRing (g, sock.withHeight (6.0f), 2.0f);
}

// =============================================================================
//  AudioHistory / SpectrumAnalyzer / Vectorscope / LoudnessPanel
// =============================================================================
void AudioHistory::pull (PassiveMasterEQProcessor::ScopeFifo& f)
{
    sampleRate = f.sampleRate.load();
    newSamples = 0;
    f.pull ([this] (const float* a, const float* b, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            l[(size_t) write] = a[i]; r[(size_t) write] = b[i];
            write = (write + 1) % kSize;
        }
        newSamples += n;
    });
}

// screen frame shared by the analyzer and the scope: black glass in a chrome bezel
static juce::Rectangle<float> drawScreen (juce::Graphics& g, juce::Rectangle<float> bounds, bool greenTint)
{
    bounds = bounds.reduced (2.0f);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (bounds, 8.0f);
    auto screen = bounds.reduced (5.0f);
    juce::ColourGradient glassBg (greenTint ? juce::Colour (0xff0b140d) : juce::Colour (0xff15130f), screen.getCentreX(), screen.getCentreY(),
                                  greenTint ? juce::Colour (0xff030604) : juce::Colour (0xff060504), screen.getX(), screen.getY(), true);
    g.setGradientFill (glassBg);
    g.fillRoundedRectangle (screen, 6.0f);
    draw::chromeRing (g, bounds, 2.5f);
    return screen;
}

static void drawGlassSheen (juce::Graphics& g, juce::Rectangle<float> screen)
{
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.06f), screen.getX(), screen.getY(),
                                juce::Colours::transparentWhite, screen.getX(), screen.getCentreY(), false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (screen.withHeight (screen.getHeight() * 0.5f), 6.0f);
}

SpectrumAnalyzer::SpectrumAnalyzer (const AudioHistory& h) : hist (h)
{
    window.resize (kN);
    work.resize (2 * kN);
    for (int i = 0; i < kN; ++i)                  // Hann window
        window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * kPiF * (float) i / (float) (kN - 1));
    level.fill (kFloorDb); peak.fill (kFloorDb); peakHold.fill (0.0f);
}

void SpectrumAnalyzer::update (float dt)
{
    // FFT of the newest 8192 samples (L+R)/2
    std::fill (work.begin(), work.end(), 0.0f);
    for (int i = 0; i < kN; ++i)
        work[(size_t) i] = 0.5f * (hist.atL (kN - 1 - i) + hist.atR (kN - 1 - i)) * window[(size_t) i];
    fft.performFrequencyOnlyForwardTransform (work.data(), true);

    // sine of amplitude A -> bin magnitude A * N/4 with a Hann window
    const float norm = 4.0f / (float) kN;
    const double binHz = hist.sampleRate / kN;
    const float riseCoef = 1.0f - std::exp (-dt / 0.012f);   // fast attack
    const float fallDbPerSec = 26.0f, peakFallDbPerSec = 14.0f;

    for (int b = 0; b < kBands; ++b)
    {
        const double fc = bandCentreHz (b), half = std::sqrt ((double) bandStep());
        const double lo = fc / half, hi = fc * half;
        int b0 = (int) std::ceil (lo / binHz), b1 = (int) std::floor (hi / binHz);
        b0 = juce::jlimit (1, kN / 2 - 1, b0); b1 = juce::jlimit (1, kN / 2 - 1, b1);
        double power = 0.0;
        if (fc >= hist.sampleRate * 0.49) power = 0.0;  // above Nyquist (e.g. 44.1 kHz): nothing there
        else if (b1 < b0)                              // pin narrower than one bin (low end): read between the bins
        {
            const double x = juce::jlimit (1.0, (double) (kN / 2 - 2), fc / binHz);
            const int k = (int) x; const double fr = x - k;
            const float m = (float) ((1.0 - fr) * work[(size_t) k] + fr * work[(size_t) k + 1]) * norm;
            power = 0.5 * m * m;
        }
        else
            for (int k = b0; k <= b1; ++k) { const float m = work[(size_t) k] * norm; power += 0.5 * m * m; }
        // Hann spreads a tone over ~1.5 bins of power; this keeps a sine in its band reading its RMS level (+3 dB = peak)
        const float target = juce::jlimit (kFloorDb, 6.0f, (float) (10.0 * std::log10 (power / 1.5 + 1e-12)) + 3.0f);

        auto& v = level[(size_t) b];
        if (target > v) v += (target - v) * riseCoef;
        else            v = juce::jmax (target, v - fallDbPerSec * dt);

        auto& p = peak[(size_t) b];
        if (v >= p) { p = v; peakHold[(size_t) b] = 1.2f; }
        else if ((peakHold[(size_t) b] -= dt) <= 0.0f) p = juce::jmax (v, p - peakFallDbPerSec * dt);
    }
    repaint();
}

void SpectrumAnalyzer::paint (juce::Graphics& g)
{
    auto screen = drawScreen (g, getLocalBounds().toFloat(), false);
    auto r = screen.reduced (34.0f, 12.0f).withTrimmedTop (10.0f).withTrimmedBottom (8.0f);

    draw::engraved (g, "SPECTRUM", { screen.getX() + 38, screen.getY() + 4, 120, 13 }, 10.0f,
                    juce::Justification::centredLeft, col::cream.withAlpha (0.65f));
    draw::engraved (g, "62 PINS  -  OUTPUT  0 TO -62 dB", { screen.getRight() - 176, screen.getY() + 4, 160, 13 }, 9.5f,
                    juce::Justification::centredRight, col::cream.withAlpha (0.45f));

    auto dbToY = [&] (float db) { return r.getBottom() - (db - kFloorDb) / (kTopDb - kFloorDb) * r.getHeight(); };

    // dB scale: 0 to -62
    g.setFont (sans (9.5f, false));
    for (float db : { 0.0f, -12.0f, -24.0f, -36.0f, -48.0f, -62.0f })
    {
        const float y = dbToY (db);
        g.setColour (col::cream.withAlpha (db > -0.5f || db < -61.5f ? 0.12f : 0.06f));
        g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        g.setColour (col::cream.withAlpha (0.45f));
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (screen.getX() + 2, y - 6, 28, 12), juce::Justification::centredRight);
    }

    // pins: a fine line from the floor to the level, a glowing round head on top, a cream peak-hold dot
    const float slot = r.getWidth() / (float) kBands;
    const float headD = juce::jlimit (3.5f, 7.0f, slot * 0.62f);
    const float lineW = juce::jmax (1.1f, slot * 0.16f);
    auto colourFor = [] (float db) { return db > -6.0f ? col::red : (db > -18.0f ? col::amber : juce::Colour (0xffe8c06a)); };
    for (int b = 0; b < kBands; ++b)
    {
        const float cx = r.getX() + slot * ((float) b + 0.5f);
        // faint guide for every pin position
        g.setColour (col::cream.withAlpha (0.04f));
        g.fillRect (juce::Rectangle<float> (cx - lineW * 0.5f, r.getY(), lineW, r.getHeight()));

        const float v = juce::jlimit (kFloorDb, kTopDb, level[(size_t) b]);
        if (v > kFloorDb + 0.5f)
        {
            const float y = dbToY (v);
            juce::ColourGradient stem (colourFor (v).withAlpha (0.95f), cx, y, juce::Colour (0xffe8c06a).withAlpha (0.25f), cx, r.getBottom(), false);
            g.setGradientFill (stem);
            g.fillRect (juce::Rectangle<float> (cx - lineW * 0.5f, y, lineW, r.getBottom() - y));
            const auto head = juce::Rectangle<float> (headD, headD).withCentre ({ cx, y });
            g.setColour (colourFor (v).withAlpha (0.28f));
            g.fillEllipse (head.expanded (headD * 0.3f));                     // glow
            g.setColour (colourFor (v));
            g.fillEllipse (head);
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.fillEllipse (head.reduced (headD * 0.3f).translated (-headD * 0.12f, -headD * 0.12f));   // highlight
        }
        // peak hold
        const float p = peak[(size_t) b];
        if (p > kFloorDb + 1.0f)
        {
            const float y = dbToY (juce::jmin (p, kTopDb));
            g.setColour (col::cream);
            g.fillEllipse (juce::Rectangle<float> (headD * 0.55f, headD * 0.55f).withCentre ({ cx, y - headD * 0.9f }));
        }
    }

    // frequency labels
    g.setFont (sans (9.5f, false));
    g.setColour (col::cream.withAlpha (0.5f));
    static const std::pair<float, const char*> labels[] = { { 25.0f, "25" }, { 50.0f, "50" }, { 100.0f, "100" }, { 200.0f, "200" }, { 400.0f, "400" },
                                                            { 1000.0f, "1k" }, { 2000.0f, "2k" }, { 4000.0f, "4k" }, { 8000.0f, "8k" }, { 16000.0f, "16k" } };
    for (auto& [hz, text] : labels)
    {
        const float pos = (float) kBand1k + 61.0f / 3.0f * std::log10 (hz / 1000.0f);     // fractional pin index
        const float cx = r.getX() + slot * (pos + 0.5f);
        g.drawText (text, juce::Rectangle<float> (cx - 18, r.getBottom() + 2, 36, 11), juce::Justification::centred);
    }
    drawGlassSheen (g, screen);
}

// -----------------------------------------------------------------------------
//  Vectorscope
// -----------------------------------------------------------------------------
static const juce::Colour kPhosphor { 0xffdcec86 };    // pale yellow-green dots

Vectorscope::Vectorscope (const AudioHistory& h) : hist (h) {}

void Vectorscope::resized()
{
    screen = getLocalBounds().toFloat().reduced (7.0f);
    auto r = screen.reduced (10.0f, 6.0f);
    r.removeFromTop (14.0f);                                   // title
    balRow  = r.removeFromTop (12.0f);
    corrRow = r.removeFromBottom (12.0f);
    r.removeFromTop (4.0f); r.removeFromBottom (4.0f);
    const int side = (int) std::floor (juce::jmin (r.getWidth(), r.getHeight()));
    plot = r.toNearestInt().withSizeKeepingCentre (side, side);
    glow.assign ((size_t) (side * side), 0.0f);
    img = juce::Image (juce::Image::ARGB, juce::jmax (1, side), juce::jmax (1, side), true);
}

void Vectorscope::update (float dt)
{
    const int side = plot.getWidth();
    if (side <= 0) return;

    // --- correlation and balance over the last 300 ms ----------------------------
    const int win = juce::jlimit (256, AudioHistory::kSize - 1, (int) (hist.sampleRate * 0.3));
    double ll = 0, rr = 0, lr = 0;
    for (int i = 0; i < win; ++i) { const double l = hist.atL (i), r = hist.atR (i); ll += l * l; rr += r * r; lr += l * r; }
    hasSignal = (ll + rr) / win > 1e-9;                         // above about -90 dBFS
    const float smooth = 1.0f - std::exp (-dt / 0.12f);
    const float cInst = hasSignal && ll > 1e-12 && rr > 1e-12 ? (float) (lr / std::sqrt (ll * rr)) : 0.0f;
    const float bInst = hasSignal ? (float) ((rr - ll) / (rr + ll)) : 0.0f;
    corr    += (juce::jlimit (-1.0f, 1.0f, cInst) - corr) * smooth;
    balance += (juce::jlimit (-1.0f, 1.0f, bInst) - balance) * smooth;
    // peak hold: lowest correlation in the last 3 s (the dangerous direction)
    if (corr <= corrHold || (corrHoldTimer -= dt) <= 0.0f) { corrHold = corr; corrHoldTimer = 3.0f; }

    // --- dot cloud with phosphor persistence ---------------------------------------
    const float decay = std::exp (-dt / 0.15f);
    for (auto& v : glow) v *= decay;

    const int n = juce::jlimit (0, 4096, juce::jmax (hist.newSamples, 1024));
    float peak = 1e-6f;
    for (int i = 0; i < n; ++i) peak = juce::jmax (peak, std::abs (hist.atL (i)), std::abs (hist.atR (i)));   // |u|+|v| = max(|L|,|R|)
    // auto gain: shrink instantly, grow slowly, between 1x and 16x
    const float want = juce::jlimit (1.0f, 16.0f, 0.85f / peak);
    gain = want < gain ? want : gain + (want - gain) * (1.0f - std::exp (-dt / 0.8f));

    const float half = (float) side * 0.5f;
    for (int i = 0; i < n; ++i)
    {
        const float l = hist.atL (i), r = hist.atR (i);
        const float u = (r - l) * 0.5f * gain;                 // side    -> horizontal
        const float v = (l + r) * 0.5f * gain;                 // mid     -> vertical (L+R = up)
        const int px = (int) (half + u * half), py = (int) (half - v * half);
        if (px < 0 || py < 0 || px >= side || py >= side) continue;
        glow[(size_t) (py * side + px)] += 0.45f;
        if (px + 1 < side) glow[(size_t) (py * side + px + 1)] += 0.15f;     // slightly larger dots
        if (py + 1 < side) glow[(size_t) ((py + 1) * side + px)] += 0.15f;
    }

    // --- render the buffer ---------------------------------------------------------------
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x)
        {
            const float g = glow[(size_t) (y * side + x)];
            const auto c = g > 0.004f ? kPhosphor.interpolatedWith (juce::Colour (0xfffbffe0), juce::jlimit (0.0f, 0.6f, (g - 1.0f) * 0.5f))
                                                 .withAlpha (juce::jmin (1.0f, std::sqrt (g * 2.0f)))
                                      : juce::Colours::transparentBlack;
            bd.setPixelColour (x, y, c);
        }
    repaint();
}

void Vectorscope::paint (juce::Graphics& g)
{
    auto scr = drawScreen (g, getLocalBounds().toFloat(), false);
    draw::engraved (g, "VECTORSCOPE", { scr.getX() + 10, scr.getY() + 3, 120, 13 }, 10.0f,
                    juce::Justification::centredLeft, col::cream.withAlpha (0.65f));
    g.setFont (sans (9.0f, false));
    g.setColour (col::cream.withAlpha (0.4f));
    g.drawText (gain > 1.05f ? "AUTO x" + juce::String (gain, 1) : juce::String ("FULL SCALE"),
                juce::Rectangle<float> (scr.getRight() - 90, scr.getY() + 3, 80, 13), juce::Justification::centredRight);

    // --- diamond and diagonal axes ---------------------------------------------------
    const auto p = plot.toFloat();
    const auto c = p.getCentre();
    const float h = p.getWidth() * 0.5f;
    juce::Path diamond;
    diamond.startNewSubPath (c.x, c.y - h); diamond.lineTo (c.x + h, c.y); diamond.lineTo (c.x, c.y + h); diamond.lineTo (c.x - h, c.y); diamond.closeSubPath();
    g.setColour (juce::Colours::white.withAlpha (0.035f));
    g.fillPath (diamond);
    g.setColour (col::cream.withAlpha (0.18f));
    g.drawLine (p.getX(), p.getY(), p.getRight(), p.getBottom(), 0.8f);    // L axis
    g.drawLine (p.getRight(), p.getY(), p.getX(), p.getBottom(), 0.8f);    // R axis
    g.setColour (col::cream.withAlpha (0.45f));
    g.strokePath (diamond, juce::PathStrokeType (1.2f));
    draw::engraved (g, "L", { p.getX() - 2, p.getY() - 2, 14, 12 }, 10.0f, juce::Justification::centredLeft, col::cream.withAlpha (0.7f));
    draw::engraved (g, "R", { p.getRight() - 12, p.getY() - 2, 14, 12 }, 10.0f, juce::Justification::centredRight, col::cream.withAlpha (0.7f));
    draw::engraved (g, "M", { c.x - 7, p.getY() - 2, 14, 10 }, 8.5f, juce::Justification::centred, col::cream.withAlpha (0.35f));

    g.setOpacity (1.0f);                         // images draw at the current colour's opacity
    g.drawImageAt (img, plot.getX(), plot.getY());

    // --- segmented meters ----------------------------------------------------------------
    auto segments = [&] (juce::Rectangle<float> row, const char* leftText, const char* rightText, auto&& colourFor)
    {
        g.setFont (sans (9.5f));
        g.setColour (col::cream.withAlpha (0.6f));
        g.drawText (leftText,  row.removeFromLeft (16.0f), juce::Justification::centredLeft);
        g.drawText (rightText, row.removeFromRight (16.0f), juce::Justification::centredRight);
        const int nSeg = 21;
        const float w = row.getWidth() / (float) nSeg;
        for (int i = 0; i < nSeg; ++i)
        {
            auto seg = juce::Rectangle<float> (row.getX() + w * (float) i + 1.0f, row.getY() + 1.0f, w - 2.0f, row.getHeight() - 2.0f);
            const float pos = -1.0f + 2.0f * ((float) i + 0.5f) / (float) nSeg;  // -1 .. +1
            g.setColour (colourFor (pos, i, nSeg));
            g.fillRect (seg);
        }
    };

    // balance: one bright segment at the current L/R balance
    const int nSeg = 21;
    const int balIdx = juce::jlimit (0, nSeg - 1, (int) std::lround ((balance + 1.0f) * 0.5f * (float) (nSeg - 1)));
    segments (balRow, "L", "R", [&] (float, int i, int)
    {
        return i == balIdx && hasSignal ? juce::Colours::white : juce::Colours::white.withAlpha (0.08f);
    });

    // correlation: lit from 0 to the current value, cream marker at the 3 s minimum
    const int zeroIdx = nSeg / 2;
    const int corrIdx = juce::jlimit (0, nSeg - 1, (int) std::lround ((corr + 1.0f) * 0.5f * (float) (nSeg - 1)));
    const int holdIdx = juce::jlimit (0, nSeg - 1, (int) std::lround ((corrHold + 1.0f) * 0.5f * (float) (nSeg - 1)));
    segments (corrRow, "-1", "+1", [&] (float pos, int i, int)
    {
        const juce::Colour on = pos < 0.0f ? juce::Colour (0xfff3b48a) : (pos < 0.5f ? juce::Colour (0xfff7cfa4) : kPhosphor);
        if (! hasSignal) return i == zeroIdx ? juce::Colours::white.withAlpha (0.4f) : on.withAlpha (0.07f);
        if (i == holdIdx && holdIdx < corrIdx) return col::cream;
        if (i == zeroIdx) return juce::Colours::white;
        const bool lit = corrIdx >= zeroIdx ? (i > zeroIdx && i <= corrIdx) : (i < zeroIdx && i >= corrIdx);
        return lit ? on : on.withAlpha (0.07f);
    });

    drawGlassSheen (g, scr);
}

// -----------------------------------------------------------------------------
//  LoudnessPanel
// -----------------------------------------------------------------------------
struct LufsTarget { float lufs; const char* name; };
static const LufsTarget kTargets[] = { { -14.0f, "-14 LUFS  Streaming" }, { -16.0f, "-16 LUFS  Apple Music" },
                                       { -23.0f, "-23 LUFS  EBU R128" },  { -24.0f, "-24 LUFS  ATSC A/85" },
                                       { -9.0f,  "-9 LUFS  Loud / Club" } };

LoudnessPanel::LoudnessPanel (PassiveMasterEQProcessor& p) : proc (p)
{
    for (int i = 0; i < (int) std::size (kTargets); ++i) targetBox.addItem (kTargets[i].name, i + 1);
    targetBox.setSelectedItemIndex (juce::jlimit (0, (int) std::size (kTargets) - 1, (int) proc.apvts.state.getProperty ("lufsTarget", 0)),
                                    juce::dontSendNotification);
    targetBox.onChange = [this] { proc.apvts.state.setProperty ("lufsTarget", targetBox.getSelectedItemIndex(), nullptr); repaint(); };
    targetBox.setTooltip ("TARGET - the loudness you're aiming for. Streaming services turn louder masters down to about -14 LUFS "
                          "(Apple Music -16). Broadcast uses -23 (EBU) or -24 (US). The INTEGRATED reading turns green within 1 LU of the target.");
    resetButton.onClick = [this] { proc.loudness.resetRequested.store (true); };
    resetButton.setTooltip ("RESET - clears Integrated, LRA and all maximum readings. Press it, then play the whole song from the start.");
    addAndMakeVisible (targetBox);
    addAndMakeVisible (resetButton);

    integ = { {}, "INTEGRATED", "LUFS", "INTEGRATED - the loudness of everything played since RESET (gated, EBU R128). This is the number streaming services and broadcasters use." };
    shortT = { {}, "SHORT-TERM", "LUFS", "SHORT-TERM - loudness of the last 3 seconds. Good for judging sections (verse vs chorus)." };
    mom   = { {}, "MOMENTARY", "LUFS", "MOMENTARY - loudness of the last 0.4 seconds: reacts like a fast meter." };
    tpMax = { {}, "TRUE PEAK MAX", "dBTP", "TRUE PEAK MAX - the highest peak since RESET, including peaks between samples (4x oversampled). "
                                            "Keep it at or below -1 dBTP for streaming so the encoded file doesn't clip. Turns red above -1." };
    lra   = { {}, "LRA", "LU", "LRA (Loudness Range) - how much the loudness varies over the song (EBU Tech 3342). Pop masters are often 4-8 LU; classical/film 15+." };
    plr   = { {}, "PLR", "LU", "PLR (Peak to Loudness Ratio) - TRUE PEAK MAX minus INTEGRATED. Higher = more dynamic; heavily limited masters are around 8 LU." };
    psr   = { {}, "PSR", "LU", "PSR (Peak to Short-term Ratio) - recent true peak minus SHORT-TERM. A live view of how punchy (high) or squashed (low) it is right now." };
    stMax = { {}, "S MAX", "LUFS", "SHORT-TERM MAX - the loudest 3-second stretch since RESET." };
    mMax  = { {}, "M MAX", "LUFS", "MOMENTARY MAX - the loudest 0.4-second moment since RESET." };
}

float LoudnessPanel::getTargetLufs() const
{
    return kTargets[juce::jlimit (0, (int) std::size (kTargets) - 1, targetBox.getSelectedItemIndex())].lufs;
}

void LoudnessPanel::update()
{
    auto& l = proc.loudness;
    vM = l.momentary.load(); vS = l.shortTerm.load(); vI = l.integrated.load(); vLra = l.lra.load();
    vMMax = l.momentaryMax.load(); vSMax = l.shortTermMax.load(); vTpMax = l.truePeakMax.load(); vTp3 = l.truePeak3s.load();
    repaint();
}

void LoudnessPanel::resized()
{
    screen = getLocalBounds().toFloat().reduced (7.0f);
    auto r = screen.reduced (10.0f, 6.0f);
    header = r.removeFromTop (22.0f);
    {
        auto hb = header.toNearestInt();
        resetButton.setBounds (hb.removeFromRight (62).reduced (0, 1));
        hb.removeFromRight (6);
        targetBox.setBounds (hb.removeFromRight (168).reduced (0, 1));
    }
    r.removeFromTop (6.0f);
    bar = r.removeFromLeft (44.0f);
    r.removeFromLeft (8.0f);

    auto small = r.removeFromRight (r.getWidth() * 0.36f);
    r.removeFromRight (6.0f);
    const float gap = 6.0f;
    const float bw = (r.getWidth() - gap) / 2.0f, bh = (r.getHeight() - gap) / 2.0f;
    integ.r  = { r.getX(),            r.getY(),            bw, bh };
    shortT.r = { r.getX() + bw + gap, r.getY(),            bw, bh };
    tpMax.r  = { r.getX(),            r.getY() + bh + gap, bw, bh };
    mom.r    = { r.getX() + bw + gap, r.getY() + bh + gap, bw, bh };

    const float sh = (small.getHeight() - 4.0f * 4.0f) / 5.0f;
    Tile* col[] = { &lra, &plr, &psr, &stMax, &mMax };
    for (int i = 0; i < 5; ++i) col[i]->r = { small.getX(), small.getY() + (float) i * (sh + 4.0f), small.getWidth(), sh };
}

static juce::String fmtLufs (float v, int decimals = 1) { return v < -69.9f ? juce::String ("--.-") : juce::String (v, decimals); }

juce::String LoudnessPanel::getTooltip()
{
    const auto m = getMouseXYRelative().toFloat();
    for (auto* t : { &integ, &shortT, &mom, &tpMax, &lra, &plr, &psr, &stMax, &mMax })
        if (t->r.contains (m)) return t->tip;
    if (bar.contains (m))
        return "LOUDNESS BAR - wide bar = SHORT-TERM, thin amber bar = MOMENTARY, green line = your TARGET.";
    return "LOUDNESS (EBU R128 / ITU-R BS.1770) of the plugin's output. Hover over a reading for what it means.";
}

void LoudnessPanel::paint (juce::Graphics& g)
{
    auto scr = drawScreen (g, getLocalBounds().toFloat(), false);
    draw::engraved (g, "LOUDNESS", header.withWidth (90.0f), 11.0f, juce::Justification::centredLeft, col::cream.withAlpha (0.75f));
    g.setFont (sans (9.0f, false));
    g.setColour (col::cream.withAlpha (0.4f));
    g.drawText ("EBU R128", header.withTrimmedLeft (70.0f).withWidth (70.0f), juce::Justification::centredLeft);
    const float target = getTargetLufs();

    // --- vertical loudness bar -----------------------------------------------------------
    {
        auto b = bar.withTrimmedLeft (20.0f);
        const float lo = -44.0f, hi = -4.0f;
        auto yOf = [&] (float v) { return b.getBottom() - (juce::jlimit (lo, hi, v) - lo) / (hi - lo) * b.getHeight(); };
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (b, 2.0f);
        g.setFont (sans (8.5f, false));
        for (float v = -40.0f; v <= -5.0f; v += 5.0f)
        {
            g.setColour (col::cream.withAlpha (0.35f));
            g.drawText (juce::String ((int) v), juce::Rectangle<float> (bar.getX() - 2.0f, yOf (v) - 5.0f, 20.0f, 10.0f), juce::Justification::centredRight);
            g.setColour (col::cream.withAlpha (0.08f));
            g.drawHorizontalLine ((int) yOf (v), b.getX(), b.getRight());
        }
        if (vS > lo)
        {
            const auto fill = juce::Rectangle<float> (b.getX() + 2, yOf (vS), b.getWidth() * 0.55f - 2, b.getBottom() - yOf (vS));
            juce::ColourGradient gr (col::cream.withAlpha (0.85f), 0, b.getY(), juce::Colour (0xff8fa7c9).withAlpha (0.7f), 0, b.getBottom(), false);
            g.setGradientFill (gr);
            g.fillRect (fill);
        }
        if (vM > lo)
        {
            g.setColour (col::amber);
            g.fillRect (juce::Rectangle<float> (b.getX() + b.getWidth() * 0.62f, yOf (vM), b.getWidth() * 0.3f, b.getBottom() - yOf (vM)));
        }
        g.setColour (col::green);
        g.fillRect (juce::Rectangle<float> (b.getX() - 3.0f, yOf (target) - 1.0f, b.getWidth() + 6.0f, 2.0f));
    }

    // --- tiles -----------------------------------------------------------------------------------
    auto tile = [&] (const Tile& t, const juce::String& value, juce::Colour tint, bool big)
    {
        auto r = t.r;
        g.setColour (juce::Colour (0xff22201c));
        g.fillRoundedRectangle (r, 4.0f);
        if (tint.getAlpha() > 0) { g.setColour (tint); g.fillRoundedRectangle (r, 4.0f); }
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRect (r.reduced (2.0f).withHeight (r.getHeight() * 0.4f));
        const auto valueCol = tint.getAlpha() > 0 ? tint.withAlpha (1.0f).brighter (0.9f) : col::cream;
        if (big)
        {
            auto top = r.withTrimmedBottom (r.getHeight() * 0.36f);
            juce::AttributedString as;
            as.append (value, sans (25.0f), valueCol);
            as.append (juce::String (" ") + t.unit, sans (11.0f, false), valueCol.withAlpha (0.8f));
            as.setJustification (juce::Justification::centredBottom);
            as.draw (g, top.withTrimmedTop (4.0f));
            g.setFont (sans (10.5f, false));
            g.setColour (col::cream.withAlpha (0.65f));
            g.drawText (t.label, r.withTrimmedTop (r.getHeight() * 0.66f), juce::Justification::centred);
        }
        else
        {
            g.setFont (sans (9.5f, false));
            g.setColour (col::cream.withAlpha (0.6f));
            g.drawText (t.label, r.reduced (7.0f, 0).withWidth (50.0f), juce::Justification::centredLeft);
            juce::AttributedString as;
            as.append (value, sans (14.5f), valueCol);
            as.append (juce::String (" ") + t.unit, sans (9.0f, false), valueCol.withAlpha (0.75f));
            as.setJustification (juce::Justification::centredRight);
            as.draw (g, r.reduced (7.0f, 0));
        }
    };

    const auto none = juce::Colours::transparentBlack;
    juce::Colour iTint = none;
    if (vI > -69.9f)
        iTint = std::abs (vI - target) <= 1.0f ? col::green.withAlpha (0.28f)
              : (vI > target ? col::red.withAlpha (0.30f) : col::blue.withAlpha (0.30f));
    const bool tpOver = vTpMax > -1.0f;
    const bool hasI = vI > -69.9f, hasS = vS > -69.9f, hasTp = vTpMax > -150.0f;

    tile (integ,  fmtLufs (vI), iTint, true);
    tile (shortT, fmtLufs (vS), none, true);
    tile (tpMax,  hasTp ? juce::String (vTpMax, 1) : juce::String ("--.-"), tpOver ? col::red.withAlpha (0.32f) : none, true);
    tile (mom,    fmtLufs (vM), none, true);
    tile (lra,    hasS ? juce::String (vLra, 1) : juce::String ("--.-"), none, false);
    tile (plr,    hasI && hasTp ? juce::String (vTpMax - vI, 1) : juce::String ("--.-"), none, false);
    tile (psr,    hasS && vTp3 > -150.0f ? juce::String (vTp3 - vS, 1) : juce::String ("--.-"),
          hasS && vTp3 - vS < 7.0f ? col::blue.withAlpha (0.30f) : none, false);
    tile (stMax,  fmtLufs (vSMax), none, false);
    tile (mMax,   fmtLufs (vMMax), none, false);

    drawGlassSheen (g, scr);
}

// -----------------------------------------------------------------------------
//  KeyPanel
// -----------------------------------------------------------------------------
static juce::String keyNameShort (int key)
{
    if (key < 0) return "--";
    return juce::String (ppm::pitchName (key % 12)) + (key >= 12 ? " minor" : " major");
}

static juce::String keyInfo (int key)
{
    if (key < 0) return {};
    const int sig = ppm::keySignature (key);
    const juce::String sigText = sig == 0 ? juce::String ("no sharps/flats")
                               : juce::String (std::abs (sig)) + (sig > 0 ? " sharp" : " flat") + (std::abs (sig) > 1 ? "s" : "");
    return "Camelot " + juce::String (ppm::camelotNumber (key)) + (key >= 12 ? "A" : "B") + "  |  " + sigText;
}

KeyPanel::KeyPanel (PassiveMasterEQProcessor& p, const AudioHistory& h)
    : proc (p), hist (h),
      semis (p.apvts, ppm::params::keySemis, "SEMITONES", "-12", "+12"),
      cents (p.apvts, ppm::params::keyCents, "FINE", "-50", "+50")
{
    semis.slider.textFromValueFunction = [] (double v) { const int i = (int) std::lround (v); return (i > 0 ? "+" : "") + juce::String (i) + " st"; };
    cents.slider.textFromValueFunction = [] (double v) { const int i = (int) std::lround (v); return (i > 0 ? "+" : "") + juce::String (i) + " ct"; };
    semis.refresh(); cents.refresh();
    semis.setTooltip ("SEMITONES - move the whole song up or down, -12 to +12 semitones (12 = one octave). "
                      "Switch KEY SHIFT on to hear it. +2 turns A minor into B minor.");
    cents.setTooltip ("FINE - fine tuning in cents (100 cents = 1 semitone), e.g. to match a track that isn't tuned to A = 440 Hz.");
    onButton.setTooltip ("KEY SHIFT - switches the key shifter on/off. While on, the plug-in adds 85 ms of latency (your DAW "
                         "compensates automatically). Best switched while playback is stopped.");
    relearn.setTooltip ("RELEARN - forget the key heard so far and start detecting again (use it when you load a new song).");
    onAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ppm::params::keyOn, onButton);
    formantAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ppm::params::keyFormant, formantButton);
    formantButton.setTooltip ("FORMANT - keeps the natural tone of voices and instruments while the key moves (no 'chipmunk' "
                              "when shifting up, no 'giant' when shifting down). Leave it lit for songs with vocals; switch it off "
                              "for pure electronic tones if you prefer the plain shift.");
    relearn.onClick = [this] { detector.reset(); outKey = detected = {}; repaint(); };
    for (auto* c : std::initializer_list<juce::Component*> { &semis, &cents, &onButton, &formantButton, &relearn }) addAndMakeVisible (c);
}

int KeyPanel::getShiftedKey() const
{
    if (detected.key < 0) return -1;
    const int st = proc.isKeyShiftOn() ? (int) std::lround (proc.getKeyShiftSemitones()) : 0;
    const int mode = detected.key >= 12 ? 12 : 0;
    return mode + (((detected.key % 12) + st) % 12 + 12) % 12;
}

void KeyPanel::update (float dt)
{
    if (std::abs (hist.sampleRate - detectorRate) > 1.0)
    {
        detectorRate = hist.sampleRate;
        detector.prepare (detectorRate);
        mono.assign ((size_t) detector.frameSize(), 0.0f);
    }
    // analyse ~10 times a second, only when new audio has arrived
    sinceAnalysis += dt;
    if (sinceAnalysis >= 0.1f && hist.newSamples > 0)
    {
        const int n = detector.frameSize();
        if (n <= AudioHistory::kSize)
        {
            for (int i = 0; i < n; ++i) mono[(size_t) i] = 0.5f * (hist.atL (n - 1 - i) + hist.atR (n - 1 - i));
            detector.addFrame (mono.data(), sinceAnalysis);
        }
        sinceAnalysis = 0.0f;
        outKey = detector.result();
        // the analysis hears the plug-in's output: remove the shift to get the song's own key
        detected = outKey;
        if (detected.key >= 0 && proc.isKeyShiftOn())
        {
            const int st = (int) std::lround (proc.getKeyShiftSemitones());
            detected.key = (detected.key >= 12 ? 12 : 0) + (((detected.key % 12) - st) % 12 + 12) % 12;
        }
    }
    repaint();
}

void KeyPanel::resized()
{
    screen = getLocalBounds().toFloat().reduced (7.0f);
    auto r = screen.reduced (10.0f, 6.0f);
    r.removeFromTop (16.0f);                                   // title
    display = r.removeFromTop (78.0f);
    r.removeFromTop (4.0f);
    auto bottom = r.toNearestInt();
    auto btns = bottom.removeFromRight (100);
    const int bh = btns.getHeight() / 3;
    onButton.setBounds      (btns.removeFromTop (bh).withSizeKeepingCentre (100, juce::jmin (30, bh - 4)));
    formantButton.setBounds (btns.removeFromTop (bh).withSizeKeepingCentre (100, juce::jmin (28, bh - 4)));
    relearn.setBounds       (btns.withSizeKeepingCentre (86, juce::jmin (26, bh - 4)));
    bottom.removeFromRight (4);
    semis.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2));
    cents.setBounds (bottom);
}

juce::String KeyPanel::getTooltip()
{
    const auto m = getMouseXYRelative().toFloat();
    if (display.contains (m))
        return "SONG KEY - the musical key detected from the audio (it listens over the last ~20 seconds, so give it a "
               "moment on a new song). The bar shows how certain it is. Camelot code is for harmonic mixing; key signature "
               "is the number of sharps or flats. With KEY SHIFT on, the arrow shows the key you'll hear now.";
    return "KEY - see the song's key and move it up or down.";
}

void KeyPanel::paint (juce::Graphics& g)
{
    auto scr = drawScreen (g, getLocalBounds().toFloat(), false);
    draw::engraved (g, "SONG KEY", { scr.getX() + 10, scr.getY() + 3, 120, 13 }, 10.0f,
                    juce::Justification::centredLeft, col::cream.withAlpha (0.65f));

    auto d = display;
    // blue readout window
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (d.expanded (2.0f), 4.0f);
    juce::ColourGradient paper (col::lcdTop, d.getX(), d.getY(), col::lcdBottom, d.getX(), d.getBottom(), false);
    g.setGradientFill (paper);
    g.fillRoundedRectangle (d, 3.0f);
    g.setColour (col::aqua.withAlpha (0.25f));
    g.drawRoundedRectangle (d.reduced (1.5f), 3.0f, 1.0f);

    auto inner = d.reduced (10.0f, 5.0f);
    const bool shiftOn = proc.isKeyShiftOn();
    const bool have = detected.key >= 0;
    const int shifted = getShiftedKey();
    const double st = proc.getKeyShiftSemitones();

    auto top = inner.removeFromTop (34.0f);
    g.setColour (col::aqua.withAlpha (have ? 1.0f : 0.5f));
    g.setFont (serif (have ? 26.0f : 18.0f));
    if (! have)
        g.drawText ("listening...", top, juce::Justification::centredLeft);
    else if (shiftOn && shifted != detected.key)
    {
        g.drawText (keyNameShort (detected.key), top.removeFromLeft (top.getWidth() * 0.47f), juce::Justification::centredLeft);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (serif (20.0f));
        g.drawText ("->", top.removeFromLeft (26.0f), juce::Justification::centred);
        g.setFont (serif (24.0f));
        g.setColour (col::aqua);
        g.drawText (keyNameShort (shifted), top, juce::Justification::centredLeft);
    }
    else
        g.drawText (keyNameShort (detected.key), top, juce::Justification::centredLeft);

    g.setFont (sans (10.5f, false));
    g.setColour (col::aquaDim);
    auto info = inner.removeFromTop (16.0f);
    const juce::String shiftText = shiftOn ? ((st >= 0 ? "  |  shift +" : "  |  shift ") + juce::String (st, 2) + " st") : juce::String();
    g.drawText (have ? keyInfo (shiftOn ? shifted : detected.key) + shiftText : juce::String ("play some music to detect the key"),
                info, juce::Justification::centredLeft);

    // certainty bar
    auto bar = inner.removeFromTop (10.0f).withTrimmedTop (2.0f);
    g.setFont (sans (9.0f, false));
    g.drawText ("CERTAINTY", bar.removeFromLeft (62.0f).translated (0, -1.0f), juce::Justification::centredLeft);
    g.setColour (col::aqua.withAlpha (0.15f));
    g.fillRoundedRectangle (bar, 2.0f);
    g.setColour (detected.confidence > 0.6 ? col::aqua : (detected.confidence > 0.3 ? col::yellow : juce::Colour (0xffff6a5a)));
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (float) juce::jlimit (0.0, 1.0, detected.confidence)), 2.0f);

    drawGlassSheen (g, scr);
}

// =============================================================================
//  Tooltip text (the "explained" part, per control)
// =============================================================================
namespace tips
{
    static const char* bandRange[] = { "22 Hz - 1 kHz: weight, punch, mud.",
                                       "82 Hz - 3.9 kHz: body, warmth, boxiness.",
                                       "220 Hz - 10 kHz: presence, attack, harshness.",
                                       "560 Hz - 27 kHz: air, sparkle, sibilance." };
    static juce::String freq (int b)
    {
        return juce::String ("FREQUENCY - picks where this band works. ") + bandRange[b]
             + "\n11 fixed positions, like the hardware rotary switch.";
    }
    static const juce::String gain = "GAIN - how much to boost or cut. 0 = no change.\n"
        "The window shows the real dB. Max is 11 dB with a narrow bell, about 6 dB with a wide bell, "
        "because gain and bandwidth interact like the passive circuit.";
    static const juce::String bw = "BANDWIDTH - WIDE (left) = broad, gentle, musical. NARROW (right) = focused, surgical.\n"
        "In SHELF mode it shapes the shelf: turn right to add the classic dip-and-bump (Pultec-style) at the corner.";
    static const juce::String mode = "BOOST / OUT / CUT - what this band does. OUT switches the band off without losing its settings.";
    static const juce::String shelf = "BELL / SHELF - BELL lifts or dips a hill around the frequency. "
        "SHELF lifts or lowers everything beyond it (below it on LOW and LOW MID, above it on HIGH MID and HIGH).";
    static const juce::String hp = "HIGH PASS - removes rumble and sub-sonic energy below 12-39 Hz (18 dB/oct). OFF = full range.";
    static const juce::String lp = "LOW PASS - gently tames the very top: 15-40 kHz at 18 dB/oct, 52 kHz at 30 dB/oct. OFF = full range.";
    static const juce::String trim = "GAIN TRIM - output level in exact 0.5 dB steps (-2.5 to +2.5 dB). Use it to level-match before/after.";
    static const juce::String eqIn = "EQ IN - quick compare. EQ OUT bypasses the four bands and filters; the tube stage stays in, like the hardware.";
    static const juce::String link = "LINK - Right follows Left (normal for stereo mastering). Unlink to EQ each side separately; "
        "unlinking starts the right side from the left side's settings.";
    static const juce::String copy = "Copy every setting from one channel to the other.";
    static const juce::String tube = "TUBE - switches the tube make-up stage on/off. It adds warm 2nd-harmonic colour that grows with level.";
    static const juce::String drive = "DRIVE - how hard the signal hits the tubes. The output stays at the same loudness; "
        "turning up just adds more harmonics (watch the HARMONICS meter).";
    static const juce::String vu = "VU METER - average output level. Aim for around 0 VU (= -18 dBFS RMS). "
        "The orange lamp flashes when peaks go above -1 dBFS.";
    static const juce::String harm = "HARMONICS - how much tube colour is being added, in % THD. "
        "Under 0.3 % = subtle sheen; 1 % and up (red zone) = clearly audible saturation.";
    static const juce::String tubes = "Tube glow follows the tube stage: dark when off, brighter with level and DRIVE.";
    static const juce::String grMeter = "GAIN REDUCTION - how many dB the compressor is turning the signal down right now. "
        "1-2 dB = gentle glue for mastering; 3-6 dB = obvious leveling; 10 dB+ (red zone) = heavy squeeze.";
    static const juce::String cThresh = "THRESHOLD - where compression starts (0 to -30 dBFS). Turn it down (clockwise) until the "
        "GAIN REDUCTION meter moves 1-3 dB on the loud parts. 0 dB = compressor effectively off.";
    static const juce::String cAttack = "ATTACK - how fast the compressor grabs (2-70 ms). Slower attack lets drum hits punch through; "
        "faster attack controls peaks more tightly.";
    static const juce::String cRecovery = "RECOVERY - how fast it lets go (0.2-8 s), like the hardware's stepped release. "
        "Short = lively/pumping, long = smooth and steady. 0.6-1.2 s suits most mastering.";
    static const juce::String cOutput = "OUTPUT - make-up gain (0 to +12 dB) to bring the level back up after compression. "
        "Match it to the GAIN REDUCTION reading for a fair before/after comparison.";
    static const juce::String cMix = "MIX - blends compressed and uncompressed signal (parallel / 'New York' compression). "
        "100 % = fully compressed; 30-50 % adds density while keeping transients.";
    static const juce::String cMode = "COMPRESS / LIMIT - COMPRESS is the soft, gentle variable-mu curve (about 1.5:1 rising to 4:1). "
        "LIMIT uses a harder knee and higher ratio for peak control.";
    static const juce::String cScHp = "SC HP - when lit, the compressor ignores bass below 100 Hz, so kick and bass don't make "
        "the whole mix pump. Turn off (SC FULL) to let the lows drive it too.";
    static const juce::String cIn = "COMP IN - switches the compressor in or out for a quick before/after (click-free).";
    static const juce::String order = "ORDER - EQ > COMP: the EQ shapes tone, then the compressor levels it (most common). "
        "COMP > EQ: compress first, then EQ, so EQ boosts don't push the compressor harder.";
    static const juce::String spectrum = "SPECTRUM - live level of your output on 62 pins (about 1/6 octave each, 19 Hz to 19 kHz). "
        "Range 0 to -62 dB. Each pin rises with the music and falls back slowly; the cream dots hold the recent peaks "
        "(amber above -18 dB, red above -6 dB). Use it to see what your EQ moves are doing.";
    static const juce::String scope = "VECTORSCOPE - shows the stereo image. A tall, narrow cloud = mostly mono/centre; wide = lots of stereo; "
        "leaning toward L or R = one side louder. Top meter: L/R balance. Bottom meter: phase correlation (+1 = mono-safe, 0 = very wide, "
        "below 0 = out of phase: parts will cancel on mono speakers). The cream mark holds the lowest reading for 3 s.";
}

// =============================================================================
//  BandStrip
// =============================================================================
BandStrip::BandStrip (juce::AudioProcessorValueTreeState& s, int ch, int b)
    : band (b),
      gain (s, params::bandId (ch, b, "gain"), "GAIN", "0", "MAX"),
      freq (s, params::bandId (ch, b, "freq"), "FREQUENCY",
            params::formatHz (kBandFreqs[b][0]), params::formatHz (kBandFreqs[b][kNumFreqSteps - 1])),
      bw   (s, params::bandId (ch, b, "bw"), "BANDWIDTH", "WIDE", "NARR"),
      mode (s, params::bandId (ch, b, "mode"))
{
    for (auto* c : std::initializer_list<juce::Component*> { &gain, &freq, &bw, &mode, &shelf }) addAndMakeVisible (c);
    shelfAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, params::bandId (ch, b, "shelf"), shelf);

    freq.slider.textFromValueFunction = [b] (double v)
    { return params::formatHz (kBandFreqs[b][juce::jlimit (0, kNumFreqSteps - 1, (int) std::lround (v))]) + " Hz"; };
    bw.slider.textFromValueFunction = [] (double v)
    {
        const int i = (int) std::lround (v);
        return i == 0 ? juce::String ("WIDE") : (i == kNumKnobSteps - 1 ? juce::String ("NARROW") : juce::String (i));
    };
    freq.refresh(); bw.refresh();

    freq.setTooltip (tips::freq (b));
    gain.setTooltip (tips::gain);
    bw.setTooltip (tips::bw);
    mode.setTooltip (tips::mode);
    shelf.setTooltip (tips::shelf);
}

void BandStrip::updateReadout (const BandSettings& b)
{
    const double db = bandSignedDb (b);
    gain.valueText = [db, b]
    {
        if (b.mode == Out) return juce::String ("OUT");
        return (db >= 0 ? "+" : "") + juce::String (db, 1) + " dB";
    };
    gain.valueColour = b.mode == Cut ? juce::Colour (0xff1d5fa8) : (b.mode == Out ? juce::Colour (0xff6a6458) : juce::Colour (0xffb34a00));
    gain.refresh();
}

void BandStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // engraved divider on the right
    if (band < kNumBands - 1)
    {
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawVerticalLine (getWidth() - 2, r.getY() + 14, r.getBottom() - 14);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawVerticalLine (getWidth() - 1, r.getY() + 14, r.getBottom() - 14);
    }
    draw::engraved (g, kBandNames[band], r.removeFromTop (22).translated (0, 4), 13.5f, juce::Justification::centred,
                    isEnabled() ? col::engrave : col::engraveDim);
}

void BandStrip::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    r.removeFromTop (22);
    auto bottom = r.removeFromBottom (28);
    r.removeFromBottom (6);

    // GAIN is the big knob (full height); FREQUENCY and BANDWIDTH sit beside it, a step smaller
    const int w = r.getWidth();
    gain.setBounds (r.removeFromLeft (w * 40 / 100));
    bw.setBounds   (r.removeFromRight (r.getWidth() / 2).withTrimmedTop (30));
    freq.setBounds (r.withTrimmedTop (30));

    bottom = bottom.withSizeKeepingCentre (juce::jmin (bottom.getWidth(), 300), bottom.getHeight());   // don't stretch on wide windows
    shelf.setBounds (bottom.removeFromRight (76));
    bottom.removeFromRight (6);
    mode.setBounds (bottom);
}

// =============================================================================
//  CenterStrip
// =============================================================================
CenterStrip::CenterStrip (juce::AudioProcessorValueTreeState& s, int ch)
    : hp   (s, params::chanId (ch, "hp"),   "HIGH PASS", "OFF", "39"),
      lp   (s, params::chanId (ch, "lp"),   "LOW PASS",  "OFF", "15k"),
      trim (s, params::chanId (ch, "trim"), "GAIN TRIM", "-2.5", "+2.5")
{
    for (auto* c : std::initializer_list<juce::Component*> { &hp, &lp, &trim, &eqIn }) addAndMakeVisible (c);
    eqInAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, params::chanId (ch, "eq_in"), eqIn);
    hp.slider.textFromValueFunction = [] (double v) { const int i = juce::jlimit (0, 5, (int) std::lround (v)); return i == 0 ? juce::String ("OFF") : params::formatHz (kHpFreqs[i]) + " Hz"; };
    lp.slider.textFromValueFunction = [] (double v) { const int i = juce::jlimit (0, 5, (int) std::lround (v)); return i == 0 ? juce::String ("OFF") : params::formatHz (kLpFreqs[i]) + "Hz"; };
    hp.refresh(); lp.refresh();
    hp.setTooltip (tips::hp); lp.setTooltip (tips::lp); trim.setTooltip (tips::trim); eqIn.setTooltip (tips::eqIn);
}

void CenterStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawVerticalLine (1, r.getY() + 14, r.getBottom() - 14);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawVerticalLine (2, r.getY() + 14, r.getBottom() - 14);
    draw::engraved (g, "FILTERS / TRIM", r.removeFromTop (22).translated (0, 4), 13.5f, juce::Justification::centred,
                    isEnabled() ? col::engrave : col::engraveDim);
}

void CenterStrip::resized()
{
    auto r = getLocalBounds().reduced (10, 6);
    r.removeFromTop (22);
    auto top = r.removeFromTop (r.getHeight() / 2 + 4);
    hp.setBounds (top.removeFromLeft (top.getWidth() / 2));
    lp.setBounds (top);
    r.removeFromTop (2);
    trim.setBounds (r.removeFromLeft (r.getWidth() / 2));
    eqIn.setBounds (r.withSizeKeepingCentre (r.getWidth() - 2, 30).translated (0, 6));
}

// =============================================================================
//  MetalToggle  (chrome bat-handle toggle)
// =============================================================================
MetalToggle::MetalToggle (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& up,
                          const juce::String& down, float upV, float downV)
    : upLabel (up), downLabel (down), upValue (upV), downValue (downV),
      attachment (*s.getParameter (paramId), [this] (float v) { value = v; repaint(); })
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    attachment.sendInitialUpdate();
}

void MetalToggle::mouseDown (const juce::MouseEvent&)
{
    if (! isEnabled()) return;
    attachment.setValueAsCompleteGesture (isUp() ? downValue : upValue);
}

void MetalToggle::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const bool up = isUp(), on = isEnabled();
    const auto lc = on ? labelColour : labelColour.withAlpha (0.4f);
    auto top = r.removeFromTop (13.0f), bottom = r.removeFromBottom (13.0f);
    draw::engraved (g, upLabel,   top,    10.0f, juce::Justification::centred, up ? lc : lc.withAlpha (on ? 0.55f : 0.3f));
    draw::engraved (g, downLabel, bottom, 10.0f, juce::Justification::centred, up ? lc.withAlpha (on ? 0.55f : 0.3f) : lc);

    const auto c = r.getCentre();
    const float R = juce::jmin (r.getHeight() * 0.26f, 9.0f);
    // mounting plate + hex nut
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (juce::Rectangle<float> (R * 2.6f, R * 2.6f).withCentre (c.translated (0, 1.5f)));
    juce::Path hex;
    for (int i = 0; i < 6; ++i)
    {
        const float a = juce::MathConstants<float>::pi / 3.0f * (float) i + juce::MathConstants<float>::pi / 6.0f;
        const juce::Point<float> p (c.x + R * 1.25f * std::cos (a), c.y + R * 1.25f * std::sin (a));
        if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
    }
    hex.closeSubPath();
    juce::ColourGradient nut (juce::Colour (0xffeeeeee), c.x - R, c.y - R, juce::Colour (0xff5a5a5a), c.x + R, c.y + R, false);
    g.setGradientFill (nut); g.fillPath (hex);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.strokePath (hex, juce::PathStrokeType (0.8f));
    // threaded bushing
    juce::ColourGradient bush (juce::Colour (0xffcfcfcf), c.x - R * 0.7f, c.y, juce::Colour (0xff3c3c3c), c.x + R * 0.7f, c.y, false);
    g.setGradientFill (bush); g.fillEllipse (juce::Rectangle<float> (R * 1.4f, R * 1.4f).withCentre (c));
    g.setColour (juce::Colours::black.withAlpha (0.7f)); g.drawEllipse (juce::Rectangle<float> (R * 1.4f, R * 1.4f).withCentre (c), 0.8f);

    // bat handle: tapered chrome lever, pointing up or down (with perspective: shorter, it leans toward you)
    const float len = r.getHeight() * 0.44f, dir = up ? -1.0f : 1.0f;
    const juce::Point<float> tip (c.x, c.y + dir * len);
    juce::Path lever;
    const float w0 = R * 0.55f, w1 = R * 0.85f;
    lever.startNewSubPath (c.x - w0, c.y);
    lever.lineTo (tip.x - w1 * 0.55f, tip.y);
    lever.lineTo (tip.x + w1 * 0.55f, tip.y);
    lever.lineTo (c.x + w0, c.y);
    lever.closeSubPath();
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillPath (lever, juce::AffineTransform::translation (2.0f, 2.5f));
    juce::ColourGradient chrome (juce::Colour (0xff6a6a6a), c.x - w1, c.y, juce::Colour (0xff4a4a4a), c.x + w1, c.y, false);
    chrome.addColour (0.35, juce::Colour (0xfff8f8f8));
    chrome.addColour (0.55, juce::Colour (0xffbdbdbd));
    g.setGradientFill (chrome); g.fillPath (lever);
    // rounded tip
    const auto tipBall = juce::Rectangle<float> (w1 * 1.25f, w1 * 1.25f).withCentre (tip);
    juce::ColourGradient ball (juce::Colours::white, tipBall.getX() + tipBall.getWidth() * 0.3f, tipBall.getY() + tipBall.getHeight() * 0.3f,
                               juce::Colour (0xff6a6a6a), tipBall.getRight(), tipBall.getBottom(), true);
    g.setGradientFill (ball); g.fillEllipse (tipBall);
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.drawEllipse (tipBall, 0.7f);
}

// =============================================================================
//  CompStrip  (one channel of the variable-mu compressor)
// =============================================================================
CompStrip::CompStrip (juce::AudioProcessorValueTreeState& s, int channel)
    : ch (channel),
      meter    (AnalogMeter::Kind::GainReduction, "", ""),
      thresh   (s, params::chanId (channel, "c_thresh"),   "THRESHOLD", "0", "-30"),
      attack   (s, params::chanId (channel, "c_attack"),   "ATTACK",    "2", "70"),
      recovery (s, params::chanId (channel, "c_recovery"), "RECOVERY",  ".2", "8s"),
      output   (s, params::chanId (channel, "c_output"),   "OUTPUT",    "0", "+12"),
      mix      (s, params::chanId (channel, "c_mix"),      "MIX",       "DRY", "WET"),
      mode     (s, params::chanId (channel, "c_mode"), "LIMIT", "COMPRESS", 1.0f, 0.0f),
      scHp     (s, params::chanId (channel, "c_schp"), "HP SC IN", "OFF", 1.0f, 0.0f),
      compIn   (s, params::chanId (channel, "c_in"),   "IN", "BYPASS", 1.0f, 0.0f)
{
    for (auto* c : std::initializer_list<juce::Component*> { &meter, &thresh, &attack, &recovery, &output, &mix, &mode, &scHp, &compIn })
        addAndMakeVisible (c);

    meter.setTooltip (tips::grMeter);
    thresh.setTooltip (tips::cThresh);   attack.setTooltip (tips::cAttack);   recovery.setTooltip (tips::cRecovery);
    output.setTooltip (tips::cOutput);   mix.setTooltip (tips::cMix);
    mode.setTooltip (tips::cMode);       scHp.setTooltip (tips::cScHp);       compIn.setTooltip (tips::cIn);
}

void CompStrip::paint (juce::Graphics& g)
{
    // black inset panel behind the knobs, like the hardware
    {
        auto k = knobPanel.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (k.translated (0, 2.0f), 6.0f);
        juce::ColourGradient blk (juce::Colour (0xff1b1b1d), k.getX(), k.getY(), juce::Colour (0xff0b0b0c), k.getX(), k.getBottom(), false);
        g.setGradientFill (blk);
        g.fillRoundedRectangle (k, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (k.reduced (0.5f), 6.0f, 1.0f);
        for (auto pt : { k.getTopLeft() + juce::Point<float> (9, 9), k.getTopRight() + juce::Point<float> (-9, 9),
                         k.getBottomLeft() + juce::Point<float> (9, -9), k.getBottomRight() + juce::Point<float> (-9, -9) })
            draw::screw (g, pt, 7.0f, 0.4f);
    }
    const auto c = ch == 0 ? col::amber : (isEnabled() ? col::blue : col::engraveDim);
    draw::engraved (g, ch == 0 ? "LEFT CHANNEL" : (isEnabled() ? "RIGHT CHANNEL" : "RIGHT  (LINKED)"),
                    meter.getBounds().toFloat().withHeight (14.0f).translated (0, -17.0f), 11.0f, juce::Justification::centred, c);
}

void CompStrip::resized()
{
    auto r = getLocalBounds();
    auto left = r.removeFromLeft (196);
    left.removeFromTop (18);
    meter.setBounds (left.withSizeKeepingCentre (left.getWidth(), juce::jmin (left.getHeight(), 142)).withY (left.getY()));
    r.removeFromLeft (10);

    auto switches = r.removeFromBottom (58);
    r.removeFromBottom (6);
    knobPanel = r.expanded (6, 4).withTrimmedRight (0);
    r = r.reduced (8, 4);
    // THRESHOLD and OUTPUT are the big knobs; ATTACK, RECOVERY and MIX sit between, a step smaller
    const int total = r.getWidth();
    const int big = total * 27 / 100, small = (total - 2 * big) / 3;
    thresh.setBounds   (r.removeFromLeft (big));
    attack.setBounds   (r.removeFromLeft (small).withTrimmedTop (30));
    recovery.setBounds (r.removeFromLeft (small).withTrimmedTop (30));
    output.setBounds   (r.removeFromLeft (big));
    mix.setBounds      (r.withTrimmedTop (30));

    // three metal toggles: LIMIT/COMPRESS, HP SC IN, IN/BYPASS
    switches = switches.withSizeKeepingCentre (juce::jmin (switches.getWidth(), 420), switches.getHeight());
    const int sw = switches.getWidth() / 3;
    mode.setBounds   (switches.removeFromLeft (sw));
    scHp.setBounds   (switches.removeFromLeft (sw));
    compIn.setBounds (switches);
}

// =============================================================================
//  AutoMasterPanel  (AUTO MASTER + MAXIMIZER)
// =============================================================================
namespace am = ppm::automaster;

AutoMasterPanel::AutoMasterPanel (PassiveMasterEQProcessor& p)
    : proc (p),
      amount  (p.apvts, ppm::params::amAmount,   "AMOUNT",  "0", "100"),
      maxGain (p.apvts, ppm::params::maxGain,    "GAIN",    "-12", "+18"),
      ceiling (p.apvts, ppm::params::maxCeiling, "CEILING", "-3", "0")
{
    for (int g = 0; g < am::kNumGenres; ++g)
    {
        const juce::String name = am::genre (g).name;
        genreButtons[g] = std::make_unique<JewelButton> (name, name, col::amber);
        genreButtons[g]->setClickingTogglesState (false);
        genreButtons[g]->onClick = [this, g] { selectGenre (g); };
        genreButtons[g]->setTooltip (juce::String ("GENRE: ") + name + " - " + am::genre (g).character + ". Target "
                                     + juce::String (am::genre (g).lufs, 0) + " LUFS. Click to choose; if a song has been "
                                     "heard already, the master is re-planned for this genre straight away.");
        addAndMakeVisible (*genreButtons[g]);
    }
    amount.slider.textFromValueFunction = [] (double v) { return juce::String ((int) std::lround (v)) + " %"; };
    maxGain.slider.textFromValueFunction = [] (double v) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; };
    ceiling.slider.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " dB"; };
    amount.refresh(); maxGain.refresh(); ceiling.refresh();

    amount.setTooltip ("AMOUNT - how far the EQ moves your mix toward the genre's tonal balance. 50-70 % is a natural "
                       "starting point; 100 % matches the target curve as closely as the EQ allows (max +/-6 dB per band).");
    maxGain.setTooltip ("MAXIMIZER GAIN - how hard the music is pushed into the final limiter. More gain = louder, but past "
                        "about 4-6 dB of limiting the punch starts to go. AUTO MASTER sets this to hit the genre's loudness.");
    ceiling.setTooltip ("CEILING - the highest peak allowed out of the plug-in (-3 to 0 dBFS). -1.0 dB is the safe choice "
                        "for streaming (Spotify, Apple Music, YouTube) so the files don't distort when converted to MP3/AAC.");
    maxOn.setTooltip ("MAXIMIZER - final look-ahead brickwall limiter after the EQ, tubes and compressor. Nothing ever gets "
                      "above CEILING. Off = it passes the sound untouched.");
    listen.setTooltip ("LISTEN - press, then play your song (ideally the fullest part, such as a chorus). After 30 seconds of "
                       "music, AUTO MASTER sets the EQ, tubes, compressor and maximizer for the chosen genre. Press again to stop.");
    undo.setTooltip ("UNDO - go back to your settings from before AUTO MASTER (good for a before/after comparison).");

    maxOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ppm::params::maxOn, maxOn);
    listen.onClick = [this]
    {
        if (proc.getAutoState() == PassiveMasterEQProcessor::AutoState::Listening) proc.autoCancel();
        else proc.autoListen();
    };
    undo.onClick = [this] { proc.autoUndo(); };
    for (auto* c : std::initializer_list<juce::Component*> { &amount, &maxGain, &ceiling, &listen, &undo, &maxOn })
        addAndMakeVisible (c);
    lastAmount = (int) std::lround (proc.apvts.getRawParameterValue (ppm::params::amAmount)->load());
}

void AutoMasterPanel::selectGenre (int g)
{
    if (auto* prm = proc.apvts.getParameter (ppm::params::amGenre))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) g));
        prm->endChangeGesture();
    }
    const auto st = proc.getAutoState();
    if (proc.autoHasRecording() && st != PassiveMasterEQProcessor::AutoState::Listening)
        proc.autoReapply();
    repaint();
}

void AutoMasterPanel::update (float dt)
{
    using S = PassiveMasterEQProcessor::AutoState;
    const auto st = proc.getAutoState();
    const int genreNow = (int) std::lround (proc.apvts.getRawParameterValue (ppm::params::amGenre)->load());
    for (int g = 0; g < am::kNumGenres; ++g) genreButtons[g]->setToggleState (g == genreNow, juce::dontSendNotification);

    listen.setButtonText (st == S::Listening ? "STOP" : (proc.autoHasRecording() ? "LISTEN AGAIN" : "LISTEN"));
    listen.setEnabled (st != S::Analysing);
    undo.setEnabled (proc.autoCanUndo());

    // AMOUNT: re-plan shortly after it stops moving
    const int amt = (int) std::lround (proc.apvts.getRawParameterValue (ppm::params::amAmount)->load());
    if (amt != lastAmount) { lastAmount = amt; amountTimer = 0.4f; }
    else if (amountTimer > 0.0f)
    {
        amountTimer -= dt;
        if (amountTimer <= 0.0f && proc.autoHasRecording() && st != S::Listening && st != S::Analysing && ! amount.slider.isMouseButtonDown())
            proc.autoReapply();
    }

    // maximizer gain-reduction meter
    const float g = proc.takeMaximizerGr();
    gr = g > gr ? g : gr + (g - gr) * juce::jmin (1.0f, dt * 6.0f);
    if (g >= grHold) { grHold = g; grHoldTimer = 1.5f; }
    else if ((grHoldTimer -= dt) <= 0.0f) grHold = juce::jmax (0.0f, grHold - dt * 6.0f);
    pulse += dt;

    lastState = st;
    lastGenre = genreNow;
    maxGain.refresh();
    repaint();
}

void AutoMasterPanel::resized()
{
    auto r = getLocalBounds().reduced (24, 8);
    r.removeFromTop (24);                                       // titles
    auto left = r.removeFromLeft (560);
    r.removeFromLeft (16);
    auto right = r.removeFromRight (300);
    r.removeFromRight (16);
    display = r.toFloat();

    auto genres = left.removeFromTop (32);
    genreArea = genres.toFloat();
    const int gw = genres.getWidth() / am::kNumGenres;
    for (int g = 0; g < am::kNumGenres; ++g)
        genreButtons[g]->setBounds (genres.removeFromLeft (gw).reduced (2, 0));
    left.removeFromTop (6);
    amount.setBounds (left.removeFromLeft (118));
    left.removeFromLeft (14);
    auto btns = left.removeFromLeft (170);
    listen.setBounds (btns.removeFromTop (btns.getHeight() / 2).withSizeKeepingCentre (164, 44).translated (0, 6));
    undo.setBounds (btns.withSizeKeepingCentre (120, 30));

    maxArea = right.toFloat();
    auto mr = right;
    grMeter = mr.removeFromRight (30).reduced (4, 2).toFloat();
    mr.removeFromRight (8);
    maxOn.setBounds (mr.removeFromTop (30).withSizeKeepingCentre (150, 30));
    mr.removeFromTop (4);
    maxGain.setBounds (mr.removeFromLeft (mr.getWidth() * 55 / 100));
    ceiling.setBounds (mr.withTrimmedTop (22));
}

juce::String AutoMasterPanel::getTooltip()
{
    const auto m = getMouseXYRelative().toFloat();
    if (grMeter.expanded (4).contains (m))
        return "LIMITING - how many dB the maximizer is turning peaks down right now. Up to 2-3 dB is clean; "
               "constantly above 5-6 dB starts to flatten the punch.";
    if (display.contains (m))
        return "AUTO MASTER DISPLAY - steel blue: the tonal balance of your mix (octave bands, relative to the mids). White dashes: "
               "the genre's target. Aqua: your mix after the EQ AUTO MASTER chose. Underneath: the moves it made, and the "
               "loudness before -> after (LUFS).";
    return "AUTO MASTER - choose a genre, press LISTEN and play the song. It measures your mix's tonal balance and loudness, "
           "then sets the EQ, tubes, compressor and MAXIMIZER for that style. Everything it sets stays adjustable.";
}

void AutoMasterPanel::paint (juce::Graphics& g)
{
    using S = PassiveMasterEQProcessor::AutoState;
    auto b = getLocalBounds().toFloat().reduced (24.0f, 8.0f);
    auto titles = b.removeFromTop (22.0f);
    draw::engraved (g, "AUTO MASTER", titles.withTrimmedLeft (100.0f).withWidth (460.0f), 14.0f, juce::Justification::centredLeft);
    draw::engraved (g, "GENRE  >  LISTEN  >  DONE", titles.withX (titles.getX() + 150.0f).withWidth (410.0f), 10.0f,
                    juce::Justification::centredRight, col::engraveDim.brighter (0.4f));
    draw::engraved (g, "MAXIMIZER", { maxArea.getX(), titles.getY(), maxArea.getWidth(), 22.0f }, 14.0f, juce::Justification::centred);

    // limiter GR meter (falls downward from the top)
    {
        auto m = grMeter;
        g.setColour (juce::Colours::black); g.fillRoundedRectangle (m.expanded (2.0f), 3.0f);
        auto inner = m.reduced (2.0f);
        const float maxDb = 12.0f;
        const int segs = 24;
        const float sh = inner.getHeight() / (float) segs;
        for (int i = 0; i < segs; ++i)
        {
            const float db = maxDb * (float) (i + 1) / (float) segs;
            const bool lit = gr >= db - maxDb / segs * 0.5f;
            const auto c = db > 6.0f ? col::red : (db > 3.0f ? col::amber : col::yellow);
            g.setColour (lit ? c : c.withAlpha (0.10f));
            g.fillRect (inner.getX(), inner.getY() + sh * (float) i + 0.5f, inner.getWidth(), sh - 1.0f);
        }
        if (grHold > 0.1f)
        {
            g.setColour (col::cream);
            g.fillRect (inner.getX(), inner.getY() + inner.getHeight() * juce::jmin (1.0f, grHold / maxDb) - 1.0f, inner.getWidth(), 2.0f);
        }
        draw::engraved (g, "GR", m.withY (m.getBottom() + 1.0f).withHeight (11.0f).expanded (6.0f, 0), 9.0f);
        draw::engraved (g, juce::String (gr, 1), m.withY (m.getY() - 13.0f).withHeight (11.0f).expanded (8.0f, 0), 9.0f,
                        juce::Justification::centred, col::yellow.withAlpha (0.9f));
    }

    // ---- cream display ---------------------------------------------------------
    auto d = display;
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (d.expanded (3.0f), 5.0f);
    juce::ColourGradient paper (col::lcdTop, d.getX(), d.getY(), col::lcdBottom, d.getX(), d.getBottom(), false);
    g.setGradientFill (paper);
    g.fillRoundedRectangle (d, 4.0f);
    g.setColour (col::aqua.withAlpha (0.25f));
    g.drawRoundedRectangle (d.reduced (1.5f), 4.0f, 1.0f);
    auto inner = d.reduced (10.0f, 6.0f);

    const auto st = proc.getAutoState();
    const auto plan = proc.getAutoPlan();
    const int genreNow = (int) std::lround (proc.apvts.getRawParameterValue (ppm::params::amGenre)->load());
    const auto& tg = am::genre (genreNow);
    const bool showPlan = plan.ok && plan.genre == genreNow && st != S::Listening;

    // status line
    auto status = inner.removeFromTop (16.0f);
    g.setFont (sans (11.5f));
    juce::String msg = proc.getAutoMessage();
    juce::Colour msgCol = juce::Colours::white;
    if (st == S::Idle && msg.isEmpty())
        msg = "Choose a GENRE, press LISTEN, then play your song.";
    if (st == S::Listening)
        msg = "LISTENING...  " + juce::String (proc.autoListenSecondsDone(), 1) + " / " + juce::String (proc.listenSeconds, 0)
            + " s of music  (play the fullest part of the song)";
    if (st == S::Analysing) { msg = "ANALYSING your mix for " + juce::String (tg.name) + "..."; msgCol = col::yellow; }
    if (st == S::Verifying)
    {
        const float v = (float) proc.getVerifyLufs();
        msg = "APPLIED for " + juce::String (tg.name) + "  -  checking loudness while it plays"
            + (v > -70.0f ? ":  " + juce::String (v, 1) + " LUFS" : juce::String ("..."));
        msgCol = col::aqua;
    }
    if (st == S::Done) msgCol = col::aqua;
    if (st == S::Failed) msgCol = juce::Colour (0xffff6a5a);
    g.setColour (msgCol);
    g.drawFittedText (msg, status.toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);

    if (st == S::Listening || st == S::Analysing)
    {
        auto bar = inner.removeFromTop (8.0f).withTrimmedTop (2.0f);
        g.setColour (col::aqua.withAlpha (0.15f)); g.fillRoundedRectangle (bar, 2.0f);
        const float frac = st == S::Analysing ? 0.5f + 0.5f * std::sin (pulse * 6.0f) : (float) proc.autoListenProgress();
        g.setColour (col::aqua);
        if (st == S::Analysing) g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * 0.25f).withX (bar.getX() + bar.getWidth() * 0.75f * frac), 2.0f);
        else g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * frac), 2.0f);
    }
    else inner.removeFromTop (8.0f);

    // text lines at the bottom
    auto lines = inner.removeFromBottom (32.0f);
    g.setFont (sans (10.5f, false));
    g.setColour (col::aquaDim);
    auto sgn = [] (double v) { return (v >= 0 ? "+" : "") + juce::String (v, 1); };
    if (showPlan)
    {
        g.drawFittedText ("EQ:  " + plan.eqText, lines.removeFromTop (16.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
        g.drawFittedText ("INPUT " + sgn (plan.inputGainDb) + " dB  |  COMP " + juce::String (ppm::compThresholdDb (plan.comp.threshold), 1)
                          + " dB (" + juce::String (plan.compGrDb, 1) + " GR)  |  MAX " + sgn (plan.maxGain) + " dB  |  "
                          + juce::String (plan.inputLufs, 1) + " -> " + juce::String (plan.predictedLufs, 1) + " LUFS",
                          lines.toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
    }
    else
    {
        g.drawFittedText (juce::String (tg.name) + ":  " + tg.character, lines.removeFromTop (16.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
        g.drawFittedText ("Target " + juce::String (tg.lufs, 0) + " LUFS  |  ceiling " + juce::String (tg.ceiling, 1)
                          + " dBFS  |  tube drive " + sgn (tg.drive) + " dB",
                          lines.toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
    }

    // ---- chart: octave bands ---------------------------------------------------
    chart = inner.reduced (0.0f, 2.0f);
    auto legendRow = chart.removeFromTop (12.0f);
    auto c = chart.withTrimmedLeft (26.0f).withTrimmedBottom (12.0f);
    const float range = 15.0f;
    auto yOf = [&] (double db) { return c.getCentreY() - (float) juce::jlimit (-1.0, 1.0, db / range) * c.getHeight() * 0.5f; };
    auto xOf = [&] (int band) { return c.getX() + c.getWidth() * ((float) band + 0.5f) / (float) am::kOct; };
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRect (c);
    g.setFont (sans (8.5f, false));
    for (double db : { -10.0, 0.0, 10.0 })
    {
        g.setColour (col::aqua.withAlpha (std::abs (db) < 0.1 ? 0.40f : 0.16f));
        g.drawHorizontalLine ((int) yOf (db), c.getX(), c.getRight());
        g.setColour (col::aquaDim.withAlpha (0.8f));
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (chart.getX(), yOf (db) - 6.0f, 22.0f, 12.0f),
                    juce::Justification::centredRight);
    }
    static const char* labels[am::kOct] = { "31", "63", "125", "250", "500", "1k", "2k", "4k", "8k", "16k" };
    for (int i = 0; i < am::kOct; ++i)
        g.drawText (labels[i], juce::Rectangle<float> (xOf (i) - 16.0f, c.getBottom() + 1.0f, 32.0f, 11.0f), juce::Justification::centred);

    auto drawCurve = [&] (const double* v, juce::Colour colr, float thick, bool dashed, bool dots)
    {
        juce::Path pth;
        for (int i = 0; i < am::kOct; ++i)
        {
            const juce::Point<float> pt (xOf (i), yOf (v[i]));
            if (i == 0) pth.startNewSubPath (pt); else pth.lineTo (pt);
        }
        g.setColour (colr);
        if (dashed)
        {
            juce::Path dash; const float pat[] = { 6.0f, 4.0f };
            juce::PathStrokeType (thick).createDashedStroke (dash, pth, pat, 2);
            g.fillPath (dash);
        }
        else g.strokePath (pth, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (dots) for (int i = 0; i < am::kOct; ++i) g.fillEllipse (xOf (i) - 2.5f, yOf (v[i]) - 2.5f, 5.0f, 5.0f);
    };

    double target[am::kOct];
    for (int i = 0; i < am::kOct; ++i) target[i] = tg.curve[i] - am::midRef (tg.curve);
    if (showPlan)
    {
        double after[am::kOct];
        for (int i = 0; i < am::kOct; ++i) after[i] = plan.corr.measuredRel[i] + plan.fitted[i];
        drawCurve (plan.corr.measuredRel, juce::Colour (0xff6f93c0), 2.0f, false, true);
        drawCurve (target, juce::Colours::white.withAlpha (0.85f), 1.8f, true, false);
        drawCurve (after, col::aqua, 2.4f, false, true);
    }
    else drawCurve (target, juce::Colours::white.withAlpha (0.85f), 1.8f, true, false);

    // legend
    auto lg = legendRow.withTrimmedLeft (26.0f);
    g.setFont (sans (9.0f));
    auto key = [&] (juce::Colour colr, const char* text)
    {
        g.setColour (colr); g.fillRect (lg.getX(), lg.getCentreY() - 1.0f, 12.0f, 2.5f);
        g.setColour (col::aquaDim); g.drawText (text, lg.withTrimmedLeft (15.0f), juce::Justification::centredLeft);
        lg.removeFromLeft (15.0f + (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 12.0f);
    };
    if (showPlan) key (juce::Colour (0xff6f93c0), "YOUR MIX");
    key (juce::Colours::white.withAlpha (0.85f), (juce::String (tg.name) + " TARGET").toRawUTF8());
    if (showPlan) key (col::aqua, "AFTER EQ");
}

// =============================================================================
//  M/S CLASS-A compressor (ash-blue unit)
// =============================================================================
namespace tips
{
    static const juce::String aThresh = "THRESHOLD - where compression starts (33 dB range, +2 to -31 dBFS). In M/S mode the left knobs work on the MID "
        "(centre), the right ones on the SIDE (width).";
    static const juce::String aAttack = "ATTACK - 0.01 to 150 ms: the time it takes to build up 10 dB of gain reduction. In FEED FWD mode it is about twice as long.";
    static const juce::String aRelease = "RELEASE - 60 ms to 1.8 s: the time to come back from 6 dB of gain reduction. The release is linear, and the attack "
        "time adds to it.";
    static const juce::String aRatio = "RATIO - FEEDBACK: 1:1.1 to 1:2.5 (the ratio at 6 dB of reduction; it rises the harder you push, ~1:4.5 at 17 dB) - smooth "
        "and even. FEED FWD: the ratios double, and the top of the scale (X) goes through limiting into NEGATIVE ratios, where louder parts come out quieter.";
    static const juce::String aGain = "GAIN - make-up gain, 0 to +12 dB. In M/S mode, use the two GAIN knobs to set the balance of centre and width.";
    static const juce::String aMix = "MIX - from DIR (direct, untouched) to COM (compressed): parallel compression for this channel.";
    static const juce::String aFF = "FEEDBACK / FEED FWD - where the detector listens. FEEDBACK (after the compressor): smooth, even, soft knee. "
        "FEED FWD (before it): clearly harder and more aggressive, higher ratios up to limiting and negative ratios, double attack times.";
    static const juce::String aAfa = "AUTO FAST (attack) - shortens the attack on fast, loud peaks once there is about 3 dB of gain reduction, then goes back to your setting.";
    static const juce::String aAfr = "AUTO FAST (release) - after a short burst the release drops to about 20 ms and then glides back to your setting; long passages keep the set release.";
    static const juce::String aWarm = "WARM - a slew-rate limiter: subtly rounds the fastest transients and the top end, for a less clinical sound.";
    static const juce::String aScGain = "SC GAIN - sidechain filter (6 dB/oct): turn left (LP) and the compressor reacts less to highs, right (HP) and it reacts "
        "less to lows (up to -12 dB). Only the detector is filtered, not the sound. Switch on with SC ON.";
    static const juce::String aScFreq = "SC FREQ - the sidechain filter's frequency, 30 Hz to 3.3 kHz. E.g. HP at ~70 Hz so the kick doesn't pump the mix, ~2 kHz for de-essing.";
    static const juce::String aEqGain = "NIVEAU FILTER - a tilt EQ after the compressor: right = brighter (highs up to +3 dB, lows down to -5 dB), left = darker "
        "(lows up to +3 dB, highs down to -4.5 dB). Switch on with EQ ON.";
    static const juce::String aEqFreq = "NIVEAU FREQ - the tilt's pivot, 20 Hz to 2 kHz; x10 = 200 Hz to 20 kHz.";
    static const juce::String aMode = "STEREO / M/S - M/S: the left channel controls work on the MID (centre: vocal, kick, bass), the right ones on the SIDE "
        "(stereo width, reverb). Unlinked STEREO = two independent channels.";
    static const juce::String aLink = "LINK - one control voltage for both channels: the left compressor controls (threshold, attack, release, ratio, feed fwd, "
        "auto fast) drive both. Filters, gain, mix and warm stay separate.";
    static const juce::String aClip = "SOFT CLIP - saturation-style peak limiter after the M/S decoder: rounds peaks off (like tape) instead of hard clipping, "
        "and never goes above its threshold. The LED should only flash briefly; keep it to a few dB.";
    static const juce::String aIn = "ACTIVE - switches the whole unit in or out (click-free).";
    static const juce::String aMeter = "GAIN REDUCTION - how many dB this channel (or MID / SIDE) is being turned down right now.";
}

// readouts
static juce::String fmtFreq (double hz) { return hz >= 1000.0 ? juce::String (hz / 1000.0, hz >= 10000.0 ? 1 : 2) + " kHz" : juce::String ((int) std::lround (hz)) + " Hz"; }
static juce::String fmtTilt (double t, const char* left, const char* right)
{
    if (std::abs (t) < 0.02) return "FLAT";
    return juce::String (t < 0 ? left : right) + " " + juce::String ((int) std::lround (std::abs (t) * 100)) + "%";
}

ClassAStrip::ClassAStrip (juce::AudioProcessorValueTreeState& s, int channel)
    : state (s), ch (channel),
      meter   (AnalogMeter::Kind::GainReduction, "", ""),
      thresh  (s, params::ca::chan (channel, "thr"),   "THRESHOLD", "+2", "-31"),
      attack  (s, params::ca::chan (channel, "att"),   "ATTACK",    ".01", "150"),
      release (s, params::ca::chan (channel, "rel"),   "RELEASE",   "60", "1.8s"),
      ratio   (s, params::ca::chan (channel, "ratio"), "RATIO",     "1.1", "X"),
      gain    (s, params::ca::chan (channel, "gain"),  "GAIN",      "0", "+12"),
      scGain  (s, params::ca::chan (channel, "scg"),   "SC GAIN",   "LP", "HP"),
      scFreq  (s, params::ca::chan (channel, "scf"),   "SC FREQ",   "30", "3k3"),
      eqGain  (s, params::ca::chan (channel, "eqg"),   "EQ GAIN",   "LO", "HI"),
      eqFreq  (s, params::ca::chan (channel, "eqf"),   "EQ FREQ",   "20", "2k"),
      mix     (s, params::ca::chan (channel, "mix"),   "MIX",       "DIR", "COM")
{
    thresh.slider.textFromValueFunction  = [] (double v) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; };
    attack.slider.textFromValueFunction  = [] (double v) { return v < 1.0 ? juce::String (v, 2) + " ms" : (v < 10 ? juce::String (v, 1) : juce::String ((int) std::lround (v))) + " ms"; };
    release.slider.textFromValueFunction = [] (double v) { return v >= 1000 ? juce::String (v / 1000.0, 2) + " s" : juce::String ((int) std::lround (v)) + " ms"; };
    gain.slider.textFromValueFunction    = [] (double v) { return "+" + juce::String (v, 1) + " dB"; };
    mix.slider.textFromValueFunction     = [] (double v) { return v < 0.5 ? juce::String ("DIR") : (v > 99.5 ? juce::String ("COM") : juce::String ((int) std::lround (v)) + " %"); };
    scGain.slider.textFromValueFunction  = [] (double v) { return fmtTilt (v, "LP", "HP"); };
    scFreq.slider.textFromValueFunction  = [] (double v) { return fmtFreq (v); };
    eqGain.slider.textFromValueFunction  = [] (double v) { return fmtTilt (v, "DARK", "BRIGHT"); };
    const juce::String ffId = params::ca::chan (channel, "ff"), x10Id = params::ca::chan (channel, "eqx");
    ratio.valueText = [this, ffId]
    {
        const double pos = ratio.slider.getValue();
        if (state.getRawParameterValue (ffId)->load() > 0.5f)
        {
            const double sl = ppm::classAFeedForwardSlope (pos);
            if (sl >= 0.995 && sl <= 1.005) return juce::String ("LIMIT");
            if (sl > 1.0) return juce::String ("NEG ") + juce::String (-1.0 / (sl - 1.0), 1);
            return juce::String ("1:") + juce::String (1.0 / (1.0 - sl), 1);
        }
        return juce::String ("1:") + juce::String (ppm::classAPrintedRatio (pos), 2);
    };
    eqFreq.valueText = [this, x10Id] { return fmtFreq (eqFreq.slider.getValue() * (state.getRawParameterValue (x10Id)->load() > 0.5f ? 10.0 : 1.0)); };

    for (auto* k : { &thresh, &attack, &release, &ratio, &gain, &scGain, &scFreq, &eqGain, &eqFreq, &mix })
    {
        k->captionColour = juce::Colours::white; k->endLabelColour = juce::Colours::white.withAlpha (0.8f);
        k->refresh();
    }
    meter.subtitle = "dB  -  CLASS-A";
    thresh.setTooltip (tips::aThresh); attack.setTooltip (tips::aAttack); release.setTooltip (tips::aRelease);
    ratio.setTooltip (tips::aRatio);   gain.setTooltip (tips::aGain);     mix.setTooltip (tips::aMix);
    scGain.setTooltip (tips::aScGain); scFreq.setTooltip (tips::aScFreq); eqGain.setTooltip (tips::aEqGain); eqFreq.setTooltip (tips::aEqFreq);
    ff.setTooltip (tips::aFF); afa.setTooltip (tips::aAfa); afr.setTooltip (tips::aAfr); warm.setTooltip (tips::aWarm);
    scOn.setTooltip (tips::aScGain); eqOn.setTooltip (tips::aEqGain); eqX10.setTooltip (tips::aEqFreq); meter.setTooltip (tips::aMeter);

    auto attach = [&] (const char* id, juce::Button& b)
    { atts.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, params::ca::chan (channel, id), b)); };
    attach ("ff", ff); attach ("afa", afa); attach ("afr", afr); attach ("warm", warm);
    attach ("scon", scOn); attach ("eqon", eqOn); attach ("eqx", eqX10);
    ff.onClick = [this] { ratio.refresh(); };
    eqX10.onClick = [this] { eqFreq.refresh(); };
    for (auto* c : std::initializer_list<juce::Component*> { &meter, &thresh, &attack, &release, &ratio, &gain, &scGain, &scFreq, &eqGain, &eqFreq, &mix,
                                                             &ff, &afa, &afr, &warm, &scOn, &eqOn, &eqX10 })
        addAndMakeVisible (c);
}

void ClassAStrip::setCompressorEnabled (bool e)
{
    for (auto* c : std::initializer_list<juce::Component*> { &thresh, &attack, &release, &ratio, &ff, &afa, &afr })
        c->setEnabled (e);
}

void ClassAStrip::paint (juce::Graphics& g)
{
    draw::engraved (g, title, meter.getBounds().toFloat().withHeight (14.0f).translated (0, -17.0f), 11.0f, juce::Justification::centred, titleColour);
    // section legends on the lower row
    auto legend = [&] (juce::Rectangle<int> r, const char* t)
    { draw::engraved (g, t, r.toFloat().withHeight (12.0f).translated (0, -13.0f), 9.5f, juce::Justification::centred, juce::Colours::white.withAlpha (0.75f)); };
    legend (scGain.getBounds().getUnion (scFreq.getBounds()).getUnion (scOn.getBounds()), "SIDECHAIN FILTER");
    legend (eqGain.getBounds().getUnion (eqFreq.getBounds()).getUnion (eqX10.getBounds()), "NIVEAU FILTER");
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.drawHorizontalLine (filterRow.getY() - 16, (float) filterRow.getX(), (float) filterRow.getRight());
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawHorizontalLine (filterRow.getY() - 15, (float) filterRow.getX(), (float) filterRow.getRight());
}

void ClassAStrip::resized()
{
    auto r = getLocalBounds();
    buttonRow = r.removeFromBottom (30);
    r.removeFromBottom (8);
    filterRow = r.removeFromBottom (112);
    r.removeFromBottom (22);

    // row 1: meter + compressor (THRESHOLD and GAIN are the big knobs)
    auto left = r.removeFromLeft (180);
    left.removeFromTop (18);
    meter.setBounds (left.withHeight (juce::jmin (left.getHeight(), 132)));
    r.removeFromLeft (8);
    const int total = r.getWidth(), big = total * 24 / 100, small = (total - 2 * big) / 3;
    thresh.setBounds  (r.removeFromLeft (big));
    attack.setBounds  (r.removeFromLeft (small).withTrimmedTop (26));
    release.setBounds (r.removeFromLeft (small).withTrimmedTop (26));
    ratio.setBounds   (r.removeFromLeft (small).withTrimmedTop (26));
    gain.setBounds    (r);

    // row 2: sidechain filter | Niveau filter | mix
    auto f = filterRow;
    const int kw = juce::jmin (96, (f.getWidth() - 2 * 70 - 40) / 5);
    auto btnCol = [] (juce::Rectangle<int> col, std::initializer_list<juce::Component*> bs)
    {
        const int h = juce::jmin (28, col.getHeight() / (int) bs.size() - 6);
        auto c = col.withSizeKeepingCentre (col.getWidth(), (h + 6) * (int) bs.size());
        for (auto* b : bs) b->setBounds (c.removeFromTop (h + 6).withSizeKeepingCentre (col.getWidth(), h));
    };
    scGain.setBounds (f.removeFromLeft (kw)); scFreq.setBounds (f.removeFromLeft (kw));
    btnCol (f.removeFromLeft (70), { &scOn });
    f.removeFromLeft (20);
    eqGain.setBounds (f.removeFromLeft (kw)); eqFreq.setBounds (f.removeFromLeft (kw));
    btnCol (f.removeFromLeft (70), { &eqOn, &eqX10 });
    f.removeFromLeft (20);
    mix.setBounds (f.removeFromLeft (kw));

    // row 3: switches
    auto b = buttonRow;
    const int bw = (b.getWidth() - 3 * 10) / 4;
    ff.setBounds (b.removeFromLeft (bw)); b.removeFromLeft (10);
    afa.setBounds (b.removeFromLeft (bw)); b.removeFromLeft (10);
    afr.setBounds (b.removeFromLeft (bw)); b.removeFromLeft (10);
    warm.setBounds (b);
}

ClassAPanel::ClassAPanel (PassiveMasterEQProcessor& p)
    : proc (p), strips { { p.apvts, 0 }, { p.apvts, 1 } },
      mode (p.apvts, params::ca::mode, { "STEREO", "M / S" }, { col::amber, col::blue }),
      clip (p.apvts, params::ca::clip, "CLIP THRESH", "-12", "0")
{
    clip.slider.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " dB"; };
    clip.captionColour = juce::Colours::white; clip.endLabelColour = juce::Colours::white.withAlpha (0.8f);
    clip.refresh();
    inAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, params::ca::in, inButton);
    linkAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, params::ca::link, link);
    clipAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, params::ca::clipOn, clipOn);
    mode.setTooltip (tips::aMode); link.setTooltip (tips::aLink); inButton.setTooltip (tips::aIn);
    clipOn.setTooltip (tips::aClip); clip.setTooltip (tips::aClip); clipLed.setTooltip (tips::aClip);
    for (auto* c : std::initializer_list<juce::Component*> { &strips[0], &strips[1], &mode, &clip, &inButton, &link, &clipOn, &clipLed })
        addAndMakeVisible (c);
}

void ClassAPanel::resized()
{
    auto r = getLocalBounds().reduced (22, 8);
    auto head = r.removeFromTop (26);
    mode.setBounds (head.removeFromRight (180).withSizeKeepingCentre (180, 26));
    r.removeFromTop (6);
    centre = r.withSizeKeepingCentre (150, r.getHeight());
    const int sideW = (r.getWidth() - centre.getWidth() - 36) / 2;
    strips[0].setBounds (r.withWidth (sideW));
    strips[1].setBounds (r.withX (r.getRight() - sideW).withWidth (sideW));
    auto c = centre.reduced (6, 0);
    inButton.setBounds (c.removeFromTop (30)); c.removeFromTop (8);
    link.setBounds (c.removeFromTop (30));     c.removeFromTop (16);
    clipLed.setBounds (c.removeFromTop (34).withSizeKeepingCentre (50, 34)); c.removeFromTop (4);
    clipOn.setBounds (c.removeFromTop (30));   c.removeFromTop (6);
    clip.setBounds (c.withHeight (juce::jmin (c.getHeight(), 130)));
}

void ClassAPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (22.0f, 8.0f);
    auto head = r.withHeight (26.0f);
    draw::engraved (g, "M/S CLASS-A COMPRESSOR", head, 14.0f, juce::Justification::centred, juce::Colours::white);
    draw::engraved (g, "FEEDBACK / FEED FWD  -  MID/SIDE  -  SOFT CLIP", head.withTrimmedLeft (106.0f).withWidth (420.0f), 9.5f, juce::Justification::centredLeft,
                    juce::Colours::white.withAlpha (0.6f));
    for (float x : { (float) centre.getX() - 10.0f, (float) centre.getRight() + 10.0f })
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawVerticalLine ((int) x, (float) centre.getY(), (float) centre.getBottom());
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawVerticalLine ((int) x + 1, (float) centre.getY(), (float) centre.getBottom());
    }
}

void ClassAPanel::update (float dt)
{
    const auto st = proc.getClassASettings();
    const bool ms = st.mode == ppm::ClassAMidSide;
    strips[0].setTitle (ms ? "MID" : "LEFT", col::amber);
    strips[1].setTitle (ms ? (st.link ? "SIDE  (LINKED)" : "SIDE") : (st.link ? "RIGHT  (LINKED)" : "RIGHT"), st.link ? col::engraveDim : col::blue);
    strips[1].setCompressorEnabled (! st.link);
    auto& m = proc.meters;
    for (int c = 0; c < 2; ++c)
    {
        const int src = (c == 1 && m.numChannels.load() < 2) ? 0 : c;
        strips[c].setMeter (m.grA[src].load(), st.in);
        strips[c].tickMeter (dt);
        strips[c].refreshReadouts();
    }
    if (m.clipA.exchange (false)) clipHold = 6;
    clipLed.setBrightness (clipHold > 0 ? 1.0f : 0.0f);
    if (clipHold > 0) --clipHold;
}

// =============================================================================
//  Parametric mastering EQ (black unit)
// =============================================================================
SkirtKnob::SkirtKnob (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, juce::StringArray scaleLabels, int labelEvery)
    : labels (std::move (scaleLabels)), every (juce::jmax (1, labelEvery))
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (kStartAngle, kEndAngle, true);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setMouseDragSensitivity (180);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    slider.getProperties().set ("skirt", true);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, paramId, slider);
    if (auto* p = s.getParameter (paramId)) slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

void SkirtKnob::resized()
{
    const float d = (float) juce::jmin (getWidth(), getHeight()) * knobFraction;
    slider.setBounds (getLocalBounds().withSizeKeepingCentre ((int) d, (int) d).translated (0, 3));
}

void SkirtKnob::paint (juce::Graphics& g)
{
    const auto sb = slider.getBounds().toFloat();
    const auto c = sb.getCentre();
    const float rk = sb.getWidth() * 0.5f;
    const int n = juce::jmax (2, labels.size());
    const auto ink = isEnabled() ? col::silkscreen : col::silkscreen.withAlpha (0.35f);
    for (int i = 0; i < n; ++i)
    {
        const float a = kStartAngle + (kEndAngle - kStartAngle) * (float) i / (float) (n - 1);
        const bool major = labels[i].isNotEmpty() && i % every == 0;
        const float r0 = rk + 2.0f, r1 = rk + (major ? 7.0f : 4.5f);
        g.setColour (ink.withAlpha (major ? 0.9f : 0.55f));
        g.drawLine ({ { c.x + r0 * std::sin (a), c.y - r0 * std::cos (a) }, { c.x + r1 * std::sin (a), c.y - r1 * std::cos (a) } }, major ? 1.1f : 0.8f);
        if (major)
        {
            const float rl = rk + 15.0f;
            g.setColour (ink);
            g.setFont (sans (8.5f, false));
            g.drawText (labels[i], juce::Rectangle<float> (34.0f, 10.0f).withCentre ({ c.x + rl * std::sin (a), c.y - rl * std::cos (a) }),
                        juce::Justification::centred, false);
        }
    }
    // red index triangle at 12 o'clock
    juce::Path tri;
    const float ty = c.y - rk - 1.0f;
    tri.addTriangle (c.x - 4.0f, ty - 6.0f, c.x + 4.0f, ty - 6.0f, c.x, ty + 0.5f);
    g.setColour (isEnabled() ? juce::Colour (0xffe0442a) : juce::Colour (0xff6a3a30));
    g.fillPath (tri);
}

void PushEqButton::paintButton (juce::Graphics& g, bool hi, bool down)
{
    auto r = getLocalBounds().toFloat();
    const float d = juce::jmin (r.getWidth(), r.getHeight()) - 2.0f;
    auto b = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
    draw::chromeRing (g, b, 2.5f);
    const bool on = getToggleState();
    auto dome = b.reduced (d * 0.2f);
    if (on)
    {
        g.setColour (juce::Colour (0xffff3a2a).withAlpha (0.35f));
        g.fillEllipse (dome.expanded (d * 0.25f));
    }
    juce::ColourGradient red (on ? juce::Colour (0xffff8a70) : juce::Colour (0xffa83a30), dome.getX() + dome.getWidth() * 0.3f, dome.getY() + dome.getHeight() * 0.3f,
                              on ? juce::Colour (0xffc01a10) : juce::Colour (0xff4a1410), dome.getRight(), dome.getBottom(), true);
    g.setGradientFill (red); g.fillEllipse (dome);
    g.setColour (juce::Colours::white.withAlpha (down ? 0.1f : (hi ? 0.4f : 0.3f)));
    g.fillEllipse (dome.reduced (dome.getWidth() * 0.3f).translated (-dome.getWidth() * 0.12f, -dome.getHeight() * 0.15f));
}

namespace tips
{
    static const juce::String pqShape = "SHAPE (dB/oct) - how wide the band is: 5 = wide and gentle, 15 = narrow and focused. The narrower the shape, "
        "the slightly less peak gain (like the hardware's interacting circuit), which keeps narrow moves smooth.";
    static const juce::String pqFreq  = "FREQ - the band's centre frequency (stepped). LO 11-570 Hz, MID 125-6800 Hz, HI 3.4-25 kHz.";
    static const juce::String pqLevel = "LEVEL - boost or cut, +/-9 dB: 0.5 dB steps up to 3 dB, then 1 dB steps.";
    static const juce::String pqHi    = "HI SHELF - lifts or lowers everything above 8 kHz (or 12 kHz with the switch), +/-9 dB.";
    static const juce::String pqLo    = "LO SHELF - lifts or lowers everything below 100 Hz (or 50 Hz with the switch), +/-9 dB.";
    static const juce::String pqMan   = "MAN / OFF - switches this channel of the parametric EQ in (MAN) or out (OFF).";
    static const juce::String pqPush  = "PUSH EQ - switches the whole parametric EQ in (lit) or out. It sits in front of the passive EQ. Starts switched out.";
}

static juce::StringArray levelLabels()
{
    juce::StringArray l;
    for (int i = 0; i < ppm::sontec::kNumLevels; ++i)
    {
        const double v = ppm::sontec::levelDb (i);
        const bool show = std::abs (v) < 0.01 || std::abs (std::abs (v) - 3.0) < 0.01 || std::abs (std::abs (v) - 6.0) < 0.01 || std::abs (std::abs (v) - 9.0) < 0.01;
        l.add (show ? (v > 0 ? "+" : "") + juce::String ((int) v) : juce::String (" "));
    }
    return l;
}
static juce::String shortHz (double f) { return f >= 1000.0 ? juce::String (f / 1000.0, f >= 10000.0 || std::fmod (f, 1000.0) < 1.0 ? 0 : 1) + "k" : juce::String ((int) f); }

ParametricPanel::ParametricPanel (PassiveMasterEQProcessor& p) : proc (p)
{
    namespace so = ppm::sontec;
    const juce::StringArray shapes { "5", "6", "9", "11", "15" };
    for (int c = 0; c < 2; ++c)
    {
        auto& ch = chans[c];
        for (int b = 0; b < so::kBands; ++b)
        {
            juce::StringArray fl; for (double f : so::bandFreqs (b)) fl.add (shortHz (f));
            ch.shape[b] = std::make_unique<SkirtKnob> (p.apvts, params::pq::band (c, b, "shape"), shapes);
            ch.freq[b]  = std::make_unique<SkirtKnob> (p.apvts, params::pq::band (c, b, "freq"), fl, 2);
            ch.level[b] = std::make_unique<SkirtKnob> (p.apvts, params::pq::band (c, b, "level"), levelLabels());
            ch.shape[b]->knobFraction = 0.56f; ch.freq[b]->knobFraction = 0.6f; ch.level[b]->knobFraction = 0.58f;
            ch.shape[b]->setTooltip (tips::pqShape); ch.freq[b]->setTooltip (tips::pqFreq); ch.level[b]->setTooltip (tips::pqLevel);
            for (auto* k : { ch.shape[b].get(), ch.freq[b].get(), ch.level[b].get() }) addAndMakeVisible (k);
        }
        ch.hiShelf = std::make_unique<SkirtKnob> (p.apvts, params::pq::id (c, "hs"), levelLabels());
        ch.loShelf = std::make_unique<SkirtKnob> (p.apvts, params::pq::id (c, "ls"), levelLabels());
        ch.hiShelf->knobFraction = ch.loShelf->knobFraction = 0.58f;
        ch.hiShelf->setTooltip (tips::pqHi); ch.loShelf->setTooltip (tips::pqLo);
        ch.loFreq = std::make_unique<MetalToggle> (p.apvts, params::pq::id (c, "lsf"), "100 Hz", "50 Hz", 1.0f, 0.0f);
        ch.hiFreq = std::make_unique<MetalToggle> (p.apvts, params::pq::id (c, "hsf"), "12 kHz", "8 kHz", 1.0f, 0.0f);
        ch.loFreq->setTooltip (tips::pqLo); ch.hiFreq->setTooltip (tips::pqHi);
        ch.loFreq->labelColour = ch.hiFreq->labelColour = col::silkscreen;
        for (auto* k : std::initializer_list<juce::Component*> { ch.hiShelf.get(), ch.loShelf.get(), ch.loFreq.get(), ch.hiFreq.get() }) addAndMakeVisible (k);
        man[c] = std::make_unique<MetalToggle> (p.apvts, params::pq::id (c, "man"), "MAN", "OFF", 1.0f, 0.0f);
        man[c]->labelColour = col::silkscreen;
        man[c]->setTooltip (tips::pqMan);
        addAndMakeVisible (*man[c]);
    }
    pushAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, params::pq::in, pushEq);
    pushEq.setTooltip (tips::pqPush);
    ledL.setTooltip ("LEFT - lit while the left channel is being equalised."); ledR.setTooltip ("RIGHT - lit while the right channel is being equalised.");
    ledOn.setTooltip (tips::pqPush);
    for (auto* c : std::initializer_list<juce::Component*> { &pushEq, &ledL, &ledR, &ledOn }) addAndMakeVisible (c);
}

void ParametricPanel::resized()
{
    auto r = getLocalBounds().reduced (18, 10);
    r.removeFromBottom (24);                                         // engraved model line
    legendCol = r.removeFromRight (74);
    const int shelfW = 136, centreW = 150;
    const int bw = (r.getWidth() - shelfW - centreW) / 6;
    const int H = r.getHeight();
    const int shapeH = H * 27 / 100, freqH = H * 41 / 100;
    auto place = [&] (juce::Rectangle<int> col, SkirtKnob& sh, SkirtKnob& fr, SkirtKnob& lv)
    {
        sh.setBounds (col.removeFromTop (shapeH).withTrimmedBottom (14));     // band name under the shape knob
        fr.setBounds (col.removeFromTop (freqH));
        lv.setBounds (col);
    };
    auto shelfLayout = [&] (Channel& ch, juce::Rectangle<int> col, bool withTop)
    {
        ch.shelfCol = col;
        juce::ignoreUnused (withTop);
        auto c = col; c.removeFromTop (shapeH);
        auto hiRow = c.removeFromTop (freqH);
        auto loRow = c;
        ch.hiShelf->setBounds (hiRow.withTrimmedRight (36).withTrimmedTop (14));
        ch.loShelf->setBounds (loRow.withTrimmedRight (36));
        ch.hiFreq->setBounds (hiRow.removeFromRight (44).withSizeKeepingCentre (44, 58).translated (0, 6));
        ch.loFreq->setBounds (juce::Rectangle<int> (col.getRight() - 44, loRow.getY() - 30, 44, 58));
    };
    // left channel
    shelfLayout (chans[0], r.removeFromLeft (shelfW), true);
    for (int b = 0; b < 3; ++b) { chans[0].bandCol[b] = r.removeFromLeft (bw); place (chans[0].bandCol[b], *chans[0].shape[b], *chans[0].freq[b], *chans[0].level[b]); }
    // centre: status lamps, MAN / OFF, PUSH EQ, then the right channel's shelving
    centreCol = r.removeFromLeft (centreW);
    {
        auto top = centreCol.withHeight (shapeH);
        auto leds = top.removeFromTop (30);
        ledL.setBounds (leds.removeFromLeft (leds.getWidth() / 2).withSizeKeepingCentre (44, 30));
        ledR.setBounds (leds.withSizeKeepingCentre (44, 30));
        auto tg = top.removeFromTop (juce::jmin (52, top.getHeight() - 22));
        man[0]->setBounds (tg.removeFromLeft (tg.getWidth() / 2).withSizeKeepingCentre (46, tg.getHeight()));
        man[1]->setBounds (tg.withSizeKeepingCentre (46, tg.getHeight()));
        pushEq.setBounds (top.withSizeKeepingCentre (26, juce::jmin (26, top.getHeight())));
        shelfLayout (chans[1], centreCol, false);
        ledOn.setBounds (juce::Rectangle<int> (centreCol.getCentreX() - 12, getLocalBounds().getBottom() - 34, 24, 24));
    }
    for (int b = 0; b < 3; ++b) { chans[1].bandCol[b] = r.removeFromLeft (bw); place (chans[1].bandCol[b], *chans[1].shape[b], *chans[1].freq[b], *chans[1].level[b]); }
    badgeL = juce::Rectangle<int> (chans[0].bandCol[1].getRight() - 12, chans[0].bandCol[1].getY() + shapeH + freqH - 22, 24, 22);
    badgeR = juce::Rectangle<int> (chans[1].bandCol[1].getRight() - 12, chans[1].bandCol[1].getY() + shapeH + freqH - 22, 24, 22);
}

void ParametricPanel::paint (juce::Graphics& g)
{
    const auto ink = col::silkscreen;
    auto engrave = [&] (const juce::String& t, juce::Rectangle<float> r, float h, juce::Justification j = juce::Justification::centred, float alpha = 1.0f)
    { draw::engraved (g, t, r, h, j, ink.withAlpha (alpha), false); };
    // engraved double divider lines between the columns (like the hardware)
    auto divider = [&] (int x)
    {
        const float y0 = 16.0f, y1 = (float) getHeight() - 38.0f;
        g.setColour (juce::Colours::black.withAlpha (0.7f)); g.drawVerticalLine (x, y0, y1); g.drawVerticalLine (x + 4, y0, y1);
        g.setColour (juce::Colours::white.withAlpha (0.16f)); g.drawVerticalLine (x + 1, y0, y1); g.drawVerticalLine (x + 5, y0, y1);
    };
    for (int c = 0; c < 2; ++c)
    {
        for (int b = 0; b < 3; ++b) divider (chans[c].bandCol[b].getX() - 3);
        // band names under the SHAPE knobs
        for (int b = 0; b < 3; ++b)
        {
            const auto sb = chans[c].shape[b]->getBounds().toFloat();
            engrave (ppm::sontec::kBandNames[b], sb.withY (sb.getBottom() - 2.0f).withHeight (14.0f), 11.0f);
        }
        const auto hs = chans[c].hiShelf->getBounds().toFloat(), ls = chans[c].loShelf->getBounds().toFloat();
        engrave ("SHELVING", hs.withY (hs.getY() - 16.0f).withHeight (14.0f), 10.5f);
        engrave ("HI", hs.withWidth (24.0f).withHeight (14.0f).translated (-4.0f, 2.0f), 10.0f, juce::Justification::centredLeft);
        engrave ("LO", ls.withWidth (24.0f).withY (ls.getBottom() - 24.0f).withHeight (14.0f).translated (-4.0f, 0), 10.0f, juce::Justification::centredLeft);
    }
    divider (legendCol.getX() - 6);
    divider (centreCol.getRight() + 1);
    // L / R badges
    for (auto [b, t] : { std::pair<juce::Rectangle<int>, const char*> { badgeL, "L" }, { badgeR, "R" } })
    {
        g.setColour (ink.withAlpha (0.8f)); g.drawRoundedRectangle (b.toFloat(), 3.0f, 1.2f);
        engrave (t, b.toFloat(), 13.0f);
    }
    // logo badge, top left
    {
        auto lb = chans[0].shelfCol.withHeight (chans[0].shape[0]->getHeight()).toFloat().reduced (24.0f, 6.0f);
        auto box = lb.withSizeKeepingCentre (52.0f, 52.0f);
        g.setColour (ink.withAlpha (0.85f)); g.drawRoundedRectangle (box, 8.0f, 2.2f);
        g.setFont (serif (34.0f)); g.setColour (ink); g.drawText ("B", box.translated (0, 1.0f), juce::Justification::centred);
        engrave ("basemental", box.withY (box.getBottom() + 1.0f).withHeight (13.0f).expanded (20.0f, 0), 10.0f);
    }
    // centre labels
    engrave ("PUSH", pushEq.getBounds().toFloat().withWidth (40.0f).translated (-44.0f, 0), 9.5f, juce::Justification::centredRight);
    engrave ("EQ.", pushEq.getBounds().toFloat().withWidth (40.0f).translated ((float) pushEq.getWidth() + 4.0f, 0), 9.5f, juce::Justification::centredLeft);
    // legend column
    const float lx = (float) legendCol.getX(), lw = (float) legendCol.getWidth();
    const auto& c0 = chans[1];
    engrave ("SHAPE", { lx, (float) c0.shape[0]->getBounds().getCentreY() - 10.0f, lw, 12.0f }, 10.5f);
    engrave ("dB/Oct", { lx, (float) c0.shape[0]->getBounds().getCentreY() + 2.0f, lw, 12.0f }, 9.5f, juce::Justification::centred, 0.85f);
    engrave ("FREQ", { lx, (float) c0.freq[0]->getBounds().getCentreY() - 10.0f, lw, 12.0f }, 10.5f);
    engrave ("Hz", { lx, (float) c0.freq[0]->getBounds().getCentreY() + 2.0f, lw, 12.0f }, 9.5f, juce::Justification::centred, 0.85f);
    engrave ("LEVEL", { lx, (float) c0.level[0]->getBounds().getCentreY() - 10.0f, lw, 12.0f }, 10.5f);
    engrave ("dB", { lx, (float) c0.level[0]->getBounds().getCentreY() + 2.0f, lw, 12.0f }, 9.5f, juce::Justification::centred, 0.85f);
    // model line, italic engraving
    g.setColour (ink.withAlpha (0.9f));
    g.setFont (juce::FontOptions (16.0f, juce::Font::italic | juce::Font::bold));
    g.drawText ("PARAMETRIC  MASTERING  EQUALIZER", juce::Rectangle<int> (chans[0].bandCol[0].getX(), getHeight() - 32, 520, 22), juce::Justification::centredLeft);
}

void ParametricPanel::update()
{
    const auto st = proc.getParametricSettings();
    ledL.setBrightness (st.in && st.ch[0].in ? 1.0f : 0.0f);
    ledR.setBrightness (st.in && st.ch[1].in ? 1.0f : 0.0f);
    ledOn.setBrightness (st.in ? 1.0f : 0.15f);
    const bool linked = proc.isLinked();
    if (linked != lastLinked)
    {
        lastLinked = linked;
        auto& ch = chans[1];
        for (int b = 0; b < 3; ++b) for (auto* k : { ch.shape[b].get(), ch.freq[b].get(), ch.level[b].get() }) k->setEnabled (! linked);
        for (auto* k : std::initializer_list<juce::Component*> { ch.hiShelf.get(), ch.loShelf.get(), ch.loFreq.get(), ch.hiFreq.get() }) k->setEnabled (! linked);
        repaint();
    }
}

// =============================================================================
//  Pop-out units
// =============================================================================
PassiveEqUnit::PassiveEqUnit (PassiveMasterEQProcessor& p) : proc (p)
{
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int b = 0; b < kNumBands; ++b) { bands[ch][b] = std::make_unique<BandStrip> (p.apvts, ch, b); addAndMakeVisible (*bands[ch][b]); }
        centers[ch] = std::make_unique<CenterStrip> (p.apvts, ch);
        addAndMakeVisible (*centers[ch]);
    }
    update();
}

void PassiveEqUnit::resized()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        rows[ch] = juce::Rectangle<float> (0.0f, (float) (ch * (kRowH + kGap)), (float) getWidth(), (float) kRowH);
        auto row = rows[ch].reduced (6.0f, 6.0f).toNearestInt();
        row.removeFromLeft (34);
        row.removeFromRight (8);
        centers[ch]->setBounds (row.removeFromRight (206));
        const int bw = row.getWidth() / kNumBands;
        for (int b = 0; b < kNumBands; ++b) bands[ch][b]->setBounds (row.removeFromLeft (bw));
    }
}

void PassiveEqUnit::paint (juce::Graphics& g)
{
    for (int ch = 0; ch < 2; ++ch)
    {
        auto r = rows[ch].withWidth (34.0f).withTrimmedTop (24).withTrimmedBottom (24);
        juce::Graphics::ScopedSaveState ss (g);
        g.addTransform (juce::AffineTransform::rotation (-kPiF * 0.5f, r.getCentreX(), r.getCentreY()));
        const auto c = ch == 0 ? col::amber : (proc.isLinked() ? col::engraveDim : col::blue);
        draw::engraved (g, ch == 0 ? "LEFT CHANNEL" : (proc.isLinked() ? "RIGHT  (LINKED)" : "RIGHT CHANNEL"),
                        juce::Rectangle<float> (180, 20).withCentre (r.getCentre()), 12.0f, juce::Justification::centred, c);
    }
}

void PassiveEqUnit::update()
{
    const bool linked = proc.isLinked();
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto st = proc.getOwnChannelSettings (ch);
        for (int b = 0; b < kNumBands; ++b) bands[ch][b]->updateReadout (st.band[b]);
    }
    for (int b = 0; b < kNumBands; ++b) bands[1][b]->setEnabled (! linked);
    centers[1]->setEnabled (! linked);
    if (linked != lastLinked) { lastLinked = linked; repaint(); }
}

VariMuUnit::VariMuUnit (PassiveMasterEQProcessor& p)
    : proc (p), orderSwitch (p.apvts, ppm::params::compOrder, { "EQ > COMP", "COMP > EQ" }, { col::amber, col::amber })
{
    for (int ch = 0; ch < 2; ++ch) { comps[ch] = std::make_unique<CompStrip> (p.apvts, ch); addAndMakeVisible (*comps[ch]); }
    orderSwitch.setTooltip (tips::order);
    addAndMakeVisible (orderSwitch);
}

void VariMuUnit::resized()
{
    auto c = getLocalBounds().reduced (26, 10);
    auto head = c.removeFromTop (26);
    orderSwitch.setBounds (head.removeFromRight (190).withSizeKeepingCentre (190, 26));
    c.removeFromTop (8);
    const int half = (c.getWidth() - 30) / 2;
    comps[0]->setBounds (c.removeFromLeft (half));
    c.removeFromLeft (30);
    comps[1]->setBounds (c);
}

void VariMuUnit::paint (juce::Graphics& g)
{
    auto head = getLocalBounds().toFloat().reduced (30.0f, 10.0f).withHeight (22.0f);
    draw::engraved (g, "STEREO VARIABLE MU  LIMITER COMPRESSOR", head, 15.0f, juce::Justification::centred, juce::Colours::white);
    draw::engraved (g, "ORDER", head.withX ((float) orderSwitch.getX() - 60.0f).withWidth (52.0f), 11.0f, juce::Justification::centredRight, juce::Colours::white);
    const float x = (float) getWidth() * 0.5f;
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawVerticalLine ((int) x, 40.0f, (float) getHeight() - 14.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawVerticalLine ((int) x + 1, 40.0f, (float) getHeight() - 14.0f);
}

void VariMuUnit::update (float dt)
{
    auto& m = proc.meters;
    const int nCh = m.numChannels.load();
    for (int ch = 0; ch < 2; ++ch)
    {
        const int src = (ch == 1 && nCh < 2) ? 0 : ch;
        comps[ch]->setMeter (m.gr[src].load(), proc.getCompSettings (ch).compIn);
        comps[ch]->tickMeter (dt);
    }
    comps[1]->setEnabled (! proc.isLinked());
}

UnitHost::UnitHost (std::unique_ptr<juce::Component> u, int designW, int designH, std::vector<juce::Rectangle<float>> p,
                    juce::Colour t, std::function<void (float)> upd)
    : unit (std::move (u)), dw (designW), dh (designH), plates (std::move (p)), tint (t), update (std::move (upd))
{
    metal = draw::makeBrushedMetal (dw + 28, dh + 28);
    addAndMakeVisible (*unit);
    unit->setBounds (0, 0, dw, dh);
    setSize (dw + 28, dh + 28);
    if (update) update (0.0f);
    startTimerHz (30);
}

UnitHost::~UnitHost() { stopTimer(); setLookAndFeel (nullptr); }

void UnitHost::resized()
{
    const float W = (float) (dw + 28), H = (float) (dh + 28);
    const float sc = juce::jmin ((float) getWidth() / W, (float) getHeight() / H);
    const float ox = ((float) getWidth() - W * sc) * 0.5f, oy = ((float) getHeight() - H * sc) * 0.5f;
    unit->setTransform (juce::AffineTransform::translation (14.0f, 14.0f).scaled (sc).translated (ox, oy));
}

void UnitHost::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0b0c));
    const float W = (float) (dw + 28), H = (float) (dh + 28);
    const float sc = juce::jmin ((float) getWidth() / W, (float) getHeight() / H);
    const float ox = ((float) getWidth() - W * sc) * 0.5f, oy = ((float) getHeight() - H * sc) * 0.5f;
    juce::Graphics::ScopedSaveState ss (g);
    g.addTransform (juce::AffineTransform::scale (sc).translated (ox, oy));
    g.drawImageAt (metal, 0, 0);
    for (auto r : plates) draw::plate (g, r.translated (14.0f, 14.0f), metal, true, tint);
}

UnitWindow::UnitWindow (const juce::String& title, std::unique_ptr<UnitHost> host, juce::LookAndFeel& laf, std::function<void()> close)
    : juce::DocumentWindow (title, juce::Colour (0xff0b0b0c), juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
      onClose (std::move (close))
{
    setUsingNativeTitleBar (true);
    host->setLookAndFeel (&laf);
    const int W = host->getWidth(), H = host->getHeight();
    float sc = 0.85f;
    if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        sc = juce::jmin (1.0f, (float) d->userArea.getWidth() * 0.9f / (float) W, (float) d->userArea.getHeight() * 0.8f / (float) H);
    setContentOwned (host.release(), false);
    setResizable (true, true);
    setResizeLimits ((int) (W * 0.3f), (int) (H * 0.3f), W * 3, H * 3);
    getConstrainer()->setFixedAspectRatio ((double) W / (double) H);
    centreWithSize ((int) (W * sc), (int) (H * sc));
    setAlwaysOnTop (true);                    // stays above the DAW's windows
    setVisible (true);
}

UnitWindow::~UnitWindow() { clearContentComponent(); }

void PopOutButton::paintButton (juce::Graphics& g, bool hi, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    juce::ColourGradient face (juce::Colour (down ? 0xff1c1c1c : (hi ? 0xff505050 : 0xff363636)), r.getX(), r.getY(),
                               juce::Colour (0xff121212), r.getX(), r.getBottom(), false);
    g.setGradientFill (face); g.fillRoundedRectangle (r, 4.0f);
    draw::chromeRing (g, r, 1.2f);
    const auto c = hi ? col::amber : col::engrave;
    g.setColour (c);
    // icon: a window with an arrow leaving it (pop out)
    auto inner = r.reduced (6.0f, 5.0f);
    auto ic = inner.removeFromLeft (inner.getHeight() * 1.15f);
    g.drawRect (ic.withTrimmedTop (ic.getHeight() * 0.25f).withTrimmedRight (ic.getWidth() * 0.25f), 1.4f);
    const juce::Point<float> a (ic.getCentreX(), ic.getCentreY()), b (ic.getRight(), ic.getY());
    g.drawArrow ({ a, b }, 1.5f, 5.5f, 5.5f);
    // label
    inner.removeFromLeft (5.0f);
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    g.drawFittedText ("POP OUT", inner.toNearestInt(), juce::Justification::centredLeft, 1);
}

// =============================================================================
//  HelpOverlay
// =============================================================================
HelpOverlay::HelpOverlay() { setInterceptsMouseClicks (true, false); }

void HelpOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.72f));
    auto card = getLocalBounds().toFloat().reduced (40.0f, 30.0f).withSizeKeepingCentre ((float) getWidth() - 80.0f, 1200.0f);
    juce::ColourGradient paper (col::cream, card.getX(), card.getY(), col::creamDark, card.getX(), card.getBottom(), false);
    g.setGradientFill (paper);
    g.fillRoundedRectangle (card, 8.0f);
    g.setColour (juce::Colour (0xff4a3a1e));
    g.drawRoundedRectangle (card.reduced (6.0f), 6.0f, 1.5f);

    auto r = card.reduced (34.0f, 24.0f);
    g.setColour (col::ink);
    g.setFont (serif (30.0f));
    g.drawText ("BASEMENTAL MASTERDESK MASTERING SUITE  -  HOW TO USE IT", r.removeFromTop (40), juce::Justification::centredLeft);
    g.setFont (sans (14.5f, false));
    g.drawText ("Hover over any control for a quick tip.  Double-click a knob to reset it.  Click anywhere to close this page.",
                r.removeFromTop (22), juce::Justification::centredLeft);
    r.removeFromTop (18);

    struct Section { const char* title; const char* body; };
    static const Section sections[] = {
        { "1  METERS (top row)",
          "VU LEFT / RIGHT show your average output level. Aim around 0 VU (= -18 dBFS RMS); the needle moves like a real VU. "
          "The orange lamp under each meter flashes when peaks go above -1 dBFS.\n"
          "HARMONICS shows how much tube colour is being added, in % THD. Below 0.3 % is a subtle sheen; the red zone (1 %+) is obvious saturation." },
        { "1b  SPECTRUM, KEY, VECTORSCOPE, LOUDNESS (second row)",
          "SPECTRUM: 62 pins moving with your output (0 to -62 dB).  SONG KEY: the detected key (give it ~20 s); SEMITONES / FINE + KEY SHIFT "
          "move the song to a new key (85 ms latency while on); FORMANT keeps voices natural.  VECTORSCOPE: stereo image; keep correlation above 0.  "
          "LOUDNESS: pick a TARGET, press RESET, play the song; keep TRUE PEAK MAX at -1 dBTP or lower." },
        { "2  LAMPS & TUBES",
          "SIGNAL (green) = audio is coming in.  LINK (yellow) = right channel follows left.  "
          "CLIP (red) = output went over 0 dBFS - turn down GAIN TRIM or your boosts.  TUBE (amber) = tube stage on.\n"
          "The glass tubes glow when the tube stage is on and get brighter with level and DRIVE." },
        { "3  INPUT, TUBE & DRIVE",
          "INPUT (top left) sets how much signal comes in, -24 to +24 dB, before everything else; the meter under it shows the "
          "level - aim for peaks around -6 dBFS.  TUBE ON/OFF switches the tube stage; DRIVE pushes it harder for more warmth "
          "(loudness stays the same)." },
        { "4  THE FOUR EQ BANDS (each channel)",
          "LOW 22 Hz-1 kHz, LOW MID 82 Hz-3.9 kHz, HIGH MID 220 Hz-10 kHz, HIGH 560 Hz-27 kHz. The ranges overlap on purpose.\n"
          "FREQUENCY picks the spot.  GAIN sets how much (0 = nothing; the cream window shows the real dB).  "
          "BANDWIDTH: WIDE = broad and gentle, NARROW = focused.  BOOST / OUT / CUT sets the direction.  "
          "BELL makes a hill or dip; SHELF lifts or lowers everything below (LOW, LOW MID) or above (HIGH MID, HIGH)." },
        { "4b  PARAMETRIC MASTERING EQUALIZER (black panel)",
          "First in the chain, before the passive EQ.  Per channel: SHELVING (HI 8/12 kHz, LO 100/50 Hz) and LO / MID / HI bands with SHAPE "
          "(5-15 dB/oct, wide to narrow), FREQ and LEVEL (+/-9 dB).  MAN / OFF per channel, red PUSH EQ switches it in.  Hover a knob to see its value." },
        { "5  WHY IT SOUNDS 'PASSIVE'",
          "Like the hardware, the bands work in parallel inside one passive network. Two bands boosting the same spot don't stack fully, "
          "a boost and cut in the same place cancel, and gain depends on bandwidth: a wide bell tops out near 6 dB, a narrow one at 11 dB. "
          "On a SHELF, turning BANDWIDTH toward NARROW adds the classic dip-and-bump at the corner." },
        { "6  FILTERS / TRIM",
          "HIGH PASS (12-39 Hz) clears rumble.  LOW PASS (15-52 kHz) softens the extreme top.  "
          "GAIN TRIM sets output level in exact 0.5 dB steps - use it to match loudness when comparing.  "
          "EQ IN / OUT bypasses the EQ for a quick before/after." },
        { "7  STEREO & WINDOW",
          "LINK keeps both sides identical; unlink to treat left and right separately. L > R / R > L copy one side to the other.  "
          "WINDOW: grab the chrome handle on the left or right side and pull: sideways makes it wider (the panel spreads out), up/down makes it bigger. The bottom-right corner works too. Nothing gets squashed.  POP OUT: the POP OUT button in each unit's top-left corner opens that unit in its own window (still connected). CLICK & ZOOM: double-click any empty spot (not a knob) to zoom in 2x right there; double-click again to zoom out. Ctrl/Cmd + mouse wheel zooms smoothly up to 4x. While zoomed, hover an edge, drag an empty spot or use the wheel to look around. MINI shows just the meters; SCROLL makes the panel fill the window's width at a bigger size; hover near the top or bottom edge to glide up or down (or use the mouse wheel; the window can be any height); "
          "FULL PANEL brings everything back. FULL SCREEN fills the screen (press EXIT FULL to go back)." },
        { "8  VARI-MU COMPRESSOR (bottom panel)",
          "A tube 'variable-mu' compressor: very soft knee, and the ratio rises the harder you push (about 1.5:1 up to 4:1), "
          "so it levels without sounding squashed.  THRESHOLD: where it starts (turn down until the GAIN REDUCTION needle moves "
          "1-3 dB).  ATTACK 2-70 ms: slower lets transients through.  RECOVERY 0.2-8 s: how fast it lets go.  "
          "OUTPUT: make-up gain.  MIX: blend with the dry signal for parallel compression." },
        { "9  COMPRESSOR SWITCHES",
          "COMPRESS / LIMIT: soft leveling or firmer peak control.  SC HP: the detector ignores bass below 100 Hz so the kick "
          "doesn't pump the mix.  COMP IN: bypass for comparison.  ORDER: EQ > COMP (usual) or COMP > EQ.  "
          "LINK also links the compressor, so both sides always move together and the stereo image stays put." },
        { "9b  M/S CLASS-A COMPRESSOR (ash-blue panel)",
          "Works like the elysia alpha.  FEEDBACK (smooth, ratio 1:1.1-1:2.5 rising with reduction) or FEED FWD (harder, double ratios and "
          "attack, up to limiting and negative ratios).  ATTACK = time to 10 dB of reduction, RELEASE = time back from 6 dB (linear).  AUTO FAST for "
          "attack and release.  SC filter (LP/HP tilt + freq) shapes what it reacts to; NIVEAU filter tilts the sound after it.  GAIN, MIX, WARM per "
          "channel.  M/S: left = MID, right = SIDE.  LINK = one control voltage.  SOFT CLIP + threshold, LED.  ACTIVE switches it in (starts off)." },
        { "10  AUTO MASTER & MAXIMIZER (above the footer)",
          "Pick a GENRE (POP, JAZZ, 70'S, 80'S, 90'S, R&B, HIP HOP), press LISTEN and play the song - its fullest part is best. "
          "After 30 s of music it measures your mix's tonal balance and loudness and sets the EQ (broad moves toward the genre's "
          "curve, scaled by AMOUNT), INPUT level, tubes, compressor and MAXIMIZER, then checks the real loudness while the song plays.  "
          "Click another genre or move AMOUNT to re-plan instantly; UNDO restores your own settings.  "
          "MAXIMIZER: GAIN pushes into a look-ahead limiter; nothing passes CEILING (use -1 dB for streaming)." },
        { "11  QUICK START",
          "Pick a preset, or: HIGH PASS 16-23 Hz.  HIGH band 16k SHELF, WIDE, +1 to +2 dB for air.  LOW band 47-68 Hz BELL, small "
          "boost for weight.  Then lower THRESHOLD until you see 1-2 dB of gain reduction, ATTACK 25 ms, RECOVERY 0.8 s, "
          "and set OUTPUT to match.  Toggle EQ IN and COMP IN to compare at equal loudness." },
    };

    const float colW = (r.getWidth() - 30.0f) / 2.0f;
    auto colA = r.removeFromLeft (colW);
    r.removeFromLeft (30.0f);
    auto colB = r;
    for (int i = 0; i < (int) std::size (sections); ++i)
    {
        auto& col = i < 8 ? colA : colB;
        juce::AttributedString s;
        s.append (juce::String (sections[i].title) + "\n", serif (19.0f), col::ink);
        s.append (sections[i].body, sans (15.0f, false), juce::Colour (0xff3a332a));
        s.setLineSpacing (3.5f);
        juce::TextLayout tl; tl.createLayout (s, colW);
        tl.draw (g, col.removeFromTop (tl.getHeight()));
        col.removeFromTop (20.0f);
    }
}

// =============================================================================
//  Editor
// =============================================================================
static constexpr float kScales[] = { 0.4f, 0.5f, 0.6f, 0.7f, 0.85f, 1.0f };

// =============================================================================
//  InputMeter
// =============================================================================
void InputMeter::setLevels (float peakL, float peakR, float dt)
{
    const float in[2] = { peakL, peakR };
    for (int c = 0; c < 2; ++c)
    {
        const float db = juce::Decibels::gainToDecibels (in[c], -80.0f);
        level[c] = db > level[c] ? db : juce::jmax (db, level[c] - 30.0f * dt);   // fast up, 30 dB/s down
        if (level[c] >= hold[c]) { hold[c] = level[c]; holdTimer[c] = 2.0f; }
        else if ((holdTimer[c] -= dt) <= 0.0f) hold[c] = juce::jmax (level[c], hold[c] - 20.0f * dt);
    }
    repaint();
}

void InputMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (r, 4.0f);
    draw::chromeRing (g, r, 1.4f);
    auto in = r.reduced (6.0f, 5.0f);
    auto label = in.removeFromLeft (10.0f);
    const float lo = -48.0f, hi = 0.0f;
    const int nSeg = 24;
    for (int c = 0; c < 2; ++c)
    {
        auto row = in.removeFromTop (in.getHeight() / (float) (2 - c));
        g.setFont (sans (8.5f));
        g.setColour (col::engraveDim.brighter (0.4f));
        g.drawText (c == 0 ? "L" : "R", label.withY (row.getY()).withHeight (row.getHeight()), juce::Justification::centredLeft);
        row = row.reduced (0, 1.5f);
        const float w = row.getWidth() / (float) nSeg;
        for (int i = 0; i < nSeg; ++i)
        {
            const float segDb = lo + (hi - lo) * (float) (i + 1) / (float) nSeg;
            const auto base = segDb > -1.0f ? col::red : (segDb > -6.0f ? col::amber : col::green);
            const bool lit = level[c] >= segDb - (hi - lo) / (float) nSeg * 0.5f;
            g.setColour (lit ? base : base.withAlpha (0.10f));
            g.fillRect (row.getX() + w * (float) i + 0.6f, row.getY(), w - 1.2f, row.getHeight());
        }
        if (hold[c] > lo)
        {
            const float x = row.getX() + row.getWidth() * juce::jlimit (0.0f, 1.0f, (hold[c] - lo) / (hi - lo));
            g.setColour (juce::Colours::white);
            g.fillRect (x - 1.0f, row.getY(), 2.0f, row.getHeight());
        }
    }
}

void Faceplate::setCompact (bool c)
{
    compact = c;
    miniButton.setButtonText (c ? "FULL PANEL" : "MINI");
    resized();
    repaint();
}

Faceplate::Faceplate (PassiveMasterEQProcessor& p)
    : proc (p),
      orderSwitch (p.apvts, ppm::params::compOrder, { "EQ > COMP", "COMP > EQ" }, { col::amber, col::amber }),
      drive (p.apvts, ppm::params::tubeDrive, "DRIVE", "-6", "+18"),
      inputKnob (p.apvts, ppm::params::inputGain, "INPUT", "-24", "+24")
{
    for (auto* c : std::initializer_list<juce::Component*> { &vuL, &vuR, &harm, &tube1, &tube2, &lampSignal, &lampLink, &lampClip, &lampTube, &analyzer, &vectorscope, &loudnessPanel, &keyPanel, &autoPanel, &classAPanel, &paraPanel })
        addAndMakeVisible (c);

    for (int ch = 0; ch < 2; ++ch)
    {
        for (int b = 0; b < kNumBands; ++b)
        {
            bands[ch][b] = std::make_unique<BandStrip> (p.apvts, ch, b);
            addAndMakeVisible (*bands[ch][b]);
        }
        centers[ch] = std::make_unique<CenterStrip> (p.apvts, ch);
        addAndMakeVisible (*centers[ch]);
        comps[ch] = std::make_unique<CompStrip> (p.apvts, ch);
        addAndMakeVisible (*comps[ch]);
    }

    linkAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ppm::params::link, linkButton);
    tubeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ppm::params::tubeOn, tubeButton);
    linkButton.onClick = [this] { if (! linkButton.getToggleState()) proc.copyChannel (0, 1); };
    copyLR.onClick = [this] { proc.copyChannel (0, 1); };
    copyRL.onClick = [this] { proc.copyChannel (1, 0); };
    helpButton.onClick = [this] { help.setVisible (! help.isVisible()); help.toFront (false); };

    drive.slider.textFromValueFunction = [] (double v) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; };
    drive.refresh();

    // explanations
    vuL.setTooltip (tips::vu); vuR.setTooltip (tips::vu); harm.setTooltip (tips::harm);
    tube1.setTooltip (tips::tubes); tube2.setTooltip (tips::tubes);
    lampSignal.setTooltip ("SIGNAL - lights when audio is coming in.");
    lampLink.setTooltip   ("LINK - lit while the right channel follows the left.");
    lampClip.setTooltip   ("CLIP - lights (and holds briefly) when the output goes over 0 dBFS. Lower GAIN TRIM or your boosts.");
    lampTube.setTooltip   ("TUBE - lit while the tube make-up stage is on.");
    analyzer.setTooltip (tips::spectrum);
    vectorscope.setTooltip (tips::scope);
    linkButton.setTooltip (tips::link);
    copyLR.setTooltip (tips::copy); copyRL.setTooltip (tips::copy);
    tubeButton.setTooltip (tips::tube);
    drive.setTooltip (tips::drive);
    helpButton.setTooltip ("Open the guide that explains every part of the panel.");

    // presets
    {   // factory presets first, then the engineer presets in one sub-menu per category
        auto* root = presetBox.getRootMenu();
        juce::StringArray cats;
        for (int i = 0; i < proc.getNumPrograms(); ++i)
            if (proc.getProgramCategory (i) == "Factory") root->addItem (i + 1, proc.getProgramName (i));
            else cats.addIfNotAlreadyThere (proc.getProgramCategory (i));
        root->addSeparator();
        root->addSectionHeader ("ENGINEER PRESETS (settings as listed)");
        for (const auto& cat : cats)
        {
            juce::PopupMenu sub;
            for (int i = 0; i < proc.getNumPrograms(); ++i)
                if (proc.getProgramCategory (i) == cat) sub.addItem (i + 1, proc.getProgramName (i));
            root->addSubMenu (cat, sub);
        }
    }
    presetBox.setTextWhenNothingSelected ("- choose a preset -");
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != proc.getCurrentProgram()) { proc.setCurrentProgram (idx); proc.updateHostDisplay(); }
        lastProgram = idx;
        updatePresetTip();
    };
    updatePresetTip();

    // window size
    for (int i = 0; i < (int) std::size (kScales); ++i) sizeBox.addItem (juce::String (juce::roundToInt (kScales[i] * 100)) + " %", i + 1);
    sizeBox.onChange = [this]
    {
        const int idx = sizeBox.getSelectedItemIndex();
        if (idx >= 0 && onScaleRequested) onScaleRequested (kScales[idx]);
    };
    sizeBox.setTooltip ("WINDOW SIZE - scale the whole panel. You can also drag the window corner.");

    orderSwitch.setTooltip (tips::order);
    inputKnob.slider.textFromValueFunction = [] (double v) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; };
    inputKnob.refresh();
    inputKnob.setTooltip ("INPUT - how much signal comes into the Masterdesk (-24 to +24 dB), before the EQ, tubes and compressor. "
                          "Use it to hit the tube stage and compressor at the right level: aim for peaks around -6 dBFS on the meter below. "
                          "Double-click to reset to 0 dB.");
    inputMeter.setTooltip ("INPUT METER - peak level after the INPUT knob, left (top) and right (bottom). Green = fine, "
                           "amber = above -6 dBFS, red = above -1 dBFS. The white tick holds the peak for 2 seconds.");
    fullButton.setTooltip ("FULL SCREEN - fill the screen with the Masterdesk. Press again (EXIT FULL) to go back to the previous size.");
    miniButton.setTooltip ("MINI - shrink to a compact metering view (VU, harmonics, spectrum, vectorscope, loudness). Press FULL PANEL to bring the EQ and compressor back.");
    fullButton.onClick = [this] { if (onToggleFullScreen) onToggleFullScreen(); };
    miniButton.onClick = [this] { if (onToggleCompact) onToggleCompact(); };
    scrollButton.setTooltip ("SCROLL - the panel fills the window's width at a bigger, easier-to-read size. Hover the mouse near the top or bottom edge "
                             "to glide up or down (faster closer to the edge), or use the mouse wheel; make the window any height. Press again for the normal fit-to-window view.");
    scrollButton.onClick = [this] { if (onToggleScroll) onToggleScroll(); };
    addAndMakeVisible (scrollButton);
    for (auto* c : std::initializer_list<juce::Component*> { &linkButton, &tubeButton, &copyLR, &copyRL, &helpButton, &drive, &presetBox, &sizeBox, &orderSwitch, &inputKnob, &inputMeter, &fullButton, &miniButton })
        addAndMakeVisible (c);
    for (int u = 0; u < kNumUnits; ++u)
    {
        popButtons[u].setTooltip ("POP OUT - open the " + unitName (u).fromFirstOccurrenceOf ("MasterDesk - ", false, false)
                                  + " in its own window. It stays connected: every knob moves in both places.");
        popButtons[u].onClick = [this, u] { openUnit (u); };
        addAndMakeVisible (popButtons[u]);
    }
    addChildComponent (help);

    setSize (kW, kH);
    timerCallback();
    startTimerHz (30);
}

juce::String Faceplate::unitName (int u)
{
    static const char* n[kNumUnits] = { "Parametric Mastering Equalizer", "Passive EQ", "Stereo Variable Mu Limiter Compressor",
                                        "M/S Class-A Compressor", "Auto Master + Maximizer" };
    return juce::String ("MasterDesk - ") + n[juce::jlimit (0, kNumUnits - 1, u)];
}

std::unique_ptr<UnitHost> Faceplate::createUnitHost (int u)
{
    constexpr int W = kW - 28;
    switch (u)
    {
        case UnitParametric:
        {
            auto unit = std::make_unique<ParametricPanel> (proc); auto* raw = unit.get();
            return std::make_unique<UnitHost> (std::move (unit), W, 410, std::vector<juce::Rectangle<float>> { { 0, 0, (float) W, 410 } },
                                               col::blackPanel, [raw] (float) { raw->update(); });
        }
        case UnitPassive:
        {
            auto unit = std::make_unique<PassiveEqUnit> (proc); auto* raw = unit.get();
            const int R = PassiveEqUnit::kRowH, G = PassiveEqUnit::kGap;
            return std::make_unique<UnitHost> (std::move (unit), W, 2 * R + G,
                                               std::vector<juce::Rectangle<float>> { { 0, 0, (float) W, (float) R }, { 0, (float) (R + G), (float) W, (float) R } },
                                               juce::Colour(), [raw] (float) { raw->update(); });
        }
        case UnitVariMu:
        {
            auto unit = std::make_unique<VariMuUnit> (proc); auto* raw = unit.get();
            return std::make_unique<UnitHost> (std::move (unit), W, 292, std::vector<juce::Rectangle<float>> { { 0, 0, (float) W, 292 } },
                                               col::steelGray, [raw] (float dt) { raw->update (dt); });
        }
        case UnitClassA:
        {
            auto unit = std::make_unique<ClassAPanel> (proc); auto* raw = unit.get();
            return std::make_unique<UnitHost> (std::move (unit), W, 380, std::vector<juce::Rectangle<float>> { { 0, 0, (float) W, 380 } },
                                               col::ashBlue, [raw] (float dt) { raw->update (dt); });
        }
        default:
        {
            auto unit = std::make_unique<AutoMasterPanel> (proc); auto* raw = unit.get();
            return std::make_unique<UnitHost> (std::move (unit), W, 200, std::vector<juce::Rectangle<float>> { { 0, 0, (float) W, 200 } },
                                               juce::Colour(), [raw] (float dt) { raw->update (dt); });
        }
    }
}

void Faceplate::openUnit (int u)
{
    if (! juce::isPositiveAndBelow (u, (int) kNumUnits)) return;
    if (unitWindows[u] != nullptr) { unitWindows[u]->setMinimised (false); unitWindows[u]->toFront (true); return; }
    juce::Component::SafePointer<Faceplate> safe (this);
    unitWindows[u] = std::make_unique<UnitWindow> (unitName (u), createUnitHost (u), getLookAndFeel(), [safe, u]
    {
        // close after the click has finished (the window can't delete itself inside its own callback)
        juce::MessageManager::callAsync ([safe, u] { if (safe != nullptr) safe->unitWindows[u].reset(); });
    });
}

void Faceplate::setScaleChoice (float scale)
{
    int best = -1;
    for (int i = 0; i < (int) std::size (kScales); ++i)
        if (std::abs (kScales[i] - scale) < 0.01f) best = i;
    if (best >= 0) sizeBox.setSelectedItemIndex (best, juce::dontSendNotification);
    else           sizeBox.setText (juce::String (juce::roundToInt (scale * 100)) + " %", juce::dontSendNotification);
}

void Faceplate::simulateMeters (float vL, float vR, float thd)
{
    simulated = true;
    auto& m = proc.meters;
    m.outRms[0].store (std::pow (10.0f, (vL - 18.0f) / 20.0f));
    m.outRms[1].store (std::pow (10.0f, (vR - 18.0f) / 20.0f));
    m.outPeak[0].store (0.95f);
    m.thd[0].store (thd / 100.0f); m.thd[1].store (thd / 100.0f);
    m.inPeak.store (0.5f);
    m.gr[0].store (vL > 0.0f ? 4.5f : 2.0f); m.gr[1].store (vL > 0.0f ? 4.8f : 2.6f);
    for (int i = 0; i < 90; ++i) timerCallback();   // let the needles settle
}

void Faceplate::timerCallback()
{
    const float dt = 1.0f / 30.0f;

    // live displays
    history.pull (proc.scope);
    analyzer.update (dt);
    vectorscope.update (dt);
    loudnessPanel.update();
    keyPanel.update (dt);
    if (! compact)
    {
        if (unitWindows[UnitAuto] == nullptr) autoPanel.update (dt);      // (the pop-out copy runs it while open)
        classAPanel.update (dt); paraPanel.update();
    }
    auto& m = proc.meters;
    const bool linked = proc.isLinked();
    const bool tubeOn = proc.apvts.getRawParameterValue (ppm::params::tubeOn)->load() > 0.5f;
    const float driveDb = proc.apvts.getRawParameterValue (ppm::params::tubeDrive)->load();
    const int nCh = m.numChannels.load();

    // input meter
    inputMeter.setLevels (simulated ? m.inLevel[0].load() : m.inLevel[0].exchange (0.0f),
                          simulated ? m.inLevel[1].load() : (nCh > 1 ? m.inLevel[1].exchange (0.0f) : m.inLevel[0].load()), dt);


    // --- meters -------------------------------------------------------------
    auto toVu = [] (float rms) { return 20.0f * std::log10 (juce::jmax (rms, 1e-6f)) + 18.0f; };
    const float l = m.outRms[0].load(), r = nCh > 1 ? m.outRms[1].load() : l;
    const float pkL = simulated ? m.outPeak[0].load() : m.outPeak[0].exchange (0.0f);
    const float pkR = simulated ? m.outPeak[1].load() : (nCh > 1 ? m.outPeak[1].exchange (0.0f) : pkL);
    const float thd = 100.0f * juce::jmax (m.thd[0].load(), nCh > 1 ? m.thd[1].load() : 0.0f);

    vuL.setTarget (toVu (l), pkL > 0.891f ? 1.0f : 0.0f);
    vuR.setTarget (toVu (r), pkR > 0.891f ? 1.0f : 0.0f);
    harm.setTarget (tubeOn ? thd : 0.0f, tubeOn ? 1.0f : 0.0f);
    vuL.tick (dt); vuR.tick (dt); harm.tick (dt);
    for (int ch = 0; ch < 2; ++ch)
    {
        const int src = (ch == 1 && nCh < 2) ? 0 : ch;
        comps[ch]->setMeter (m.gr[src].load(), proc.getCompSettings (ch).compIn);
        comps[ch]->tickMeter (dt);
    }

    // --- lamps ----------------------------------------------------------------
    const float inPk = simulated ? m.inPeak.load() : m.inPeak.exchange (0.0f);
    lampSignal.setBrightness (inPk > 0.001f ? 1.0f : 0.0f);
    lampLink.setBrightness (linked ? 1.0f : 0.0f);
    if (juce::jmax (pkL, pkR) >= 0.999f) clipHold = 30;
    lampClip.setBrightness (clipHold > 0 ? 1.0f : 0.0f);
    if (clipHold > 0) --clipHold;
    lampTube.setBrightness (tubeOn ? 1.0f : 0.0f);

    // --- tube glow --------------------------------------------------------------
    flicker += (rng.nextFloat() - 0.5f) * 0.02f; flicker *= 0.9f;
    const float level = juce::jlimit (0.0f, 1.0f, (toVu (juce::jmax (l, r)) + 24.0f) / 27.0f);
    const float glow = tubeOn ? juce::jlimit (0.0f, 1.0f, 0.72f + 0.2f * level + 0.12f * (driveDb + 6.0f) / 24.0f + flicker) : 0.04f;
    const float g1 = tube1.getGlow(), g2 = tube2.getGlow();
    tube1.setGlow (glow); tube2.setGlow (juce::jlimit (0.0f, 1.0f, glow * 0.97f - flicker));
    if (std::abs (tube1.getGlow() - g1) > 0.004f || std::abs (tube2.getGlow() - g2) > 0.004f)
        repaint (tube1.getBounds().getUnion (tube2.getBounds()).expanded (90, 50).getIntersection (topPlate.toNearestInt()));

    // --- linked state & readouts --------------------------------------------------
    if (linked != lastLinked) { lastLinked = linked; repaint(); }
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto s = proc.getOwnChannelSettings (ch);
        for (int b = 0; b < kNumBands; ++b) bands[ch][b]->updateReadout (s.band[b]);
    }
    for (int b = 0; b < kNumBands; ++b) bands[1][b]->setEnabled (! linked);
    centers[1]->setEnabled (! linked);
    comps[1]->setEnabled (! linked);
    copyLR.setEnabled (! linked);
    copyRL.setEnabled (! linked);

    if (proc.getCurrentProgram() != lastProgram)
    {
        lastProgram = proc.getCurrentProgram();
        presetBox.setSelectedId (lastProgram + 1, juce::dontSendNotification);
        updatePresetTip();
    }
}

void Faceplate::updatePresetTip()
{
    const int idx = presetBox.getSelectedId() - 1;
    const auto notes = idx >= 0 ? proc.getProgramNotes (idx) : juce::String();
    presetBox.setTooltip (notes.isNotEmpty() ? proc.getProgramName (idx) + "\n\n" + notes
                                             : "PRESETS - mastering starting points, plus engineer presets by category. Pick one, then adjust to taste. "
                                               "Your own settings are saved with your DAW session.");
}

void Faceplate::paint (juce::Graphics& g)
{
    g.drawImageAt (metal, 0, 0);

    draw::plate (g, topPlate, metal);
    {   // tube glow spilling onto the panel (kept inside the top plate)
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (topPlate.reduced (4.0f).toNearestInt());
        TubeGlass::paintHalo (g, tube1.getBounds().toFloat(), tube1.getGlow());
        TubeGlass::paintHalo (g, tube2.getBounds().toFloat(), tube2.getGlow());
    }
    draw::plate (g, curvePlate, metal, false);
    draw::plate (g, footerPlate, metal);
    if (! compact)
    {
    draw::plate (g, rowPlates[0], metal);
    draw::plate (g, rowPlates[1], metal);
    draw::plate (g, compPlate, metal, true, col::steelGray);
    draw::plate (g, autoPlate, metal);
    draw::plate (g, classAPlate, metal, true, col::ashBlue);
    draw::plate (g, paraPlate, metal, true, col::blackPanel);
    {
        auto head = compPlate.reduced (30.0f, 10.0f).withHeight (22.0f);
        draw::engraved (g, "STEREO VARIABLE MU  LIMITER COMPRESSOR", head, 15.0f, juce::Justification::centred, juce::Colours::white);
        draw::engraved (g, "ORDER", head.withX ((float) orderSwitch.getX() - 60.0f).withWidth (52.0f), 11.0f, juce::Justification::centredRight, juce::Colours::white);
        // divider between the two channels
        const float x = compPlate.getCentreX();
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawVerticalLine ((int) x, compPlate.getY() + 40.0f, compPlate.getBottom() - 14.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawVerticalLine ((int) x + 1, compPlate.getY() + 40.0f, compPlate.getBottom() - 14.0f);
    }

    // row tags
    for (int ch = 0; ch < 2; ++ch)
    {
        auto r = rowPlates[ch].withWidth (34.0f).withTrimmedTop (24).withTrimmedBottom (24);
        juce::Graphics::ScopedSaveState s (g);
        g.addTransform (juce::AffineTransform::rotation (-kPiF * 0.5f, r.getCentreX(), r.getCentreY()));
        const auto c = ch == 0 ? col::amber : (proc.isLinked() ? col::engraveDim : col::blue);
        draw::engraved (g, ch == 0 ? "LEFT CHANNEL" : (proc.isLinked() ? "RIGHT  (LINKED)" : "RIGHT CHANNEL"),
                        juce::Rectangle<float> (180, 20).withCentre (r.getCentre()), 12.0f, juce::Justification::centred, c);
    }
    }   // ! compact

    // lamp + tube legends on the top plate
    draw::engraved (g, "TUBE STAGE", tube1.getBounds().getUnion (tube2.getBounds()).toFloat().withHeight (14).translated (0, -16), 10.0f);

    draw::engraved (g, "PRESET", presetLabel, 11.0f, juce::Justification::centredRight);
    draw::engraved (g, "SIZE", presetLabel.withX (sizeBox.getX() - presetLabel.getWidth() - 6.0f), 11.0f, juce::Justification::centredRight);

    // brass nameplate
    {
        auto p = nameplate;
        juce::ColourGradient metalGrad (juce::Colour (0xff2e2e2e), p.getX(), p.getY(), juce::Colour (0xff0c0c0c), p.getX(), p.getBottom(), false);
        g.setGradientFill (metalGrad); g.fillRoundedRectangle (p, 4.0f);
        draw::chromeRing (g, p, 2.4f);
        for (auto pt : { p.getTopLeft() + juce::Point<float> (11, 11), p.getTopRight() + juce::Point<float> (-11, 11),
                         p.getBottomLeft() + juce::Point<float> (11, -11), p.getBottomRight() + juce::Point<float> (-11, -11) })
            draw::screw (g, pt, 8.0f, 0.5f);
        auto top = p.withTrimmedBottom (p.getHeight() * 0.36f).withTrimmedTop (3.0f);
        g.setFont (serif (19.0f));
        g.setColour (juce::Colours::black.withAlpha (0.9f));
        g.drawText ("BASEMENTAL MASTERDESK", top.translated (0, 1.5f), juce::Justification::centred);
        juce::ColourGradient goldText (juce::Colour (0xfff3dca0), p.getX(), top.getY() + 4, juce::Colour (0xffa8833e), p.getX(), top.getBottom(), false);
        g.setGradientFill (goldText);
        g.drawText ("BASEMENTAL MASTERDESK", top, juce::Justification::centred);
        draw::engraved (g, "MASTERING SUITE", p.withTrimmedTop (p.getHeight() * 0.6f).withTrimmedBottom (4.0f), 9.5f,
                        juce::Justification::centred, col::brass.withAlpha (0.8f));
    }
}

void Faceplate::resized()
{
    metal = draw::makeBrushedMetal (getWidth(), getHeight());
    auto r = getLocalBounds().toFloat().reduced (14.0f);

    // ---------------- top plate: input, meters, tubes, lamps, drive ---------
    topPlate = r.removeFromTop (236.0f);
    {
        auto t = topPlate.reduced (22.0f, 18.0f).toNearestInt();
        // meters grow a little when the panel is made wider; then everything is spaced evenly across the plate
        const int extra = juce::jmax (0, t.getWidth() - 1368);
        const int vuW = juce::jmin (330, 286 + extra / 6), harmW = juce::jmin (260, 226 + extra / 8);
        const int tubeW = 62, pairW = 2 * tubeW + 16, inW = 100, lampW = 60, driveW = 106;
        const int gap = juce::jmax (12, (t.getWidth() - (inW + 2 * vuW + harmW + pairW + lampW + driveW)) / 6);

        auto inCol = t.removeFromLeft (inW);
        inputKnob.setBounds (inCol.removeFromTop (150));
        inCol.removeFromTop (6);
        inputMeter.setBounds (inCol.removeFromTop (40));
        t.removeFromLeft (gap);
        vuL.setBounds (t.removeFromLeft (vuW));
        t.removeFromLeft (gap);
        vuR.setBounds (t.removeFromLeft (vuW));
        t.removeFromLeft (gap);
        harm.setBounds (t.removeFromLeft (harmW).withSizeKeepingCentre (harmW, juce::jmin (172, t.getHeight())));
        t.removeFromLeft (gap);

        auto driveCol = t.removeFromRight (driveW);
        drive.setBounds (driveCol.removeFromTop (138));
        driveCol.removeFromTop (8);
        tubeButton.setBounds (driveCol.removeFromTop (32));
        t.removeFromRight (gap);

        auto lampCol = t.removeFromRight (lampW);
        const int lampH = lampCol.getHeight() / 4;
        for (auto* lamp : { &lampSignal, &lampLink, &lampClip, &lampTube })
            lamp->setBounds (lampCol.removeFromTop (lampH));

        // the two tubes stay together as a pair, centred in the space between the meters and the lamps
        auto tubes = t.withTrimmedTop (22);
        const int tw = juce::jlimit (30, tubeW, tubes.getWidth() / 2 - 8);
        auto pair = tubes.withSizeKeepingCentre (juce::jmin (tubes.getWidth(), 2 * tw + 16), tubes.getHeight());
        tube1.setBounds (pair.removeFromLeft (tw).withTrimmedBottom (6));
        tube2.setBounds (pair.removeFromRight (tw).withTrimmedBottom (6));
    }
    r.removeFromTop (10.0f);

    // ---------------- response curve ----------------------------------------
    // in the MINI view the displays take whatever height is left above the footer
    curvePlate = compact ? r.withTrimmedBottom (72.0f) : r.removeFromTop (238.0f);
    if (compact) r.removeFromTop (curvePlate.getHeight());
    {
        auto d = curvePlate.reduced (8.0f).toNearestInt();
        // standard widths, then extra width (a wider panel) mostly goes to the spectrum
        const int std = 1396, ex = juce::jmax (0, d.getWidth() - std);
        const int specW = std * 31 / 100 + ex * 60 / 100, keyW = 300 + ex * 8 / 100, scopeW = 232 + ex * 7 / 100;
        analyzer.setBounds (d.removeFromLeft (ex > 0 ? specW : d.getWidth() * 31 / 100));
        d.removeFromLeft (8);
        keyPanel.setBounds (d.removeFromLeft (keyW));
        d.removeFromLeft (8);
        vectorscope.setBounds (d.removeFromLeft (scopeW));
        d.removeFromLeft (8);
        loudnessPanel.setBounds (d);
    }
    r.removeFromTop (10.0f);

    // ---------------- footer ----------------------------------------------------
    footerPlate = r.removeFromBottom (62.0f);
    {
        auto f = footerPlate.reduced (30.0f, 12.0f);
        auto bar = f.toNearestInt();
        linkButton.setBounds (bar.removeFromLeft (130)); bar.removeFromLeft (10);
        copyLR.setBounds (bar.removeFromLeft (76));      bar.removeFromLeft (8);
        copyRL.setBounds (bar.removeFromLeft (76));      bar.removeFromLeft (18);
        helpButton.setBounds (bar.removeFromLeft (92));  bar.removeFromLeft (8);
        miniButton.setBounds (bar.removeFromLeft (70));  bar.removeFromLeft (8);
        fullButton.setBounds (bar.removeFromLeft (114)); bar.removeFromLeft (8);
        scrollButton.setBounds (bar.removeFromLeft (98));
        nameplate  = f.removeFromRight (270.0f).withSizeKeepingCentre (270.0f, 48.0f);
        auto mid = f.withTrimmedLeft ((float) (scrollButton.getRight() - f.getX() + 14)).withTrimmedRight (16.0f).toNearestInt();
        sizeBox.setBounds (mid.removeFromRight (84).withSizeKeepingCentre (84, 30));
        mid.removeFromRight (44);
        presetLabel = mid.removeFromLeft (52).toFloat();
        mid.removeFromLeft (8);
        presetBox.setBounds (mid.withWidth (juce::jmin (mid.getWidth(), 380)).withSizeKeepingCentre (juce::jmin (mid.getWidth(), 380), 30));
    }
    r.removeFromBottom (10.0f);

    for (int ch = 0; ch < 2; ++ch)
    {
        for (int b = 0; b < kNumBands; ++b) bands[ch][b]->setVisible (! compact);
        centers[ch]->setVisible (! compact);
        comps[ch]->setVisible (! compact);
    }
    orderSwitch.setVisible (! compact);
    autoPanel.setVisible (! compact);
    classAPanel.setVisible (! compact);
    paraPanel.setVisible (! compact);
    linkButton.setVisible (! compact); copyLR.setVisible (! compact); copyRL.setVisible (! compact);
    for (auto& b : popButtons) b.setVisible (! compact);
    if (compact) { help.setBounds (getLocalBounds()); return; }

    // ---------------- AUTO MASTER + MAXIMIZER (above the footer) --------------------
    autoPlate = r.removeFromBottom (200.0f);
    autoPanel.setBounds (autoPlate.toNearestInt());
    r.removeFromBottom (10.0f);

    // ---------------- M/S CLASS-A compressor (ash-blue unit) ---------------------------
    classAPlate = r.removeFromBottom (380.0f);
    classAPanel.setBounds (classAPlate.toNearestInt());
    r.removeFromBottom (10.0f);

    // ---------------- variable-mu compressor ----------------------------------------
    compPlate = r.removeFromBottom (292.0f);
    {
        auto c = compPlate.reduced (26.0f, 10.0f).toNearestInt();
        auto head = c.removeFromTop (26);
        orderSwitch.setBounds (head.removeFromRight (190).withSizeKeepingCentre (190, 26));
        c.removeFromTop (8);
        const int half = (c.getWidth() - 30) / 2;
        comps[0]->setBounds (c.removeFromLeft (half));
        c.removeFromLeft (30);
        comps[1]->setBounds (c);
    }
    r.removeFromBottom (10.0f);

    // ---------------- parametric mastering EQ (in front of the passive EQ) -----------
    paraPlate = r.removeFromTop (410.0f);
    paraPanel.setBounds (paraPlate.toNearestInt());
    r.removeFromTop (10.0f);

    // ---------------- two channel rows ------------------------------------------
    const float rowH = (r.getHeight() - 10.0f) / 2.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        rowPlates[ch] = r.removeFromTop (rowH);
        if (ch == 0) r.removeFromTop (10.0f);
        auto row = rowPlates[ch].reduced (6.0f, 6.0f).toNearestInt();
        row.removeFromLeft (34);
        row.removeFromRight (8);
        centers[ch]->setBounds (row.removeFromRight (206));
        const int bw = row.getWidth() / kNumBands;
        for (int b = 0; b < kNumBands; ++b) bands[ch][b]->setBounds (row.removeFromLeft (bw));
    }

    // pop-out icons in each unit's top-left corner (next to the screw)
    const juce::Rectangle<float> unitPlates[kNumUnits] = { paraPlate, rowPlates[0], compPlate, classAPlate, autoPlate };
    for (int u = 0; u < kNumUnits; ++u)
        popButtons[u].setBounds ((int) unitPlates[u].getX() + 24, (int) unitPlates[u].getY() + 3, 92, 24);

    help.setBounds (getLocalBounds());
    help.toFront (false);
}

// =============================================================================
//  Editor: hosts the faceplate and scales it to the window size
// =============================================================================
// =============================================================================
//  EdgeGrip
// =============================================================================
EdgeGrip::EdgeGrip (bool rightEdge) : right (rightEdge)
{
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    setTooltip ("Grab and pull: sideways makes the MasterDesk wider (the panel spreads out), up or down makes it bigger. "
                "Nothing ever gets squashed or stretched.");
}

void EdgeGrip::paint (juce::Graphics& g)
{
    const bool hot = dragging || isMouseOver();
    auto r = getLocalBounds().toFloat();
    // a chrome handle with three ridges, centred vertically
    const float hh = juce::jmin (90.0f, r.getHeight() * 0.3f);
    auto h = r.withSizeKeepingCentre (juce::jmin (8.0f, r.getWidth() - 2.0f), hh);
    g.setColour (juce::Colours::black.withAlpha (hot ? 0.75f : 0.5f));
    g.fillRoundedRectangle (h.expanded (1.5f), 4.0f);
    juce::ColourGradient metalGrad (juce::Colour (hot ? 0xffd8d8d8 : 0xff8a8a8a), h.getX(), h.getY(),
                                    juce::Colour (hot ? 0xff7a7a7a : 0xff3a3a3a), h.getRight(), h.getY(), false);
    g.setGradientFill (metalGrad);
    g.fillRoundedRectangle (h, 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    for (int i = -1; i <= 1; ++i)
        g.fillRect (juce::Rectangle<float> (h.getX() + 1.5f, h.getCentreY() + (float) i * 7.0f - 0.75f, h.getWidth() - 3.0f, 1.5f));
    if (hot)
    {
        g.setColour (col::amber.withAlpha (0.8f));
        g.drawRoundedRectangle (h.expanded (1.5f), 4.0f, 1.0f);
    }
}

void EdgeGrip::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    startX = e.getScreenX(); startY = e.getScreenY();
    if (onDrag) onDrag (0, 0, right, true);
    repaint();
}

void EdgeGrip::mouseDrag (const juce::MouseEvent& e)
{
    if (onDrag) onDrag (e.getScreenX() - startX, e.getScreenY() - startY, right, false);
}

void WidthRangeConstrainer::checkBounds (juce::Rectangle<int>& b, const juce::Rectangle<int>& prev, const juce::Rectangle<int>& limits,
                                         bool top, bool left, bool bottom, bool right)
{
    ComponentBoundsConstrainer::checkBounds (b, prev, limits, top, left, bottom, right);
    const bool vertical = top || bottom;
    if (minRatio > 0.0 && b.getWidth() < b.getHeight() * minRatio)
    {
        if (vertical) b.setWidth ((int) std::ceil (b.getHeight() * minRatio));       // taller -> also wider: bigger
        else          b.setHeight ((int) std::floor (b.getWidth() / minRatio));     // narrower than the panel -> smaller
    }
    if (maxRatio > 0.0 && b.getWidth() > b.getHeight() * maxRatio)
        b.setWidth ((int) std::floor (b.getHeight() * maxRatio));
    if (left) b.setX (prev.getRight() - b.getWidth());
    if (top)  b.setY (prev.getBottom() - b.getHeight());
}

PassiveMasterEQEditor::PassiveMasterEQEditor (PassiveMasterEQProcessor& p)
    : AudioProcessorEditor (&p), proc (p), face (p)
{
    // Remembered size, or the largest standard size that fits this screen.
    // (Read it first: setting resize limits below triggers a resize.)
    float fit = 0.7f;
    if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = d->userArea;
        fit = kScales[0];
        for (float sc : kScales)
            if (Faceplate::kH * sc <= area.getHeight() * 0.92f && Faceplate::kW * sc <= area.getWidth() * 0.95f)
                fit = sc;
    }
    // a remembered zoom never opens bigger than this screen allows
    float screenMax = 1.6f;
    if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        screenMax = juce::jmin ((float) d->userArea.getHeight() * 0.95f / (float) Faceplate::kH, (float) d->userArea.getWidth() / (float) Faceplate::kW);
    const float scale = juce::jlimit (0.3f, juce::jmax (0.3f, juce::jmin (1.6f, screenMax)), (float) proc.apvts.state.getProperty ("uiScale", fit));
    face.setCompact ((bool) proc.apvts.state.getProperty ("uiCompact", false));
    scrollMode = (bool) proc.apvts.state.getProperty ("uiScroll", false);
    face.setScrollState (scrollMode);

    setLookAndFeel (&laf);
    addAndMakeVisible (face);
    face.setBounds (0, 0, Faceplate::kW, designH());

    setResizable (true, true);
    setConstrainer (&shapeConstrainer);
    updateLimits();

    face.onScaleRequested   = [this] (float sc)
    {
        if (scrolling()) setSize (juce::roundToInt (Faceplate::kW * sc) + kScrollBarW, getHeight());     // zoom = width; keep the height
        else             setSize (juce::roundToInt (Faceplate::kW * sc), juce::roundToInt (designH() * sc));
    };
    face.onToggleScroll     = [this] { toggleScroll(); };
    addChildComponent (hints);
    startTimerHz (60);                                  // hover scrolling / panning
    face.addMouseListener (&zoomMouse, true);           // click & zoom
    face.onToggleFullScreen = [this] { toggleFullScreen(); };
    face.onToggleCompact    = [this] { toggleCompact(); };

    for (auto* gr : { &leftGrip, &rightGrip })
    {
        gr->onDrag = [this] (int dx, int dy, bool rightEdge, bool start) { edgeDrag (dx, dy, rightEdge, start); };
        addAndMakeVisible (*gr);
    }

    // remembered window size (may be wider than the standard shape), else the standard shape at `scale`
    const int savedW = (int) proc.apvts.state.getProperty ("uiW", 0), savedH = (int) proc.apvts.state.getProperty ("uiH", 0);
    if (savedW > 200 && savedH > 150)
    {
        int w = savedW, h = savedH;                     // shrink to fit if it was saved on a bigger screen
        if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            const double k = juce::jmin (1.0, (double) d->userArea.getWidth() / w, (double) d->userArea.getHeight() * 0.95 / h);
            w = (int) (w * k); h = (int) (h * k);
        }
        setSize (w, h);
    }
    else setSize (juce::roundToInt (Faceplate::kW * scale), juce::roundToInt (designH() * scale));
    sizeRestored = true;
}

// Pulling a side handle: sideways = wider / narrower, up / down = bigger / smaller
void PassiveMasterEQEditor::edgeDrag (int deltaX, int deltaY, bool rightEdge, bool start)
{
    auto* win = standaloneWindow();
    if (start)
    {
        dragStartBounds = getBounds();
        if (win != nullptr) dragStartWindow = win->getBounds();
        return;
    }
    if (win != nullptr && win->isFullScreen()) return;
    juce::Rectangle<int> b (dragStartBounds.getX(), dragStartBounds.getY(),
                            dragStartBounds.getWidth() + (rightEdge ? deltaX : -deltaX), dragStartBounds.getHeight() + deltaY);
    const auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds());
    if (d != nullptr)                                   // fit the screen
        b.setSize (juce::jmin (b.getWidth(), d->userArea.getWidth()), juce::jmin (b.getHeight(), d->userArea.getHeight() - (win != nullptr ? 40 : 80)));
    shapeConstrainer.checkBounds (b, dragStartBounds, {}, false, ! rightEdge && deltaX != 0, deltaY != 0, rightEdge && deltaX != 0);
    if (d != nullptr && (b.getHeight() > d->userArea.getHeight() || b.getWidth() > d->userArea.getWidth()))
    {   // the shape rule pushed it past the screen: shrink to fit, keeping that shape
        const double k = juce::jmin ((double) d->userArea.getWidth() / b.getWidth(), (double) d->userArea.getHeight() / b.getHeight());
        b.setSize ((int) (b.getWidth() * k), (int) (b.getHeight() * k));
    }
    setSize (b.getWidth(), b.getHeight());
    if (win != nullptr && ! rightEdge)                  // standalone: the left edge moves, the right stays put
        win->setTopLeftPosition (dragStartWindow.getRight() - win->getWidth(), win->getY());
}

PassiveMasterEQEditor::~PassiveMasterEQEditor() { setLookAndFeel (nullptr); }

void PassiveMasterEQEditor::updateLimits()
{
    // Everything scales together, so the panel always keeps its proportions.
    // In a DAW the corner drag is locked to the panel's shape. In the standalone app the window
    // may take any shape (e.g. full screen); the panel is then centred at the largest size that fits.
    // Nothing is ever squashed: the height sets the zoom, and extra width (up to kMaxWide x) spreads the panel out.
    // In a DAW the window can't be narrower than the panel's own shape; in the standalone app it can be any shape
    // (the panel is then centred).
    shapeConstrainer.setSizeLimits (juce::roundToInt (Faceplate::kW * 0.3), juce::roundToInt (designH() * 0.3),
                                    juce::roundToInt (Faceplate::kW * kMaxWide * 3.0), juce::roundToInt (designH() * 3.0));
    const bool standalone = proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
    const double ratio = (double) Faceplate::kW / (double) designH();
    if (scrolling())
    {   // SCROLL: any height (the panel scrolls), the width sets the zoom
        shapeConstrainer.setSizeLimits (juce::roundToInt (Faceplate::kW * 0.3) + kScrollBarW, 240,
                                        juce::roundToInt (Faceplate::kW * 3.0) + kScrollBarW, juce::roundToInt (designH() * 3.0));
        shapeConstrainer.minRatio = shapeConstrainer.maxRatio = 0.0;
        return;
    }
    shapeConstrainer.minRatio = standalone ? 0.0 : ratio;
    shapeConstrainer.maxRatio = standalone ? 0.0 : ratio * kMaxWide;
}

void PassiveMasterEQEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0b0c));         // shows only around the panel when the window is a different shape
}

void PassiveMasterEQEditor::resized()
{
    float scale;
    if (scrolling())
    {
        // SCROLL: the standard-width panel fills the window's width; hover near the top / bottom edge (or the wheel) moves it up and down
        const int sb = kScrollBarW;
        scale = juce::jmax (0.05f, (float) (getWidth() - sb) / (float) Faceplate::kW);
        if (face.getWidth() != Faceplate::kW || face.getHeight() != designH()) face.setBounds (0, 0, Faceplate::kW, designH());
        const double visH = (double) getHeight() / scale;
        const double maxPos = juce::jmax (0.0, (double) designH() - visH);
        scrollPos = juce::jlimit (0.0, maxPos, scrollPos);
        hints.setBounds (getLocalBounds());
        hints.setVisible (maxPos > 0.0);
        const float offY = maxPos > 0.0 ? (float) (-scrollPos * scale) : ((float) getHeight() - (float) designH() * scale) * 0.5f;
        face.setTransform (juce::AffineTransform::scale (scale).translated (0.0f, offY));
        const int gw = juce::jlimit (8, 14, juce::roundToInt (12.0f * scale + 2.0f));
        leftGrip.setBounds (0, 0, gw, getHeight());
        rightGrip.setBounds (getWidth() - sb - gw, 0, gw, getHeight());
    }
    else
    {
        hints.setVisible (false);
        // one uniform zoom for the whole panel; width beyond the standard shape makes the panel itself wider
        scale = juce::jmin ((float) getWidth() / (float) Faceplate::kW, (float) getHeight() / (float) designH());
        const int panelW = juce::jlimit (Faceplate::kW, (int) (Faceplate::kW * kMaxWide), (int) std::floor ((float) getWidth() / scale));
        if (face.getWidth() != panelW || face.getHeight() != designH()) face.setBounds (0, 0, panelW, designH());
        const float offX = ((float) getWidth()  - (float) panelW * scale) * 0.5f;
        const float offY = ((float) getHeight() - (float) designH() * scale) * 0.5f;
        face.setTransform (juce::AffineTransform::scale (scale).translated (offX, offY));
        const int gw = juce::jlimit (8, 14, juce::roundToInt (12.0f * scale + 2.0f));
        leftGrip.setBounds (0, 0, gw, getHeight());
        rightGrip.setBounds (getWidth() - gw, 0, gw, getHeight());
    }
    if (zoom > 1.001f)
    {   // CLICK & ZOOM: magnify on top of the normal view; keep the view inside the panel
        const double sz = (double) scale * zoom;
        const double visW = getWidth() / sz, visH = getHeight() / sz, pw = face.getWidth(), ph = face.getHeight();
        zx = pw > visW ? juce::jlimit (0.0, pw - visW, zx) : -(visW - pw) * 0.5;
        zy = ph > visH ? juce::jlimit (0.0, ph - visH, zy) : -(visH - ph) * 0.5;
        face.setTransform (juce::AffineTransform::translation ((float) -zx, (float) -zy).scaled ((float) sz));
        hints.setBounds (getLocalBounds());
        hints.setVisible (true);
    }
    hints.toFront (false); leftGrip.toFront (false); rightGrip.toFront (false);
    if (sizeRestored && ! pluginFullScreen && (standaloneWindow() == nullptr || ! standaloneWindow()->isFullScreen()))
    {
        proc.apvts.state.setProperty ("uiScale", scale, nullptr);
        proc.apvts.state.setProperty ("uiW", getWidth(), nullptr);
        proc.apvts.state.setProperty ("uiH", getHeight(), nullptr);
    }
    face.setScaleChoice (scale);
}

double PassiveMasterEQEditor::scrollMax() const
{
    if (! scrolling()) return 0.0;
    const double scale = (double) (getWidth() - kScrollBarW) / (double) Faceplate::kW;
    return juce::jmax (0.0, (double) designH() - (double) getHeight() / scale);
}


float PassiveMasterEQEditor::baseScale() const
{
    if (scrolling()) return juce::jmax (0.05f, (float) (getWidth() - kScrollBarW) / (float) Faceplate::kW);
    return juce::jmin ((float) getWidth() / (float) Faceplate::kW, (float) getHeight() / (float) designH());
}

juce::Rectangle<float> PassiveMasterEQEditor::visiblePanelArea() const
{
    const auto a = face.getLocalPoint (this, juce::Point<float> (0.0f, 0.0f));
    const auto b = face.getLocalPoint (this, juce::Point<float> ((float) getWidth(), (float) getHeight()));
    return juce::Rectangle<float> (a, b).getIntersection (face.getLocalBounds().toFloat());
}

// Zoom so that the panel point under `where` (editor pixels) stays under it
void PassiveMasterEQEditor::zoomAt (juce::Point<float> where, float newZoom)
{
    newZoom = juce::jlimit (1.0f, kMaxZoom, newZoom);
    const auto p = panelPointAt (where);
    const float wasZoom = zoom;
    zoom = newZoom < 1.02f ? 1.0f : newZoom;
    if (zoom > 1.0f)
    {
        const double sz = (double) baseScale() * zoom;
        zx = p.x - where.x / sz;
        zy = p.y - where.y / sz;
    }
    else if (wasZoom > 1.0f && scrolling())
    {   // back out in the scroll view: keep the same part of the panel in the middle
        const double s0 = baseScale();
        scrollPos = p.y - where.y / s0;
    }
    resized();
    bool can[4] {}; float hot[4] {};
    hints.set (can, hot, zoom);
}

// a spot with no control on it (so double-click there zooms instead of resetting a knob)
bool PassiveMasterEQEditor::isEmptySpot (juce::Component* c) const
{
    for (auto* k = c; k != nullptr && k != this; k = k->getParentComponent())
        if (dynamic_cast<juce::Slider*> (k) || dynamic_cast<juce::Button*> (k) || dynamic_cast<juce::ComboBox*> (k)
            || dynamic_cast<juce::TextEditor*> (k) || dynamic_cast<ui::ModeSwitch*> (k) || dynamic_cast<ui::MetalToggle*> (k)
            || dynamic_cast<ui::EdgeGrip*> (k) || dynamic_cast<ui::HelpOverlay*> (k))
            return false;
    return true;
}

void PassiveMasterEQEditor::ZoomMouse::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! ed.isEmptySpot (e.originalComponent)) return;
    ed.toggleZoomAt (e.getEventRelativeTo (&ed).position);
}

void PassiveMasterEQEditor::ZoomMouse::mouseDown (const juce::MouseEvent& e)
{
    panning = ed.zoom > 1.0f && ed.isEmptySpot (e.originalComponent);
    last = e.getEventRelativeTo (&ed).position;
}

void PassiveMasterEQEditor::ZoomMouse::mouseDrag (const juce::MouseEvent& e)
{
    if (! panning) return;                              // drag an empty spot to move the zoomed view around
    const auto now = e.getEventRelativeTo (&ed).position;
    const double sz = (double) ed.baseScale() * ed.zoom;
    ed.zx -= (now.x - last.x) / sz;
    ed.zy -= (now.y - last.y) / sz;
    last = now;
    ed.resized();
}

void PassiveMasterEQEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto pos = e.getEventRelativeTo (this).position;
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())  // Ctrl / Cmd + wheel: zoom at the mouse
    {
        zoomAt (pos, zoom * std::pow (2.0f, w.deltaY * 2.0f));
        return;
    }
    if (zoom > 1.0f)
    {   // zoomed: the wheel looks around (sideways too on a trackpad)
        const double sz = (double) baseScale() * zoom;
        zy -= (double) w.deltaY * 400.0 / sz;
        zx -= (double) w.deltaX * 400.0 / sz;
        resized();
        return;
    }
    if (! scrolling() || scrollMax() <= 0.0) { AudioProcessorEditor::mouseWheelMove (e, w); return; }
    scrollPos -= (double) w.deltaY * 600.0;           // one wheel notch ~ 60 panel pixels
    resized();
}

// Hover scrolling: rest the mouse near an edge and the panel glides that way (faster the closer you get to the edge).
// SCROLL view: top / bottom. Zoomed: all four edges. Not while a mouse button is held (e.g. turning a knob).
void PassiveMasterEQEditor::hoverScroll (float mouseX, float mouseY, double seconds)
{
    const float zone = (float) ui::ScrollHints::kZone, w = (float) getWidth(), h = (float) getHeight();
    bool can[4] {}; float hot[4] {};
    const bool inside = mouseX >= 0.0f && mouseX <= w && mouseY >= 0.0f && mouseY <= h;
    if (zoom > 1.0f)
    {
        const auto v = visiblePanelArea();
        can[0] = v.getY() > 0.5f;                      can[1] = v.getBottom() < (float) face.getHeight() - 0.5f;
        can[2] = v.getX() > 0.5f;                      can[3] = v.getRight()  < (float) face.getWidth()  - 0.5f;
        if (inside)
        {
            if (can[0] && mouseY < zone)      hot[0] = 1.0f - mouseY / zone;
            if (can[1] && mouseY > h - zone)  hot[1] = 1.0f - (h - mouseY) / zone;
            if (can[2] && mouseX < zone)      hot[2] = 1.0f - mouseX / zone;
            if (can[3] && mouseX > w - zone)  hot[3] = 1.0f - (w - mouseX) / zone;
        }
        const double sz = (double) baseScale() * zoom;
        auto speed = [] (float k) { return 80.0 + 700.0 * (double) (k * k); };   // window pixels per second
        const double dy = (hot[1] > 0.0f ? speed (hot[1]) : 0.0) - (hot[0] > 0.0f ? speed (hot[0]) : 0.0);
        const double dx = (hot[3] > 0.0f ? speed (hot[3]) : 0.0) - (hot[2] > 0.0f ? speed (hot[2]) : 0.0);
        if (dx != 0.0 || dy != 0.0) { zx += dx * seconds / sz; zy += dy * seconds / sz; resized(); }
        hints.set (can, hot, zoom);
        return;
    }
    const double maxPos = scrollMax();
    if (maxPos > 0.0 && inside)
    {
        if (mouseY < zone && scrollPos > 0.5)              hot[0] = 1.0f - mouseY / zone;
        if (mouseY > h - zone && scrollPos < maxPos - 0.5) hot[1] = 1.0f - (h - mouseY) / zone;
        const float k = hot[0] > 0.0f ? -hot[0] : hot[1];
        if (k != 0.0f)
        {
            const double scale = baseScale();
            const double pxPerSec = 80.0 + 700.0 * (double) (k * k);      // window pixels per second
            scrollPos += (k < 0.0f ? -1.0 : 1.0) * pxPerSec * seconds / scale;
            resized();
        }
    }
    can[0] = scrollPos > 0.5; can[1] = scrollPos < maxPos - 0.5;
    hints.set (can, hot, zoom);
}

void PassiveMasterEQEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const double dt = juce::jlimit (0.0, 0.05, now - lastTick);
    lastTick = now;
    if ((! scrolling() && zoom <= 1.0f) || ! isShowing()) return;
    const auto p = getMouseXYRelative();
    const bool over = getLocalBounds().contains (p) && isMouseOver (true)
                      && ! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown();
    hoverScroll (over ? (float) p.x : -1.0f, over ? (float) p.y : -1.0f, dt);
}

void ui::ScrollHints::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float zh = (float) kZone;
    auto edge = [&] (int dir, float k)
    {
        juce::Rectangle<float> r;
        juce::Point<float> from, to;
        switch (dir)
        {
            case 0:  r = b.withHeight (zh);                    from = r.getTopLeft();    to = r.getBottomLeft(); break;
            case 1:  r = b.withTop (b.getBottom() - zh);       from = r.getBottomLeft(); to = r.getTopLeft();    break;
            case 2:  r = b.withWidth (zh);                     from = r.getTopLeft();    to = r.getTopRight();   break;
            default: r = b.withLeft (b.getRight() - zh);       from = r.getTopRight();   to = r.getTopLeft();    break;
        }
        const float a = 0.30f + 0.45f * k;
        juce::ColourGradient fade (juce::Colours::black.withAlpha (a), from, juce::Colours::transparentBlack, to, false);
        g.setGradientFill (fade); g.fillRect (r);
        // chevron in the middle of the edge, pointing out
        juce::Path chev;
        const float w = 16.0f, d = 6.0f;
        if (dir < 2)
        {
            const float cx = b.getCentreX(), cy = dir == 0 ? r.getY() + 13.0f : r.getBottom() - 13.0f, s = dir == 0 ? -d : d;
            chev.startNewSubPath (cx - w, cy - s); chev.lineTo (cx, cy + s); chev.lineTo (cx + w, cy - s);
        }
        else
        {
            const float cy = b.getCentreY(), cx = dir == 2 ? r.getX() + 13.0f : r.getRight() - 13.0f, s = dir == 2 ? -d : d;
            chev.startNewSubPath (cx - s, cy - w); chev.lineTo (cx + s, cy); chev.lineTo (cx - s, cy + w);
        }
        g.setColour (juce::Colour (0xffd8dce2).withAlpha (0.35f + 0.6f * k));
        g.strokePath (chev, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    for (int i = 0; i < 4; ++i) if (can[i]) edge (i, hot[i]);

    if (zoom > 1.001f)
    {   // badge: how far in, and how to get out
        const juce::String txt = "ZOOM " + juce::String (zoom, 1) + "x  -  double-click an empty spot to zoom out";
        const juce::Font f (juce::FontOptions (13.0f, juce::Font::bold));
        const float tw = juce::GlyphArrangement::getStringWidth (f, txt) + 28.0f;
        auto pill = juce::Rectangle<float> (tw, 26.0f).withCentre ({ b.getCentreX(), b.getY() + 36.0f });
        g.setColour (juce::Colours::black.withAlpha (0.72f)); g.fillRoundedRectangle (pill, 13.0f);
        g.setColour (juce::Colour (0xff9fe8ff).withAlpha (0.9f)); g.drawRoundedRectangle (pill, 13.0f, 1.2f);
        g.setFont (f); g.drawText (txt, pill, juce::Justification::centred);
    }
}

void PassiveMasterEQEditor::toggleScroll()
{
    const int w = getWidth(), h = getHeight();
    if (! scrollMode)
    {   // keep the window; the panel now fills its width (start at the top)
        scrollMode = true;
        scrollPos = 0.0;
        updateLimits();
        setSize (juce::jmax (w, juce::roundToInt (Faceplate::kW * 0.3) + kScrollBarW), juce::jmax (h, 240));
    }
    else
    {   // back to fit-to-window: keep the zoom the scroll view had where the window allows it
        scrollMode = false;
        updateLimits();
        const double ratio = (double) Faceplate::kW / (double) designH();
        const bool standalone = proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
        int nw = w, nh = h;
        if (! standalone)
        {
            nh = juce::jmax (juce::roundToInt (designH() * 0.3), juce::jmin (h, (int) std::floor (w / ratio)));   // never narrower than the panel's shape
            nw = juce::jlimit ((int) std::ceil (nh * ratio), (int) std::floor (nh * ratio * kMaxWide), w);
        }
        setSize (nw, nh);
    }
    proc.apvts.state.setProperty ("uiScroll", scrollMode, nullptr);
    face.setScrollState (scrollMode);
    resized();
}

juce::DocumentWindow* PassiveMasterEQEditor::standaloneWindow() const
{
    return proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone ? findParentComponentOfClass<juce::DocumentWindow>() : nullptr;
}

void PassiveMasterEQEditor::parentHierarchyChanged()
{
    // Standalone app: give its window the normal green full-screen / zoom button
    // (JUCE's default standalone window only has close + minimise).
    if (standaloneSetupDone) return;
    if (auto* w = standaloneWindow())
    {
        standaloneSetupDone = true;
        juce::Component::SafePointer<juce::DocumentWindow> safe (w);
        juce::MessageManager::callAsync ([safe]
        {
            if (safe == nullptr) return;
            safe->setTitleBarButtonsRequired (juce::DocumentWindow::allButtons, false);
            safe->setResizable (true, false);      // re-creates the native window with the new buttons
        });
    }
}

void PassiveMasterEQEditor::toggleFullScreen()
{
    if (auto* w = standaloneWindow())                  // standalone: real full screen
    {
        const bool fs = ! w->isFullScreen();
        w->setFullScreen (fs);
        face.setFullScreenState (fs);
        return;
    }

    // Inside a DAW the window belongs to the host, so fill the screen with the plug-in instead
    if (! pluginFullScreen)
    {
        sizeBeforeFull = getBounds();
        auto area = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds())->userArea;
        pluginFullScreen = true;
        // largest size with the panel's proportions; leave room for the host's window header
        setSize (area.getWidth() - 20, area.getHeight() - 110);      // the panel spreads out to fill it
    }
    else
    {
        pluginFullScreen = false;
        setSize (sizeBeforeFull.getWidth(), sizeBeforeFull.getHeight());
    }
    face.setFullScreenState (pluginFullScreen);
}

void PassiveMasterEQEditor::toggleCompact()
{
    const float scale = scrolling() ? (float) (getWidth() - kScrollBarW) / (float) Faceplate::kW
                                    : juce::jmin ((float) getWidth() / (float) Faceplate::kW, (float) getHeight() / (float) designH());
    face.setCompact (! face.isCompact());
    proc.apvts.state.setProperty ("uiCompact", face.isCompact(), nullptr);
    updateLimits();
    if (scrollMode)
    {   // MINI isn't scrolled (it's short); FULL PANEL comes back in the scroll view
        int h = face.isCompact() ? juce::roundToInt (designH() * scale) : juce::roundToInt (getWidth() * 0.9);
        if (auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
            h = juce::jmin (h, d->userArea.getHeight() - 80);
        setSize (getWidth(), juce::jmax (240, h));
        resized();
        return;
    }
    setSize (juce::jmax (getWidth(), juce::roundToInt (Faceplate::kW * scale)), juce::roundToInt (designH() * scale));     // same zoom and width, new height
    resized();
}
