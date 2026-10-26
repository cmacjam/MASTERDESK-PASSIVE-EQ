// =============================================================================
//  Params.h  —  parameter IDs, layout, and APVTS -> ChannelSettings
//  Every hardware control is a discrete (choice/bool) parameter, so host
//  automation snaps to detents exactly like the stepped hardware.
// =============================================================================
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Model.h"

namespace ppm::params
{
inline const char* chanPrefix (int ch) { return ch == 0 ? "L_" : "R_"; }

inline juce::String bandId (int ch, int band, const char* what)
{
    return juce::String (chanPrefix (ch)) + "b" + juce::String (band + 1) + "_" + what;
}
inline juce::String chanId (int ch, const char* what) { return juce::String (chanPrefix (ch)) + what; }

inline const juce::String link      { "link" };
inline const juce::String tubeOn    { "tube_on" };
inline const juce::String tubeDrive { "tube_drive" };

inline juce::String formatHz (double hz)
{
    if (hz <= 0.0)     return "OFF";
    if (hz >= 1000.0)
    {
        const double k = hz / 1000.0;
        return (std::abs (k - std::round (k)) < 1e-6 ? juce::String ((int) std::round (k))
                                                     : juce::String (k, 1)) + "k";
    }
    return juce::String ((int) std::round (hz));
}

// Every parameter ID belonging to one channel (used for L->R / R->L copy).
inline juce::StringArray channelParamSuffixes()
{
    juce::StringArray s;
    for (int b = 0; b < kNumBands; ++b)
        for (auto* w : { "freq", "gain", "bw", "mode", "shelf" })
            s.add ("b" + juce::String (b + 1) + "_" + w);
    s.addArray ({ "hp", "lp", "trim", "eq_in" });
    return s;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    int version = 1;

    for (int ch = 0; ch < 2; ++ch)
    {
        const String chName = ch == 0 ? "L " : "R ";
        auto group = std::make_unique<AudioProcessorParameterGroup> (chanPrefix (ch), ch == 0 ? "Left" : "Right", " | ");

        for (int b = 0; b < kNumBands; ++b)
        {
            const String bn = chName + kBandNames[b] + " ";

            StringArray freqs;
            for (double f : kBandFreqs[b]) freqs.add (formatHz (f));
            StringArray steps;
            for (int i = 0; i < kNumKnobSteps; ++i) steps.add (String (i));

            group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { bandId (ch, b, "freq"), version }, bn + "Freq", freqs, 5));
            group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { bandId (ch, b, "gain"), version }, bn + "Gain", steps, 0));
            group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { bandId (ch, b, "bw"),   version }, bn + "Bandwidth", steps, 7));
            group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { bandId (ch, b, "mode"), version }, bn + "Mode",
                                                                     StringArray { "Boost", "Out", "Cut" }, 0));
            group->addChild (std::make_unique<AudioParameterBool>   (ParameterID { bandId (ch, b, "shelf"), version }, bn + "Shelf", false));
        }

        StringArray hp, lp, trim;
        for (double f : kHpFreqs) hp.add (formatHz (f));
        for (double f : kLpFreqs) lp.add (formatHz (f));
        for (int i = 0; i < kNumTrimSteps; ++i) trim.add (String (trimDb (i), 1) + " dB");

        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { chanId (ch, "hp"),   version }, chName + "High Pass", hp, 0));
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { chanId (ch, "lp"),   version }, chName + "Low Pass",  lp, 0));
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { chanId (ch, "trim"), version }, chName + "Gain Trim", trim, 5));
        group->addChild (std::make_unique<AudioParameterBool>   (ParameterID { chanId (ch, "eq_in"), version }, chName + "EQ In", true));

        layout.add (std::move (group));
    }

    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { link, version },   "Stereo Link", true));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { tubeOn, version }, "Tube Stage", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { tubeDrive, version }, "Tube Drive",
                                                       NormalisableRange<float> (-6.0f, 18.0f, 0.5f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}

// Cached raw pointers for lock-free reads on the audio thread.
struct ChannelParamPtrs
{
    struct Band { std::atomic<float>* freq; std::atomic<float>* gain; std::atomic<float>* bw;
                  std::atomic<float>* mode; std::atomic<float>* shelf; } band[kNumBands];
    std::atomic<float>* hp; std::atomic<float>* lp; std::atomic<float>* trim; std::atomic<float>* eqIn;

    void init (juce::AudioProcessorValueTreeState& s, int ch)
    {
        for (int b = 0; b < kNumBands; ++b)
            band[b] = { s.getRawParameterValue (bandId (ch, b, "freq")), s.getRawParameterValue (bandId (ch, b, "gain")),
                        s.getRawParameterValue (bandId (ch, b, "bw")),   s.getRawParameterValue (bandId (ch, b, "mode")),
                        s.getRawParameterValue (bandId (ch, b, "shelf")) };
        hp = s.getRawParameterValue (chanId (ch, "hp"));
        lp = s.getRawParameterValue (chanId (ch, "lp"));
        trim = s.getRawParameterValue (chanId (ch, "trim"));
        eqIn = s.getRawParameterValue (chanId (ch, "eq_in"));
    }

    ChannelSettings read() const
    {
        auto i = [] (std::atomic<float>* p) { return (int) std::lround (p->load()); };
        ChannelSettings c;
        for (int b = 0; b < kNumBands; ++b)
            c.band[b] = { i (band[b].freq), i (band[b].gain), i (band[b].bw), i (band[b].mode), band[b].shelf->load() > 0.5f };
        c.hp = i (hp); c.lp = i (lp); c.trim = i (trim); c.eqIn = eqIn->load() > 0.5f;
        return c;
    }
};
} // namespace ppm::params
