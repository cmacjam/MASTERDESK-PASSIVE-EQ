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

void draw::plate (juce::Graphics& g, juce::Rectangle<float> r, const juce::Image& texture, bool screws)
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
                    juce::Justification::centred, on ? col::engrave : col::engraveDim);

    // end labels in the strip just under the knob, below the 7:30 / 4:30 ticks
    const auto sb = slider.getBounds().toFloat();
    const float R = juce::jmin (sb.getWidth(), sb.getHeight()) * 0.5f;
    const float cx = sb.getCentreX();
    const float y  = sb.getBottom() - 1.0f;
    const float off = juce::jmin (R * 0.72f, sb.getWidth() * 0.5f - 15.0f);
    const auto lc = on ? col::engraveDim.brighter (0.5f) : col::engraveDim.darker (0.4f);
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

        // cream face
        juce::ColourGradient paper (col::cream, face.getCentreX(), face.getY() + face.getHeight() * 0.35f,
                                    col::creamDark.darker (0.15f), face.getX(), face.getBottom(), true);
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
        arc (aMin, aRed, rs, col::ink, 1.6f);
        arc (aRed, aMax, rs, col::needleRed, 1.6f);
        arc (aRed, aMax, rs + 4.0f, col::needleRed.withAlpha (0.85f), 6.0f);

        for (auto& m : marks)
        {
            const float a = valueToAngle (m.v);
            const bool red = m.v >= redFrom && ! (kind == Kind::VU && std::abs (m.v) < 0.01f);
            g.setColour (red ? col::needleRed : col::ink);
            g.drawLine ({ onArc (a, rs - (m.major ? 9.0f : 5.0f)), onArc (a, rs + 1.0f) }, m.major ? 1.6f : 1.0f);
            const auto lp = onArc (a, rs + 15.0f);
            g.setFont (serif (m.major ? 13.0f : 11.0f));
            g.drawText (m.text, juce::Rectangle<float> (30, 14).withCentre (lp), juce::Justification::centred);
        }
        if (kind == Kind::VU)
        {
            g.setColour (col::ink);
            g.setFont (serif (11.0f, false));
            g.drawText ("-", juce::Rectangle<float> (14, 12).withCentre (onArc (aMin - 0.12f, rs + 15.0f)), juce::Justification::centred);
            g.setColour (col::needleRed);
            g.drawText ("+", juce::Rectangle<float> (14, 12).withCentre (onArc (aMax + 0.12f, rs + 15.0f)), juce::Justification::centred);
        }

        // centre legend
        g.setColour (col::ink);
        g.setFont (serif (kind == Kind::VU ? 22.0f : (isGr ? 13.5f : 15.0f)));
        g.drawText (kind == Kind::VU ? "VU" : (isGr ? "GAIN REDUCTION" : "HARMONICS"), juce::Rectangle<float> (face.getWidth(), 24).withCentre ({ face.getCentreX(), face.getY() + face.getHeight() * 0.66f }),
                    juce::Justification::centred);
        g.setFont (sans (8.5f, false));
        g.drawText (kind == Kind::VU ? "0 VU = -18 dBFS" : (isGr ? "dB  -  VARIABLE MU" : "TUBE MAKE-UP STAGE"),
                    juce::Rectangle<float> (face.getWidth(), 12).withCentre ({ face.getCentreX(), face.getY() + face.getHeight() * 0.83f }),
                    juce::Justification::centred);

        // needle (shadow first)
        const float na = valueToAngle (needle);
        const auto tip = onArc (na, rs + 6.0f);
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.drawLine ({ pivot.translated (3.0f, 3.0f), tip.translated (3.0f, 3.0f) }, 2.0f);
        g.setColour (juce::Colour (0xff141414));
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
        g.setColour (col::ink);
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
        juce::ColourGradient glass (juce::Colours::white.withAlpha (0.18f), window.getX(), window.getY(),
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
    if (glow > 0.02f)
    {
        juce::ColourGradient halo (col::amber.withAlpha (0.55f * glow), c.x, c.y, col::amber.withAlpha (0.0f), c.x + gw * 1.3f, c.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (juce::Rectangle<float> (gw * 2.6f, glass.getHeight() * 1.4f).withCentre (c));
    }

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (env);
        g.setColour (juce::Colour (0xff1a120a).withAlpha (0.85f));
        g.fillRect (glass.expanded (2.0f, 10.0f));
        // plate structure
        auto plates = glass.reduced (gw * 0.22f, glass.getHeight() * 0.22f).translated (0, glass.getHeight() * 0.06f);
        g.setColour (juce::Colour (0xff3a3a3a));
        g.fillRect (plates.removeFromLeft (plates.getWidth() * 0.32f));
        g.fillRect (plates.removeFromRight (plates.getWidth() * 0.48f));
        // filament glow
        juce::ColourGradient fil (juce::Colour (0xffffd08a).withAlpha (0.25f + 0.75f * glow), c.x, c.y,
                                  col::amber.withAlpha (0.0f), c.x + gw * 0.5f, c.y, true);
        g.setGradientFill (fil);
        g.fillEllipse (juce::Rectangle<float> (gw * 0.9f, glass.getHeight() * 0.75f).withCentre (c));
        g.setColour (juce::Colour (0xffffc070).withAlpha (0.35f + 0.65f * glow));
        for (float dx : { -gw * 0.06f, gw * 0.06f })
            g.drawLine (c.x + dx, glass.getY() + glass.getHeight() * 0.3f, c.x + dx, glass.getBottom() - glass.getHeight() * 0.1f, 1.4f);
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
    // FFT of the newest 4096 samples (L+R)/2
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
        const double fc = bandCentreHz (b), lo = fc * std::pow (2.0, -1.0 / 6.0), hi = fc * std::pow (2.0, 1.0 / 6.0);
        int b0 = (int) std::ceil (lo / binHz), b1 = (int) std::floor (hi / binHz);
        b0 = juce::jlimit (1, kN / 2 - 1, b0); b1 = juce::jlimit (1, kN / 2 - 1, b1);
        double power = 0.0;
        if (b1 < b0)                                   // band narrower than one bin (low end): nearest bin
        {
            const int k = juce::jlimit (1, kN / 2 - 1, (int) std::lround (fc / binHz));
            const float m = work[(size_t) k] * norm;
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
    draw::engraved (g, "1/3 OCT  -  OUTPUT", { screen.getRight() - 176, screen.getY() + 4, 160, 13 }, 9.5f,
                    juce::Justification::centredRight, col::cream.withAlpha (0.45f));

    auto dbToY = [&] (float db) { return r.getBottom() - (db - kFloorDb) / (kTopDb - kFloorDb) * r.getHeight(); };

    // dB scale
    g.setFont (sans (9.5f, false));
    for (float db : { 0.0f, -12.0f, -24.0f, -36.0f, -48.0f, -60.0f })
    {
        const float y = dbToY (db);
        g.setColour (col::cream.withAlpha (0.06f));
        g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        g.setColour (col::cream.withAlpha (0.45f));
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (screen.getX() + 2, y - 6, 28, 12), juce::Justification::centredRight);
    }

    // segmented bars
    const float slot = r.getWidth() / (float) kBands;
    const float barW = slot * 0.72f;
    const float segDb = 3.0f;
    const int   nSeg = (int) ((kTopDb - kFloorDb) / segDb);
    const float segH = r.getHeight() / (float) nSeg;
    for (int b = 0; b < kBands; ++b)
    {
        const float x = r.getX() + slot * (float) b + (slot - barW) * 0.5f;
        const float v = level[(size_t) b];
        for (int sgi = 0; sgi < nSeg; ++sgi)
        {
            const float segTop = kFloorDb + segDb * (float) (sgi + 1);
            const auto seg = juce::Rectangle<float> (x, r.getBottom() - segH * (float) (sgi + 1) + 1.0f, barW, segH - 1.6f);
            const juce::Colour base = segTop > -6.0f ? col::red : (segTop > -18.0f ? col::amber : juce::Colour (0xffe8c06a));
            if (v >= segTop - segDb * 0.5f)
            {
                g.setColour (base.withAlpha (0.25f));
                g.fillRect (seg.expanded (1.0f, 0.5f));           // glow
                g.setColour (base);
                g.fillRect (seg);
            }
            else
            {
                g.setColour (base.withAlpha (0.07f));             // unlit segment
                g.fillRect (seg);
            }
        }
        // peak-hold cap
        const float p = peak[(size_t) b];
        if (p > kFloorDb + 1.0f)
        {
            const float y = dbToY (juce::jmin (p, kTopDb));
            g.setColour (col::cream);
            g.fillRect (juce::Rectangle<float> (x, y - 1.5f, barW, 2.0f));
        }
    }

    // frequency labels
    g.setFont (sans (9.5f, false));
    g.setColour (col::cream.withAlpha (0.5f));
    static const std::pair<int, const char*> labels[] = { { 1, "25" }, { 4, "50" }, { 7, "100" }, { 10, "200" }, { 13, "400" },
                                                          { 17, "1k" }, { 20, "2k" }, { 23, "4k" }, { 26, "8k" }, { 29, "16k" } };
    for (auto& [b, text] : labels)
    {
        const float cx = r.getX() + slot * ((float) b + 0.5f);
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
    static const juce::String spectrum = "SPECTRUM - live level of your output in 31 one-third-octave bands (25 Hz to 16 kHz). "
        "Bars jump up with the music and fall back slowly; the cream caps hold the recent peaks. Use it to see what your EQ moves are doing.";
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
      mode     (s, params::chanId (channel, "c_mode"), { "COMPRESS", "LIMIT" }, { col::amber, col::red })
{
    for (auto* c : std::initializer_list<juce::Component*> { &meter, &thresh, &attack, &recovery, &output, &mix, &mode, &scHp, &compIn })
        addAndMakeVisible (c);
    scHpAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, params::chanId (channel, "c_schp"), scHp);
    compInAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, params::chanId (channel, "c_in"), compIn);

    meter.setTooltip (tips::grMeter);
    thresh.setTooltip (tips::cThresh);   attack.setTooltip (tips::cAttack);   recovery.setTooltip (tips::cRecovery);
    output.setTooltip (tips::cOutput);   mix.setTooltip (tips::cMix);
    mode.setTooltip (tips::cMode);       scHp.setTooltip (tips::cScHp);       compIn.setTooltip (tips::cIn);
}

void CompStrip::paint (juce::Graphics& g)
{
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

    auto switches = r.removeFromBottom (30);
    r.removeFromBottom (8);
    // THRESHOLD and OUTPUT are the big knobs; ATTACK, RECOVERY and MIX sit between, a step smaller
    const int total = r.getWidth();
    const int big = total * 27 / 100, small = (total - 2 * big) / 3;
    thresh.setBounds   (r.removeFromLeft (big));
    attack.setBounds   (r.removeFromLeft (small).withTrimmedTop (30));
    recovery.setBounds (r.removeFromLeft (small).withTrimmedTop (30));
    output.setBounds   (r.removeFromLeft (big));
    mix.setBounds      (r.withTrimmedTop (30));

    const int sw = switches.getWidth();
    mode.setBounds   (switches.removeFromLeft (sw * 42 / 100)); switches.removeFromLeft (8);
    scHp.setBounds   (switches.removeFromLeft ((switches.getWidth() - 8) / 2)); switches.removeFromLeft (8);
    compIn.setBounds (switches);
}

// =============================================================================
//  HelpOverlay
// =============================================================================
HelpOverlay::HelpOverlay() { setInterceptsMouseClicks (true, false); }

void HelpOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.72f));
    auto card = getLocalBounds().toFloat().reduced (40.0f, 30.0f).withSizeKeepingCentre ((float) getWidth() - 80.0f, 770.0f);
    juce::ColourGradient paper (col::cream, card.getX(), card.getY(), col::creamDark, card.getX(), card.getBottom(), false);
    g.setGradientFill (paper);
    g.fillRoundedRectangle (card, 8.0f);
    g.setColour (juce::Colour (0xff4a3a1e));
    g.drawRoundedRectangle (card.reduced (6.0f), 6.0f, 1.5f);

    auto r = card.reduced (34.0f, 24.0f);
    g.setColour (col::ink);
    g.setFont (serif (30.0f));
    g.drawText ("BASEMENTAL MASTERDESK  -  HOW TO USE IT", r.removeFromTop (40), juce::Justification::centredLeft);
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
        { "1b  SPECTRUM, VECTORSCOPE, LOUDNESS (second row)",
          "SPECTRUM: 31 bars (25 Hz - 16 kHz) moving with your output; cream caps hold recent peaks.  VECTORSCOPE: tall cloud = "
          "centred, wide = stereo; bottom meter is phase correlation (keep it above 0 for mono safety).  LOUDNESS: pick a TARGET, "
          "press RESET, play the song; INTEGRATED goes green within 1 LU of target. Keep TRUE PEAK MAX at -1 dBTP or lower." },
        { "2  LAMPS & TUBES",
          "SIGNAL (green) = audio is coming in.  LINK (yellow) = right channel follows left.  "
          "CLIP (red) = output went over 0 dBFS - turn down GAIN TRIM or your boosts.  TUBE (amber) = tube stage on.\n"
          "The glass tubes glow when the tube stage is on and get brighter with level and DRIVE." },
        { "3  TUBE & DRIVE",
          "TUBE ON/OFF switches the tube make-up stage. DRIVE pushes the signal harder into the tubes for more warmth; "
          "loudness stays the same, so you hear colour, not just 'louder'." },
        { "4  THE FOUR EQ BANDS (each channel)",
          "LOW 22 Hz-1 kHz, LOW MID 82 Hz-3.9 kHz, HIGH MID 220 Hz-10 kHz, HIGH 560 Hz-27 kHz. The ranges overlap on purpose.\n"
          "FREQUENCY picks the spot.  GAIN sets how much (0 = nothing; the cream window shows the real dB).  "
          "BANDWIDTH: WIDE = broad and gentle, NARROW = focused.  BOOST / OUT / CUT sets the direction.  "
          "BELL makes a hill or dip; SHELF lifts or lowers everything below (LOW, LOW MID) or above (HIGH MID, HIGH)." },
        { "5  WHY IT SOUNDS 'PASSIVE'",
          "Like the hardware, the bands work in parallel inside one passive network. Two bands boosting the same spot don't stack fully, "
          "a boost and cut in the same place cancel, and gain depends on bandwidth: a wide bell tops out near 6 dB, a narrow one at 11 dB. "
          "On a SHELF, turning BANDWIDTH toward NARROW adds the classic dip-and-bump at the corner." },
        { "6  FILTERS / TRIM",
          "HIGH PASS (12-39 Hz) clears rumble.  LOW PASS (15-52 kHz) softens the extreme top.  "
          "GAIN TRIM sets output level in exact 0.5 dB steps - use it to match loudness when comparing.  "
          "EQ IN / OUT bypasses the EQ for a quick before/after." },
        { "7  STEREO",
          "LINK keeps both sides identical (the right-hand row dims). Unlink to treat left and right separately - "
          "the right side starts from the left side's settings. L > R and R > L copy one side to the other." },
        { "8  VARI-MU COMPRESSOR (bottom panel)",
          "A tube 'variable-mu' compressor: very soft knee, and the ratio rises the harder you push (about 1.5:1 up to 4:1), "
          "so it levels without sounding squashed.  THRESHOLD: where it starts (turn down until the GAIN REDUCTION needle moves "
          "1-3 dB).  ATTACK 2-70 ms: slower lets transients through.  RECOVERY 0.2-8 s: how fast it lets go.  "
          "OUTPUT: make-up gain.  MIX: blend with the dry signal for parallel compression." },
        { "9  COMPRESSOR SWITCHES",
          "COMPRESS / LIMIT: soft leveling or firmer peak control.  SC HP: the detector ignores bass below 100 Hz so the kick "
          "doesn't pump the mix.  COMP IN: bypass for comparison.  ORDER: EQ > COMP (usual) or COMP > EQ.  "
          "LINK also links the compressor, so both sides always move together and the stereo image stays put." },
        { "10  QUICK START",
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
        auto& col = i < 5 ? colA : colB;
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
static constexpr float kScales[] = { 0.6f, 0.7f, 0.85f, 1.0f };

Faceplate::Faceplate (PassiveMasterEQProcessor& p)
    : proc (p),
      orderSwitch (p.apvts, ppm::params::compOrder, { "EQ > COMP", "COMP > EQ" }, { col::amber, col::amber }),
      drive (p.apvts, ppm::params::tubeDrive, "DRIVE", "-6", "+18")
{
    for (auto* c : std::initializer_list<juce::Component*> { &vuL, &vuR, &harm, &tube1, &tube2, &lampSignal, &lampLink, &lampClip, &lampTube, &analyzer, &vectorscope, &loudnessPanel })
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
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setTextWhenNothingSelected ("- choose a preset -");
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0 && idx != proc.getCurrentProgram()) { proc.setCurrentProgram (idx); proc.updateHostDisplay(); }
        lastProgram = idx;
    };
    presetBox.setTooltip ("PRESETS - mastering starting points. Pick one, then adjust to taste. "
                          "Your own settings are saved with your DAW session.");

    // window size
    for (int i = 0; i < (int) std::size (kScales); ++i) sizeBox.addItem (juce::String (juce::roundToInt (kScales[i] * 100)) + " %", i + 1);
    sizeBox.onChange = [this]
    {
        const int idx = sizeBox.getSelectedItemIndex();
        if (idx >= 0 && onScaleRequested) onScaleRequested (kScales[idx]);
    };
    sizeBox.setTooltip ("WINDOW SIZE - scale the whole panel. You can also drag the window corner.");

    orderSwitch.setTooltip (tips::order);
    for (auto* c : std::initializer_list<juce::Component*> { &linkButton, &tubeButton, &copyLR, &copyRL, &helpButton, &drive, &presetBox, &sizeBox, &orderSwitch })
        addAndMakeVisible (c);
    addChildComponent (help);

    setSize (kW, kH);
    timerCallback();
    startTimerHz (30);
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
    auto& m = proc.meters;
    const bool linked = proc.isLinked();
    const bool tubeOn = proc.apvts.getRawParameterValue (ppm::params::tubeOn)->load() > 0.5f;
    const float driveDb = proc.apvts.getRawParameterValue (ppm::params::tubeDrive)->load();
    const int nCh = m.numChannels.load();

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
    const float glow = tubeOn ? juce::jlimit (0.0f, 1.0f, 0.45f + 0.35f * level + 0.2f * (driveDb + 6.0f) / 24.0f + flicker) : 0.04f;
    tube1.setGlow (glow); tube2.setGlow (juce::jlimit (0.0f, 1.0f, glow * 0.97f - flicker));

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
        presetBox.setSelectedItemIndex (lastProgram, juce::dontSendNotification);
    }
}

void Faceplate::paint (juce::Graphics& g)
{
    g.drawImageAt (metal, 0, 0);

    draw::plate (g, topPlate, metal);
    draw::plate (g, curvePlate, metal, false);
    draw::plate (g, rowPlates[0], metal);
    draw::plate (g, rowPlates[1], metal);
    draw::plate (g, footerPlate, metal);
    draw::plate (g, compPlate, metal);
    {
        auto head = compPlate.reduced (30.0f, 10.0f).withHeight (22.0f);
        draw::engraved (g, "VARI-MU TUBE COMPRESSOR", head, 14.0f, juce::Justification::centred);
        draw::engraved (g, "ORDER", head.withX ((float) orderSwitch.getX() - 60.0f).withWidth (52.0f), 11.0f, juce::Justification::centredRight);
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
        draw::engraved (g, "PASSIVE EQ  +  VARI-MU COMPRESSOR", p.withTrimmedTop (p.getHeight() * 0.6f).withTrimmedBottom (4.0f), 9.5f,
                        juce::Justification::centred, col::brass.withAlpha (0.8f));
    }
}

void Faceplate::resized()
{
    metal = draw::makeBrushedMetal (getWidth(), getHeight());
    auto r = getLocalBounds().toFloat().reduced (14.0f);

    // ---------------- top plate: meters, tubes, lamps, drive -----------------
    topPlate = r.removeFromTop (236.0f);
    {
        auto t = topPlate.reduced (26.0f, 18.0f).toNearestInt();
        vuL.setBounds (t.removeFromLeft (310));
        t.removeFromLeft (16);
        vuR.setBounds (t.removeFromLeft (310));
        t.removeFromLeft (16);
        harm.setBounds (t.removeFromLeft (250).withSizeKeepingCentre (250, 172));
        t.removeFromLeft (14);

        auto driveCol = t.removeFromRight (110);
        drive.setBounds (driveCol.removeFromTop (138));
        driveCol.removeFromTop (8);
        tubeButton.setBounds (driveCol.removeFromTop (32));

        auto lampCol = t.removeFromRight (64);
        const int lampH = lampCol.getHeight() / 4;
        for (auto* lamp : { &lampSignal, &lampLink, &lampClip, &lampTube })
            lamp->setBounds (lampCol.removeFromTop (lampH));

        auto tubes = t.withTrimmedTop (22);
        const int tw = juce::jmin (62, tubes.getWidth() / 2 - 4);
        tube1.setBounds (tubes.removeFromLeft (tubes.getWidth() / 2).withSizeKeepingCentre (tw, tubes.getHeight() - 6));
        tube2.setBounds (tubes.withSizeKeepingCentre (tw, tubes.getHeight() - 6));
    }
    r.removeFromTop (10.0f);

    // ---------------- response curve ----------------------------------------
    curvePlate = r.removeFromTop (238.0f);
    {
        auto d = curvePlate.reduced (8.0f).toNearestInt();
        analyzer.setBounds (d.removeFromLeft (d.getWidth() * 43 / 100));
        d.removeFromLeft (8);
        vectorscope.setBounds (d.removeFromLeft (238));
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
        helpButton.setBounds (bar.removeFromLeft (92));
        nameplate  = f.removeFromRight (310.0f).withSizeKeepingCentre (310.0f, 48.0f);
        auto mid = f.withTrimmedLeft ((float) (helpButton.getRight() - f.getX() + 14)).withTrimmedRight (16.0f).toNearestInt();
        sizeBox.setBounds (mid.removeFromRight (84).withSizeKeepingCentre (84, 30));
        mid.removeFromRight (44);
        presetLabel = mid.removeFromLeft (52).toFloat();
        mid.removeFromLeft (8);
        presetBox.setBounds (mid.withSizeKeepingCentre (mid.getWidth(), 30));
    }
    r.removeFromBottom (10.0f);

    // ---------------- variable-mu compressor (bottom) ----------------------------
    compPlate = r.removeFromBottom (262.0f);
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

    help.setBounds (getLocalBounds());
}

// =============================================================================
//  Editor: hosts the faceplate and scales it to the window size
// =============================================================================
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
    const float scale = juce::jlimit (0.5f, 1.25f, (float) proc.apvts.state.getProperty ("uiScale", fit));

    setLookAndFeel (&laf);
    addAndMakeVisible (face);
    face.setBounds (0, 0, Faceplate::kW, Faceplate::kH);

    const double aspect = (double) Faceplate::kW / Faceplate::kH;
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (Faceplate::kW * 0.5), juce::roundToInt (Faceplate::kH * 0.5),
                     juce::roundToInt (Faceplate::kW * 1.25), juce::roundToInt (Faceplate::kH * 1.25));
    getConstrainer()->setFixedAspectRatio (aspect);

    face.onScaleRequested = [this] (float s)
    { setSize (juce::roundToInt (Faceplate::kW * s), juce::roundToInt (Faceplate::kH * s)); };

    setSize (juce::roundToInt (Faceplate::kW * scale), juce::roundToInt (Faceplate::kH * scale));
    sizeRestored = true;
}

PassiveMasterEQEditor::~PassiveMasterEQEditor() { setLookAndFeel (nullptr); }

void PassiveMasterEQEditor::resized()
{
    const float scale = (float) getWidth() / (float) Faceplate::kW;
    face.setTransform (juce::AffineTransform::scale (scale));
    if (sizeRestored) proc.apvts.state.setProperty ("uiScale", scale, nullptr);
    face.setScaleChoice (scale);
}
