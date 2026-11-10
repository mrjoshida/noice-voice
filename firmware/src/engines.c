/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Engine table (order = PRESETS browsing order and the engine numbers of the editor protocol), the
 * factory patterns (SAVE > PHRASES) and the parts' sounds at power-on. */
#include "../engines/src/dsp.c"   /* Felucca-Engines (MIT): the shared DSP blocks */

/* The parts' engine memory (1.5): a part plays one engine at a time, so the engines with large per-part state
 * (engine_t.mem: PHYS's slots, GRAIN's grains and index, DRUM's lanes, FM6's notes, SLICE's reverse windows)
 * keep it in one region per part instead of an array each. The region holds the state of the engine that
 * last claimed it (eng_mem_owner); a switch to another such engine zeroes its engine_t.mem bytes, which is
 * the state each had at power-on, so a switch sounds as it did with the arrays (tests/union_test.c). State
 * that runs on across notes and switches stays outside (WHEEL's wheels, GRAIN's random numbers).
 * Sized by the largest, ADPCM when it is built (else PHYS, 3 x SYMP's slot); an engine that needs more than ENG_MEM_PART grows it for
 * every part (x NPART of the pool). Zero-initialised pool section: main.c fm1_cstart clears it. */
#if FELUCCA_ADPCM
#define ENG_MEM_PART 13344u    /* ADPCM: its 192 ms echo line, 8 voices and the DSP (13288 B; the host's pointers 13320) */
#else
#define ENG_MEM_PART 12888u
#endif
static union {
    uint64_t align;
    uint8_t b[ENG_MEM_PART];
} eng_mem[NPART] __attribute__((section(".pool")));
static uint8_t eng_mem_owner[NPART];       /* engine index + 1 whose state is in the region, 0 = none (zeros) */
#define ENG_MEM(p) ((void *)eng_mem[p].b)  /* part p's region (p < NPART) */
#define ENG_MEM_FITS(name, size) _Static_assert((size) <= ENG_MEM_PART, name " state over ENG_MEM_PART")

#include "../engines/src/eng_analog.c"   /* Felucca-Engines (MIT): ANALOG LOFI TRIO WHEEL NOISE */
#include "../engines/src/eng_phase.c"   /* Felucca-Engines (MIT): PHASE */
#include "../engines/src/eng_lofi.c"
#include "eng_sample.c"
#include "eng_formant.c"
#ifndef NOICE_PROBE
#define NOICE_PROBE 1            /* (felucca.c's default, for the host tests that include this file directly) */
#endif
#include "eng_tract.c"          /* TRACT: the NOICE VOICE vocal tract (NOICE_PROBE: in VOICE's slot) */
#include "../engines/src/eng_trio.c"
#include "../engines/src/eng_wheel.c"
#include "eng_grain.c"
#include "eng_phys.c"           /* PHYS: DaisySP physical models (phys_dsp.c, MIT) */
#include "eng_drum.c"           /* DRUM: the 8-lane kit (drum_voice.c) */
#include "../engines/src/eng_noise.c"
#include "eng_fm6.c"            /* FM6: 6-operator FM, msfa ported (fm6_core.c, Apache-2.0) */
#include "fm4_convert.c"        /* DIGITAL's tables, and its sounds -> FM6 */
#if FELUCCA_FM4
#include "eng_digital.c"        /* DIGITAL: four-operator FM (retired; FELUCCA_FM4=1 builds it) */
#endif
#if FELUCCA_SLICE
#include "eng_slice.c"
#endif
#if FELUCCA_ADPCM
#include "eng_adpcm.c"           /* ADPCM: BRR waves and an FIR echo, its DSP ported from snes_spc (adpcm_dsp.c, LGPL-2.1+) */
#endif

/* the editor protocol, user presets and projects store these indices: append, never reorder */
static const engine_t *const ENGINES[NENGINES] = {
    &ENG_ANALOG,                 /* 0 */
#if FELUCCA_FM4
    &ENG_DIGITAL,                /* 1 (ENGI_DIGITAL) */
#else
    &ENG_FM4_GONE,               /* 1: reserved (DIGITAL, retired: its sounds convert to FM6, fm4_convert.c) */
#endif
    &ENG_PHASE,                  /* 2 */
    &ENG_LOFI,                   /* 3 */
    &ENG_SAMPLE,                 /* 4 */
#if NOICE_PROBE
    &ENG_HUMAN,                  /* 5: HUMAN */
    &ENG_TROLL,                  /* 6: TROLL */
    &ENG_ELF,                    /* 7: ELF */
    &ENG_BEAST,                  /* 8: BEAST */
#else
    &ENG_FORMANT,                /* 5 VOICE (eng_formant.c: "voice" is a sounding note in voice.c) */
    &ENG_TRIO,                   /* 6 */
    &ENG_WHEEL,                  /* 7 */
    &ENG_GRAIN,                  /* 8 */
#endif
    &ENG_PHYS,                   /* 9 (ENGI_PHYS) */
    &ENG_DRUM,                   /* 10 (ENGI_DRUM) */
    &ENG_NOISE,                  /* 11 */
    &ENG_FM6,                    /* 12 (ENGI_FM6) */
#if FELUCCA_SLICE
    &ENG_SLICE,                  /* 13 (FELUCCA_SLICE=0 builds without it) */
#endif
#if FELUCCA_ADPCM
    &ENG_ADPCM,                  /* 14 (ENGI_ADPCM; FELUCCA_ADPCM=0 builds without it) */
#endif
};

/* a track's engine number as an index (the audio paths: a compare, cheaper than % NENGINES; a bad number: 0) */
static inline uint32_t eng_idx(uint32_t e) { return e < NENGINES ? e : 0u; }

/* part t's engine memory goes to its engine (t->engine; voice.c engine_block, once a block: on a switch, after
 * the old engine's fade, before any note on the new one): zeroed (its engine_t.mem bytes) when another
 * engine's state is in it. An engine without memory leaves it as it is: a switch back finds its state. */
static void eng_mem_claim(const track_t *t)
{
    uint32_t p = (uint32_t)(t - trk), e, k, n;
    uint32_t *w;
    if (p >= NPART)
        return;
    e = eng_idx(t->engine);
    n = ENGINES[e]->mem;
    if (!n || eng_mem_owner[p] == e + 1u)
        return;
    w = (uint32_t *)ENG_MEM(p);
    for (k = 0; k < (n + 3u) / 4u; k++)
        w[k] = 0;
    eng_mem_owner[p] = (uint8_t)(e + 1u);
}

/* the sample source a track's sound plays that has no data (SAMPLE's SET, GRAIN's SRC, SLICE's SRC: an empty or
 * invalid user slot, a set or PIANO this build lacks): its name (it plays a sine: eng_sample.c smp_sine), 0 = none;
 * *code a small number for it (1.., per engine and source) so the UI says it once (ui_input.c sample_notice) */
static uint8_t snd_said[NTRK];   /* per track: the code last said, 0 = none (a project load clears them: said again) */
static const char *snd_missing(const track_t *t, uint32_t *code)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    uint32_t si = (uint32_t)t->p[P_E0] % SMP_NALL;
#if FELUCCA_SLICE
    if (e == &ENG_SLICE) {
        si = slc_src_of(t->p);
        *code = 64u + si;
        return slc_get(si) ? 0 : N_SLC_SRC[si];
    }
#endif
    if (e != &ENG_SAMPLE && e != &ENG_GRAIN)
        return 0;
    *code = (e == &ENG_GRAIN ? 32u : 1u) + si % 32u;
    return smp_set_missing(si) ? SMP_ALL_NAMES[si] : 0;
}

/* the order the engines are shown in (PRESETS browsing and its ENG knob, the EDIT layer's keys, the editor's list):
 * engine indices, never DIGITAL's reserved 1 (with FELUCCA_FM4 it follows FM6). The indices stay as they are (the
 * stores and the protocol hold them); only this table orders them */
#if NOICE_PROBE
static const uint8_t ENGINE_ORDER[NENG_SHOWN] = {
    5,                           /* HUMAN */
    6,                           /* TROLL */
    7,                           /* ELF */
    8,                           /* BEAST */
};
#else
static const uint8_t ENGINE_ORDER[NENG_SHOWN] = {
    0,                           /* ANALOG */
    12,                          /* FM6 */
#if FELUCCA_FM4
    1,                           /* DIGITAL */
#endif
    2, 3, 4, 5, 6, 7, 8, 9,      /* PHASE LOFI SAMPLE VOICE TRIO WHEEL GRAIN PHYS */
    11,                          /* NOISE */
#if FELUCCA_SLICE
    13,                          /* SLICE */
#endif
    10,                          /* DRUM */
#if FELUCCA_ADPCM
    14,                          /* ADPCM (1.8): last, so the EDIT layer's white keys keep the 13 above (ui_layer.c LY_ENG_KEYS) */
#endif
};
#endif

/* the engines one can pick (engine 1 only with FELUCCA_FM4), in ENGINE_ORDER: eng_ok(e), the n-th of them
 * eng_vis(n), e's place among them eng_rank(e), the next / previous one eng_step(e, dir) (wraps) */
#if NOICE_PROBE
static int eng_ok(uint32_t e) { return e >= 5u && e <= 8u; }
#else
static int eng_ok(uint32_t e) { return e < NENGINES && (FELUCCA_FM4 || e != ENGI_DIGITAL); }
#endif
static uint32_t eng_vis(uint32_t n) { return ENGINE_ORDER[n % NENG_SHOWN]; }
static uint32_t eng_rank(uint32_t e)
{
    uint32_t n;
    if (!eng_ok(e))
        e = 5u;                                  /* Default to HUMAN */
    for (n = 0; n < NENG_SHOWN && ENGINE_ORDER[n] != e; n++)
        ;
    return n < NENG_SHOWN ? n : 0u;
}
static uint32_t eng_step(uint32_t e, int32_t dir)
{
    return eng_vis((eng_rank(e % NENGINES) + (dir > 0 ? 1u : NENG_SHOWN - 1u)) % NENG_SHOWN);
}

/* factory sequence patterns: SAVE > PHRASES loads one into the selected track (ui.c pat_load); a preset
 * only suggests one with PAT(n), loading a sound never touches the steps. Absolute notes, loaded as they
 * are (DRUM plays them as GM drums, SLICE as slices; SCL TRANS and OCT transpose): 0 = rest;
 * flags 1 = accent, 2 = slide, 4 = tie (holds the previous note). Names: at most 8 characters */
#define T_ 4
static const struct {
    const char *name;
    uint8_t note[16], flags[16];
} PATTERNS[] = {
    /* 1: NOICE VOICE CAN SPEAK WORDS TO YOU NOW */
    {"NOICE",   {57, 0, 59, 0, 60, 0, 62, 0, 60, 0, 59, 0, 57, 0, 55, 0},
     {1, T_, 1, T_, 1, T_, 1, T_, 1, T_, 1, T_, 1, T_, 1, T_}},
    /* 2: BREATHE IN BREATHE OUT I AM MADE OF AIR */
    {"MADE AIR",{64, 60, 62, 57, 57, 60, 64, 62, 60, 65, 64, 62, 60, 62, 59, 57},
     {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0}},
    /* 3: DEEP CAVE OGRES ROAR AND CRUSH ROCK */
    {"OGRE",    {36, 0, 38, 0, 39, 0, 41, 0, 41, 0, 39, 0, 38, 0, 36, 0},
     {1, T_, 1, T_, 1, T_, 1, T_, 1, T_, 1, T_, 1, T_, 1, T_}},
    /* 4: SILVER LIGHT SHINES IN FAIRY GLADE */
    {"ELF SONG",{72, 0, 74, 0, 76, 0, 79, 0, 76, 0, 74, 0, 72, 0, 72, 0},
     {1, T_, 1, T_, 1, T_, 1, T_, 1, T_, 1, T_, 1, T_, 1, T_}},
    /* 5: BOOTS AND CATS BEATBOX */
    {"BOOTS",   {36, 0, 42, 0, 38, 0, 42, 0, 36, 0, 42, 0, 38, 0, 42, 0},
     {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0}},
    /* 6: CLICKS AND POPS VOCAL GROOVE */
    {"CLICKS",  {36, 0, 60, 0, 65, 0, 60, 0, 36, 60, 0, 60, 65, 0, 60, 60},
     {1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1}},
    /* 7: RED ROADS RUN ROUND RETRO RIVERS */
    {"RETRO",   {57, 0, 60, 0, 62, 0, 60, 0, 59, 0, 57, 0, 60, 0, 57, 0},
     {1, T_, 1, T_, 1, T_, 1, T_, 1, T_, 1, T_, 1, T_, 1, T_}},
    /* 8: HARMONIC CHOIR */
    {"CHOIR",   {60, 0, 64, 0, 67, 0, 72, 0, 67, 0, 64, 0, 60, 0, 55, 0},
     {1, T_, 1, T_, 1, T_, 1, T_, 1, T_, 1, T_, 1, T_, 1, T_}},
    /* 9: ACID */
    {"ACID",    {45, 45, 57, 45, 0, 48, 45, 55, 45, 0, 57, 52, 45, 48, 0, 50},
     {1, 0, 2, 0, 0, 0, 1, 2, 0, 0, 1, 0, 0, 2, 0, 1}},
    /* 10: OFFBEAT */
    {"OFFBEAT", {0, 36, 0, 36, 0, 36, 0, 48, 0, 36, 0, 36, 0, 39, 0, 43},
     {0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0}},
    /* 11: MELODY */
    {"MELODY",  {60, 0, 67, 0, 72, 67, 0, 64, 62, 0, 69, 0, 74, 69, 0, 67},
     {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}},
    /* 12: BEAT */
    {"BEAT",    {36, 42, 42, 42, 38, 42, 36, 42, 36, 42, 42, 36, 38, 42, 46, 42},
     {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}},
    /* 13: ARP */
    {"ARP",     {48, 51, 55, 60, 63, 67, 72, 75, 48, 51, 55, 60, 63, 67, 72, 75},
     {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0}},
};
#undef T_
#define NPATTERNS (sizeof PATTERNS / sizeof PATTERNS[0])

/* the parts at power-on (engine, preset, PATTERNS[n - 1] in the sequencer, 0 = empty: all are): bass, pad, lead, drums */
#if NOICE_PROBE                     /* NOICE VOICE: 4 parts default to HUMAN, TROLL, ELF, BEAST */
static const uint8_t TRK_DEF[NPART][3] = {{5, 0, 1}, {6, 0, 3}, {7, 0, 2}, {8, 0, 5}};
#else
static const uint8_t TRK_DEF[NPART][3] = {{0, 4, 0}, {ENGI_FM6, 4, 0}, {3, 0, 0}, {ENGI_DRUM, 0, 0}}; /* ANALOG ACID,
                                                                       * FM6 PAD (was DIGITAL PAD), LOFI PULSE LD, DRUM KIT */
#endif
static uint32_t trk_def_engine(uint32_t i) { return TRK_DEF[i % NPART][0]; }
