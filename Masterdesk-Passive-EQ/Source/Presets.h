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
#include <vector>

namespace ppm::presets
{
struct Preset
{
    const char* name;
    ChannelSettings ch;
    bool  tubeOn  = true;
    float driveDb = 0.0f;
    CompSettings comp {};      // default: compressor in, threshold 0 dB (transparent)
    int   order   = 0;         // 0 = EQ > COMP, 1 = COMP > EQ
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

inline const std::vector<Preset>& all()
{
    static const std::vector<Preset> list = {
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
    };
    return list;
}
} // namespace ppm::presets
