// =============================================================================
//  make_engineer_presets.cpp  -  builds Source/EngineerPresets.h
//
//  The settings below are copied from the engineer preset lists supplied for the
//  plug-in (EQ moves, ratio / attack / release / gain reduction). They are the
//  settings as given in those lists; they have not been verified with, or
//  endorsed by, the engineers named.
//
//  This program turns each list entry into real control positions on this
//  plug-in's units, using the units' own analog models:
//    bells          -> Parametric (Sontec-style) EQ bands, else passive EQ bands
//    shelves        -> Parametric shelves (50/100 Hz, 8/12 kHz) or passive EQ shelves
//    high/low pass  -> passive EQ filters (12-39 Hz, 15-52 kHz)
//    compressors    -> M/S Class-A compressor (ratio, attack, release in ms)
//                      or the Vari-Mu (tube / opto style units)
//  EQ presets are for the stereo master only: track presets (vocals, drums, bass, guitars,
//  keys) keep their compressor settings and leave their EQ moves out.
//  Moves the plug-in cannot make (for example a 300 Hz high-pass) are left out
//  and named in the preset's notes. Every preset's notes say how close the EQ
//  curve came out.
//
//  Build and run:  c++ -std=c++17 -O2 -I Source tools/make_engineer_presets.cpp -o /tmp/mkp
//                  /tmp/mkp > Source/EngineerPresets.h
// =============================================================================
#include "dsp/Model.h"
#include "dsp/SontecEQ.h"
#include "dsp/ClassAComp.h"
#include "dsp/VariMu.h"
#include <cstdio>
#include <string>
#include <vector>
#include <functional>

using namespace ppm;
namespace so = ppm::sontec;

// ---- the list entries ---------------------------------------------------------------------
struct Move { char type; double f, g, q; int order; };  // B bell, L low shelf, H high shelf, P high-pass, Q low-pass
static Move bell (double f, double g, double q = 1.0) { return { 'B', f, g, q, 0 }; }
static Move lshelf (double f, double g, double q = 0.707) { return { 'L', f, g, q, 0 }; }
static Move hshelf (double f, double g, double q = 0.707) { return { 'H', f, g, q, 0 }; }
static Move hpf (double f, int dbPerOct = 12) { return { 'P', f, 0, 0, dbPerOct / 6 }; }
static Move lpf (double f, int dbPerOct = 12) { return { 'Q', f, 0, 0, dbPerOct / 6 }; }

struct Comp
{
    char unit = 0;                 // 'A' Class-A, 'V' Vari-Mu
    double ratio = 2, attMs = 13, relMs = 300, grDb = 2, mix = 1.0;
    bool autoRel = false, autoAtt = false, warm = false, limit = false;
    double scHpHz = 0;             // detector high-pass (Class-A SC filter), 0 = off
};
static Comp classA (double ratio, double attMs, double relMs, double grDb, double mix = 1.0)
{ Comp c; c.unit = 'A'; c.ratio = ratio; c.attMs = attMs; c.relMs = relMs; c.grDb = grDb; c.mix = mix; return c; }
static Comp variMu (double attMs, double relMs, double grDb, double mix = 1.0, bool limit = false)
{ Comp c; c.unit = 'V'; c.attMs = attMs; c.relMs = relMs; c.grDb = grDb; c.mix = mix; c.limit = limit; return c; }
static Comp autoRel (Comp c) { c.autoRel = true; return c; }
static Comp autoAtt (Comp c) { c.autoAtt = true; return c; }
static Comp warm (Comp c) { c.warm = true; return c; }
static Comp scHp (Comp c, double hz) { c.scHpHz = hz; return c; }

// 1176 knobs (1 = slowest .. 7 = fastest): attack 800 us .. 20 us, release 1100 ms .. 50 ms
static double k1176Att (double k) { return 0.8 - 0.13 * (k - 1.0); }
static double k1176Rel (double k) { return 1100.0 - 175.0 * (k - 1.0); }

struct Spec
{
    std::string engineer, title, cat, target;
    std::vector<Move> eq;
    std::vector<Comp> comps;
    std::string notes;             // what the list says that the plug-in reads differently
    bool tube = false; double drive = 0; int order = 0;
};

static std::vector<Spec> specs()
{
    const char* V = "Vocals"; const char* D = "Drums"; const char* B = "Bass"; const char* G = "Guitars & Keys"; const char* M = "Mix & Master Bus";
    return {
        { "Chris Lord-Alge", "Lead Vocals (1176 Fast Stage)", V, "Aggressive pop/rock lead vocals",
          {}, { classA (4, 0.6, 50, 8.5) }, "1176 attack 3 (~0.6 ms), release 7 (~50 ms) as listed; target 7-10 dB of gain reduction on loud spikes." },
        { "Chris Lord-Alge", "Rock Bass Guitar (LA-2A)", B, "Aggressive electric bass",
          {}, { variMu (10, 600, 4) }, "Opto leveler in COMPRESS, 3-5 dB of smooth gain reduction. The output-transformer drive is the tube stage (+3 dB drive).", true, 3.0 },
        { "Chris Lord-Alge", "Snare Drum Snap", D, "Snare top mic",
          { lshelf (80, -2), hshelf (5000, 3.5) }, {}, "Bass slider -2 dB and treble +3.5 dB at 5 kHz as listed. The CLA Drums compressor slider has no ratio/time values, so it is not included." },
        { "Andrew Scheps", "Parallel Vocal Crush", V, "Auxiliary vocal parallel bus",
          { bell (150, -3), bell (100, 2.5) }, { warm (classA (20, k1176Att (1), k1176Rel (7), 10)) },
          "All-buttons-in (~20:1) with attack 1 (slowest) and release 7 (fastest); WARM adds the grit. Pre-EQ -3 dB at 150 Hz; the +2.5 dB at 100 Hz post-lift runs before the compressor here (this plug-in's EQs come first). Use on a parallel bus, or blend with MIX." },
        { "Andrew Scheps", "Rear Bus Parallel Mix Compression", M, "Mix minus drums, parallel bus",
          {}, { classA (4, k1176Att (1), 100, 3) }, "4:1, slow attack (1176 slowest), 100 ms release as listed. Gain reduction not given: threshold set for about 3 dB." },
        { "Bob Katz", "Parallel Mastering 'Tonalisation Warm'", M, "Full mix master bus",
          {}, { scHp (classA (1.15, 13, 300, 1.5, 0.5), 50) },
          "Listed: threshold -45 dBFS, ratio 1.1-1.2:1, soft knee 40, parallel on, detector band-pass 50 Hz-5 kHz. Here: lowest threshold -31 dB, 6 dB knee, MIX 50 %, detector high-pass at 50 Hz (no 5 kHz upper edge)." },
        { "Bob Katz", "Mastering Broadband Compression", M, "Master bus glue",
          {}, { classA (1.35, 175, 350, 1.5) }, "Ratio 1.2-1.5:1, attack 100-250 ms, release 300-400 ms (middle values used). Gain reduction not given: threshold set for about 1.5 dB." },
        { "Greg Calbi", "Mastering 'General Clarity'", M, "Dull mix master bus",
          { lshelf (120, -0.5), bell (1500, 0.5, 0.5), hshelf (12000, 0.8) }, {}, "" },
        { "Michael Brauer", "Lead Vocal Path A (Warmth)", V, "Lead vocal body bus (Brauerizing)",
          {}, { classA (2, 12, 250, 2.5) }, "Distressor 2:1, attack 5 (~12 ms), 2-3 dB of gain reduction as listed. Release 3.5 has no time given: about 250 ms used." },
        { "Michael Brauer", "Lead Vocal Path B (Excitement)", V, "Parallel lead vocal top-end bus",
          {}, { classA (4, 0.6, 50, 6, 0.35) }, "1176 4:1, attack 3, release 7, up to 6 dB on loud notes, blended softly (MIX 35 %). Paths A and B are two presets: this plug-in can't run two compressors side by side." },
        { "Michael Brauer", "Acoustic Guitar Multi-Bus", G, "Acoustic guitar bus",
          {}, { classA (3, 13, 400, 1.75) }, "33609-style 3:1, 400 ms recovery, 1.5-2 dB as listed. Attack not given: 13 ms." },
        { "Manny Marroquin", "Universal Hip-Hop/Pop Kick EQ", D, "Kick drum channel",
          { hpf (30), lshelf (60, 3, 0.5), bell (300, -4, 1.4), bell (3200, 2, 2.5) }, {}, "Q not given for 300 Hz (1.4 used) or the 'sharp' 3.2 kHz boost (2.5 used)." },
        { "Manny Marroquin", "Kick Drum Punch Compression", D, "Kick drum dynamics",
          {}, { classA (4, 30, 100, 3.5) }, "SSL-style 4:1, 30 ms attack, 100 ms release, 3-4 dB of gain reduction." },
        { "Manny Marroquin", "Kick Chain (Punch Comp + Character EQ)", D, "Kick drum channel",
          { hpf (30), lshelf (60, 3, 0.5), bell (300, -4, 1.4), bell (3200, 2, 2.5) }, { classA (4, 30, 100, 3.5) },
          "The two steps together. The list puts the compressor first; in this plug-in the EQ runs before the Class-A compressor." },
        { "Eric Boulanger", "Surgical Mastering EQ", M, "Master bus correction",
          { hpf (20, 6), bell (179, -1, 1.0), bell (3500, -0.65, 2.5), hshelf (16000, 0.5) }, {}, "3.5 kHz cut listed as -0.5 to -0.8 dB (middle value used)." },
        { "Dave Pensado", "Modern Pop Vocal 'Air' (Pultec)", V, "Lead vocal",
          { bell (16000, 5, 0.6), hshelf (20000, -1.5) }, {}, "Pultec HF 16 kHz, boost 3.5 and atten 1 at 20 kHz as listed; the knob numbers are read as about +5 dB and -1.5 dB." },
        { "Dave Pensado", "Lead Vocal 'Warmth Rescue'", V, "Thin vocal tracks",
          { bell (220, 1.5, 1.2) }, {}, "" },
        { "Andrew Scheps", "Classic Rock Guitar Presence (73)", G, "Distorted rhythm guitars",
          { hpf (80), bell (3200, 2, 1.0) }, {}, "Neve-style mid at 3.2 kHz +2 dB. Q not given: 1.0." },
        { "Ian Shepherd", "Non-Destructive Multi-Band Template", M, "Transparent master bus balance",
          {}, { autoRel (classA (2, 100, 300, 1.5)) }, "Three identical bands at 2:1 / 100 ms / auto release act like one broadband compressor, which is what this is." },
        { "Joe Chiccarelli", "Analog Mastering Baxandall Curve", M, "Master bus final polish",
          { lshelf (35, 0.5, 0.4), hshelf (16000, 0.5, 0.4) }, {}, "" },
        { "Eddie Kramer", "Classic Rock Drum Bus (Pye)", D, "Rock drum overhead / room bus",
          {}, { classA (3, 5, 100, 4) }, "3:1 as listed. Attack 'medium-fast' and release 'fast' have no times given: 5 ms and 100 ms used; gain reduction about 4 dB." },
        { "Serban Ghenea", "Pristine Pop Vocal Air (40 kHz)", V, "Main pop vocal",
          { hshelf (40000, 2.5, 0.5) }, {}, "Air band 40 kHz, +2-3 dB as listed (+2.5 used)." },
        { "Serban Ghenea", "Pop Vocal Boxiness Cut", V, "Lead vocal cleaning stage",
          { bell (750, -1.5, 2.5) }, {}, "700-800 Hz as listed (750 Hz used)." },
        { "Sören von Malmborg", "Modern Pop Master Bus Glue (SSL G)", M, "High-energy pop master bus",
          {}, { autoRel (classA (4, 30, 300, 1.5)) }, "4:1, 30 ms attack, auto release, 1-2 dB of gain reduction." },
        { "Alan Parsons", "Classic Rock Guitar EQ (TG12345)", G, "Heavy distorted guitars",
          { bell (2800, 2, 1.0), lshelf (200, -2) }, {}, "Presence +2 dB at 2.8 kHz, bass -2 dB at 200 Hz (console shelf)." },
        { "Tom Lord-Alge", "Explosive Rock Toms (1176)", D, "Floor and rack toms",
          {}, { classA (8, k1176Att (4), k1176Rel (1), 6) }, "4:1 or 8:1 (8:1 used), attack 4, release 1 (slowest). Gain reduction not given: about 6 dB." },
        { "Al Schmitt", "Silky Tube Vocal Leveler", V, "Natural jazz/pop lead vocal",
          {}, { variMu (20, 800, 1.5) }, "Tube leveler, attack and release on Medium, 1-2 dB on peaks." },
        { "Dave Pensado", "Vocal 'In-Your-Face' Mid Push", V, "Lead vocal",
          { bell (1500, 2, 1.5) }, {}, "" },
        { "Chris Lord-Alge", "Smooth Pop Ballad Vocal (LA-2A)", V, "Dynamic pop/R&B vocals",
          {}, { variMu (10, 1200, 5) }, "Opto in COMPRESS, 4-6 dB of reduction with the slow two-stage release (1.2 s recovery here)." },
        { "Lu Diaz", "R&B Vocal Top-End", V, "Clean high-fidelity lead vocal",
          { hshelf (8000, 3), hpf (110, 18) }, {}, "" },
        { "Michael Brauer", "Soulful R&B Vocal Bus (Fairchild)", V, "Multi-bus vocal path",
          {}, { variMu (0.2, 800, 1.5) }, "Time constant 2 (0.2 ms attack, 0.8 s release), 1.5 dB as listed. The Vari-Mu's fastest attack is 2 ms." },
        { "Sean Divine", "Melodic Vocal Chain Baseline", V, "Melodic rap / R&B lead vocals",
          {}, { classA (4, k1176Att (4), k1176Rel (4), 3), variMu (10, 800, 2) },
          "1176 at 4:1 (3 dB) and an opto (2 dB) as listed; 1176 times not given (knobs at 4). The 5.8 kHz de-esser isn't included, and the opto runs before the 1176 here." },
        { "Andrew Scheps", "Background Vocal Stack Separation", V, "Background vocal group bus",
          { bell (1000, -3, 0.8) }, {}, "" },
        { "Phil Tan", "Airy R&B Backing Vocals", V, "Wide backing harmony stacks",
          { hshelf (12000, 4), lshelf (200, -3) }, {}, "" },
        { "Eric Boulanger", "Precision Vocal Mud Notch", V, "Resonant lead vocal",
          { bell (250, -1.5, 3.5) }, {}, "" },
        { "Lu Diaz", "Aggressive Hip-Hop Vocal Leveling (1176)", V, "Raw modern hip-hop lead",
          {}, { classA (4, k1176Att (5), k1176Rel (7), 6) }, "4:1, attack 5, release 7 as listed. Gain reduction not given: about 6 dB." },
        { "Manny Marroquin", "Punchy Snare EQ", D, "Snare top mic",
          { bell (400, -3, 1.0), bell (4500, 3, 1.0) }, {}, "Neve-style bands; Q not given: 1.0." },
        { "Chris Lord-Alge", "Heavy Rock Toms (1176)", D, "Rack and floor toms",
          {}, { classA (8, k1176Att (4), k1176Rel (1), 6) }, "8:1, attack 4, release 1 (slowest). Gain reduction not given: about 6 dB." },
        { "Andy Wallace", "Snare Rim Crack (SSL)", D, "Snare top channel",
          { bell (8000, 4, 1.0), bell (180, 2, 1.0) }, {}, "SSL bells; Q not given: 1.0." },
        { "Tom Lord-Alge", "Explosive Parallel Drum Crush (Nuke)", D, "Parallel drum aux",
          {}, { classA (100, 1, 150, 10, 0.15) }, "Nuke (brickwall) at 15 % blend as listed. Distressor attack 2 / release 2 have no times given: about 1 ms and 150 ms." },
        { "Dave Rat", "Live Punchy Kick Definition", D, "Kick drum in mic",
          { bell (250, -6, 3.0), bell (3500, 4, 1.2) }, {}, "3.5 kHz boost Q not given: 1.2." },
        { "Bob Clearmountain", "Transparent Overhead Leveling", D, "Stereo drum overheads",
          {}, { classA (2, 45, 120, 2) }, "2:1, 45 ms attack, 120 ms release. Gain reduction not given: about 2 dB." },
        { "Andrew Scheps", "Snare Bottom Wire Excitation (1176)", D, "Snare bottom mic",
          {}, { classA (4, k1176Att (7), k1176Rel (7), 6) }, "4:1, attack 7 and release 7 (fastest). Gain reduction not given: about 6 dB." },
        { "Eddie Kramer", "Vintage Room Mics (Pye)", D, "Stereo drum room channels",
          {}, { classA (5, 5, 100, 9) }, "5:1, driven to 8-10 dB of gain reduction. Attack/release not given: 5 ms / 100 ms." },
        { "Michael Brauer", "'Pumped Room' Drum Parallel (Sta-Level)", D, "Parallel drum ambience aux",
          {}, { variMu (2, 4000, 5) }, "Gates Sta-Level style, slow recovery (4 s here). Amount not given: about 5 dB." },
        { "Lu Diaz", "Hip-Hop 808/Kick Separation", D, "Hip-hop sub-kick channel",
          { bell (80, -3, 2.5) }, {}, "" },
        { "Greg Fidelman", "Hard Rock Floor Tom Attack", D, "Floor toms",
          { bell (3500, 4, 1.0), bell (90, 3, 1.0) }, {}, "SSL bells; Q not given: 1.0." },
        { "Chris Lord-Alge", "Wall-of-Sound Guitars (SSL G comp)", G, "Hard-rock L/R guitars",
          {}, { classA (2, 1, 100, 1.75) }, "2:1, 1 ms attack, 100 ms release, 1.5-2 dB." },
        { "Dave Pensado", "Lush Acoustic Guitar Brilliance", G, "Strummed acoustic guitars",
          { hshelf (10000, 3), bell (350, -4, 1.0) }, {}, "" },
        { "Alan Parsons", "Prog-Rock Acoustic Guitar (LA-3A)", G, "Fingerpicked acoustic",
          {}, { variMu (2, 400, 2.5) }, "LA-3A in COMP, 2-3 dB on note attacks. Times not given: fast attack, 0.4 s recovery." },
        { "Michael Brauer", "Indie Electric Guitar Bus Glue (LA-3A)", G, "Electric guitar sub-mix bus",
          {}, { variMu (2, 400, 0.75) }, "LA-3A, 0.5-1 dB of gain reduction. Times not given: fast attack, 0.4 s recovery." },
        { "Eddie Kramer", "Vintage Fuzz Guitar EQ (Helios)", G, "Fuzz guitar solos",
          { hshelf (10000, 3), lshelf (60, -2) }, {}, "" },
        { "Manny Marroquin", "Warm Grand Piano Body", G, "Stereo acoustic piano",
          { bell (250, 1.5, 0.6), bell (2500, -2, 1.0) }, {}, "Broad 250 Hz bell (Q 0.6); 2.5 kHz Q not given: 1.0." },
        { "Joe Chiccarelli", "Indie Rock Bass Saturation (SSL comp)", B, "Electric bass",
          {}, { classA (4, 3, 100, 4) }, "4:1, 3 ms attack, 100 ms release. Gain reduction not given: about 4 dB." },
        { "Greg Fidelman", "Metal Riff Guitar Fizz Cut", G, "High-gain rhythm guitars",
          { bell (4000, -3, 4.0) }, {}, "" },
        { "Tchad Blake", "Acoustic Guitar Parallel Smash (1176)", G, "Acoustic guitar parallel bus",
          {}, { classA (20, k1176Att (1), k1176Rel (7), 10, 0.25) }, "20:1, attack 1, release 7, blended under the dry guitar (MIX 25 %)." },
        { "Butch Vig", "Synth Bass Low-End Stabilizer (dbx 160)", B, "Electronic synth bass",
          {}, { autoAtt (autoRel (classA (3, 13, 300, 4))) }, "3:1, 4 dB on the lowest notes. The dbx 160's program-dependent times are the AUTO FAST switches." },
        { "Sören von Malmborg", "Pop Acoustic Guitar Shimmer (Air 20k)", G, "Stereo pop acoustic guitars",
          { hshelf (20000, 3, 0.5) }, {}, "" },
        { "Bob Katz", "Parallel Mastering 'Tonalisation Clear'", M, "Full mix master bus",
          {}, { scHp (classA (1.2, 13, 300, 1.5, 0.5), 500) },
          "Listed: threshold -48 dBFS, parallel on, detector high-pass at 500 Hz. Here: lowest threshold -31 dB, MIX 50 %, SC filter high-pass at 500 Hz. Ratio not given: 1.2:1 (as in 'Warm')." },
        { "Greg Calbi", "Mastering 'Smooth Bass' Anchor", M, "Master bus low-end balance",
          { lshelf (65, 0.5, 0.5), bell (240, -0.3, 1.0) }, {}, "" },
        { "Ian Shepherd", "Safe Master Bus Glue (SSL G)", M, "Mix bus cohesion",
          {}, { autoRel (classA (2, 30, 300, 1.5)) }, "2:1, 30 ms attack, auto release. Gain reduction not given: about 1.5 dB." },
        { "Joe Chiccarelli", "Mastering Air Lifter (Curve Bender)", M, "Full mix final air",
          { hshelf (16000, 0.5, 0.4) }, {}, "" },
    };
}

// ---- ideal curves of the listed moves ------------------------------------------------------
static double idealDb (const Move& m, double f)
{
    const std::complex<double> s (0.0, f / m.f);
    std::complex<double> h = 1.0;
    const double A = std::pow (10.0, m.g / 40.0);
    switch (m.type)
    {
        case 'B': h = (s * s + s * (A / m.q) + 1.0) / (s * s + s / (A * m.q) + 1.0); break;
        case 'L': h = A * (s * s + (std::sqrt (A) / m.q) * s + A) / (A * s * s + (std::sqrt (A) / m.q) * s + 1.0); break;
        case 'H': h = A * (A * s * s + (std::sqrt (A) / m.q) * s + 1.0) / (s * s + (std::sqrt (A) / m.q) * s + A); break;
        case 'P': h = butterworthResponse (true,  std::max (1, m.order), m.f, f); break;
        case 'Q': h = butterworthResponse (false, std::max (1, m.order), m.f, f); break;
    }
    return 20.0 * std::log10 (std::max (std::abs (h), 1e-9));
}

static std::vector<double> grid()
{
    std::vector<double> g;
    for (int i = 0; i < 72; ++i) g.push_back (20.0 * std::pow (1000.0, i / 71.0));
    return g;
}

static so::ChannelSettings flatSontec() { so::ChannelSettings c; for (int b = 0; b < so::kBands; ++b) c.band[b] = { (int) so::bandFreqs (b).size() / 2, 2, 12 }; return c; }
static ChannelSettings flatPassive()
{
    ChannelSettings c;
    for (int b = 0; b < kNumBands; ++b) c.band[b] = { 5, 0, 7, Boost, false };
    c.hp = 0; c.lp = 0; c.trim = 5; c.eqIn = true;
    return c;
}

struct Fit
{
    so::ChannelSettings pq = flatSontec();
    ChannelSettings pa = flatPassive();
    bool usedS[3] {}, usedSHs = false, usedSLs = false, usedP[4] {}, usedHp = false, usedLp = false;
    std::vector<Move> placed, dropped;
};

static double err (const std::function<double (double)>& got, const Move& m, double& maxErr)
{
    double sum = 0; maxErr = 0; int n = 0;
    for (double f : grid())
    {
        const double d = got (f) - idealDb (m, f);
        sum += d * d; maxErr = std::max (maxErr, std::abs (d)); ++n;
    }
    return std::sqrt (sum / n);
}

static void place (Fit& fit, const Move& m)
{
    double best = 1e9, bestMax = 0; std::function<void()> apply;
    auto consider = [&] (double e, double mx, std::function<void()> a) { if (e < best) { best = e; bestMax = mx; apply = a; } };

    if (m.type == 'B')
    {
        for (int b = 0; b < so::kBands; ++b)
        {
            if (fit.usedS[b]) continue;
            const auto& fr = so::bandFreqs (b);
            if (m.f < fr.front() * 0.85 || m.f > fr.back() * 1.15) continue;
            for (int fi = 0; fi < (int) fr.size(); ++fi)
                for (int sh = 0; sh < so::kNumShapes; ++sh)
                    for (int lv = 0; lv < so::kNumLevels; ++lv)
                    {
                        so::ChannelSettings c = flatSontec(); c.band[b] = { fi, sh, lv };
                        double mx; const double e = err ([&] (double f) { return so::channelResponseDb (c, f); }, m, mx) - 0.03;   // slight preference: the parametric
                        consider (e, mx, [&fit, b, fi, sh, lv] { fit.pq.band[b] = { fi, sh, lv }; fit.usedS[b] = true; });
                    }
        }
    }
    if (m.type == 'L' && ! fit.usedSLs)
        for (int f100 = 0; f100 < 2; ++f100)
            for (int lv = 0; lv < so::kNumLevels; ++lv)
            {
                so::ChannelSettings c = flatSontec(); c.loShelfLevel = lv; c.loShelf100 = f100 == 1;
                double mx; const double e = err ([&] (double f) { return so::channelResponseDb (c, f); }, m, mx) - 0.03;
                consider (e, mx, [&fit, lv, f100] { fit.pq.loShelfLevel = lv; fit.pq.loShelf100 = f100 == 1; fit.usedSLs = true; });
            }
    if (m.type == 'H' && ! fit.usedSHs)
        for (int f12 = 0; f12 < 2; ++f12)
            for (int lv = 0; lv < so::kNumLevels; ++lv)
            {
                so::ChannelSettings c = flatSontec(); c.hiShelfLevel = lv; c.hiShelf12k = f12 == 1;
                double mx; const double e = err ([&] (double f) { return so::channelResponseDb (c, f); }, m, mx) - 0.03;
                consider (e, mx, [&fit, lv, f12] { fit.pq.hiShelfLevel = lv; fit.pq.hiShelf12k = f12 == 1; fit.usedSHs = true; });
            }
    if (m.type == 'B' || m.type == 'L' || m.type == 'H')
    {
        const bool shelf = m.type != 'B';
        for (int b = 0; b < kNumBands; ++b)
        {
            if (fit.usedP[b]) continue;
            if (m.type == 'L' && ! bandShelfIsLow (b)) continue;
            if (m.type == 'H' && bandShelfIsLow (b)) continue;
            if (m.f < kBandFreqs[b][0] * 0.8 || m.f > kBandFreqs[b][kNumFreqSteps - 1] * 1.25) continue;
            const int mode = m.g >= 0 ? Boost : Cut;
            for (int fi = 0; fi < kNumFreqSteps; ++fi)
                for (int gs = 1; gs < kNumKnobSteps; ++gs)
                    for (int bw = 0; bw < kNumKnobSteps; ++bw)
                    {
                        ChannelSettings c = flatPassive(); c.band[b] = { fi, gs, bw, mode, shelf };
                        double mx; const double e = err ([&] (double f) { return channelResponseDb (c, f); }, m, mx);
                        consider (e, mx, [&fit, b, fi, gs, bw, mode, shelf] { fit.pa.band[b] = { fi, gs, bw, mode, shelf }; fit.usedP[b] = true; });
                    }
        }
    }
    if (m.type == 'P' && ! fit.usedHp)
        for (int i = 1; i < kNumFilterPos; ++i)
        {
            ChannelSettings c = flatPassive(); c.hp = i;
            double mx; const double e = err ([&] (double f) { return channelResponseDb (c, f); }, m, mx);
            consider (e, mx, [&fit, i] { fit.pa.hp = i; fit.usedHp = true; });
        }
    if (m.type == 'Q' && ! fit.usedLp)
        for (int i = 1; i < kNumFilterPos; ++i)
        {
            ChannelSettings c = flatPassive(); c.lp = i;
            double mx; const double e = err ([&] (double f) { return channelResponseDb (c, f); }, m, mx);
            consider (e, mx, [&fit, i] { fit.pa.lp = i; fit.usedLp = true; });
        }

    // a move this plug-in can't really make (e.g. a 300 Hz high-pass) is left out rather than faked
    const double limit = std::max (1.0, 0.25 * std::abs (m.type == 'P' || m.type == 'Q' ? 12.0 : m.g));
    if (apply && (best + 0.03 < limit || bestMax < 1.0)) { apply(); fit.placed.push_back (m); }
    else fit.dropped.push_back (m);
}

static std::string moveText (const Move& m)
{
    char b[96];
    auto hz = [] (double f) { char t[24]; if (f >= 1000) std::snprintf (t, sizeof t, "%g kHz", f / 1000.0); else std::snprintf (t, sizeof t, "%g Hz", f); return std::string (t); };
    switch (m.type)
    {
        case 'P': std::snprintf (b, sizeof b, "high-pass %s", hz (m.f).c_str()); break;
        case 'Q': std::snprintf (b, sizeof b, "low-pass %s", hz (m.f).c_str()); break;
        case 'L': std::snprintf (b, sizeof b, "low shelf %+g dB at %s", m.g, hz (m.f).c_str()); break;
        case 'H': std::snprintf (b, sizeof b, "high shelf %+g dB at %s", m.g, hz (m.f).c_str()); break;
        default:  std::snprintf (b, sizeof b, "bell %+g dB at %s", m.g, hz (m.f).c_str()); break;
    }
    return b;
}

// ---- compressors ----------------------------------------------------------------------------
static double ratioPosFor (double r, bool& ff)
{
    // feed forward slope = 1 - 1/(2 x printed) up to position 10, then on through limiting
    if (r >= 2.0 * kClassARatioScale[0])
    {
        ff = true;
        const double slope = r > 1e3 ? 1.0 : 1.0 - 1.0 / r;
        for (int i = 0; i < 10; ++i)
        {
            const double a = 1.0 - 1.0 / (2.0 * kClassARatioScale[i]), b = 1.0 - 1.0 / (2.0 * kClassARatioScale[i + 1]);
            if (slope >= a && slope <= b) return i + (slope - a) / (b - a);
        }
        const double s10 = 1.0 - 1.0 / (2.0 * kClassARatioScale[10]);
        return std::min (kClassARatioTop, 10.0 + (slope - s10) / (1.67 - s10));
    }
    ff = false;                       // feedback: the printed ratio (gentle ratios)
    for (int i = 0; i < 11; ++i)
        if (r >= kClassARatioScale[i] && r <= kClassARatioScale[i + 1])
            return i + (r - kClassARatioScale[i]) / (kClassARatioScale[i + 1] - kClassARatioScale[i]);
    return 0.0;
}

static const char* yes (bool b) { return b ? "true" : "false"; }

int main()
{
    std::printf ("// =============================================================================\n"
                 "//  EngineerPresets.h  -  generated by tools/make_engineer_presets.cpp: do not edit by hand.\n"
                 "//  Settings as given in the supplied engineer preset lists, placed on this plug-in's controls.\n"
                 "//  Not verified with or endorsed by the engineers named.\n"
                 "// =============================================================================\n"
                 "#pragma once\n\nnamespace ppm::presets\n{\ninline std::vector<Preset> engineerPresets()\n{\n    std::vector<Preset> v;\n");
    int count = 0;
    std::fprintf (stderr, "%-64s %s\n", "preset", "EQ fit (max dB over 30 Hz-16 kHz) / dropped");
    int strippedEq = 0, skipped = 0;
    for (auto sp : specs())
    {
        // EQ presets are for the stereo master only: track presets (vocals, drums, bass, guitars, keys)
        // keep their compressor settings, their EQ moves are left out, and EQ-only track presets are skipped
        if (sp.cat != "Mix & Master Bus" && ! sp.eq.empty())
        {
            if (sp.comps.empty() || sp.title.find ("Kick Chain") != std::string::npos) { ++skipped; continue; }   // the kick chain = the kick compressor preset
            sp.eq.clear();
            sp.notes += " Its EQ moves are left out: EQ presets here are for the stereo master only.";
            ++strippedEq;
        }
        Fit fit;
        for (const auto& m : sp.eq) place (fit, m);

        // how close the whole curve came out (30 Hz - 16 kHz)
        double worst = 0;
        for (double f : grid())
        {
            if (f < 30 || f > 16000) continue;
            double want = 0; for (const auto& m : fit.placed) want += idealDb (m, f);
            const double got = so::channelResponseDb (fit.pq, f) + channelResponseDb (fit.pa, f);
            worst = std::max (worst, std::abs (got - want));
        }

        std::string notes = sp.target + ". ";
        if (! sp.notes.empty()) notes += sp.notes + " ";
        if (! fit.placed.empty())
        {
            char t[96]; std::snprintf (t, sizeof t, "EQ curve within %.1f dB of the listed moves (30 Hz-16 kHz). ", worst + 0.049);
            notes += t;
        }
        if (! fit.dropped.empty())
        {
            notes += "Not possible here, left out: ";
            for (size_t i = 0; i < fit.dropped.size(); ++i) notes += (i ? ", " : "") + moveText (fit.dropped[i]);
            notes += " (the EQs' high-pass stops at 39 Hz, the low-pass at 15 kHz). ";
        }

        // ---- compressor units
        ClassASettings ca; ca.in = false;
        CompSettings vm; vm.compIn = false;
        std::string compNotes;
        for (const auto& c : sp.comps)
        {
            if (c.unit == 'A')
            {
                bool ff = false; const double pos = ratioPosFor (c.ratio, ff);
                const double slope = ff ? classAFeedForwardSlope (pos) : 1.0 - 1.0 / classAPrintedRatio (pos);
                const double lref = -8.0;                                    // loud passages at the detector
                const double thr = std::clamp (lref - c.grDb / std::max (0.05, slope), -31.0, 2.0);
                ca.in = true; ca.mode = ClassAStereo; ca.link = true;
                for (auto& ch : ca.ch)
                {
                    ch.thresholdDb = std::round (thr * 10) / 10;
                    ch.attackMs = std::clamp (c.attMs / (ff ? 2.0 : 1.0), 0.01, 150.0);      // FF doubles the attack time
                    ch.releaseMs = std::clamp (c.relMs, 60.0, 1800.0);
                    ch.ratioPos = std::round (pos * 100) / 100;
                    ch.feedForward = ff;
                    ch.autoFastAttack = c.autoAtt; ch.autoFastRelease = c.autoRel;
                    ch.gainDb = std::clamp (std::round (c.grDb * (c.mix < 0.99 ? 0.5 * c.mix : 1.0) * 10) / 10, 0.0, 12.0);
                    ch.mix = c.mix; ch.warm = c.warm;
                    ch.scOn = c.scHpHz > 0; ch.scTilt = c.scHpHz > 0 ? 1.0 : 0.0; ch.scFreq = c.scHpHz > 0 ? std::clamp (c.scHpHz, 30.0, 3300.0) : 280.0;
                }
                char t[200];
                std::snprintf (t, sizeof t, "Class-A: %s, threshold %.1f dB (set it for about %.1f dB of gain reduction). ",
                               ff ? "FEED FWD" : "FEEDBACK", ca.ch[0].thresholdDb, c.grDb);
                compNotes += t;
                if (c.relMs < 60) compNotes += "Release 60 ms (its fastest). ";
            }
            else if (c.unit == 'V')
            {
                int bestStep = 0; double bestE = 1e9;
                for (int st = 0; st < kNumThreshSteps; ++st)
                {
                    const double gr = variMuGainReductionDb (-6.0 - compThresholdDb (st), c.limit ? Limit : Compress);
                    if (std::abs (gr - c.grDb) < bestE) { bestE = std::abs (gr - c.grDb); bestStep = st; }
                }
                int at = 0, rc = 0;
                for (int i = 0; i < kNumAttackSteps; ++i)   if (std::abs (std::log (kAttackMs[i] / c.attMs)) < std::abs (std::log (kAttackMs[at] / c.attMs))) at = i;
                for (int i = 0; i < kNumRecoverySteps; ++i) if (std::abs (std::log (kRecoverySec[i] * 1000 / c.relMs)) < std::abs (std::log (kRecoverySec[rc] * 1000 / c.relMs))) rc = i;
                vm.compIn = true; vm.threshold = bestStep; vm.attack = at; vm.recovery = rc;
                vm.output = std::clamp ((int) std::lround (c.grDb / 0.5), 0, kNumOutputSteps - 1);
                vm.mix = std::clamp ((int) std::lround (c.mix * 10), 0, 10);
                vm.mode = c.limit ? Limit : Compress; vm.scHp = true;
                char t[200];
                std::snprintf (t, sizeof t, "Vari-Mu: threshold %.1f dB (set it for about %.1f dB), attack %g ms, recovery %g s. ",
                               compThresholdDb (bestStep), c.grDb, kAttackMs[at], kRecoverySec[rc]);
                compNotes += t;
            }
        }
        notes += compNotes;
        notes += "Settings as given in the supplied list; not verified with or endorsed by " + sp.engineer + ".";

        // escape for a C++ string literal
        auto esc = [] (const std::string& s) { std::string o; for (char ch : s) { if (ch == '"' || ch == '\\') o += '\\'; o += ch; } return o; };

        std::printf ("    {   // %s - %s\n        Preset p; p.name = \"%s - %s\"; p.category = \"%s\"; p.full = true;\n",
                     sp.engineer.c_str(), sp.title.c_str(), esc (sp.engineer).c_str(), esc (sp.title).c_str(), sp.cat.c_str());
        std::printf ("        p.notes = \"%s\";\n", esc (notes).c_str());
        std::printf ("        p.tubeOn = %s; p.driveDb = %.1ff; p.order = %d;\n", yes (sp.tube), sp.drive, sp.order);
        const auto& a = fit.pa;
        std::printf ("        p.ch = channel ({ %d, %d, %d, %d, %s }, { %d, %d, %d, %d, %s }, { %d, %d, %d, %d, %s }, { %d, %d, %d, %d, %s }, %d, %d);\n",
                     a.band[0].freqIndex, a.band[0].gainStep, a.band[0].bwStep, a.band[0].mode, yes (a.band[0].shelf),
                     a.band[1].freqIndex, a.band[1].gainStep, a.band[1].bwStep, a.band[1].mode, yes (a.band[1].shelf),
                     a.band[2].freqIndex, a.band[2].gainStep, a.band[2].bwStep, a.band[2].mode, yes (a.band[2].shelf),
                     a.band[3].freqIndex, a.band[3].gainStep, a.band[3].bwStep, a.band[3].mode, yes (a.band[3].shelf), a.hp, a.lp);
        const auto& q = fit.pq;
        const bool pqIn = fit.usedS[0] || fit.usedS[1] || fit.usedS[2] || fit.usedSHs || fit.usedSLs;
        std::printf ("        p.pq.in = %s; for (auto& c : p.pq.ch) { c.band[0] = { %d, %d, %d }; c.band[1] = { %d, %d, %d }; c.band[2] = { %d, %d, %d };"
                     " c.hiShelfLevel = %d; c.hiShelf12k = %s; c.loShelfLevel = %d; c.loShelf100 = %s; c.in = true; }\n",
                     yes (pqIn), q.band[0].freqIdx, q.band[0].shape, q.band[0].level, q.band[1].freqIdx, q.band[1].shape, q.band[1].level,
                     q.band[2].freqIdx, q.band[2].shape, q.band[2].level, q.hiShelfLevel, yes (q.hiShelf12k), q.loShelfLevel, yes (q.loShelf100));
        std::printf ("        p.comp.compIn = %s; p.comp.threshold = %d; p.comp.attack = %d; p.comp.recovery = %d; p.comp.output = %d; p.comp.mix = %d; p.comp.mode = %d; p.comp.scHp = true;\n",
                     yes (vm.compIn), vm.threshold, vm.attack, vm.recovery, vm.output, vm.mix, vm.mode);
        const auto& c0 = ca.ch[0];
        std::printf ("        p.ca.in = %s; p.ca.mode = ClassAStereo; p.ca.link = true;\n"
                     "        for (auto& c : p.ca.ch) { c.thresholdDb = %.1f; c.attackMs = %.3f; c.releaseMs = %.0f; c.ratioPos = %.2f; c.feedForward = %s;"
                     " c.autoFastAttack = %s; c.autoFastRelease = %s; c.gainDb = %.1f; c.mix = %.2f; c.warm = %s; c.scOn = %s; c.scTilt = %.1f; c.scFreq = %.0f; }\n",
                     yes (ca.in), c0.thresholdDb, c0.attackMs, c0.releaseMs, c0.ratioPos, yes (c0.feedForward), yes (c0.autoFastAttack), yes (c0.autoFastRelease),
                     c0.gainDb, c0.mix, yes (c0.warm), yes (c0.scOn), c0.scTilt, c0.scFreq);
        std::printf ("        v.push_back (p);\n    }\n");
        ++count;

        std::string dropped;
        for (const auto& m : fit.dropped) dropped += " [" + moveText (m) + "]";
        std::fprintf (stderr, "%-64s %s%s\n", (sp.engineer + " - " + sp.title).c_str(),
                      fit.placed.empty() ? "-" : (std::to_string (worst).substr (0, 4) + " dB").c_str(), dropped.c_str());
    }
    std::printf ("    return v;\n}\n} // namespace ppm::presets\n");
    std::fprintf (stderr, "%d presets (%d track EQ presets skipped, %d kept their compressor only)\n", count, skipped, strippedEq);
    return 0;
}
