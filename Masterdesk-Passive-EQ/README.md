# Basemental MasterDesk Mastering Suite

A complete mastering suite in one plug-in:

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
   - `MasterDesk-Mastering-Suite-macOS` → **.pkg installer** (VST3 + AU + app)
   - `MasterDesk-Mastering-Suite-Windows` → **Setup .exe** (VST3 + app)
   - `MasterDesk-Mastering-Suite-Linux` → **.tar.gz** with `install.sh`
5. Optional: create a tag such as `v1.0.0` (Releases → *Draft a new release*)
   and the installers are attached to a public-facing release automatically.

### Installing
- **macOS:** double-click the `.pkg`. Signed builds open without a warning (see
  "Signing" below). Unsigned builds: right-click → **Open** the first time (or
  System Settings → Privacy & Security → *Open Anyway*). Then
  rescan in your DAW; Logic validates AUs on launch.
- **Windows:** run the Setup `.exe` → installs to `C:\Program Files\Common Files\VST3`.
  Windows SmartScreen may warn on unsigned installers: *More info → Run anyway*.
- **Linux:** extract, run `./install.sh` (installs to `~/.vst3`).

### Signing for public distribution (removes the macOS warning)
macOS shows *"Apple could not verify … is free of malware"* for any installer that
isn't signed with an Apple **Developer ID** and **notarized** by Apple. Only Apple
can issue those, through the Apple Developer Program ($99/year). Once you have it,
the workflow signs and notarizes everything automatically:

1. In Xcode (Settings → Accounts → Manage Certificates) or at developer.apple.com,
   create a **Developer ID Application** and a **Developer ID Installer** certificate.
2. In Keychain Access, select both certificates (with their private keys) →
   right-click → **Export 2 items…** → save as `certs.p12` with a password.
3. In Terminal: `base64 -i certs.p12 | pbcopy` (copies it as text).
4. At appleid.apple.com → Sign-In and Security → **App-Specific Passwords**, create one.
5. In your GitHub repository → Settings → Secrets and variables → Actions →
   **New repository secret**, add:

| Secret | Value |
|---|---|
| `MACOS_CERTS_P12` | the text copied in step 3 |
| `MACOS_CERTS_PASSWORD` | the password from step 2 |
| `APPLE_ID` | your Apple Account email |
| `APPLE_TEAM_ID` | your 10-character Team ID (developer.apple.com → Membership) |
| `APPLE_APP_PASSWORD` | the app-specific password from step 4 |

The next build produces a signed, notarized and stapled `.pkg` (and a notarized
plug-in zip) that opens with no warning. Until then the build is unsigned; to
install an unsigned build: right-click the `.pkg` → **Open** → **Open**, or
System Settings → Privacy & Security → **Open Anyway**.

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
`key_test` checks the key shifter (pitch within 0.1 cent from −12 to +12 semitones, level within 0.1 dB on tones and
~1 dB on noise, exact reproduction at 0, stereo phase kept) and key detection on major/minor progressions, also after shifting.
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
| Engineer Move – Air Lift (27k shelf) | The classic passive "air" band: wide 27 kHz shelf, ~+2.8 dB |
| Engineer Move – Silk Top (16k up, 3.3k down) | More top end without more harshness |
| Engineer Move – Low-End Tuck (Pultec trick) | 68 Hz shelf with the dip-and-bump, mud trimmed at 180 Hz, HP 23 |
| Engineer Move – Mud Scoop (270 Hz) | Clears the low mids, a touch of 8.2k shelf to balance |
| Engineer Move – Box Clear (470 Hz) | Removes boxy / cardboard tone |
| Engineer Move – Vocal Forward (2.2k) | Broad presence lift, low mids eased |
| Engineer Move – Smile Curve (club / loud) | Lows and highs up, 820 Hz relaxed |
| Engineer Move – Dark Mix Rescue | 5.6 kHz shelf + 4.7 kHz presence to open a dull mix |
| Engineer Move – Harsh Mix Tame | Softens 3.3 kHz bite and lowers the top shelf |
| Engineer Move – Thin Mix Weight | 100 Hz shelf + 180 Hz body |
| Engineer Move – Boomy Mix Tighten | Cuts 150 Hz boom, HP 30 |
| Engineer Move – Vinyl Pre-Master | HP 30, sibilance eased at 6.8k, LP 20k |
| Engineer Move – Streaming Translation | Gentle all-rounder for earbuds, cars and phones |
| Engineer Move – Analog Warmth (tubes) | 120 Hz bloom, soft air, +6 dB tube drive |

The "Engineer Move" presets are the well-known moves mastering engineers use on passive EQs. They are not any particular engineer's settings, and no real engineers' names are used.

The first ten presets leave the compressor at threshold 0 dB (transparent).

Edit or add presets in `Source/Presets.h`.

---

## The faceplate, explained

The interface follows the "Basemental Masterdesk" look: brushed black metal,
chrome-ring knobs, cream meter faces, glass tubes and jewel lamps. Hover over
anything for a tooltip, or press **HELP ?** for the full guide page.

| Area | What it is | What it tells you / does |
|---|---|---|
| **INPUT** knob + meter (top left) | Input level | −24 to +24 dB of gain before everything else (EQ, tubes, compressor, meters), click-free. The L/R LED meter under it shows the peak level after INPUT: green, amber above −6 dBFS, red above −1 dBFS; white tick = 2 s peak hold. |
| **VU LEFT / VU RIGHT** | Moving-needle VU meters on the output (black faces, white scale) | Average loudness. 0 VU = −18 dBFS RMS. Real VU ballistics (~300 ms). The orange lamp under each meter flashes when peaks pass −1 dBFS. |
| **HARMONICS** meter | Needle meter in % THD | How much colour the tube stage is adding. < 0.3 % = subtle sheen, red zone (1 %+) = audible saturation. Green lamp = tube stage on. |
| **TUBE STAGE** glass tubes | Visual indicator | Dark when the tube stage is off; glow brighter with level and DRIVE. |
| **Jewel lamps** | SIGNAL (green), LINK (yellow), CLIP (red), TUBE (amber) | Audio present · right follows left · output went over 0 dBFS (holds ~1 s) · tube stage on. |
| **DRIVE** knob + **TUBE ON** | Tube make-up stage | DRIVE pushes harder into the tubes for more harmonics; output loudness stays matched. |
| **SPECTRUM** screen | Live 1/3-octave analyzer | 62 pin-style meters (about ⅙ octave each, 19 Hz–19 kHz): a fine line with a glowing round head for each band that moves up and down with the output, plus a cream peak-hold dot. Range 0 to −62 dB; heads turn amber above −18 dB and red within 6 dB of full scale. A full-scale sine reads 0 dB in its band. |
| **SONG KEY** panel | Key detection + key shift | Shows the song's musical key (e.g. *A minor*), its Camelot code and key signature, with a certainty bar. It listens to about the last 20 s; **RELEARN** starts over for a new song. **SEMITONES** (−12…+12) and **FINE** (−50…+50 cents) with **KEY SHIFT** on move the whole song to a new key; the panel then shows *A minor → B minor*. The shifter is a stereo-locked, peak-locked phase vocoder (left/right phase kept exactly) that keeps each note — even a quiet one right next to a loud one — on its own new pitch. **FORMANT** (on by default) keeps the spectral envelope in place, so voices and instruments keep their natural tone instead of the "chipmunk" (up) or "giant" (down) sound. It adds 85 ms latency only while on (your DAW compensates), so switch it while stopped. Small shifts (±1–3 st) sound most natural. |
| **VECTORSCOPE** screen | Stereo image | Dot cloud of the output: vertical = mid (L+R), horizontal = side (L−R); tall/narrow = centred, wide = stereo. Auto-zooms quiet material (shown as "AUTO x…"). Top meter: L/R balance. Bottom meter: phase correlation −1…+1 (above 0 = mono-safe); the cream mark holds the lowest value for 3 s. |
| **LOUDNESS** panel | EBU R128 / ITU-R BS.1770-4 | INTEGRATED, SHORT-TERM (3 s), MOMENTARY (0.4 s), TRUE PEAK MAX (4× oversampled, dBTP), LRA, PLR, PSR, short-term and momentary maxima. Choose a TARGET (−14 streaming, −16 Apple Music, −23 EBU, −24 ATSC, −9 loud); INTEGRATED turns green within ±1 LU, red above, blue below. TRUE PEAK MAX turns red above −1 dBTP. RESET clears the integrated and maximum readings. Measured on the plugin's output and keeps running with the window closed. |
| **M/S CLASS-A COMPRESSOR** (ash-blue plate) | Works like the elysia alpha compressor | Built from the controls, ranges and behaviour published in the elysia alpha compressor manuals (an independent model, not affiliated with elysia). Signal flow as on the hardware: M/S encode → sidechain filter → compressor → Niveau filter → gain → mix → Warm → M/S decode → soft clip. Per channel (left = L or MID, right = R or SIDE): **THRESHOLD** (33 dB range, +2 to −31 dBFS); **FEEDBACK / FEED FWD** — feedback is smooth with a soft knee and a ratio that rises with reduction (printed 1:2.5 at 6 dB → ~1:4.5 at 17 dB), feed forward doubles the ratios and the attack and runs past limiting into negative ratios; **RATIO** 1:1.1 … 1:2.5 / X; **ATTACK** 0.01–150 ms (time to 10 dB of reduction); **RELEASE** 60 ms–1.8 s (time back from 6 dB, linear, attack added); **AUTO FAST** for attack (fast loud peaks once there's ~3 dB of reduction) and release (≈20 ms after short bursts, back to the set value for long passages); **SC filter** (6 dB/oct LP↔HP tilt up to −12 dB, 30 Hz–3.3 kHz, detector only); **NIVEAU filter** (tilt after the compressor: brighter = highs +3 / lows −5 dB, darker = lows +3 / highs −4.5 dB, 20 Hz–2 kHz, ×10); **GAIN** 0–12 dB; **MIX** DIR…COM; **WARM** (slew-rate limiter). Global: **ACTIVE**, **STEREO / M/S**, **LINK** (one control voltage from the left compressor controls; filters, gain, mix and warm stay separate), **SOFT CLIP** with threshold (−12…0 dB) and LED. It runs at the plug-in's 4× / 2× oversampled rate, like the original plug-in version, and starts switched off, so older sessions sound the same. Note: this follows the published behaviour; it is not a component-level measurement of the hardware. |
| **AUTO MASTER** (plate above the footer) | Genre-based mastering assistant | Choose a **GENRE** (POP, JAZZ, 70'S, 80'S, 90'S, R&B, HIP HOP), press **LISTEN** and play the song, ideally its fullest part. After 30 s of music (silence is skipped), it does the following: (1) measures the mix's tonal balance in 10 octave bands, using only the louder passages; (2) compares that balance with the genre's target curve; (3) fits the four passive bands to the difference using the real switch positions, preferring a few broad, gentle moves (max ±6 dB, scaled by **AMOUNT**); (4) sets INPUT so the mix arrives at about −16 LUFS (gain staging), then sets the tubes, filters and compressor for the style, finding the THRESHOLD that gives the genre's amount of gain reduction; (5) sets the **MAXIMIZER** gain that lands the song on the genre's loudness. Steps 4 and 5 work by rendering the recording through the real EQ, compressor and limiter. After applying, it measures the actual loudness for a few seconds and corrects the maximizer by up to two small steps. The blue display shows the mix (steel blue), the target (white dashes) and the result after EQ (aqua), plus the moves made and the loudness before → after. Clicking another genre or moving AMOUNT re-plans straight away from the same recording. **UNDO** returns to your own settings. Everything it sets stays adjustable. The genre targets are stylistic starting points, not measurements of particular records: POP −10, JAZZ −16, 70'S −14, 80'S −12, 90'S −10.5, R&B −10, HIP HOP −9 LUFS. |
| **MAXIMIZER** (right of AUTO MASTER) | Final look-ahead brickwall limiter | **GAIN** (−12…+18 dB) pushes into the limiter; **CEILING** (−3…0 dBFS) is the highest sample peak that can come out (−1 dB is the safe choice for streaming). It uses 1.5 ms of look-ahead, a release that slows down when limiting is sustained, and identical gain on both sides. The GR bar shows how much it is limiting: up to 2–3 dB is clean, while constant 5–6 dB+ starts to flatten the punch. It is the last stage, and its 1.5 ms delay is always included in the reported latency, so switching it never shifts timing. |
| **PARAMETRIC MASTERING EQUALIZER** (black panel, in front of the passive EQ) | Parametric mastering EQ | Laid out and scaled after the classic MES-432C/9 (an independent model, not affiliated with Sontec). First in the signal chain. Per channel: **SHELVING** — HI shelf (8 kHz, or 12 kHz with its switch) and LO shelf (100 Hz or 50 Hz), and three parametric bands **LO** 11–570 Hz, **MID** 125–6800 Hz, **HI** 3.4–25 kHz, each with **SHAPE** (5 / 6 / 9 / 11 / 15 dB/oct, wide → narrow), **FREQ** (stepped) and **LEVEL** (±9 dB: 0.5 dB steps to 3 dB, then 1 dB steps). The narrower the SHAPE, the slightly less peak gain (about 1.35 dB less for a 12 dB move at the narrowest setting), which keeps narrow moves smooth. Output stage: linear below −10 dBFS, then soft asymmetric rounding (2nd harmonic about −85 dB at 0 dBFS, 3rd about −103 dB). Filters are pre-warped in frequency and bandwidth and run at the plug-in's 4× / 2× rate, so even a 25 kHz air boost keeps its true analog shape (within 0.02 dB). Centre: LEFT / RIGHT lamps, **MAN / OFF** per channel, red **PUSH EQ** switches the whole unit in (starts switched out). With LINK on, the right side follows the left. Hover any knob to see its value. |
| **Channel rows** (LEFT / RIGHT) | Four EQ bands + FILTERS / TRIM | See the table below. When LINK is on, the right row dims because it follows the left. |
| **Cream windows** under knobs | Value readouts | The GAIN window shows the *actual* dB after the gain/bandwidth interaction. |
| **LINK · L > R · R > L · HELP** (footer) | Stereo + guide | Link channels, copy settings between them, open the help page. |
| **STEREO VARIABLE MU LIMITER COMPRESSOR** (gray brushed-metal panel) | Left and right compressor strips | Knobs sit on a black inset panel; **LIMIT / COMPRESS**, **HP SC IN / OFF** and **IN / BYPASS** are chrome bat-handle toggles (click to flip; the lit legend shows the position). See "Compressor controls" below. Each side has its own GAIN REDUCTION meter; with LINK on, the right side follows the left. |
| **ORDER** switch (compressor panel) | Signal order | EQ > COMP (default: shape tone, then level it) or COMP > EQ. Switching is click-free. |
| **PRESET** menu (footer) | Factory presets | Mastering starting points (list below). Your own settings save with your DAW session. |
| **POP OUT** button (top-left corner of each unit) | Open a unit in its own window | Click it on the Parametric EQ, Passive EQ, Vari-Mu compressor, M/S Class-A compressor or Auto Master + Maximizer to open that unit in its own resizable window (it keeps its proportions and stays on top of the DAW). The window is a live view of the same unit: every knob moves in both places, meters run in both, and it keeps working when you close the window. Open as many as you like; they close with the plug-in window. |
| **Side handles** (left and right) | Stretch the plug-in | Grab the chrome handle on either side and pull. **Sideways** makes the plug-in wider — up to 2.2× its standard width — and the panel spreads out to fill it (meters spaced out, wider spectrum, wider knob rows and displays); **up / down** makes it bigger. Nothing is ever squashed or stretched: the height sets the zoom and extra width becomes extra panel. Pulling it narrower than the standard shape just makes it smaller. The size is remembered with your session. In the standalone app the left handle grows the window to the left. |
| **SIZE** menu (footer) | Window size | 60 / 70 / 85 / 100 %, drag the bottom-right corner, or pull an edge handle. Everything scales **in proportion**; pull a side handle to make it wider. Opens at the largest size that fits your screen; size is remembered per session. |
| **MINI / FULL PANEL** (footer) | Compact view | MINI collapses to the metering only (VU, harmonics, input, spectrum, vectorscope, loudness) — handy on a second screen while mixing. FULL PANEL brings the EQ and compressor back. |
| **SCROLL** (footer) | Scroll view | The panel fills the window's width at a bigger, easier-to-read size Hover the mouse near the top or bottom edge and the panel glides up or down (faster the closer you get to the edge; a faint arrow shows which way there is more), or use the mouse wheel over the panel. Hover scrolling pauses while a mouse button is held, so turning a knob near the edge won't move the panel. The window can be any height, so it fits a laptop screen without shrinking the panel; pull the sides to zoom. Press **SCROLL ON** to go back to the normal fit-to-window view. Remembered with the session. |
| **FULL SCREEN / EXIT FULL** (footer) | Fill the screen | In the standalone app this is real macOS full screen (the window's green button also works, and the yellow button minimises); the panel is centred at the largest size that keeps its proportions. Inside a DAW the plug-in window grows as large as the screen allows; press again to return to the previous size. |
| **Nameplate** (footer right) | Branding | "BASEMENTAL MASTERDESK / MASTERING SUITE" brass plate; edit the text in `PluginEditor.cpp` (`Faceplate::paint`). |

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
