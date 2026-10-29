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
    };
    // then the engineer presets, grouped by category (each group keeps the list's order)
    auto eng = engineerPresets();
    for (const char* cat : kEngineerCategories)
        for (const auto& p : eng)
            if (p.category == cat) list.push_back (p);
    return list;
    }();
    return list;
}
} // namespace ppm::presets
