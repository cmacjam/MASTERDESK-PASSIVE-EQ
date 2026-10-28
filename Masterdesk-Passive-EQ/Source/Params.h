// =============================================================================
//  Params.h  —  parameter IDs, layout, and APVTS -> ChannelSettings
//  Every hardware control is a discrete (choice/bool) parameter, so host
//  automation snaps to detents exactly like the stepped hardware.
// =============================================================================
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Model.h"
#include "dsp/VariMu.h"
#include "dsp/ClassAComp.h"

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
inline const juce::String compOrder { "comp_order" };   // 0 = EQ > COMP, 1 = COMP > EQ
inline const juce::String inputGain { "input_gain" };   // dB, applied before everything
inline const juce::String keyOn     { "key_on" };       // key shifter on/off
inline const juce::String keySemis  { "key_semis" };    // -12 .. +12 semitones
inline const juce::String keyCents  { "key_cents" };    // -50 .. +50 cents
inline const juce::String keyFormant { "key_formant" }; // keep formants while shifting (natural timbre)
inline const juce::String maxOn     { "max_on" };       // MAXIMIZER (final limiter) on/off
inline const juce::String maxGain   { "max_gain" };     // dB pushed into the limiter
inline const juce::String maxCeiling { "max_ceiling" }; // highest peak allowed out, dBFS
inline const juce::String amGenre   { "am_genre" };     // AUTO MASTER genre
inline const juce::String amAmount  { "am_amount" };    // AUTO MASTER EQ amount, %

// M/S CLASS-A compressor (ash-blue unit). Channel 1 = Left or Mid, channel 2 = Right or Side.
namespace ca
{
    inline const juce::String in { "x_in" }, mode { "x_mode" }, link { "x_link" }, clipOn { "x_clipon" }, clip { "x_clip" };
    inline juce::String chan (int ch, const char* what) { return juce::String ("x_") + what + juce::String (ch + 1); }   // x_thr1, x_att2 ...
}

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
    s.addArray ({ "c_in", "c_thresh", "c_attack", "c_recovery", "c_output", "c_mix", "c_mode", "c_schp" });
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

        // ---- variable-mu compressor ----
        StringArray thr, att, rec, out, mix;
        for (int i = 0; i < kNumThreshSteps; ++i)   thr.add (String (compThresholdDb (i), 1) + " dB");
        for (double a : kAttackMs)                  att.add (String ((int) a) + " ms");
        for (double r : kRecoverySec)               rec.add (String (r, 1) + " s");
        for (int i = 0; i < kNumOutputSteps; ++i)   out.add ("+" + String (compOutputDb (i), 1) + " dB");
        for (int i = 0; i < kNumMixSteps; ++i)      mix.add (String (i * 10) + " %");
        const CompSettings d;
        group->addChild (std::make_unique<AudioParameterBool>   (ParameterID { chanId (ch, "c_in"), version },       chName + "Comp In", d.compIn));
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { chanId (ch, "c_thresh"), version },   chName + "Comp Threshold", thr, d.threshold));
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { chanId (ch, "c_attack"), version },   chName + "Comp Attack", att, d.attack));
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { chanId (ch, "c_recovery"), version }, chName + "Comp Recovery", rec, d.recovery));
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { chanId (ch, "c_output"), version },   chName + "Comp Output", out, d.output));
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { chanId (ch, "c_mix"), version },      chName + "Comp Mix", mix, d.mix));
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { chanId (ch, "c_mode"), version },     chName + "Comp Mode",
                                                                 StringArray { "Compress", "Limit" }, d.mode));
        group->addChild (std::make_unique<AudioParameterBool>   (ParameterID { chanId (ch, "c_schp"), version },     chName + "Comp SC HP", d.scHp));

        layout.add (std::move (group));
    }

    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { link, version },   "Stereo Link", true));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { tubeOn, version }, "Tube Stage", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { tubeDrive, version }, "Tube Drive",
                                                       NormalisableRange<float> (-6.0f, 18.0f, 0.5f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { inputGain, version }, "Input Gain",
                                                       NormalisableRange<float> (-24.0f, 24.0f, 0.5f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { keyOn, version }, "Key Shift On", false));
    layout.add (std::make_unique<AudioParameterInt>  (ParameterID { keySemis, version }, "Key Shift Semitones", -12, 12, 0,
                                                      AudioParameterIntAttributes().withLabel ("st")));
    layout.add (std::make_unique<AudioParameterInt>  (ParameterID { keyCents, version }, "Key Shift Fine", -50, 50, 0,
                                                      AudioParameterIntAttributes().withLabel ("cents")));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { keyFormant, version }, "Key Shift Formant", true));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { maxOn, version }, "Maximizer On", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { maxGain, version }, "Maximizer Gain",
                                                       NormalisableRange<float> (-12.0f, 18.0f, 0.1f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { maxCeiling, version }, "Maximizer Ceiling",
                                                       NormalisableRange<float> (-3.0f, 0.0f, 0.1f), -1.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dBFS")));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { amGenre, version }, "Auto Master Genre",
                                                        StringArray { "Pop", "Jazz", "70's", "80's", "90's", "R&B", "Hip Hop" }, 0));
    layout.add (std::make_unique<AudioParameterInt>  (ParameterID { amAmount, version }, "Auto Master Amount", 0, 100, 60,
                                                      AudioParameterIntAttributes().withLabel ("%")));
    // ---- M/S CLASS-A compressor (controls and ranges follow the hardware's front panel) ----
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> ("classa", "M/S Class-A Compressor", " | ");
        auto fl = [&g] (const String& id, const String& name, NormalisableRange<float> r, float def, const char* label)
        { g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withLabel (label))); };
        auto bl = [&g] (const String& id, const String& name, bool def) { g->addChild (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
        bl (ca::in, "Class-A Active", false);
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ca::mode, version }, "Class-A M/S", StringArray { "Stereo", "M/S" }, 0));
        bl (ca::link, "Class-A Link", true);
        bl (ca::clipOn, "Class-A Soft Clip", false);
        fl (ca::clip, "Class-A Soft Clip Threshold", NormalisableRange<float> (-12.0f, 0.0f, 0.1f), 0.0f, "dB");
        for (int ch = 0; ch < 2; ++ch)
        {
            const String n = ch == 0 ? "Class-A L/Mid " : "Class-A R/Side ";
            auto skew = [] (float lo, float hi, float step, float centre) { NormalisableRange<float> r (lo, hi, step); r.setSkewForCentre (centre); return r; };
            fl (ca::chan (ch, "thr"),   n + "Threshold", NormalisableRange<float> (-31.0f, 2.0f, 0.1f), -10.0f, "dB");
            fl (ca::chan (ch, "att"),   n + "Attack",    skew (0.01f, 150.0f, 0.01f, 13.0f), 13.0f, "ms");
            fl (ca::chan (ch, "rel"),   n + "Release",   skew (60.0f, 1800.0f, 1.0f, 430.0f), 300.0f, "ms");
            fl (ca::chan (ch, "ratio"), n + "Ratio",     NormalisableRange<float> (0.0f, (float) ppm::kClassARatioTop, 0.01f), 6.0f, "");
            fl (ca::chan (ch, "gain"),  n + "Gain",      NormalisableRange<float> (0.0f, 12.0f, 0.1f), 0.0f, "dB");
            fl (ca::chan (ch, "mix"),   n + "Mix",       NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, "%");
            bl (ca::chan (ch, "ff"),    n + "Feed Forward", false);
            bl (ca::chan (ch, "afa"),   n + "Auto Fast Attack", false);
            bl (ca::chan (ch, "afr"),   n + "Auto Fast Release", false);
            bl (ca::chan (ch, "warm"),  n + "Warm", false);
            bl (ca::chan (ch, "scon"),  n + "SC Filter", false);
            fl (ca::chan (ch, "scg"),   n + "SC Gain (LP..HP)", NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f, "");
            fl (ca::chan (ch, "scf"),   n + "SC Freq",   skew (30.0f, 3300.0f, 1.0f, 340.0f), 280.0f, "Hz");
            bl (ca::chan (ch, "eqon"),  n + "Niveau Filter", false);
            fl (ca::chan (ch, "eqg"),   n + "Niveau Gain", NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f, "");
            fl (ca::chan (ch, "eqf"),   n + "Niveau Freq", skew (20.0f, 2000.0f, 1.0f, 220.0f), 500.0f, "Hz");
            bl (ca::chan (ch, "eqx"),   n + "Niveau x10", false);
        }
        layout.add (std::move (g));
    }
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { compOrder, version }, "Comp Order",
                                                        StringArray { "EQ > COMP", "COMP > EQ" }, 0));
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
struct CompParamPtrs
{
    std::atomic<float>* in; std::atomic<float>* thr; std::atomic<float>* att; std::atomic<float>* rec;
    std::atomic<float>* out; std::atomic<float>* mix; std::atomic<float>* mode; std::atomic<float>* schp;

    void init (juce::AudioProcessorValueTreeState& s, int ch)
    {
        in  = s.getRawParameterValue (chanId (ch, "c_in"));     thr  = s.getRawParameterValue (chanId (ch, "c_thresh"));
        att = s.getRawParameterValue (chanId (ch, "c_attack")); rec  = s.getRawParameterValue (chanId (ch, "c_recovery"));
        out = s.getRawParameterValue (chanId (ch, "c_output")); mix  = s.getRawParameterValue (chanId (ch, "c_mix"));
        mode = s.getRawParameterValue (chanId (ch, "c_mode"));  schp = s.getRawParameterValue (chanId (ch, "c_schp"));
    }

    CompSettings read() const
    {
        auto i = [] (std::atomic<float>* p) { return (int) std::lround (p->load()); };
        CompSettings c;
        c.compIn = in->load() > 0.5f; c.threshold = i (thr); c.attack = i (att); c.recovery = i (rec);
        c.output = i (out); c.mix = i (mix); c.mode = i (mode); c.scHp = schp->load() > 0.5f;
        return c;
    }
};

struct ClassAParamPtrs
{
    std::atomic<float>* in; std::atomic<float>* mode; std::atomic<float>* link; std::atomic<float>* clipOn; std::atomic<float>* clip;
    struct Ch { std::atomic<float>* p[17]; } ch[2];
    static constexpr const char* names[17] = { "thr", "att", "rel", "ratio", "gain", "mix", "ff", "afa", "afr", "warm",
                                               "scon", "scg", "scf", "eqon", "eqg", "eqf", "eqx" };

    void init (juce::AudioProcessorValueTreeState& s)
    {
        in = s.getRawParameterValue (ca::in); mode = s.getRawParameterValue (ca::mode); link = s.getRawParameterValue (ca::link);
        clipOn = s.getRawParameterValue (ca::clipOn); clip = s.getRawParameterValue (ca::clip);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 17; ++i) ch[c].p[i] = s.getRawParameterValue (ca::chan (c, names[i]));
    }

    ClassASettings read() const
    {
        ClassASettings st;
        st.in = in->load() > 0.5f; st.mode = (int) std::lround (mode->load()); st.link = link->load() > 0.5f;
        st.softClip = clipOn->load() > 0.5f; st.clipDb = clip->load();
        for (int c = 0; c < 2; ++c)
        {
            auto v = [&] (int i) { return (double) ch[c].p[i]->load(); };
            auto b = [&] (int i) { return ch[c].p[i]->load() > 0.5f; };
            auto& o = st.ch[c];
            o.thresholdDb = v (0); o.attackMs = v (1); o.releaseMs = v (2); o.ratioPos = v (3); o.gainDb = v (4); o.mix = v (5) / 100.0;
            o.feedForward = b (6); o.autoFastAttack = b (7); o.autoFastRelease = b (8); o.warm = b (9);
            o.scOn = b (10); o.scTilt = v (11); o.scFreq = v (12);
            o.eqOn = b (13); o.eqTilt = v (14); o.eqFreq = v (15); o.eqX10 = b (16);
        }
        return st;
    }
};
} // namespace ppm::params
