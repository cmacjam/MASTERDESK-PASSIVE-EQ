// =============================================================================
//  PluginEditor.h  —  "Basemental MasterDesk Mastering Suite" faceplate
//
//  Brushed black metal, chrome-ring knobs, working VU meters, a tube-harmonics
//  meter, glowing tubes, jewel lamps and an engraved nameplate. Every control
//  has a hover tooltip, and the HELP button opens an overlay that explains
//  each section of the panel.
// =============================================================================
#pragma once
#include "PluginProcessor.h"
#include "dsp/KeyDetect.h"

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
    const juce::Colour lcdTop     { 0xff0e3566 };   // blue display glass
    const juce::Colour lcdBottom  { 0xff061a38 };
    const juce::Colour aqua       { 0xff4ff5ff };   // display lines / text
    const juce::Colour aquaDim    { 0xff8fd8e8 };
    const juce::Colour meterInk   { 0xfff2f2f2 };   // white scale on the black meter faces
    const juce::Colour steelGray  { 0xff7c838f };
    const juce::Colour blackPanel { 0xff232426 };   // the parametric EQ's black anodised faceplate
    const juce::Colour silkscreen { 0xffbfc1c4 };   // its engraved legends   // the vari-mu compressor's faceplate (gray brushed metal)
    const juce::Colour ashBlue    { 0xff7d94a7 };   // the M/S CLASS-A compressor's faceplate
    const juce::Colour ashBlueDark { 0xff566b7c };
}

// Shared drawing helpers --------------------------------------------------------
namespace draw
{
    juce::Image makeBrushedMetal (int w, int h);
    void plate (juce::Graphics&, juce::Rectangle<float>, const juce::Image& texture, bool screws = true,
                juce::Colour tint = juce::Colour());           // tint: e.g. ash blue for the class-A unit
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
    juce::Colour captionColour = col::engrave, endLabelColour = col::engraveDim.brighter (0.5f);   // engraved legends
private:
    juce::String caption, minLabel, maxLabel;
    juce::Label valueBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// BOOST / OUT / CUT three-position switch ---------------------------------------
class ModeSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModeSwitch (juce::AudioProcessorValueTreeState&, const juce::String& paramId,
                juce::StringArray names = { "BOOST", "OUT", "CUT" },
                juce::Array<juce::Colour> lamps = { col::amber, col::engraveDim, col::blue });
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    int value = 0;
    juce::StringArray names;
    juce::Array<juce::Colour> lamps;
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
    enum class Kind { VU, Harmonics, GainReduction };
    AnalogMeter (Kind, const juce::String& leftLegend, const juce::String& rightLegend);
    void setTarget (float valueDbOrPercent, float lampBrightness);   // VU: dB re 0 VU, Harmonics: % THD
    void tick (float dtSeconds);
    void paint (juce::Graphics&) override;
    juce::String subtitle;                       // small line under the meter name (empty = default)
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
    float getGlow() const { return glow; }
    static void paintHalo (juce::Graphics&, juce::Rectangle<float> tubeBounds, float glow);   // drawn on the panel behind the tube
    void paint (juce::Graphics&) override;
private:
    float glow = 0.0f;
};

// Rolling copy of the plugin's output, filled from the processor's FIFO each frame
struct AudioHistory
{
    static constexpr int kSize = 65536;            // enough for key detection (0.34 s) even at 192 kHz
    std::vector<float> l = std::vector<float> (kSize, 0.0f), r = std::vector<float> (kSize, 0.0f);
    int write = 0;
    int newSamples = 0;                            // arrived since the last pull
    double sampleRate = 48000.0;

    void pull (PassiveMasterEQProcessor::ScopeFifo&);
    float atL (int ago) const { return l[(size_t) ((write - 1 - ago + 4 * kSize) % kSize)]; }   // 0 = newest
    float atR (int ago) const { return r[(size_t) ((write - 1 - ago + 4 * kSize) % kSize)]; }
};

// Live spectrum analyzer: 62 pins (about 1/6 octave each, 19 Hz - 19 kHz) that move with the music
class SpectrumAnalyzer : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SpectrumAnalyzer (const AudioHistory&);
    void update (float dtSeconds);                 // call once per frame after AudioHistory::pull
    void paint (juce::Graphics&) override;

    static constexpr int kBands = 62;              // 19 Hz .. 19 kHz, log-spaced, 1 kHz is pin 35
    static constexpr float kFloorDb = -62.0f, kTopDb = 0.0f;     // 62 dB live range
    float bandLevelDb (int b) const { return level[(size_t) b]; }
    static constexpr int kBand1k = 35;
    static float bandStep() { return std::pow (10.0f, 3.0f / 61.0f); }          // ratio between neighbouring pins (~1/6 octave)
    static float bandCentreHz (int b) { return 1000.0f * std::pow (10.0f, 3.0f * (float) (b - kBand1k) / 61.0f); }
    static int bandForHz (float hz) { return juce::jlimit (0, kBands - 1, (int) std::lround (kBand1k + 61.0f / 3.0f * std::log10 (hz / 1000.0f))); }

private:
    const AudioHistory& hist;
    static constexpr int kOrder = 13, kN = 1 << kOrder;        // 8192: fine enough for the narrow low pins
    juce::dsp::FFT fft { kOrder };
    std::vector<float> window, work;
    std::array<float, kBands> level {}, peak {}, peakHold {};
};

// Vectorscope (goniometer): stereo dot cloud with balance and correlation meters --
class Vectorscope : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Vectorscope (const AudioHistory&);
    void update (float dtSeconds);
    void paint (juce::Graphics&) override;
    void resized() override;
    float getCorrelation() const { return corr; }
    float getBalance() const     { return balance; }
private:
    const AudioHistory& hist;
    juce::Rectangle<int> plot;                 // square plotting area
    juce::Rectangle<float> screen, balRow, corrRow;
    std::vector<float> glow;                   // phosphor persistence buffer (plot-sized)
    juce::Image img;
    float corr = 0.0f, corrHold = 1.0f, corrHoldTimer = 0.0f;
    float balance = 0.0f, gain = 1.0f;
    bool  hasSignal = false;
};

// KEY panel: detected song key + key shifter controls ------------------------------
class KeyPanel : public juce::Component, public juce::TooltipClient
{
public:
    KeyPanel (PassiveMasterEQProcessor&, const AudioHistory&);
    void update (float dtSeconds);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::String getTooltip() override;
    ppm::KeyResult getDetected() const { return detected; }     // key of the song before shifting
    int getShiftedKey() const;

private:
    PassiveMasterEQProcessor& proc;
    const AudioHistory& hist;
    ppm::KeyDetector detector;
    double detectorRate = 0.0;
    std::vector<float> mono;
    float sinceAnalysis = 0.0f;
    ppm::KeyResult outKey, detected;
    SteppedKnob semis, cents;
    JewelButton onButton { "KEY SHIFT", "SHIFT OFF", col::amber };
    JewelButton formantButton { "FORMANT", "FORMANT", col::green };
    juce::TextButton relearn { "RELEARN" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt, formantAtt;
    juce::Rectangle<float> screen, display;
};

// LUFS loudness panel (EBU R128) --------------------------------------------------
class LoudnessPanel : public juce::Component, public juce::TooltipClient
{
public:
    explicit LoudnessPanel (PassiveMasterEQProcessor&);
    void update();                              // read the processor's loudness values
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::String getTooltip() override;
    float getTargetLufs() const;

private:
    struct Tile { juce::Rectangle<float> r; const char* label; const char* unit; const char* tip; };
    PassiveMasterEQProcessor& proc;
    juce::ComboBox targetBox;
    juce::TextButton resetButton { "RESET" };
    juce::Rectangle<float> screen, header, bar;
    Tile integ, shortT, mom, tpMax, lra, plr, psr, stMax, mMax;
    float vM = -200, vS = -200, vI = -200, vLra = 0, vMMax = -200, vSMax = -200, vTpMax = -200, vTp3 = -200;
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

// Chrome bat-handle toggle switch (hardware style): label above = up, label below = down
class MetalToggle : public juce::Component, public juce::SettableTooltipClient
{
public:
    // upValue: the parameter value (plain) when the handle points up; the other position is the other value
    MetalToggle (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& upLabel,
                 const juce::String& downLabel, float upValue, float downValue);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool isUp() const { return std::abs (value - upValue) < 0.5f; }
    juce::Colour labelColour = juce::Colours::white;
private:
    juce::String upLabel, downLabel;
    float upValue, downValue, value = 0.0f;
    juce::ParameterAttachment attachment;
};

// One channel of the variable-mu compressor --------------------------------------
class CompStrip : public juce::Component
{
public:
    CompStrip (juce::AudioProcessorValueTreeState&, int ch);
    void resized() override;
    void paint (juce::Graphics&) override;
    void setMeter (float grDb, bool active) { meter.setTarget (grDb, active ? 1.0f : 0.0f); }
    void tickMeter (float dt) { meter.tick (dt); }
private:
    int ch;
    AnalogMeter meter;
    SteppedKnob thresh, attack, recovery, output, mix;
    MetalToggle mode, scHp, compIn;
    juce::Rectangle<int> knobPanel;
};

// AUTO MASTER + MAXIMIZER plate --------------------------------------------------
class AutoMasterPanel : public juce::Component, public juce::TooltipClient
{
public:
    explicit AutoMasterPanel (PassiveMasterEQProcessor&);
    void update (float dtSeconds);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::String getTooltip() override;

private:
    void selectGenre (int g);
    PassiveMasterEQProcessor& proc;
    std::unique_ptr<JewelButton> genreButtons[ppm::automaster::kNumGenres];
    SteppedKnob amount, maxGain, ceiling;
    juce::TextButton listen { "LISTEN" }, undo { "UNDO" };
    JewelButton maxOn { "MAXIMIZER", "MAX OFF", col::red };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> maxOnAtt;
    juce::Rectangle<float> genreArea, display, chart, grMeter, maxArea;
    PassiveMasterEQProcessor::AutoState lastState = PassiveMasterEQProcessor::AutoState::Idle;
    int lastGenre = -1, lastAmount = -1;
    float amountTimer = 0.0f, gr = 0.0f, grHold = 0.0f, grHoldTimer = 0.0f, pulse = 0.0f;
};

// One channel of the M/S CLASS-A compressor (ash-blue unit) ------------------------
class ClassAStrip : public juce::Component
{
public:
    ClassAStrip (juce::AudioProcessorValueTreeState&, int ch);
    void resized() override;
    void paint (juce::Graphics&) override;
    void setMeter (float grDb, bool active) { meter.setTarget (grDb, active ? 1.0f : 0.0f); }
    void tickMeter (float dt) { meter.tick (dt); }
    void setTitle (const juce::String& t, juce::Colour c) { if (t != title || c != titleColour) { title = t; titleColour = c; repaint(); } }
    void setCompressorEnabled (bool e);                 // LINK: the right compressor controls follow the left
    void refreshReadouts() { ratio.refresh(); eqFreq.refresh(); }
private:
    juce::AudioProcessorValueTreeState& state;
    int ch;
    AnalogMeter meter;
    SteppedKnob thresh, attack, release, ratio, gain, scGain, scFreq, eqGain, eqFreq, mix;
    JewelButton ff { "FEED FWD", "FEEDBACK", col::amber }, afa { "AUTO FAST ATT", "AUTO FAST ATT", col::amber },
                afr { "AUTO FAST REL", "AUTO FAST REL", col::amber }, warm { "WARM", "WARM", col::amber },
                scOn { "SC ON", "SC OFF", col::blue }, eqOn { "EQ ON", "EQ OFF", col::blue }, eqX10 { "x10", "x1", col::blue };
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> atts;
    juce::Rectangle<int> filterRow, buttonRow;
    juce::String title;
    juce::Colour titleColour = col::engrave;
};

class ClassAPanel : public juce::Component
{
public:
    explicit ClassAPanel (PassiveMasterEQProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void update (float dt);
private:
    PassiveMasterEQProcessor& proc;
    ClassAStrip strips[2];
    ModeSwitch mode;
    SteppedKnob clip;
    JewelButton inButton { "ACTIVE", "BYPASS", col::green }, link { "LINK", "UNLINKED", col::yellow },
                clipOn { "SOFT CLIP", "SOFT CLIP", col::red };
    JewelLamp clipLed { "CLIP", col::red };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> inAtt, linkAtt, clipAtt;
    juce::Rectangle<int> centre;
    int clipHold = 0;
};

// Parametric mastering EQ (black unit) ----------------------------------------------
// Knob with a glossy black cap and silver knurled skirt, printed scale around it, red index mark
class SkirtKnob : public juce::Component, public juce::SettableTooltipClient
{
public:
    SkirtKnob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, juce::StringArray scaleLabels, int labelEvery = 1);
    void paint (juce::Graphics&) override;
    void resized() override;
    void setTooltip (const juce::String& t) { SettableTooltipClient::setTooltip (t); slider.setTooltip (t); }
    juce::Slider slider;
    float knobFraction = 0.62f;                  // knob diameter relative to the component
private:
    juce::StringArray labels;
    int every = 1;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// round red PUSH EQ button
class PushEqButton : public juce::ToggleButton
{
public:
    PushEqButton() { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

class ParametricPanel : public juce::Component
{
public:
    explicit ParametricPanel (PassiveMasterEQProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void update();
private:
    struct Channel
    {
        std::unique_ptr<SkirtKnob> shape[3], freq[3], level[3], hiShelf, loShelf;
        std::unique_ptr<MetalToggle> loFreq, hiFreq;
        juce::Rectangle<int> shelfCol, bandCol[3];
    };
    PassiveMasterEQProcessor& proc;
    Channel chans[2];
    std::unique_ptr<MetalToggle> man[2];
    PushEqButton pushEq;
    JewelLamp ledL { "LEFT", col::green }, ledR { "RIGHT", col::green }, ledOn { "", col::green };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pushAtt;
    juce::Rectangle<int> centreCol, legendCol, badgeL, badgeR;
    bool lastLinked = false;
};

// ---- pop-out units ------------------------------------------------------------------
// The passive EQ as a self-contained unit (both channel rows)
class PassiveEqUnit : public juce::Component
{
public:
    explicit PassiveEqUnit (PassiveMasterEQProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void update();
    static constexpr int kRowH = 222, kGap = 10;
private:
    PassiveMasterEQProcessor& proc;
    std::unique_ptr<BandStrip> bands[2][ppm::kNumBands];
    std::unique_ptr<CenterStrip> centers[2];
    juce::Rectangle<float> rows[2];
    bool lastLinked = true;
};

// The vari-mu compressor as a self-contained unit
class VariMuUnit : public juce::Component
{
public:
    explicit VariMuUnit (PassiveMasterEQProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void update (float dt);
private:
    PassiveMasterEQProcessor& proc;
    std::unique_ptr<CompStrip> comps[2];
    ModeSwitch orderSwitch;
};

// Hosts one unit on its own plate(s) at a fixed design size, scaled to the window
class UnitHost : public juce::Component, private juce::Timer
{
public:
    UnitHost (std::unique_ptr<juce::Component> unit, int designW, int designH, std::vector<juce::Rectangle<float>> plates,
              juce::Colour tint, std::function<void (float)> update);
    ~UnitHost() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Component& getUnit() { return *unit; }
    void tick() { timerCallback(); }
private:
    void timerCallback() override { if (update) update (1.0f / 30.0f); }
    std::unique_ptr<juce::Component> unit;
    int dw, dh;
    std::vector<juce::Rectangle<float>> plates;
    juce::Colour tint;
    std::function<void (float)> update;
    juce::Image metal;
};

class UnitWindow : public juce::DocumentWindow
{
public:
    UnitWindow (const juce::String& title, std::unique_ptr<UnitHost> host, juce::LookAndFeel& laf, std::function<void()> onClose);
    ~UnitWindow() override;
    void closeButtonPressed() override { if (onClose) onClose(); }
private:
    std::function<void()> onClose;
};

// small chrome "pop out" icon button
class PopOutButton : public juce::Button
{
public:
    PopOutButton() : juce::Button ("pop out") {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

// HELP overlay: a printed "manual card" explaining the panel --------------------
class HelpOverlay : public juce::Component
{
public:
    HelpOverlay();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { setVisible (false); }
};
// Small L/R LED input meter that sits under the INPUT knob -----------------------
class InputMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setLevels (float peakL, float peakR, float dt);
    void paint (juce::Graphics&) override;
private:
    float level[2] { -80.0f, -80.0f }, hold[2] { -80.0f, -80.0f }, holdTimer[2] {};
};

// The whole panel, drawn at a fixed design size (kW x kH); the editor scales it.
class Faceplate : public juce::Component, private juce::Timer
{
public:
    static constexpr int kW = 1440, kH = 2380;
    static constexpr int kHCompact = 594;        // MINI view: meters, displays and footer only

    explicit Faceplate (PassiveMasterEQProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

    void simulateMeters (float vuL, float vuR, float thdPercent);   // screenshot test
    void animate (int frames) { for (int i = 0; i < frames; ++i) timerCallback(); }   // tests: advance the displays
    const SpectrumAnalyzer& getAnalyzer() const { return analyzer; }
    const Vectorscope& getVectorscope() const { return vectorscope; }
    const KeyPanel& getKeyPanel() const { return keyPanel; }
    void showHelp() { help.setVisible (true); help.toFront (false); }
    void setScaleChoice (float scale);
    std::function<void (float)> onScaleRequested;                  // set by the editor
    std::function<void()> onToggleFullScreen, onToggleCompact, onToggleScroll;      // set by the editor
    void setCompact (bool c);
    bool isCompact() const { return compact; }
    void updatePresetTip();
    void setFullScreenState (bool fs) { fullButton.setButtonText (fs ? "EXIT FULL" : "FULL SCREEN"); }
    void setScrollState (bool on) { scrollButton.setToggleState (on, juce::dontSendNotification); scrollButton.setButtonText (on ? "SCROLL ON" : "SCROLL"); }

    enum Unit { UnitParametric = 0, UnitPassive, UnitVariMu, UnitClassA, UnitAuto, kNumUnits };
    void openUnit (int unit);                                       // pop a unit out into its own window
    std::unique_ptr<UnitHost> createUnitHost (int unit);            // (also used by the tests for snapshots)
    static juce::String unitName (int unit);

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

    AudioHistory history;
    SpectrumAnalyzer analyzer { history };
    Vectorscope vectorscope { history };
    LoudnessPanel loudnessPanel { proc };
    KeyPanel keyPanel { proc, history };
    AutoMasterPanel autoPanel { proc };
    ClassAPanel classAPanel { proc };
    ParametricPanel paraPanel { proc };
    PopOutButton popButtons[kNumUnits];
    std::unique_ptr<UnitWindow> unitWindows[kNumUnits];
    std::unique_ptr<BandStrip>   bands[2][ppm::kNumBands];
    std::unique_ptr<CenterStrip> centers[2];
    std::unique_ptr<CompStrip>   comps[2];
    ModeSwitch orderSwitch;

    JewelButton linkButton { "LINK", "UNLINKED", col::yellow };
    JewelButton tubeButton { "TUBE ON", "TUBE OFF", col::amber };
    juce::TextButton copyLR { "L > R" }, copyRL { "R > L" }, helpButton { "HELP  ?" };
    juce::ComboBox presetBox, sizeBox;
    SteppedKnob drive, inputKnob;
    InputMeter inputMeter;
    juce::TextButton fullButton { "FULL SCREEN" }, miniButton { "MINI" }, scrollButton { "SCROLL" };
    HelpOverlay help;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> linkAtt, tubeAtt;

    juce::Rectangle<float> topPlate, curvePlate, analyzerScreen, scopeScreen, rowPlates[2], compPlate, classAPlate, autoPlate, paraPlate, footerPlate, nameplate, presetLabel;
    bool lastLinked = true, compact = false;
    int clipHold = 0;
    int lastProgram = -1;
    float flicker = 0.0f;
    bool simulated = false;
    juce::Random rng;
};
// Grab handle on the left or right edge: drag sideways to make the whole plug-in bigger or smaller
class EdgeGrip : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit EdgeGrip (bool rightEdge);
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; repaint(); }
    std::function<void (int deltaX, int deltaY, bool rightEdge, bool start)> onDrag;
private:
    bool right, dragging = false;
    int startX = 0, startY = 0;
};

// Window shape rules: never narrower than the panel's own shape (it shrinks instead),
// and up to kMaxWide times wider (the panel spreads out to fill the width).
class WidthRangeConstrainer : public juce::ComponentBoundsConstrainer
{
public:
    double minRatio = 0.0, maxRatio = 0.0;            // width / height, 0 = no limit
    void checkBounds (juce::Rectangle<int>& b, const juce::Rectangle<int>& prev, const juce::Rectangle<int>& limits,
                      bool top, bool left, bool bottom, bool right) override;
};
} // namespace ui

namespace ui
{
// Faint fade + arrow at the top / bottom edge while the panel can scroll that way (hover there to scroll)
class ScrollHints : public juce::Component
{
public:
    ScrollHints() { setInterceptsMouseClicks (false, false); }
    // can[i] / hot[i]: 0 up, 1 down, 2 left, 3 right
    void set (const bool* canDir, const float* hotDir, float zoomNow)
    {
        bool same = std::abs (zoomNow - zoom) < 0.001f;
        for (int i = 0; i < 4; ++i) same = same && can[i] == canDir[i] && std::abs (hot[i] - hotDir[i]) < 0.01f;
        if (same) return;
        for (int i = 0; i < 4; ++i) { can[i] = canDir[i]; hot[i] = hotDir[i]; }
        zoom = zoomNow; repaint();
    }
    void paint (juce::Graphics&) override;
    static constexpr int kZone = 44;                   // hover this close to an edge to scroll / pan
private:
    bool can[4] {};
    float hot[4] {}, zoom = 1.0f;
};
} // namespace ui

class PassiveMasterEQEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PassiveMasterEQEditor (PassiveMasterEQProcessor&);
    ~PassiveMasterEQEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    void simulateMeters (float vuL, float vuR, float thd) { face.simulateMeters (vuL, vuR, thd); }
    void showHelp() { face.showHelp(); }
    void animate (int frames) { face.animate (frames); }
    float analyzerBandDb (int band) const { return face.getAnalyzer().bandLevelDb (band); }
    float vectorscopeCorrelation() const  { return face.getVectorscope().getCorrelation(); }
    float vectorscopeBalance() const      { return face.getVectorscope().getBalance(); }
    int detectedKey() const               { return face.getKeyPanel().getDetected().key; }
    int shiftedKey() const                { return face.getKeyPanel().getShiftedKey(); }
    static constexpr double kMaxWide = 2.2;     // the panel can become up to 2.2x its standard width
    int panelWidth() const { return face.getWidth(); }           // tests
    static int designWidth()  { return ui::Faceplate::kW; }
    static int designHeight() { return ui::Faceplate::kH; }
    void parentHierarchyChanged() override;
    void toggleFullScreen();
    void toggleCompact();
    void toggleScroll();
    bool isScrollMode() const { return scrollMode; }
    double scrollMax() const;                                   // how far the panel can scroll (panel pixels)
    void scrollTo (double pos) { scrollPos = pos; resized(); } // tests
    double scrollPosition() const { return scrollPos; }
    void hoverScrollStep (int mouseY, double seconds) { hoverScroll ((float) getWidth() * 0.5f, (float) mouseY, seconds); }   // tests
    void hoverPanStep (int mouseX, int mouseY, double seconds) { hoverScroll ((float) mouseX, (float) mouseY, seconds); }       // tests
    // click & zoom: double-click an empty spot to zoom in there (2x); double-click again to zoom back out.
    // Ctrl/Cmd + mouse wheel zooms smoothly (1x - 4x). While zoomed: hover an edge, drag an empty spot or use the wheel to look around.
    void zoomAt (juce::Point<float> where, float newZoom);
    void toggleZoomAt (juce::Point<float> where) { zoomAt (where, zoom > 1.5f ? 1.0f : 2.0f); }
    float zoomFactor() const { return zoom; }
    juce::Rectangle<float> visiblePanelArea() const;           // the part of the panel in view (panel pixels)
    static constexpr float kMaxZoom = 4.0f;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    static constexpr int kScrollBarW = 0;                     // no scroll bar: hover at the top / bottom edge (or use the wheel)
    void testEdgeDrag (int deltaX, int deltaY, bool rightEdge) { edgeDrag (0, 0, rightEdge, true); edgeDrag (deltaX, deltaY, rightEdge, false); }   // tests

private:
    int designH() const { return face.isCompact() ? ui::Faceplate::kHCompact : ui::Faceplate::kH; }
public:
    std::unique_ptr<ui::UnitHost> createUnitHost (int unit) { return face.createUnitHost (unit); }   // tests
private:
    void updateLimits();
    juce::DocumentWindow* standaloneWindow() const;
    PassiveMasterEQProcessor& proc;
    ui::MasterdeskLookAndFeel laf;
    ui::Faceplate face;
    juce::TooltipWindow tooltips { this, 500 };
    ui::EdgeGrip leftGrip { false }, rightGrip { true };
    juce::Rectangle<int> dragStartBounds, dragStartWindow;
    void edgeDrag (int deltaX, int deltaY, bool rightEdge, bool start);
    ui::WidthRangeConstrainer shapeConstrainer;
    bool sizeRestored = false, standaloneSetupDone = false, pluginFullScreen = false;
    // SCROLL mode: the panel fills the window's width and scrolls up/down (hover at the top / bottom edge, or the wheel)
    ui::ScrollHints hints;
    double lastTick = 0.0;
    void timerCallback() override;
    void hoverScroll (float mouseX, float mouseY, double seconds);
    // zoom state: factor on top of the normal fit, and the panel point at the window's top-left (panel pixels)
    float zoom = 1.0f;
    double zx = 0.0, zy = 0.0;
    float baseScale() const;
    juce::Point<float> panelPointAt (juce::Point<float> editorPos) const { return face.getLocalPoint (this, editorPos); }
    bool isEmptySpot (juce::Component* c) const;
    struct ZoomMouse : public juce::MouseListener      // watches clicks on the whole panel (not the wheel)
    {
        explicit ZoomMouse (PassiveMasterEQEditor& e) : ed (e) {}
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        PassiveMasterEQEditor& ed;
        bool panning = false;
        juce::Point<float> last;
    } zoomMouse { *this };
    bool scrollMode = false;
    double scrollPos = 0.0;                                     // top of the visible part, in panel pixels
    bool scrolling() const { return scrollMode && ! face.isCompact(); }
    juce::Rectangle<int> sizeBeforeFull;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PassiveMasterEQEditor)
};
