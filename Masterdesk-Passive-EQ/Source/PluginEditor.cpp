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
    const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float R   = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 2.0f;
    const auto  c   = area.getCentre();
    const bool  on  = s.isEnabled();
    auto circle = [&] (float rad) { return juce::Rectangle<float> (rad * 2, rad * 2).withCentre (c); };

    // shadow
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.fillEllipse (circle (R).translated (0, R * 0.06f).expanded (R * 0.03f));

    // chrome skirt ring
    juce::ColourGradient ring (juce::Colour (0xfff4f4f4), c.x - R, c.y - R, juce::Colour (0xff3a3a3a), c.x + R, c.y + R, false);
    ring.addColour (0.35, juce::Colour (0xffbdbdbd));
    ring.addColour (0.55, juce::Colour (0xff7d7d7d));
    ring.addColour (0.7,  juce::Colour (0xffd8d8d8));
    g.setGradientFill (ring);
    g.fillEllipse (circle (R));
    // spun-metal rings on the chrome
    for (int i = 0; i < 10; ++i)
    {
        g.setColour ((i % 2 ? juce::Colours::white : juce::Colours::black).withAlpha (0.05f));
        g.drawEllipse (circle (R * (0.74f + 0.025f * (float) i)), 0.8f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawEllipse (circle (R), 1.0f);

    // detent ticks engraved into the ring
    const double interval = s.getInterval() > 0 ? s.getInterval() : 1.0;
    const int steps   = juce::jlimit (2, 64, (int) std::lround ((s.getMaximum() - s.getMinimum()) / interval) + 1);
    const int current = (int) std::lround ((s.getValue() - s.getMinimum()) / interval);
    const bool dense  = steps > 30;
    for (int i = 0; i < steps; ++i)
    {
        if (dense && i % 2 != 0 && i != current) continue;
        const float a  = startAngle + (endAngle - startAngle) * (float) i / (float) (steps - 1);
        const bool major = (i == 0 || i == steps - 1);
        const auto p0 = c.getPointOnCircumference (R * (major ? 0.76f : 0.80f), a);
        const auto p1 = c.getPointOnCircumference (R * 0.94f, a);
        g.setColour (i == current && on ? juce::Colour (0xffd0451b) : juce::Colour (0xff111111));
        g.drawLine ({ p0, p1 }, i == current ? 2.4f : (major ? 1.8f : 1.2f));
    }

    // black spun cap
    const float rc = R * 0.68f;
    g.setColour (juce::Colours::black);
    g.fillEllipse (circle (rc + 1.5f));
    juce::ColourGradient cap (juce::Colour (0xff3c3c3c), c.x - rc * 0.6f, c.y - rc * 0.8f, juce::Colour (0xff050505), c.x + rc * 0.5f, c.y + rc, false);
    g.setGradientFill (cap);
    g.fillEllipse (circle (rc));
    for (int i = 1; i < 16; ++i)
    {
        g.setColour (juce::Colours::white.withAlpha (i % 2 ? 0.035f : 0.015f));
        g.drawEllipse (circle (rc * (float) i / 16.0f), 0.7f);
    }
    // conical sheen (two soft wedges like the reference)
    for (float a : { -0.9f, kPiF - 0.9f })
    {
        juce::Path wedge;
        wedge.addPieSegment (circle (rc), a - 0.35f, a + 0.35f, 0.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillPath (wedge);
    }
    g.setColour (juce::Colour (0xff8a8a8a).withAlpha (0.7f));
    g.drawEllipse (circle (rc), 1.0f);

    // white pointer
    const float a = startAngle + pos * (endAngle - startAngle);
    juce::Path ptr;
    ptr.addRoundedRectangle (-R * 0.035f - 1.0f, -rc * 0.95f, R * 0.07f + 2.0f, rc * 0.5f, 1.5f);
    ptr.applyTransform (juce::AffineTransform::rotation (a).translated (c));
    g.setColour (on ? juce::Colour (0xfff7f7f7) : juce::Colour (0xff777777));
    g.fillPath (ptr);

    if (! on)
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (circle (R));
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
    valueBox.setBounds (box.withSizeKeepingCentre (juce::jmin (box.getWidth(), 76), 19));
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
ModeSwitch::ModeSwitch (juce::AudioProcessorValueTreeState& s, const juce::String& paramId)
    : attachment (*s.getParameter (paramId), [this] (float v) { value = (int) std::lround (v); repaint(); })
{
    attachment.sendInitialUpdate();
}

void ModeSwitch::paint (juce::Graphics& g)
{
    static const char* names[] = { "BOOST", "OUT", "CUT" };
    const juce::Colour lamps[] = { col::amber, col::engraveDim, col::blue };
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    // recessed slot
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (r, 5.0f);
    juce::ColourGradient inner (juce::Colour (0xff050505), r.getX(), r.getY(), juce::Colour (0xff202020), r.getX(), r.getBottom(), false);
    g.setGradientFill (inner);
    g.fillRoundedRectangle (r.reduced (2.0f), 4.0f);
    draw::chromeRing (g, r, 1.4f);

    const float w = (r.getWidth() - 4.0f) / 3.0f;
    for (int i = 0; i < 3; ++i)
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
    attachment.setValueAsCompleteGesture ((float) juce::jlimit (0, 2, (int) (3.0f * (float) e.x / (float) getWidth())));
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
    needle = target = (kind == Kind::VU ? -40.0f : 0.001f);
}

void AnalogMeter::setTarget (float v, float lampB) { target = v; lamp = lampB; }

void AnalogMeter::tick (float dt)
{
    // VU ballistics: ~300 ms rise to 99 %. Harmonics: a little slower.
    const float tau = kind == Kind::VU ? 0.065f : 0.15f;
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
        const bool isVu = kind == Kind::VU;
        const juce::Span<const Mark> marks (isVu ? vuMarks : thdMarks, isVu ? std::size (vuMarks) : std::size (thdMarks));
        const float redFrom = isVu ? 0.0f : 1.0f;
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
        g.setFont (serif (kind == Kind::VU ? 22.0f : 15.0f));
        g.drawText (kind == Kind::VU ? "VU" : "HARMONICS", juce::Rectangle<float> (face.getWidth(), 24).withCentre ({ face.getCentreX(), face.getY() + face.getHeight() * 0.66f }),
                    juce::Justification::centred);
        g.setFont (sans (8.5f, false));
        g.drawText (kind == Kind::VU ? "0 VU = -18 dBFS" : "TUBE MAKE-UP STAGE",
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
                     kind == Kind::VU ? col::amber : col::green, lamp);
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
//  ResponseCurve
// =============================================================================
ResponseCurve::ResponseCurve (PassiveMasterEQProcessor& p) : proc (p) { startTimerHz (30); }

static juce::String settingsKey (const ChannelSettings& c)
{
    juce::String k;
    for (auto& b : c.band) k << b.freqIndex << ',' << b.gainStep << ',' << b.bwStep << ',' << b.mode << ',' << (int) b.shelf << ';';
    k << c.hp << ',' << c.lp << ',' << c.trim << ',' << (int) c.eqIn;
    return k;
}

void ResponseCurve::timerCallback()
{
    const auto key = settingsKey (proc.getChannelSettings (0)) + "|" + settingsKey (proc.getChannelSettings (1)) + (proc.isLinked() ? "L" : "U");
    if (key != lastKey) { lastKey = key; repaint(); }
}

float ResponseCurve::freqToX (double f, juce::Rectangle<float> r) const
{
    return r.getX() + r.getWidth() * (float) (std::log (f / kMinF) / std::log (kMaxF / kMinF));
}
float ResponseCurve::dbToY (double db, juce::Rectangle<float> r) const
{
    return r.getCentreY() - (float) (db / kRangeDb) * r.getHeight() * 0.5f;
}

juce::Path ResponseCurve::buildPath (const ChannelSettings& c, juce::Rectangle<float> r) const
{
    juce::Path p;
    const int n = juce::jmax (64, (int) r.getWidth() / 2);
    for (int i = 0; i <= n; ++i)
    {
        const double f  = kMinF * std::pow (kMaxF / kMinF, (double) i / n);
        const double db = juce::jlimit (-kRangeDb - 3.0, kRangeDb + 3.0, channelResponseDb (c, f));
        const float x = freqToX (f, r), y = dbToY (db, r);
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    return p;
}

void ResponseCurve::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (bounds, 8.0f);
    auto screen = bounds.reduced (5.0f);
    juce::ColourGradient glassBg (juce::Colour (0xff15130f), screen.getCentreX(), screen.getCentreY(), juce::Colour (0xff060504), screen.getX(), screen.getY(), true);
    g.setGradientFill (glassBg);
    g.fillRoundedRectangle (screen, 6.0f);
    draw::chromeRing (g, bounds, 2.5f);

    auto r = screen.reduced (40.0f, 16.0f);
    g.setFont (sans (10.0f, false));
    for (double db : { -12.0, -6.0, 0.0, 6.0, 12.0 })
    {
        const float y = dbToY (db, r);
        g.setColour (col::cream.withAlpha (std::abs (db) < 0.1 ? 0.25f : 0.07f));
        g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        g.setColour (col::cream.withAlpha (0.45f));
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (screen.getX() + 4, y - 7, 30, 14), juce::Justification::centredRight);
    }
    for (double f : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
    {
        const float x = freqToX (f, r);
        g.setColour (col::cream.withAlpha (0.07f));
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        g.setColour (col::cream.withAlpha (0.45f));
        g.drawText (params::formatHz (f), juce::Rectangle<float> (x - 20, r.getBottom() + 1, 40, 12), juce::Justification::centred);
    }

    const bool linked = proc.isLinked();
    const auto left = buildPath (proc.getChannelSettings (0), r);
    juce::Path fill (left);
    fill.lineTo (r.getRight(), dbToY (0, r)); fill.lineTo (r.getX(), dbToY (0, r)); fill.closeSubPath();
    g.setColour (col::amber.withAlpha (0.10f));
    g.fillPath (fill);
    if (! linked)
    {
        g.setColour (col::blue.withAlpha (0.9f));
        g.strokePath (buildPath (proc.getChannelSettings (1), r), juce::PathStrokeType (1.8f));
    }
    g.setColour (col::amber.withAlpha (0.25f));
    g.strokePath (left, juce::PathStrokeType (5.0f));      // phosphor bloom
    g.setColour (col::amber);
    g.strokePath (left, juce::PathStrokeType (2.0f));

    draw::engraved (g, "FREQUENCY RESPONSE", { screen.getX() + 44, screen.getY() + 4, 200, 14 }, 10.0f,
                    juce::Justification::centredLeft, col::cream.withAlpha (0.6f));
    auto legend = juce::Rectangle<float> (screen.getRight() - 210, screen.getY() + 4, 196, 14);
    g.setFont (sans (10.5f));
    if (linked) { g.setColour (col::amber); g.drawText ("L + R  (LINKED)", legend, juce::Justification::centredRight); }
    else
    {
        g.setColour (col::amber); g.drawText ("LEFT", legend.removeFromLeft (120), juce::Justification::centredRight);
        g.setColour (col::blue);  g.drawText ("RIGHT", legend, juce::Justification::centredRight);
    }
    // glass sheen
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.05f), screen.getX(), screen.getY(), juce::Colours::transparentWhite, screen.getX(), screen.getCentreY(), false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (screen.withHeight (screen.getHeight() * 0.5f), 6.0f);
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
    static const juce::String curve = "FREQUENCY RESPONSE - the combined EQ curve. Amber = left (or both when linked), blue = right.";
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

    const int side = juce::jmin (78, r.getWidth() / 3 - 4);
    gain.setBounds (r.removeFromLeft (side).withTrimmedTop (26));
    bw.setBounds   (r.removeFromRight (side).withTrimmedTop (26));
    freq.setBounds (r);

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
//  HelpOverlay
// =============================================================================
HelpOverlay::HelpOverlay() { setInterceptsMouseClicks (true, false); }

void HelpOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.72f));
    auto card = getLocalBounds().toFloat().reduced (40.0f, 30.0f).withSizeKeepingCentre ((float) getWidth() - 80.0f, 660.0f);
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
        { "8  QUICK START",
          "Start with everything at 0.  HIGH PASS 16-23 Hz.  HIGH band 16k SHELF, BANDWIDTH WIDE, GAIN +1 to +2 dB for air.  "
          "LOW band 47-68 Hz BELL, small boost for weight.  Cut mud around 270-390 Hz on LOW MID if needed.  "
          "Toggle EQ IN to compare, then match level with GAIN TRIM." },
    };

    const float colW = (r.getWidth() - 30.0f) / 2.0f;
    auto colA = r.removeFromLeft (colW);
    r.removeFromLeft (30.0f);
    auto colB = r;
    for (int i = 0; i < 8; ++i)
    {
        auto& col = i < 4 ? colA : colB;
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
static constexpr float kScales[] = { 0.75f, 0.85f, 1.0f, 1.25f };

Faceplate::Faceplate (PassiveMasterEQProcessor& p)
    : proc (p), curve (p), drive (p.apvts, ppm::params::tubeDrive, "DRIVE", "-6", "+18")
{
    for (auto* c : std::initializer_list<juce::Component*> { &vuL, &vuR, &harm, &tube1, &tube2, &lampSignal, &lampLink, &lampClip, &lampTube, &curve })
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
    curve.setTooltip (tips::curve);
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

    for (auto* c : std::initializer_list<juce::Component*> { &linkButton, &tubeButton, &copyLR, &copyRL, &helpButton, &drive, &presetBox, &sizeBox })
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
    for (int i = 0; i < 90; ++i) timerCallback();   // let the needles settle
}

void Faceplate::timerCallback()
{
    const float dt = 1.0f / 30.0f;
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
        draw::engraved (g, "PASSIVE  MASTERING  EQ", p.withTrimmedTop (p.getHeight() * 0.6f).withTrimmedBottom (4.0f), 9.5f,
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
    curvePlate = r.removeFromTop (170.0f);
    curve.setBounds (curvePlate.reduced (8.0f).toNearestInt());
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
    // read the remembered size first: setting resize limits below triggers a resize
    const float scale = juce::jlimit (0.6f, 1.5f, (float) proc.apvts.state.getProperty ("uiScale", 0.85f));

    setLookAndFeel (&laf);
    addAndMakeVisible (face);
    face.setBounds (0, 0, Faceplate::kW, Faceplate::kH);

    const double aspect = (double) Faceplate::kW / Faceplate::kH;
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (Faceplate::kW * 0.6), juce::roundToInt (Faceplate::kH * 0.6),
                     juce::roundToInt (Faceplate::kW * 1.5), juce::roundToInt (Faceplate::kH * 1.5));
    getConstrainer()->setFixedAspectRatio (aspect);

    face.onScaleRequested = [this] (float s)
    { setSize (juce::roundToInt (Faceplate::kW * s), juce::roundToInt (Faceplate::kH * s)); };

    // 85 % by default so it fits a laptop screen; the last size is remembered per session
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
