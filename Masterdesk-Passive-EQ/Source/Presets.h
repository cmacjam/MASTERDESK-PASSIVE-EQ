// =============================================================================
//  Presets.h  —  factory mastering starting points
//
//  Indices below refer to switch positions:
//    LOW      freq: 22 33 47 68 100 150 220 330 470 680 1k        (0..10)
//    LOW MID  freq: 82 120 180 270 390 560 820 1.2k 1.8k 2.7k 3.9k
//    HIGH MID freq: 220 330 470 680 1k 1.5k 2.2k 3.3k 4.7k 6.8k 10k
//    HIGH     freq: 560 820 1.2k 1.8k 2.7k 3.9k 5.6k 8.2k 12k 16k 27k
//    gain / bandwidth: 0..15 (bandwidth 0 = WIDE, 15 = NARROW)
//    HP: 0 OFF, 1 12, 2 16, 3 23, 4 30, 5 39 Hz
//    LP: 0 OFF, 1 52k, 2 40k, 3 27k, 4 20k, 5 15k
// =============================================================================
#pragma once
#include "dsp/Model.h"
#include "dsp/VariMu.h"
#include "dsp/SontecEQ.h"
#include "dsp/ClassAComp.h"
#include <algorithm>
#include <string>
#include <vector>

namespace ppm::presets
{
struct Preset
{
    std::string name;
    ChannelSettings ch;
    bool  tubeOn  = true;
    float driveDb = 0.0f;
    CompSettings comp {};      // default: compressor in, threshold 0 dB (transparent)
    int   order   = 0;         // 0 = EQ > COMP, 1 = COMP > EQ
    // engineer presets set every unit (parametric EQ and Class-A too); factory presets leave those alone
    std::string category = "Factory";
    std::string notes;
    bool full = false;
    sontec::Settings pq {};
    ClassASettings   ca {};
};

//            in    thr  att  rec  out  mix  mode      scHp
inline CompSettings comp (int thr, int att, int rec, int out, int mix = 10, int mode = Compress, bool scHp = true)
{
    CompSettings c; c.compIn = true; c.threshold = thr; c.attack = att; c.recovery = rec;
    c.output = out; c.mix = mix; c.mode = mode; c.scHp = scHp;
    return c;
}

inline BandSettings band (int freq, int gain, int bw, int mode = Boost, bool shelf = false)
{
    return { freq, gain, bw, mode, shelf };
}
inline BandSettings flat() { return band (5, 0, 7, Boost, false); }

inline ChannelSettings channel (BandSettings low, BandSettings lowMid, BandSettings highMid, BandSettings high,
                                int hp = 0, int lp = 0, int trim = 5)
{
    ChannelSettings c;
    c.band[0] = low; c.band[1] = lowMid; c.band[2] = highMid; c.band[3] = high;
    c.hp = hp; c.lp = lp; c.trim = trim; c.eqIn = true;
    return c;
}

} // namespace ppm::presets
#include "EngineerPresets.h"
namespace ppm::presets
{
inline const char* const kEngineerCategories[] = { "Vocals", "Drums", "Bass", "Guitars & Keys", "Mix & Master Bus" };
inline const char* const kTrackCategories[]    = { "Track: Keys & Pads", "Track: Guitars", "Track: Drums" };
inline bool isTrackCategory (const std::string& c) { return c.rfind ("Track: ", 0) == 0; }

// ---- every preset sits in one category ----------------------------------------------------------
//  Master: a stereo-master EQ curve that loads a complete chain (both EQs, both compressors, maximizer)
//  Compressor: a compressor setting (vocals, drums, ... or the mix bus)
//  Track: an EQ + compressor for one instrument on its own track
inline const char* const kCategoryOrder[] = {
    "Master: Clean & Corrective", "Master: Balanced Polish", "Master: Warm & Analog", "Master: Bright & Open", "Master: Loud & Punchy",
    "Compressor: Mix Bus", "Compressor: Vocals", "Compressor: Drums", "Compressor: Bass", "Compressor: Guitars & Keys",
    "Track: Keys & Pads", "Track: Guitars", "Track: Drums" };
inline bool isMasterCategory (const std::string& c)     { return c.rfind ("Master: ", 0) == 0; }
inline bool isCompressorCategory (const std::string& c) { return c.rfind ("Compressor: ", 0) == 0; }
inline int categoryRank (const std::string& c)
{
    int i = 0;
    for (const char* k : kCategoryOrder) { if (c == k) return i; ++i; }
    return i;
}
// the mastering character of each stereo-master curve (chosen from its measured shape)
inline std::string masterCategoryOf (const std::string& name)
{
    static const char* const clean[]  = { "De-Mud", "Smooth Top / De-Harsh", "Subsonic Clean-up Only", "Engineer Move - Mud Scoop (270 Hz)",
                                          "Engineer Move - Box Clear (470 Hz)", "Engineer Move - Harsh Mix Tame", "Engineer Move - Boomy Mix Tighten",
                                          "Engineer Move - Vinyl Pre-Master", "Eric Boulanger - Surgical Mastering EQ" };
    static const char* const polish[] = { "Init (Flat)", "Gentle Polish", "Air & Weight", "Engineer Move - Streaming Translation",
                                          "Joe Chiccarelli - Analog Mastering Baxandall Curve", "Gentle Leveler (Comp > EQ)" };
    static const char* const warm[]   = { "Warm Tube Glue", "Pultec-Style Low End", "Engineer Move - Low-End Tuck (Pultec trick)",
                                          "Engineer Move - Thin Mix Weight", "Engineer Move - Analog Warmth (tubes)", "Mastering Glue (EQ + Vari-Mu)",
                                          "Greg Calbi - Mastering 'Smooth Bass' Anchor" };
    static const char* const loud[]   = { "Bright Pop Master", "Loud & Warm Master", "Parallel Punch", "Engineer Move - Smile Curve (club / loud)" };
    for (const char* n : clean)  if (name == n) return "Master: Clean & Corrective";
    for (const char* n : polish) if (name == n) return "Master: Balanced Polish";
    for (const char* n : warm)   if (name == n) return "Master: Warm & Analog";
    for (const char* n : loud)   if (name == n) return "Master: Loud & Punchy";
    return "Master: Bright & Open";      // Air Lift, Silk Top, Dark Mix Rescue, Vocal Presence / Forward, Calbi Clarity, Chiccarelli Air, Stacked Shelves
}

inline const std::vector<Preset>& all()
{
    static const std::vector<Preset> list = []
    {
    std::vector<Preset> list = {
        { "Init (Flat)",
          channel (flat(), flat(), flat(), flat()) },

        { "Air & Weight",          // the classic: low-end heft + open top
          channel (band (3, 6, 4), flat(), flat(), band (9, 5, 0, Boost, true), 2) },

        { "Gentle Polish",
          channel (band (2, 4, 0, Boost, true), flat(), band (8, 3, 2), band (10, 6, 0, Boost, true), 3, 2) },

        { "De-Mud",
          channel (flat(), band (3, 6, 8, Cut), band (7, 3, 3), flat(), 4) },

        { "Warm Tube Glue",        // mostly the tubes, a touch of tilt
          channel (band (4, 3, 2), flat(), flat(), band (8, 3, 0, Boost, true), 2), true, 9.0f },

        { "Pultec-Style Low End",  // narrow shelf = bump below, dip above
          channel (band (2, 9, 13, Boost, true), band (3, 4, 6, Cut), flat(), flat(), 2) },

        { "Bright Pop Master",
          channel (band (3, 5, 6), band (4, 4, 9, Cut), band (6, 3, 3), band (9, 7, 2, Boost, true), 3), true, 3.0f },

        { "Smooth Top / De-Harsh",
          channel (flat(), flat(), band (8, 5, 10, Cut), band (10, 5, 0, Boost, true), 0, 3) },

        { "Vocal Presence (mix bus)",
          channel (flat(), band (2, 5, 8, Cut), band (7, 6, 9), band (8, 5, 4, Boost, true), 5) },

        { "Subsonic Clean-up Only",
          channel (flat(), flat(), flat(), flat(), 4, 1) },

        // ---- EQ + variable-mu compressor -------------------------------------
        { "Mastering Glue (EQ + Vari-Mu)",       // 1-2 dB of slow, smooth leveling
          channel (band (3, 5, 4), flat(), flat(), band (9, 5, 0, Boost, true), 2),
          true, 0.0f, comp (8, 6, 3, 3) },

        { "Loud & Warm Master",                  // more squeeze, limit mode, tubes pushed
          channel (band (3, 6, 6), band (4, 3, 9, Cut), band (6, 3, 3), band (9, 6, 0, Boost, true), 3),
          true, 6.0f, comp (12, 4, 2, 10, 10, Limit) },

        { "Parallel Punch",                      // heavy compression blended in at 40 %
          channel (band (4, 4, 6), flat(), band (7, 3, 4), band (9, 4, 0, Boost, true), 2),
          true, 3.0f, comp (16, 3, 1, 14, 4) },

        { "Gentle Leveler (Comp > EQ)",          // compress first, then shape tone
          channel (band (2, 4, 0, Boost, true), flat(), flat(), band (10, 5, 0, Boost, true), 2),
          true, 0.0f, comp (10, 7, 5, 4), 1 },

        // ---- Engineer moves: classic mastering-room EQ moves on this EQ ------
        //      (the well-known techniques, not any one engineer's settings)
        { "Engineer Move - Air Lift (27k shelf)",          // the famous passive "air" band: wide shelf way up top
          channel (flat(), flat(), flat(), band (10, 5, 0, Boost, true), 2) },

        { "Engineer Move - Silk Top (16k up, 3.3k down)",  // more top without more harshness
          channel (flat(), flat(), band (7, 3, 6, Cut), band (9, 4, 0, Boost, true), 2) },

        { "Engineer Move - Low-End Tuck (Pultec trick)",   // shelf boost with the dip-and-bump, mud trimmed above it
          channel (band (3, 5, 12, Boost, true), band (2, 3, 6, Cut), flat(), flat(), 3) },

        { "Engineer Move - Mud Scoop (270 Hz)",            // clears the low mids, a touch of top to balance
          channel (flat(), band (3, 4, 6, Cut), flat(), band (8, 3, 0, Boost, true), 2) },

        { "Engineer Move - Box Clear (470 Hz)",            // removes the cardboard / boxy tone
          channel (flat(), flat(), band (2, 4, 8, Cut), flat(), 2) },

        { "Engineer Move - Vocal Forward (2.2k)",          // broad presence lift, low mids eased to make room
          channel (flat(), band (3, 2, 4, Cut), band (6, 4, 2), flat(), 2) },

        { "Engineer Move - Smile Curve (club / loud)",     // lows and highs up, mids relaxed
          channel (band (2, 4, 4, Boost, true), band (6, 3, 0, Cut), flat(), band (8, 4, 0, Boost, true), 2) },

        { "Engineer Move - Dark Mix Rescue",               // opens a dull mix: 5.6k shelf + 4.7k presence
          channel (flat(), flat(), band (8, 3, 2), band (6, 5, 0, Boost, true), 2) },

        { "Engineer Move - Harsh Mix Tame",                // softens 3.3k bite and the top shelf
          channel (flat(), flat(), band (7, 5, 6, Cut), band (7, 3, 0, Cut, true), 2) },

        { "Engineer Move - Thin Mix Weight",               // adds body and bottom to a lightweight mix
          channel (band (4, 5, 4, Boost, true), band (2, 3, 0), flat(), flat(), 2) },

        { "Engineer Move - Boomy Mix Tighten",             // pulls down the 150 Hz boom, firmer high-pass
          channel (band (5, 4, 6, Cut), flat(), flat(), band (9, 2, 0, Boost, true), 4) },

        { "Engineer Move - Vinyl Pre-Master",              // high-pass 30, sibilance eased, top rolled at 20k
          channel (flat(), flat(), band (9, 3, 6, Cut), flat(), 4, 4) },

        { "Engineer Move - Streaming Translation",         // gentle all-rounder for earbuds, cars and phones
          channel (flat(), band (3, 2, 2, Cut), band (6, 2, 0), band (9, 3, 0, Boost, true), 3) },

        { "Engineer Move - Analog Warmth (tubes)",         // low-mid bloom, soft air, tubes pushed
          channel (flat(), band (1, 3, 0), flat(), band (10, 3, 0, Boost, true), 2),
          true, 6.0f },

        // ---- From the Manley manuals: the starting settings Manley's own manuals give --------
        //      (the hardware has no presets; Sontec and elysia publish no settings)
        { "Massive Passive manual - Stacked High Shelves",
          channel (flat(), flat(), band (7, 8, 7, Boost, true), band (8, 15, 3, Boost, true), 0), false, 0.0f, CompSettings {}, 0, "Manley manuals",
          "From the Massive Passive manual (Shelf & Bell): \"start boosting at 3K3, say 4 dB, then apply another high shelf to boost 12K, say for 10 dB.\" "
          "3.3 kHz shelf +4 dB at 12:00 bandwidth (the manual's starting point for shelves); 12 kHz shelf +10 dB, bandwidth a little wider than 12:00 so this model reaches 10 dB. "
          "An illustration of stacking shelves: a lot of top for a master, so pull the gains back to taste." },

        { "Variable Mu manual - Mastering Limit (2-4 dB)",
          channel (flat(), flat(), flat(), flat()), false, 0.0f, comp (6, 8, 6, 6, 10, Limit), 0, "Manley manuals",
          "From the Variable Mu manual: for clean limiting \"set the input for 2 to 4 dB of limiting, then adjust the outputs\", and slower recovery is \"the most inaudible\". "
          "LIMIT mode, threshold for about 3 dB, attack at 12:00 (~50 ms), recovery 4 s, +3 dB output. Adjust the threshold for 2-4 dB on the meter." },

        { "Variable Mu manual - Gentle Mix Compression",
          channel (flat(), flat(), flat(), flat()), false, 0.0f, comp (7, 8, 2, 7, 10, Compress), 0, "Manley manuals",
          "From the Variable Mu manual: start with the controls at 12:00 and \"typical amounts of gain reduction ... should be 2 to 6 dB\". "
          "COMPRESS mode (about 1.5:1), attack ~50 ms, recovery 0.6 s (12:00), about 3 dB of gain reduction." },

        { "Variable Mu manual - Drums (Fast Attack)",
          channel (flat(), flat(), flat(), flat()), false, 0.0f, comp (8, 5, 2, 8, 10, Compress), 0, "Manley manuals",
          "From the Variable Mu manual: for drums, try fast attack with medium recovery (and slow the attack if the drums already sit well). "
          "Attack 25 ms (the manual's Fast), recovery 0.6 s, about 4 dB." },

        { "Variable Mu manual - Vocals & Bass (Slow Attack)",
          channel (flat(), flat(), flat(), flat()), false, 0.0f, comp (7, 9, 2, 7, 10, Compress), 0, "Manley manuals",
          "From the Variable Mu manual: a slow attack ignores the drums and reacts to vocals and bass. Attack 70 ms (the manual's Slow), recovery 0.6 s, about 3 dB." },

        { "Variable Mu manual - Tube Colour",
          channel (flat(), flat(), flat(), flat()), true, 6.0f, comp (3, 8, 2, 2, 10, Compress), 0, "Manley manuals",
          "From the Variable Mu manual: for creative distortion, raise the input, lower the outputs and use little or no compression. "
          "About 1 dB of compression with the tube stage driven +6 dB." },
    };
    for (const auto& p : engineerPresets()) list.push_back (p);
    // every preset into its category: stereo-master curves by character, compressor settings by source,
    // track presets stay as they are; then the list is ordered by category (each keeps its own order)
    for (auto& p : list)
    {
        if (isTrackCategory (p.category)) continue;
        bool eq = (p.full && p.pq.in) || p.ch.hp != 0 || p.ch.lp != 0 || p.name == "Init (Flat)";
        for (const auto& b : p.ch.band) eq = eq || (b.gainStep > 0 && b.mode != Out);
        if (eq)                                        { p.category = masterCategoryOf (p.name); continue; }
        const std::string n = p.name;
        if (p.category == "Mix & Master Bus")          p.category = "Compressor: Mix Bus";
        else if (p.category == "Manley manuals")       p.category = n.find ("Drums") != std::string::npos ? "Compressor: Drums"
                                                                  : n.find ("Vocals") != std::string::npos ? "Compressor: Vocals" : "Compressor: Mix Bus";
        else                                           p.category = "Compressor: " + p.category;
    }
    std::stable_sort (list.begin(), list.end(), [] (const Preset& a, const Preset& b) { return categoryRank (a.category) < categoryRank (b.category); });
    return list;
    }();
    return list;
}


// ---- per-unit preset lists (indices into all()) ------------------------------------------
inline bool presetHasEq (const Preset& p)
{
    if (p.full && p.pq.in) return true;
    for (const auto& b : p.ch.band) if (b.gainStep > 0 && b.mode != Out) return true;
    return p.ch.hp != 0 || p.ch.lp != 0 || p.name == "Init (Flat)";
}
enum class UnitKind { Eq, VariMu, ClassA };
inline std::vector<int> unitPresets (UnitKind k)
{
    std::vector<int> out;
    const auto& l = all();
    for (int i = 0; i < (int) l.size(); ++i)
    {
        const auto& p = l[(size_t) i];
        const bool ok = k == UnitKind::Eq     ? presetHasEq (p)
                      : k == UnitKind::VariMu ? (p.comp.compIn && p.comp.threshold > 0)
                                              : (p.full && p.ca.in);
        if (ok) out.push_back (i);
    }
    return out;
}

inline int presetIndex (const char* name)
{
    const auto& l = all();
    for (int i = 0; i < (int) l.size(); ++i) if (l[(size_t) i].name == name) return i;
    return -1;
}
inline double presetEqDb (const Preset& p, double f)
{
    ChannelSettings c = p.ch; c.hp = 0; c.lp = 0;
    return channelResponseDb (c, f) + (p.full && p.pq.in ? sontec::channelResponseDb (p.pq.ch[0], f) : 0.0);
}

// ---- the complete master chain behind every EQ preset -------------------------------------------
//  EQ preset -> second EQ -> Vari-Mu -> Class-A -> Maximizer.
//  * The preset's curve is the first EQ. The other EQ stays neutral (switched out) unless a small
//    complementary move helps the curve: a warm curve that lifts the lows gets a gentle low-mid clean-up,
//    a bright curve that lifts the top gets a gentle 3.5 kHz smoothing (each -0.5 dB, wide, only where the
//    curve itself isn't already working). Engineer and track curves already use both EQs as fitted.
//  * The compressors follow the curve's character: one, or both in series when both help (Loud & Punchy);
//    the one not used is switched out. A preset with its own compressor keeps it.
//  * The maximizer is on as a safety limiter (gain 0 dB, ceiling -1 dB): it only catches peaks. Track presets leave it off.
struct MasterChain
{
    bool valid = false;
    ChannelSettings eq;            // passive EQ (both channels)
    bool tubeOn = false; float driveDb = 0.0f; int order = 0;
    sontec::Settings pq {};        // parametric EQ
    CompSettings vm {};            // vari-mu
    ClassASettings ca {};          // class-A
    bool maxOn = false; float maxGainDb = 0.0f, maxCeilingDb = -1.0f;
    std::string secondEq, comps;   // what the chain does, in words
};

inline int variMuThresholdFor (double grDb, int mode)
{
    int best = 0; double be = 1e9;
    for (int st = 0; st < kNumThreshSteps; ++st)
    {
        const double e = std::abs (variMuGainReductionDb (-6.0 - compThresholdDb (st), mode) - grDb);   // loud passages peak around -6 dBFS
        if (e < be) { be = e; best = st; }
    }
    return best;
}
inline CompSettings variMuGlue (double grDb, int attackIdx, int recoveryIdx)
{
    CompSettings c; c.compIn = true; c.mode = Compress; c.scHp = true; c.mix = 10;
    c.threshold = variMuThresholdFor (grDb, Compress); c.attack = attackIdx; c.recovery = recoveryIdx;
    c.output = std::clamp ((int) std::lround (grDb / 0.5), 0, kNumOutputSteps - 1);
    return c;
}
inline ClassASettings classAGlue (bool punch, double grDb)
{
    ClassASettings s; s.in = true; s.mode = ClassAStereo; s.link = true;
    for (auto& c : s.ch)
    {
        c.ratioPos = 7.5;                         // printed 2.0: feedback 2:1 (clean), feed-forward 4:1 (punch)
        c.feedForward = punch;
        c.attackMs = punch ? 15.0 : 30.0;         // feed-forward doubles it: 30 ms either way
        c.releaseMs = 300.0; c.autoFastRelease = true;
        c.thresholdDb = std::round ((-12.0 - grDb / (punch ? 0.75 : 0.5)) * 10.0) / 10.0;   // loud passages ~ -12 dBFS at the detector
        c.gainDb = std::round (grDb * 10.0) / 10.0; c.mix = 1.0;
    }
    if (punch) { s.softClip = true; s.clipDb = -1.0; }
    return s;
}
inline sontec::ChannelSettings neutralSontec()
{
    sontec::ChannelSettings c;
    for (int b = 0; b < sontec::kBands; ++b) c.band[b] = { (int) sontec::bandFreqs (b).size() / 2, 2, 12 };
    c.in = true;
    return c;
}
inline int nearestFreqIdx (int band, double hz)
{
    const auto& f = sontec::bandFreqs (band);
    int best = 0;
    for (int i = 0; i < (int) f.size(); ++i) if (std::abs (std::log (f[(size_t) i] / hz)) < std::abs (std::log (f[(size_t) best] / hz))) best = i;
    return best;
}

inline MasterChain masterChain (int index)
{
    MasterChain m;
    const auto& l = all();
    if (! (index >= 0 && index < (int) l.size())) return m;
    const auto& p = l[(size_t) index];
    const bool master = isMasterCategory (p.category), track = isTrackCategory (p.category);
    if (! master && ! track) return m;                       // compressor-only presets have no chain
    m.valid = true;
    m.eq = p.ch; m.tubeOn = p.tubeOn; m.driveDb = p.driveDb; m.order = p.order;
    const bool ownVm = p.comp.compIn && p.comp.threshold > 0, ownCa = p.full && p.ca.in;
    m.vm = p.comp; if (! ownVm) m.vm.compIn = false;
    m.ca = p.ca;   if (! ownCa) m.ca.in = false;

    // second EQ
    if (p.full) { m.pq = p.pq; m.secondEq = p.pq.in ? "both EQs as fitted" : "neutral"; }
    else
    {
        m.pq.in = false; for (auto& c : m.pq.ch) c = neutralSontec();
        m.secondEq = "neutral";
        auto maxDb = [&p] (double a, double b) { double v = -99.0; for (double f = a; f <= b; f *= 1.06) v = std::max (v, presetEqDb (p, f)); return v; };
        const double lo = maxDb (25.0, 150.0), hi = maxDb (6000.0, 20000.0);
        if (p.category == "Master: Warm & Analog" && lo > 0.8 && presetEqDb (p, 250.0) > -0.2)
        {   // lows lifted: keep the low-mids clear (-0.5 dB, wide, LO band ~250 Hz)
            m.pq.in = true; for (auto& c : m.pq.ch) c.band[0] = { nearestFreqIdx (0, 250.0), 1, 11 };
            m.secondEq = "-0.5 dB at 250 Hz (keeps the lifted lows clear)";
        }
        else if (p.category == "Master: Bright & Open" && hi > 0.8 && presetEqDb (p, 3500.0) > -0.2 && presetEqDb (p, 3500.0) < 0.5)
        {   // top lifted: keep the presence range smooth (-0.5 dB, wide, MID band ~3.5 kHz)
            m.pq.in = true; for (auto& c : m.pq.ch) c.band[1] = { nearestFreqIdx (1, 3500.0), 1, 11 };
            m.secondEq = "-0.5 dB at 3.5 kHz (keeps the lifted top smooth)";
        }
    }

    // compressors by character
    if (master && p.name != "Init (Flat)")
    {
        const auto& c = p.category;
        if (c == "Master: Clean & Corrective")   { if (! ownVm && ! ownCa) m.ca = classAGlue (false, 1.0); }
        else if (c == "Master: Balanced Polish") { if (! ownVm && ! ownCa) m.vm = variMuGlue (1.5, 7, 4); }
        else if (c == "Master: Warm & Analog")   { if (! ownVm && ! ownCa) m.vm = variMuGlue (2.0, 6, 3); }
        else if (c == "Master: Bright & Open")   { if (! ownVm && ! ownCa) m.ca = classAGlue (false, 1.5); }
        else if (c == "Master: Loud & Punchy")   { if (! ownVm) m.vm = variMuGlue (2.0, 5, 2); if (! ownCa) m.ca = classAGlue (true, 1.5); }
        m.maxOn = true;
    }
    m.comps = m.vm.compIn && m.ca.in ? "Vari-Mu into Class-A" : m.vm.compIn ? "Vari-Mu" : m.ca.in ? "Class-A" : "none";
    return m;
}

} // namespace ppm::presets
