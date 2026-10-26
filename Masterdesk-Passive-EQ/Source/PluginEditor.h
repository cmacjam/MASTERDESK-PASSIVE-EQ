// =============================================================================
//  PluginEditor.h  —  "Basemental Masterdesk" faceplate
//
//  Brushed black metal, chrome-ring knobs, working VU meters, a tube-harmonics
//  meter, glowing tubes, jewel lamps and an engraved nameplate. Every control
//  has a hover tooltip, and the HELP button opens an overlay that explains
//  each section of the panel.
// =============================================================================
#pragma once
#include "PluginProcessor.h"

namespace ui
{
namespace col
{
    const juce::Colour metal      { 0xff141414 };
    const juce::Colour plate      { 0xff1b1b1c };
    const juce::Colour engrave    { 0xffc9c9c9 };   // light engraved legends
    const juce::Colour engraveDim { 0xff7a7a7a };
    const juce::Colour cream      { 0xfff1e6c6 };   // meter faces / value windows
    const juce::Colour creamDark  { 0xffd8c79d };
    const juce::Colour ink        { 0xff262320 };
    const juce::Colour needleRed  { 0xffc0392b };
    const juce::Colour amber      { 0xffff9a2e };
    const juce::Colour green      { 0xff3ddc62 };
    const juce::Colour yellow     { 0xffffd23a };
    const juce::Colour red        { 0xffff3b30 };
    const juce::Colour blue       { 0xff5fb2ff };
    const juce::Colour brass      { 0xffd9b874 };
}

// Shared drawing helpers --------------------------------------------------------
namespace draw
{
    juce::Image makeBrushedMetal (int w, int h);
    void plate (juce::Graphics&, juce::Rectangle<float>, const juce::Image& texture, bool screws = true);
    void screw (juce::Graphics&, juce::Point<float> centre, float diameter, float slotAngle);
    void chromeRing (juce::Graphics&, juce::Rectangle<float>, float thickness);
    void jewel (juce::Graphics&, juce::Rectangle<float>, juce::Colour, float brightness);
    void engraved (juce::Graphics&, const juce::String&, juce::Rectangle<float>, float height,
                   juce::Justification = juce::Justification::centred, juce::Colour = col::engrave, bool bold = true);
}

class MasterdeskLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MasterdeskLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
};

// Chrome-ring stepped knob: caption, end labels and a cream value window ---------
class SteppedKnob : public juce::Component
{
public:
    SteppedKnob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& caption,
                 const juce::String& minLabel, const juce::String& maxLabel);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();
    void setTooltip (const juce::String& t) { slider.setTooltip (t); valueBox.setTooltip (t); }

    juce::Slider slider;
    std::function<juce::String()> valueText;            // custom readout (e.g. realised dB)
    juce::Colour valueColour = col::ink;
private:
    juce::String caption, minLabel, maxLabel;
    juce::Label valueBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// BOOST / OUT / CUT three-position switch ---------------------------------------
class ModeSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModeSwitch (juce::AudioProcessorValueTreeState&, const juce::String& paramId);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    int value = 0;
    juce::ParameterAttachment attachment;
};

// Latching chrome push-button with a jewel lamp ---------------------------------
class JewelButton : public juce::ToggleButton
{
public:
    JewelButton (const juce::String& onText, const juce::String& offText, juce::Colour lamp);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
private:
    juce::String onText, offText;
    juce::Colour lamp;
};

// Panel indicator lamp ---------------------------------------------------------
class JewelLamp : public juce::Component, public juce::SettableTooltipClient
{
public:
    JewelLamp (const juce::String& text, juce::Colour c) : label (text), colour (c) {}
    void setBrightness (float b) { if (std::abs (b - brightness) > 0.01f) { brightness = b; repaint(); } }
    void paint (juce::Graphics&) override;
private:
    juce::String label;
    juce::Colour colour;
    float brightness = 0.0f;
};

// Moving-needle meter: VU (output level) or HARMONICS (tube THD) ----------------
class AnalogMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Kind { VU, Harmonics };
    AnalogMeter (Kind, const juce::String& leftLegend, const juce::String& rightLegend);
    void setTarget (float valueDbOrPercent, float lampBrightness);   // VU: dB re 0 VU, Harmonics: % THD
    void tick (float dtSeconds);
    void paint (juce::Graphics&) override;
private:
    float valueToAngle (float v) const;
    Kind kind;
    juce::String leftLegend, rightLegend;
    float target = -40.0f, needle = -40.0f, lamp = 0.0f;
};

// Glass tube with a glow that follows the tube stage ----------------------------
class TubeGlass : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setGlow (float g) { if (std::abs (g - glow) > 0.005f) { glow = g; repaint(); } }
    void paint (juce::Graphics&) override;
private:
    float glow = 0.0f;
};

// Frequency-response display ----------------------------------------------------
class ResponseCurve : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit ResponseCurve (PassiveMasterEQProcessor&);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    juce::Path buildPath (const ppm::ChannelSettings&, juce::Rectangle<float>) const;
    float freqToX (double f, juce::Rectangle<float> r) const;
    float dbToY (double db, juce::Rectangle<float> r) const;
    PassiveMasterEQProcessor& proc;
    juce::String lastKey;
    static constexpr double kMinF = 10.0, kMaxF = 40000.0, kRangeDb = 15.0;
};

// One EQ band of one channel -------------------------------------------------------
class BandStrip : public juce::Component
{
public:
    BandStrip (juce::AudioProcessorValueTreeState&, int ch, int band);
    void resized() override;
    void paint (juce::Graphics&) override;
    void updateReadout (const ppm::BandSettings&);
private:
    int band;
    SteppedKnob gain, freq, bw;
    ModeSwitch mode;
    JewelButton shelf { "SHELF", "BELL", col::amber };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> shelfAtt;
};

// HP / LP / trim / EQ in --------------------------------------------------------
class CenterStrip : public juce::Component
{
public:
    CenterStrip (juce::AudioProcessorValueTreeState&, int ch);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    SteppedKnob hp, lp, trim;
    JewelButton eqIn { "EQ IN", "EQ OUT", col::green };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> eqInAtt;
};

// HELP overlay: a printed "manual card" explaining the panel --------------------
class HelpOverlay : public juce::Component
{
public:
    HelpOverlay();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { setVisible (false); }
};
// The whole panel, drawn at a fixed design size (kW x kH); the editor scales it.
class Faceplate : public juce::Component, private juce::Timer
{
public:
    static constexpr int kW = 1280, kH = 990;

    explicit Faceplate (PassiveMasterEQProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

    void simulateMeters (float vuL, float vuR, float thdPercent);   // screenshot test
    void showHelp() { help.setVisible (true); help.toFront (false); }
    void setScaleChoice (float scale);
    std::function<void (float)> onScaleRequested;                  // set by the editor

private:
    void timerCallback() override;

    PassiveMasterEQProcessor& proc;
    juce::Image metal;

    AnalogMeter vuL  { AnalogMeter::Kind::VU, "LEFT", "OUTPUT" };
    AnalogMeter vuR  { AnalogMeter::Kind::VU, "RIGHT", "OUTPUT" };
    AnalogMeter harm { AnalogMeter::Kind::Harmonics, "TUBE", "% THD" };
    TubeGlass tube1, tube2;
    JewelLamp lampSignal { "SIGNAL", col::green },  lampLink { "LINK", col::yellow },
              lampClip   { "CLIP",   col::red },    lampTube { "TUBE", col::amber };

    ResponseCurve curve;
    std::unique_ptr<BandStrip>   bands[2][ppm::kNumBands];
    std::unique_ptr<CenterStrip> centers[2];

    JewelButton linkButton { "LINK", "UNLINKED", col::yellow };
    JewelButton tubeButton { "TUBE ON", "TUBE OFF", col::amber };
    juce::TextButton copyLR { "L > R" }, copyRL { "R > L" }, helpButton { "HELP  ?" };
    juce::ComboBox presetBox, sizeBox;
    SteppedKnob drive;
    HelpOverlay help;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> linkAtt, tubeAtt;

    juce::Rectangle<float> topPlate, curvePlate, rowPlates[2], footerPlate, nameplate, presetLabel;
    bool lastLinked = true;
    int clipHold = 0;
    int lastProgram = -1;
    float flicker = 0.0f;
    bool simulated = false;
    juce::Random rng;
};
} // namespace ui

class PassiveMasterEQEditor : public juce::AudioProcessorEditor
{
public:
    explicit PassiveMasterEQEditor (PassiveMasterEQProcessor&);
    ~PassiveMasterEQEditor() override;
    void resized() override;

    void simulateMeters (float vuL, float vuR, float thd) { face.simulateMeters (vuL, vuR, thd); }
    void showHelp() { face.showHelp(); }

private:
    PassiveMasterEQProcessor& proc;
    ui::MasterdeskLookAndFeel laf;
    ui::Faceplate face;
    juce::TooltipWindow tooltips { this, 500 };
    bool sizeRestored = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PassiveMasterEQEditor)
};
