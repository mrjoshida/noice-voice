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

- **Real-Time Mid-Sagittal LCD Visualization (240×240)**:
  - Cranium, cervical spine, nasal cavity, and hard palate anatomy.
  - Live 4-anchor muscular tongue spline responding dynamically to `TONGUE` and `JAW` controls.
  - Retractable soft palate velum opening to ventilate the nasal chamber.
  - Highlighting pink fleshy lips (`#f472b6`) opening and protruding with lip aperture.
  - Acoustic particle/wavefront radiation pulsing outward from the mouth in real time.
  - 2D Formant Vowel Quad chart (`[i]`, `[e]`, `[a]`, `[o]`, `[u]`) with real-time tracking crosshair.
  - Syllable transition progress banner and live 4-column parameter cards.

- **4 Vocal Creature Archetypes**:
  1. **HUMAN**: Standard 17.5 cm vocal tract, natural speech formants, balanced vocal warmth.
  2. **TROLL**: Cavernous 24.0 cm elongated airway, deep guttural formant downward shift, resonant subglottal chest pulse.
  3. **ELF**: Agile 13.5 cm shortened airway, bright bell-like high formant resonances, rapid articulatory transients.
  4. **BEAST**: Predatory 21.0 cm airway with growl overdrive saturation, harsh tracheal turbulence, heavy sub-bass drone.

- **Articulatory Mouth Envelope (Syllable Morphing)**:
  - **Start Shape** $\rightarrow$ **Target Shape** transition across every note trigger:
    - Start Articulators: `TONGUE`, `JAW`, `LIPS`, `VELUM`
    - Target Articulators: `END TNG`, `END JAW`, `END LIP`, `END VEL`
  - Articulatory Envelope Controls:
    - `SPEED`: Syllable transition duration (10 ms to 1.5 s).
    - `HOLD`: Duration the vocal tract sustains the target position before releasing.
    - `RETURN`: Time taken to transition back to rest or next phoneme.
    - `MODE`: `GLIDE` (continuous morph), `RETURN` (spring back), `BOUNCE` (damped oscillation), `TOUCH` (velocity-scaled).

- **16-Step Sequencer & Speech Sentence Engine**:
  - Preloaded factory speech sentence: **"NOICE VOICE CAN SPEAK WORDS TO YOU NOW"**.
  - 16 steps with per-step parameter locks (`motion_set_lock`) controlling start articulators, target articulators, and pitch melody ($A3 \rightarrow B3 \rightarrow C4 \rightarrow D4 \rightarrow C4 \rightarrow B3 \rightarrow A3 \rightarrow G3$).
  - Pressing `PLAY` immediately speaks the sentence with synchronized 60 FPS vocal tract animation.

---

## Hardware Navigation & Page Map

All FM-1 hardware buttons are mapped to dedicated vocal synthesis parameter families with cycling page counters (`[1/3]`, `[2/3]`, etc.):

| Button | Family | Page 1 | Page 2 | Page 3 |
|---|---|---|---|---|
| **HOME** | `HOME` | **VOCAL TRACT** (Midsagittal Vector Art + Live Formant Quad) | — | — |
| **EDIT** | `FAM_EDIT` | **MOUTH [1/3]**<br>`TONGUE`, `JAW`, `LIPS`, `VELUM` | **END MOUTH [2/3]**<br>`END TNG`, `END JAW`, `END LIP`, `END VEL` | **GLOTTIS [3/3]**<br>`BODY`, `CHOIR`, `TOUCH`, `VOICING` |
| **ENV** | `FAM_ENV` | **ENV TIME [1/3]**<br>`SPEED`, `HOLD`, `RETURN`, `MODE` | **END SHAPE [2/3]**<br>`END TNG`, `END JAW`, `END LIP`, `END VEL` | **BREATH [3/3]**<br>`ATTACK`, `RELEASE`, `TENSION`, `BREATH` |
| **FX** | `FAM_FX` | **RESONANCE [1/2]**<br>`WALL Q`, `TRACT LEN`, `TRACHEA`, `NASAL Q` | **SPACE [2/2]**<br>`GROWL`, `CHORUS`, `DELAY`, `REVERB` | — |
| **SEQ** | `FAM_SEQ` | **STEP [1/1]**<br>16-Step Piano Roll & Parameter Lock Editor | — | — |
| **SCL** | `FAM_SCL` | **SCALE [1/1]**<br>`ROOT`, `SCALE`, `QUANT`, `TRANS` | — | — |
| **LFO** | `FAM_LFO` | **VIBRATO [1/1]**<br>`VIB DEPTH`, `VIB RATE`, `FADE IN`, `GLIDE` | — | — |
| **SAVE** | `FAM_SAVE` | **USER [1/2]**<br>User Patch Storage | **PRESETS [2/2]**<br>32 Creature Presets Browser | — |

### Transport & Action Buttons
- **`PLAY`**: Starts or stops 16-step sequencer playback (illuminates bright green when playing).
- **`ARP`**: On Home screen, taps trigger **`RE-SPEAK`** to re-articulate the active syllable transition.
- **`OCT-` / `OCT+`**: Transpose pitch octave down/up. Holding both resets octave to 0.

---

## 32 Creature Presets

NOICE VOICE includes 32 built-in presets across the 4 creatures (8 presets per engine):

| # | HUMAN (Eng 5) | TROLL (Eng 6) | ELF (Eng 7) | BEAST (Eng 8) |
|---|---|---|---|---|
| **0** | **YOU** (`/j/` $\rightarrow$ `/u/`) | **GRUNT** (`/g/` $\rightarrow$ `/uh/`) | **BELL** (`/b/` $\rightarrow$ `/e/`) | **ROAR** (`/r/` $\rightarrow$ `/a/`) |
| **1** | **WOW** (`/w/` $\rightarrow$ `/a/`) | **BOULDER** (`/b/` $\rightarrow$ `/o/`) | **WHISPER** (`/w/` $\rightarrow$ `/i/`) | **SNARL** (`/s/` $\rightarrow$ `/a/`) |
| **2** | **NOICE** (`/n/` $\rightarrow$ `/oi/`) | **DEEP** (`/d/` $\rightarrow$ `/i/`) | **SING** (`/s/` $\rightarrow$ `/i/`) | **GROWL** (`/g/` $\rightarrow$ `/o/`) |
| **3** | **BABY** (`/b/` $\rightarrow$ `/e/`) | **CAVE** (`/k/` $\rightarrow$ `/e/`) | **CHIRP** (`/ch/` $\rightarrow$ `/i/`) | **HISS** (`/h/` $\rightarrow$ `/s/`) |
| **4** | **CHEST** (`/ch/` $\rightarrow$ `/e/`) | **SLUMBER** (`/s/` $\rightarrow$ `/uh/`) | **CRYSTAL** (`/k/` $\rightarrow$ `/i/`) | **BITE** (`/b/` $\rightarrow$ `/ai/`) |
| **5** | **CHOIR** (`/k/` $\rightarrow$ `/w/`) | **HOLLOW** (`/h/` $\rightarrow$ `/a/`) | **AIR** (`/e/` $\rightarrow$ `/uh/`) | **HOWL** (`/h/` $\rightarrow$ `/u/`) |
| **6** | **TALK** (`/t/` $\rightarrow$ `/o/`) | **THUNDER** (`/th/` $\rightarrow$ `/uh/`) | **SIREN** (`/s/` $\rightarrow$ `/ai/`) | **SCRAPE** (`/s/` $\rightarrow$ `/e/`) |
| **7** | **SCREAM** (`/s/` $\rightarrow$ `/i/`) | **EARTH** (`/uh/` $\rightarrow$ `/th/`) | **NYMPH** (`/n/` $\rightarrow$ `/i/`) | **PREDATOR** (`/p/` $\rightarrow$ `/e/`) |

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
