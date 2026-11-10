# NOICE VOICE

[![License: GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-blue.svg)](LICENSE)
[![Device: M-VAVE FM-1](https://img.shields.io/badge/hardware-M--VAVE%20FM--1-emerald.svg)](BUILDING.md)
[![Simulator: WebAudio](https://img.shields.io/badge/simulator-WebAudio%20HTML5-cyan.svg)](sim/index.html)

**NOICE VOICE** is an advanced physical modeling acoustic waveguide vocal synthesizer and speech sequencer for the **M-VAVE FM-1** groovebox hardware (JieLi AC79NN dual-core DSP). 

Instead of traditional subtractive oscillators and static formant filters, NOICE VOICE simulates the physical human (and creature) vocal tract in real time using a 44-section **Kelly-Lochbaum reflection waveguide**, dynamic nasal cavity branching, non-linear glottal pulse flow modeling, and an articulatory mouth envelope capable of speaking continuous English sentences.

An interactive **Web Simulator** is included in [`sim/index.html`](sim/index.html) and [`web/sim.html`](web/sim.html), replicating the full FM-1 faceplate, real-time 240×240 vector mid-sagittal LCD display, 4 rotary encoders, and live WebAudio physical synthesis.

---

## Key Features

- **Kelly-Lochbaum Acoustic Waveguide**:
  - 44 cylindrical tube sections simulating acoustic wave reflection and propagation through the pharyngeal and oral airways.
  - Coupled 16-section nasal cavity branching off at section 17, gated by a dynamic velum (soft palate) valve for natural nasal consonants (`/m/`, `/n/`, `/ng/`).
  - Rosenberg glottal flow excitation pulse with parameter-controlled pitch, pulse width, high-frequency spectral tilt, and aspiration breath turbulence.
  - Lip radiation impedance filter with fleshy pink lip opening and protrusion.
  - Subglottal tracheal pulse coupling for resonant chest warmth.

- **Articulatory Phoneme Engine (44 Onsets, 29 Codas & Beatbox)**:
  - **44 Onset Phonemes** across stops (`/p, b, t, d, k, g, ʔ, q/`), nasals (`/m, n, ŋ, ɲ/`), fricatives (`/f, v, θ, ð, s, z, ʃ, ʒ, h, x, ʁ, ħ, ʕ/`), affricates (`/tʃ, dʒ, ts/`), liquids/retroflexes (`/w, j, l, r, ɻ, ɹ, ɾ, ʈ, ɖ, ʂ/`), and non-pulmonic vocal beatbox percussion (`KICK`, `SNARE`, `HIHAT`, `RIM`, `CLICK`, `POP`).
  - **29 Coda Phonemes** including nasal closures, liquid/retroflex releases, fricative tails, stops, affricates, glottal stops, and clicks.
  - **Voice Onset Time (VOT) & Cavitation Physics**: Suppresses voiced glottal excitation during voiceless closures/bursts and triggers beatbox sub-bass pitch drop + lingual click impulses.

- **Real-Time Mid-Sagittal LCD Visualization (240×240)**:
  - Cranium, cervical spine, nasal cavity, and hard palate anatomy.
  - Live muscular tongue spline responding dynamically to `TONGUE`, `JAW`, and curled retroflex (`R-RET`, `T-RET`, `D-RET`, `S-RET`) postures.
  - Constriction site indicator across all 22 anatomical tube sections and context-aware page visualizers (`MOUTH`, `END MOUTH`, `PHONEMES`, `ENV TIME`, `END SHAPE`, `RESONANCE`).
  - Retractable soft palate velum opening to ventilate the nasal chamber, highlighting pink fleshy lips (`#f472b6`), and live acoustic radiation wavefronts.
  - 2D Formant Vowel Quad chart (`[i]`, `[e]`, `[a]`, `[o]`, `[u]`) with real-time tracking crosshair.
  - Syllable transition progress banner and live 4-column parameter cards.

- **4 Vocal Creature Archetypes**:
  1. **HUMAN**: Standard 17.5 cm vocal tract, natural speech formants, balanced vocal warmth.
  2. **TROLL**: Cavernous 24.0 cm elongated airway, deep guttural formant downward shift, ventricular subharmonic + subglottal chest pulse.
  3. **ELF**: Agile 13.5 cm shortened airway, bright bell-like high formant resonances, rapid articulatory transients.
  4. **BEAST**: Predatory 21.0 cm airway with growl overdrive saturation, harsh tracheal turbulence, heavy sub-bass drone.

- **Articulatory Mouth Envelope (Syllable Morphing)**:
  - **Start Shape** $\rightarrow$ **Target Shape** transition across every note trigger:
    - Start Articulators: `TONGUE`, `JAW`, `LIPS`, `VELUM`
    - Target Articulators: `END TNG`, `END JAW`, `END LIP`, `END VEL`
  - Articulatory Envelope Controls:
    - `SPEED`: Syllable transition duration (`2 ms` to `4.5 s`).
    - `HOLD`: Duration the vocal tract sustains the target position before returning.
    - `RETURN`: Time taken to transition back to start posture.
    - `MODE`: `GLIDE` (continuous morph), `RETURN` (glide & return on release), `BOUNCE` (cyclic oscillation), `TOUCH` (velocity/pressure-scaled).

- **16-Step Sequencer & 4-Track Speech Sentence Engine**:
  - **Preloaded across all 4 hardware tracks at boot (`110 BPM`)** using `motion_set_event` / `motion_set_lock` parameter locks:
    - **Track 1 (`HUMAN`)**: *"NOICE VOICE CAN SPEAK WORDS TO YOU NOW"*
    - **Track 2 (`TROLL`)**: *"DEEP CAVE OGRES ROAR AND CRUSH ROCK"* (with pharyngeal/uvular/retroflex growls)
    - **Track 3 (`ELF`)**: *"BREATHE IN BREATHE OUT I AM MADE OF AIR"*
    - **Track 4 (`BEAST`)**: *"BOOTS AND CATS AND BOOTS AND CATS"* (vocal beatbox groove)
  - Web Simulator includes 7 selectable phrases (`NOICE`, `OGRE`, `MADE AIR`, `BOOTS`, `CLICKS`, `RETRO`, `ELF SONG`).
  - Pressing `PLAY` immediately speaks with synchronized 60 FPS vocal tract animation.

---

## Hardware Navigation & Page Map

All FM-1 hardware buttons are mapped to dedicated vocal synthesis parameter families with cycling page counters (`[1/4]`, `[2/4]`, etc.):

| Button | Family | Page 1 | Page 2 | Page 3 | Page 4 |
|---|---|---|---|---|---|
| **HOME** | `HOME` | **VOCAL TRACT** (Midsagittal Vector Art + Live Formant Quad) | — | — | — |
| **EDIT** | `FAM_EDIT` | **MOUTH [1/4]**<br>`TONGUE`, `JAW`, `LIPS`, `VELUM` | **END MOUTH [2/4]**<br>`END TNG`, `END JAW`, `END LIP`, `END VEL` | **PHONEMES [3/4]**<br>`ONSET`, `CODA`, `BODY`, `CHOIR` | **GLOTTIS [4/4]**<br>`TENSION`, `BREATH`, `AT`, `VCE` |
| **ENV** | `FAM_ENV` | **ENV TIME [1/3]**<br>`SPEED`, `HOLD`, `RETURN`, `MODE` | **END SHAPE [2/3]**<br>`END TNG`, `END JAW`, `END LIP`, `END VEL` | **BREATH [3/3]**<br>`ATK`, `REL`, `TENSION`, `BREATH` | — |
| **FX** | `FAM_FX` | **RESONANCE [1/2]**<br>`WALL Q`, `LENGTH`, `TRACHEA`, `NASAL Q` | **SPACE [2/2]**<br>`DST`, `CHO`, `DLY`, `REV` | — | — |
| **SEQ** | `FAM_SEQ` | **STEP [1/1]**<br>16-Step Piano Roll & Parameter Lock Editor | — | — | — |
| **SCL** | `FAM_SCL` | **SCALE [1/1]**<br>`ROOT`, `SCL`, `QNT`, `TRN` | — | — | — |
| **LFO** | `FAM_LFO` | **VIBRATO [1/1]**<br>`DEPTH`, `RATE`, `FADE`, `GLD` | — | — | — |
| **SAVE** | `FAM_SAVE` | **PRESETS [1/2]**<br>48 Creature Presets Browser | **USER [2/2]**<br>User Patch Storage | — | — |

### Transport & Action Buttons
- **`PLAY`**: Starts or stops 16-step sequencer playback (illuminates bright green when playing).
- **`ARP`**: On Home screen, taps trigger **`RE-SPEAK`** to re-articulate the active syllable transition.
- **`OCT-` / `OCT+`**: Transpose pitch octave down/up. Holding both resets octave to 0.

---

## 48 Creature Presets

NOICE VOICE includes 48 built-in presets across the 4 creatures (12 presets per engine), each defining mouth postures, onsets/codas, envelope dynamics, and vocal tract resonance:

| # | HUMAN (Eng 5) | TROLL (Eng 6) | ELF (Eng 7) | BEAST (Eng 8) |
|---|---|---|---|---|
| **0** | **AH MALE** | **DEEP GROWL** | **SILVER** | **SNARL** |
| **1** | **WEE** | **CAVE ROAR** | **AIRY WE** | **PREDATOR** |
| **2** | **YOU** | **GUTTURAL** | **SPELL** | **HOWL** |
| **3** | **WHY** | **BOULDER** | **WHISPER** | **GUTTURAL** |
| **4** | **NOISE** | **OGRE WA** | **CHIME** | **BITE** |
| **5** | **MA-MA** | **TOMB** | **FAIRY** | **FRENZY** |
| **6** | **LA-LA** | **GIANT LA** | **MOONLIGHT** | **CHITIN** |
| **7** | **CHOIR 3** | **WAR CHANT** | **HIGH CHOIR** | **PACK** |
| **8** | **RETRO-R** (`/ɻ/` -> `/ɻ/`) | **UVULAR KH** (`/q/` -> `/k/`) | **LIQUID L** (`/l/` -> `/l/`) | **CLICK-POP** (`CLICK` -> `POP`) |
| **9** | **TH-FLOW** (`/θ/` -> `/ð/`) | **PHARYNX** (`/ħ/`) | **FLAPTAP** (`/ɾ/` -> `/ɾ/`) | **EJECT-K** (`KICK` -> `/p/`) |
| **10** | **CH-BURST** (`/tʃ/` -> `/tʃ/`) | **THUD** (`KICK` -> `/b/`) | **NY-SONG** (`/ɲ/` -> `/ɲ/`) | **RIMSHOT** (`RIM` -> `/k/`) |
| **11** | **BEATBOX** (`KICK` -> `CLICK`) | **GROWL TR** (`/r/` -> `/r/`) | **SH-WIND** (`/ʃ/` -> `/ʃ/`) | **SNARE-X** (`SNARE` -> `/f/`) |

---

## Interactive Web Simulator

The browser simulator runs locally without any build steps or server dependencies:

```bash
open sim/index.html
# or
open web/sim.html
```

### Simulator Controls
- **Mouse / Touch**: Drag knobs vertically to turn. Click buttons or piano keys to trigger notes.
- **Computer Keyboard**: Keys `A S D F G H J K` play white keys, `W E T Y U O P` play black keys. `Spacebar` toggles sequencer `PLAY`.
- **Live Sound**: Uses native WebAudio to run the physical waveguide equations in real time.

---

## Building and Flashing

### Prerequisites
- macOS (Apple Silicon or Intel) or Linux x86_64
- Docker (running, for cross-compiling with JieLi toolchain)
- Python 3 with Pillow (`pip install Pillow mido pyserial`)

### Host Unit Verification
Run the host-compiled test suites to verify creature presets, waveguide acoustic safety, formant ordering, and instruction cycle budgets:

```bash
# Verify 4 creature engines & parameter bounds
clang -O2 -I firmware/src -I firmware/bsp/chip -I firmware/bsp/boards/fm1 -I build/gen tests/creature_test.c -o build/host/creature_test && build/host/creature_test

# Verify acoustic waveguide DSP, formant ordering (/a/ vs /i/), and device CPU limits (< 15%)
clang -O2 -I firmware/src -I firmware/bsp/chip -I firmware/bsp/boards/fm1 -I build/gen tests/tract_test.c -o build/host/tract_test && build/host/tract_test
```

### Compile Firmware Package
Compile the full firmware image and package into `build/noice.fwsc`:

```bash
PYTHON=.venv/bin/python3 ./build.sh
```

### Install onto Hardware
Connect the M-VAVE FM-1 via USB-C and run the installer:

```bash
.venv/bin/python3 tools/fm1_install.py build/noice.fwsc --yes
```

### Test USB Communication
Verify status and play speech sentences over USB CDC serial and USB MIDI:

```bash
# Check CPU load, engine status, and battery over CDC console
.venv/bin/python3 scripts/fm1_status.py

# Send MIDI Start/Stop to speak the 16-step sentence
.venv/bin/python3 scripts/test_sentence_play.py

# Test word articulation over USB MIDI
.venv/bin/python3 scripts/test_words.py
```

---

## Credits & License

- **NOICE VOICE Physical Synthesis & UI**: Designed and developed for the M-VAVE FM-1.
- **Felucca Base**: Free software licensed under [GPL-3.0-only](LICENSE).
- **Hardware Platform**: M-VAVE FM-1 (JieLi AC79NN dual-core DSP).
