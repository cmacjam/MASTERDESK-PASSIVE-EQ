# Masterdesk Passive EQ — by Basemental

A two-in-one mastering channel:

- **Top: Passive EQ.** A stereo, four-band, all-passive *parallel* equaliser with
  tube make-up gain, modelled on the behaviour of the Manley Massive Passive **Mastering Version**.
- **Bottom: Vari-Mu tube compressor.** A variable-mu (remote-cutoff tube)
  compressor in the spirit of the Manley Variable Mu: soft knee, rising ratio,
  stepped attack/recovery, compress/limit, sidechain HP, parallel mix.

Every control is stepped and recallable, with VU, harmonics and gain-reduction metering.

| Format | macOS | Windows | Linux |
|---|---|---|---|
| VST3 | ✅ (universal: Apple Silicon + Intel) | ✅ x64 | ✅ x64 |
| AU (Logic, GarageBand) | ✅ | — | — |
| Standalone app | ✅ | ✅ | ✅ |
| AAX (Pro Tools) | needs Avid SDK + agreement (see below) | | |

> "Manley", "Massive Passive" and "Variable Mu" are Manley Laboratories' trademarks. This plugin
> uses its own name and panel design; describe it as "inspired by" when releasing.

---

## Get the installers (no developer tools needed)

The project includes an automatic build (`.github/workflows/build.yml`) that
compiles, tests and packages the plugin on Apple, Windows and Linux machines
provided free by GitHub.

1. Create a free account at github.com (if you don't have one).
2. Click **New repository** → name it e.g. `masterdesk-eq` → **Private** → **Create**.
3. On the new repository page click **uploading an existing file**, drag in
   *everything inside* the `PassiveMasterEQ` folder (including the hidden
   `.github` folder — on a Mac press ⌘⇧. in Finder to show it), and **Commit**.
4. Open the **Actions** tab. A run called **Build plugin** starts by itself
   (≈ 15–25 min). When it shows a green tick, open it and download from **Artifacts**:
   - `Masterdesk-Passive-EQ-macOS` → **.pkg installer** (VST3 + AU + app)
   - `Masterdesk-Passive-EQ-Windows` → **Setup .exe** (VST3 + app)
   - `Masterdesk-Passive-EQ-Linux` → **.tar.gz** with `install.sh`
5. Optional: create a tag such as `v1.0.0` (Releases → *Draft a new release*)
   and the installers are attached to a public-facing release automatically.

### Installing
- **macOS:** double-click the `.pkg`. Unsigned builds: right-click → **Open** the
  first time (or System Settings → Privacy & Security → *Open Anyway*). Then
  rescan in your DAW; Logic validates AUs on launch.
- **Windows:** run the Setup `.exe` → installs to `C:\Program Files\Common Files\VST3`.
  Windows SmartScreen may warn on unsigned installers: *More info → Run anyway*.
- **Linux:** extract, run `./install.sh` (installs to `~/.vst3`).

### Signing for public distribution (optional)
Add these repository secrets (Settings → Secrets → Actions) and the macOS
installer is signed and notarized automatically, so it opens without warnings:
`MACOS_APP_IDENTITY`, `MACOS_INSTALLER_IDENTITY`, `APPLE_ID`, `APPLE_TEAM_ID`,
`APPLE_APP_PASSWORD` (requires an Apple Developer account, $99/yr; the signing
certificates must be in the runner keychain — see Apple's "Developer ID" docs).

---

## Build it yourself

Requirements: CMake ≥ 3.22 and a C++17 compiler. JUCE 8 is downloaded
automatically on first configure (or pass `-DJUCE_DIR=/path/to/JUCE`).

**macOS** (Xcode or command-line tools):
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DPPM_COPY_AFTER_BUILD=ON
cmake --build build --config Release
installer/macos/build_pkg.sh build/PassiveMasterEQ_artefacts/Release 1.0.0 dist   # optional .pkg
```
`PPM_COPY_AFTER_BUILD=ON` installs into `~/Library/Audio/Plug-Ins/{VST3,Components}`.
Validate the AU: `auval -v aufx Mdeq Bsmt`.

**Windows** (Visual Studio 2022 with "Desktop development with C++"):
```bat
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
iscc /DAppVersion=1.0.0 /DArtefacts=%CD%\build\PassiveMasterEQ_artefacts\Release /DOutDir=%CD%\dist installer\windows\installer.iss
```

**Linux:**
```bash
sudo apt install libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev \
                 libxcomposite-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
```

**Tests** (also run by the automatic build):
```bash
cmake -B build -DPPM_BUILD_TESTS=ON && cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
`loudness_test` runs the EBU Tech 3341 and 3342 conformance cases (loudness within
±0.1 LU and LRA within ±1 LU at 44.1/48/96 kHz, all pass) and true-peak checks.
`comp_test` checks the compressor's curve, timing, link, sidechain, mix/bypass and
stability. `model_test` checks the control law, parallel band interaction, digital-vs-analog
curve match, stability at every extreme setting and tube harmonics. `host_test`
runs the real processor (oversampling, smoothing, stress test), every factory
preset, state save/restore, and renders UI screenshots.

### Releasing checklist
- Bump `VERSION` in `project(...)` in `CMakeLists.txt`.
- Never change `PLUGIN_CODE` / `PLUGIN_MANUFACTURER_CODE` after release (sessions would lose the plugin).
- **Licensing:** JUCE is dual-licensed (AGPLv3 or a commercial/free-tier JUCE licence);
  selling closed-source plugins requires the appropriate JUCE licence. Check juce.com/get-juce.
- **AAX / Pro Tools:** get the AAX SDK from Avid (developer agreement + PACE signing), then add
  `AAX` to `FORMATS` and call `juce_set_aax_sdk_path(...)` before `juce_add_plugin`.

---

## Factory presets
| Preset | What it does |
|---|---|
| Init (Flat) | Everything neutral, tube stage on at 0 dB drive |
| Air & Weight | HP 16 Hz, small 68 Hz bell, wide 16 kHz air shelf |
| Gentle Polish | Low shelf 47 Hz, soft 4.7 kHz lift, 27 kHz air, LP 40k |
| De-Mud | Cuts 270 Hz, adds a little 3.3 kHz clarity, HP 30 Hz |
| Warm Tube Glue | Mostly tube colour (+9 dB drive) with a gentle tilt |
| Pultec-Style Low End | Narrow 47 Hz shelf (bump + dip) with a 270 Hz cut |
| Bright Pop Master | Weight at 68 Hz, cut 390 Hz, presence 2.2k, air 16k |
| Smooth Top / De-Harsh | Cuts 4.7 kHz harshness, keeps the air, LP 27k |
| Vocal Presence (mix bus) | For mix buses: cut 180 Hz, lift 3.3k and 12k |
| Subsonic Clean-up Only | Just HP 30 Hz and LP 52 kHz |
| Mastering Glue (EQ + Vari-Mu) | Weight + air, then 1–2 dB of slow leveling (threshold −12, 0.8 s) |
| Loud & Warm Master | More EQ, LIMIT mode at −18, +6 dB tube drive, +5 dB make-up |
| Parallel Punch | Heavy compression (−24) blended in at 40 % for density without losing transients |
| Gentle Leveler (Comp > EQ) | Compressor first (−15, 2 s recovery), then gentle shelves |

The first ten presets leave the compressor at threshold 0 dB (transparent).

Edit or add presets in `Source/Presets.h`.

---

## The faceplate, explained

The interface follows the "Basemental Masterdesk" look: brushed black metal,
chrome-ring knobs, cream meter faces, glass tubes and jewel lamps. Hover over
anything for a tooltip, or press **HELP ?** for the full guide page.

| Area | What it is | What it tells you / does |
|---|---|---|
| **VU LEFT / VU RIGHT** (top left) | Moving-needle VU meters on the output | Average loudness. 0 VU = −18 dBFS RMS. Real VU ballistics (~300 ms). The orange lamp under each meter flashes when peaks pass −1 dBFS. |
| **HARMONICS** meter | Needle meter in % THD | How much colour the tube stage is adding. < 0.3 % = subtle sheen, red zone (1 %+) = audible saturation. Green lamp = tube stage on. |
| **TUBE STAGE** glass tubes | Visual indicator | Dark when the tube stage is off; glow brighter with level and DRIVE. |
| **Jewel lamps** | SIGNAL (green), LINK (yellow), CLIP (red), TUBE (amber) | Audio present · right follows left · output went over 0 dBFS (holds ~1 s) · tube stage on. |
| **DRIVE** knob + **TUBE ON** | Tube make-up stage | DRIVE pushes harder into the tubes for more harmonics; output loudness stays matched. |
| **SPECTRUM** screen | Live 1/3-octave analyzer | 31 segmented bars (25 Hz–16 kHz) that move up and down with the output, with peak-hold caps. Range 0 to −66 dBFS; red segments = within 6 dB of full scale. A full-scale sine reads 0 dB in its band. |
| **VECTORSCOPE** screen | Stereo image | Dot cloud of the output: vertical = mid (L+R), horizontal = side (L−R); tall/narrow = centred, wide = stereo. Auto-zooms quiet material (shown as "AUTO x…"). Top meter: L/R balance. Bottom meter: phase correlation −1…+1 (above 0 = mono-safe); the cream mark holds the lowest value for 3 s. |
| **LOUDNESS** panel | EBU R128 / ITU-R BS.1770-4 | INTEGRATED, SHORT-TERM (3 s), MOMENTARY (0.4 s), TRUE PEAK MAX (4× oversampled, dBTP), LRA, PLR, PSR, short-term and momentary maxima. Choose a TARGET (−14 streaming, −16 Apple Music, −23 EBU, −24 ATSC, −9 loud); INTEGRATED turns green within ±1 LU, red above, blue below. TRUE PEAK MAX turns red above −1 dBTP. RESET clears the integrated and maximum readings. Measured on the plugin's output and keeps running with the window closed. |
| **Channel rows** (LEFT / RIGHT) | Four EQ bands + FILTERS / TRIM | See the table below. When LINK is on, the right row dims because it follows the left. |
| **Cream windows** under knobs | Value readouts | The GAIN window shows the *actual* dB after the gain/bandwidth interaction. |
| **LINK · L > R · R > L · HELP** (footer) | Stereo + guide | Link channels, copy settings between them, open the help page. |
| **VARI-MU TUBE COMPRESSOR** (bottom panel) | Left and right compressor strips | See "Compressor controls" below. Each side has its own GAIN REDUCTION meter; with LINK on, the right side follows the left. |
| **ORDER** switch (compressor panel) | Signal order | EQ > COMP (default: shape tone, then level it) or COMP > EQ. Switching is click-free. |
| **PRESET** menu (footer) | Factory presets | Mastering starting points (list below). Your own settings save with your DAW session. |
| **SIZE** menu (footer) | Window size | 60 / 70 / 85 / 100 %, or drag the bottom-right corner. Opens at the largest size that fits your screen; remembered per session. |
| **Nameplate** (footer right) | Branding | "BASEMENTAL MASTERDESK" brass plate; edit the text in `PluginEditor.cpp` (`Faceplate::paint`). |

Tips: double-click any knob to reset it; every knob clicks through its detents like the hardware.

## Controls (per channel, Left and Right)

| Section | Control | Positions |
|---|---|---|
| Each of 4 bands | FREQ | 11-position switch (table below) |
| | GAIN | 16 detents, 0 = flat; realised dB shown under the knob |
| | BANDWIDTH | 16 detents, CCW = wide, CW = narrow |
| | BOOST / OUT / CUT | 3-way |
| | BELL / SHELF | low two bands → low shelf, high two → high shelf |
| Filters/trim | HIGH PASS | OFF, 12, 16, 23, 30, 39 Hz (18 dB/oct) |
| | LOW PASS | OFF, 52k (30 dB/oct), 40k, 27k, 20k, 15k (18 dB/oct) |
| | GAIN TRIM | −2.5 … +2.5 dB in 0.5 dB steps |
| | EQ IN | bypasses the EQ network (tube stage stays in, like the hardware EQ switch) |
| Global | LINK | Right follows Left; unlinking copies L → R as a starting point |
| | L > R / R > L | copy one channel's settings to the other |
| | TUBE / DRIVE | tube colour on/off; drive −6 … +18 dB, level-compensated |

### Compressor controls (per channel)
| Control | Positions | What it does |
|---|---|---|
| THRESHOLD | 0 … −30 dBFS, 1.5 dB steps | Where compression starts. 0 dB = effectively off. Lower it until the meter shows 1–3 dB on loud parts. |
| ATTACK | 2, 5, 10, 15, 20, 25, 35, 45, 55, 70 ms | How fast it grabs. Slower lets transients through. |
| RECOVERY | 0.2, 0.4, 0.6, 0.8, 1.2, 2, 4, 8 s | How fast it lets go (time to recover ~95 %). |
| OUTPUT | 0 … +12 dB, 0.5 dB steps | Make-up gain on the compressed signal. |
| MIX | 0 … 100 %, 10 % steps | Parallel compression: blend compressed with dry. |
| COMPRESS / LIMIT | 2-way | Soft variable-mu curve (≈1.5:1 rising to 4:1) or harder knee/higher ratio (4:1 rising to 20:1). |
| SC HP | on / off | Detector ignores bass below 100 Hz so lows don't pump the mix. |
| COMP IN | on / off | Click-free bypass of the compressor. |

**Measured** (offline tests): static curve within 0.05 dB of design; ATTACK and
RECOVERY land within a few ms / 1 % of their marked values; 40 Hz bass at
−6 dBFS triggers 9.1 dB of gain reduction with SC HP off but only 0.7 dB with
it on; the tube 2nd harmonic rises from −71 dBc (no compression) to −52 dBc
(heavy compression).

Every control is a discrete host parameter, so automation and recall snap to
detents exactly.

### Band frequencies (Hz)
| Band | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| LOW | 22 | 33 | 47 | 68 | 100 | 150 | 220 | 330 | 470 | 680 | 1k |
| LOW MID | 82 | 120 | 180 | 270 | 390 | 560 | 820 | 1.2k | 1.8k | 2.7k | 3.9k |
| HIGH MID | 220 | 330 | 470 | 680 | 1k | 1.5k | 2.2k | 3.3k | 4.7k | 6.8k | 10k |
| HIGH | 560 | 820 | 1.2k | 1.8k | 2.7k | 3.9k | 5.6k | 8.2k | 12k | 16k | 27k |

---

## How the model works

### 1. One parallel passive network, not four biquads
```
Vin ──[ Ys ]──┬── Vo ──► tube make-up (×K)
              │
            [ Yp ]
              │
             GND
Ys = Gs + Σ boost branches        Yp = Gp + Σ cut branches
H  = K · Ys / (Ys + Yp)
```
Each band is a resonant admittance (series-RLC band-pass for bells; 2nd-order
low/high-pass shapes for shelves). Boost puts the branch in the series arm,
cut puts it in the shunt arm. Because all bands share one divider:

- **Bands are sub-additive.** Two bands boosting 11 dB at 1 kHz give ≈ +14.6 dB,
  not +22 — they compete for the same headroom, as the hardware does.
- **Boost + cut at the same spot nearly cancel** (≈ +1 dB) instead of making a notch-on-a-bell.
- **Narrow shelves produce the "Pultec" bump/dip** automatically: the resonant
  shelf branch's phase swings past 90° just beyond the corner, which reduces the
  arm's admittance there.

### 2. Gain/bandwidth interaction (mastering version)
- Bell: max gain ≈ 6 dB at the widest setting, 11 dB at the narrowest.
- Shelf: 11 dB at the widest setting, ≈ 6.6 dB at the narrowest.
- dB per detent therefore changes with bandwidth — the readout under each GAIN knob shows the real value.
- Bandwidth → Q: bell 0.5 … 3.0, shelf 0.55 … 1.6.

### 3. Discretisation
Each branch is bilinear-transformed (trapezoidal rule, pre-warped at its own
frequency) — the same idea as a SPICE companion model. The node voltage is
solved exactly each sample, so there's no delay-free-loop approximation.
Everything runs in double precision at ≥ 176.4 kHz (4× at 44.1/48k, 2× at
88.2/96k), so the 27 kHz shelf and 52 kHz filter behave like the analog
curves. Measured worst-case deviation from the analog model over 40 random
settings: 0.36 dB (20 Hz–20 kHz).

### 4. Click-free stepping
Branch admittance, frequency and Q glide (≈20 ms) in the log domain; Boost↔Cut
moves cross between the arms; Bell↔Shelf fades the band out, swaps shape, and
fades back in; filter switches and EQ IN crossfade.

### 5. Tube make-up stage
Output-transformer low-frequency saturation, a biased (asymmetric) triode
curve normalised to unity small-signal gain, then a 4 Hz DC blocker. At
0 dB drive a 0 dBFS sine shows H2 ≈ −54 dBc, H3 ≈ −66 dBc (≈0.2 % THD,
mostly 2nd harmonic) and it is effectively clean at −18 dBFS; raise DRIVE for
more colour. Runs oversampled, so harmonics don't alias.

---

### 6. Variable-mu compressor
Feed-forward detector (3 ms RMS, optional 100 Hz high-pass sidechain) → soft-plus
knee → ratio that rises with depth of compression (the remote-cutoff tube's
falling "mu") → attack/recovery smoothing in dB → gain, small GR-dependent
2nd harmonic, make-up and dry/wet mix. Linked mode drives both channels from
the louder side. Runs at the same oversampled rate as the EQ.

---

## Corrections to the earlier notes / code
- **High band** is 560, 820, 1.2k, 1.8k, 2.7k, 3.9k, 5.6k, 8.2k, 12k, 16k, 27k (the earlier list had wrong values).
- The **16-position detents are the GAIN and BANDWIDTH knobs**; frequency is the 11-position switch.
- Each band has a **Boost / Out / Cut** toggle; the gain knob is unipolar.
- Shelves: the **two left bands are low shelves, the two right bands are high shelves** (the earlier code used a low-shelf formula for every band).
- `1-0 / (sampleRate * …)` in the smoothing code is a typo for `1.0 / …` (it evaluated to 1, i.e. no smoothing).
- Summing `(biquad − input)` deltas is still a series-style model: overlapping boosts add fully. The network above gives the real parallel interaction.
- `tanh(x · 1.15)` on every sample compresses a 0 dBFS master by ~2 dB with ~4 % distortion — far more than the hardware. The tube stage here is level-calibrated instead.
- Recomputing `sin/cos` coefficients every sample is unnecessary; coefficients update every 16 samples while gliding and not at all when idle.
