/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 NOICE VOICE contributors */
/* TRACT: a vocal tract you play. A glottal source (one, or up to three for CHOIR: several vocal folds
 * through one mouth) drives a Kelly-Lochbaum waveguide of the airway, glottis -> lips, with a nasal
 * branch at the velum and turbulence noise at the narrowest constriction.
 *
 * Design references (no code copied): J. L. Kelly & C. Lochbaum (1962); N. Thapen, Pink Trombone
 * (MIT, 2017) for the waveguide layout, the three-way nasal junction and the reflection constants;
 * J. Hodge, CHIMERA (MIT, 2026) for the articulator model this tract will be driven by.
 *
 * PROBE STAGE (NOICE_PROBE): the inner loops at their final cost with a placeholder area function
 * (two vowel shapes and a lip opening), to measure the CPU on the FM-1 before the rest is built.
 *
 *   waves     R[i] (towards the lips) and L[i] (towards the glottis), int32, nominal |x| < 2^15
 *   k[i]      reflection at the junction between sections i-1 and i, Q14: (A[i-1] - A[i]) / (A[i-1] + A[i])
 *   step      one pass of the scattering at the internal rate (FS x TR_OS): 1 multiply per junction,
 *             losses as a shift (x - x >> d), in place (no junction arrays)
 *   control   the area function -> k[] every call (32 samples), the nasal junction's three coefficients,
 *             the constriction (noise site)
 *
 * State: one tract per part in the part's engine memory (engine_t.mem); the engine is mono (poly 1). */

#include "nv_artic.h"
#include "nv_artic.c"

#define TR_NMAX 44u              /* oral sections at 2x (17.5 cm / (c / 88.2 kHz)) */
#define TR_NNMAX 28u             /* nasal sections at 2x */
#define TR_Q 14                  /* reflection coefficients: Q14 */
#define TR_GREF 12288            /* glottal reflection 0.75 (Q14) */
#define TR_LREF (-13926)         /* lip reflection -0.85 */
#define TR_NREF (-14746)         /* nostril reflection -0.9 */
#define TR_NGLOT 3u              /* CHOIR: glottal sources through one tract */

typedef struct {
    nv_artic_t artic;            /* articulator & syllable state machine */
    int32_t R[TR_NMAX], L[TR_NMAX], k[TR_NMAX];
    int32_t nR[TR_NNMAX], nL[TR_NNMAX], nk[TR_NNMAX];
    int32_t A[TR_NMAX];          /* areas, cm^2 Q12 */
    int32_t jl, jr, jn;          /* three-way junction (Q14): left, right, nose */
    int32_t gref, lref, nref;    /* end reflections with the guide's losses lumped in (Q14) */
    int32_t jacc, nlast;         /* MODE 2X N1: the nose's junction input held over a step, its last output */
    uint32_t gph[TR_NGLOT];      /* glottal phases */
    uint32_t sub_ph;             /* subharmonic phase for TROLL growl */
    int32_t tilt[TR_NGLOT];      /* source low-pass */
    int32_t nz, nf1, nf2;        /* noise: xorshift, its band-pass state */
    int32_t dc_x, dc_y;          /* output DC blocker */
    uint8_t creature;            /* CREATURE_HUMAN, TROLL, ELF, BEAST */
    uint8_t n, nn, ns, os;       /* oral sections, nasal sections, nasal junction index, oversampling */
    uint8_t nsite;               /* noise injection section */
    int32_t ngain;               /* noise gain at the site (Q15) */
} tract_t;
ENG_MEM_FITS("TRACT", sizeof(tract_t));

static int is_tract_engine(uint32_t e) { uint32_t ei = e % NENGINES; return ei >= 5u && ei <= 8u; }
static tract_t *tr_of(const track_t *t) { return (tract_t *)ENG_MEM((uint32_t)(t - trk) % NPART); }

/* placeholder area functions (cm^2 Q12), glottis -> lips, 22 points (resampled to n): /a/ and /i/ */
static const uint16_t TR_SHAPE_A[22] = {
    2048, 2458, 2867, 2458, 2048, 1638, 1229, 1229, 1638, 2458, 4096,
    6144, 8192, 10240, 12288, 13107, 13107, 12288, 11469, 10240, 8192, 6144,
};
static const uint16_t TR_SHAPE_I[22] = {
    2867, 4096, 6144, 8192, 10240, 12288, 13107, 12288, 10240, 8192, 6144,
    4096, 2458, 1229, 819, 737, 819, 1229, 2048, 3277, 4096, 4915,
};
/* the nose: a fixed taper (cm^2 Q12), 14 points */
static const uint16_t TR_NOSE[14] = {
    4096, 5734, 7373, 8192, 8602, 8602, 8192, 7373, 6554, 5734, 4915, 4096, 3277, 2867,
};

static int32_t tr_refl(int32_t a, int32_t b)
{
    int32_t s = a + b;
    return s > 0 ? (int32_t)(((int64_t)(a - b) << TR_Q) / s) : 0;
}

static void tract_setup(tract_t *tr, uint32_t mode);

/* the control step: shape -> areas -> reflections (called every render, i.e. every 32 samples) */
static void tr_control(tract_t *tr, const int16_t *p)
{
    if (!tr->n || !tr->nn || !tr->ns || tr->ns >= tr->n)
        tract_setup(tr, 0u);
    uint32_t i, n = tr->n, nn = tr->nn;
    int32_t vel = tr->artic.cur_velum, amin = 0x7FFFFFFF;
    int32_t tlen = p ? clamp(p[P_TRACT_LEN], 0, 127) : 64;
    int32_t len_scale = 160 + ((tlen * 190) >> 7); /* 160 (short, ~0.62x) .. 350 (long, ~1.37x), 256 is nominal */
    for (i = 0; i < n; i++) {
        int32_t x_raw = ((int32_t)i * 21 * 256) / (int32_t)(n - 1u);
        int32_t x = (x_raw * 256) / len_scale;
        if (x < 0) x = 0;
        if (x > 21 * 256) x = 21 * 256;
        uint32_t j = (uint32_t)(x >> 8);
        uint32_t f = (uint32_t)(x & 255);
        uint32_t j1 = j < 21u ? j + 1u : 21u;
        int32_t a = tr->artic.cur_areas[j] + (((tr->artic.cur_areas[j1] - tr->artic.cur_areas[j]) * (int32_t)f) >> 8);
        if (a < 41)
            a = 41;                                        /* 0.01 cm^2 floor */
        tr->A[i] = a;
        if (i > 1u && i + 1u < n && a < amin) {
            amin = a;
            tr->nsite = (uint8_t)i;
        }
    }
    for (i = 1; i < n; i++)
        tr->k[i] = tr_refl(tr->A[i - 1], tr->A[i]);
    for (i = 1; i < nn; i++) {
        uint32_t x = (i * 13u << 8) / (nn - 1u), j = x >> 8, j1 = j < 13u ? j + 1u : 13u;
        int32_t a0 = TR_NOSE[j] + (((TR_NOSE[j1] - TR_NOSE[j]) * (int32_t)(x & 255u)) >> 8);
        x = ((i - 1u) * 13u << 8) / (nn - 1u), j = x >> 8, j1 = j < 13u ? j + 1u : 13u;
        tr->nk[i] = tr_refl(TR_NOSE[j] + (((TR_NOSE[j1] - TR_NOSE[j]) * (int32_t)(x & 255u)) >> 8), a0);
    }
    {   /* three-way junction at the velum: the port's area opens the nose */
        int32_t al = tr->A[tr->ns - 1u], ar = tr->A[tr->ns], an = 41 + vel, s = al + ar + an;
        tr->jl = (int32_t)(((int64_t)(2 * al - s) << TR_Q) / s);
        tr->jr = (int32_t)(((int64_t)(2 * ar - s) << TR_Q) / s);
        tr->jn = (int32_t)(((int64_t)(2 * an - s) << TR_Q) / s);
    }
    {   /* turbulence: louder the narrower the constriction */
        int32_t g = amin < 1229 ? (1229 - amin) * 26 : 0;  /* below 0.3 cm^2 */
        tr->ngain = (g * tr->artic.noise_amt) >> 7;
    }
    {   /* Nasal branch reflection (NASAL Q) */
        int32_t nq = p ? clamp(p[P_NASAL_Q], 0, 127) : 64;
        tr->nref = -8000 - ((nq * 8200) >> 7);
    }
}

/* one step of the waveguide at the internal rate: the glottal input g, returns lips + nose. The wall losses
 * are lumped into the three end reflections (tr->gref, lref, nref: tr_control), not taken at every section:
 * a uniform loss per section commutes with the lossless scattering for the paths between the ends, which
 * carry nearly all the energy, and it saves two operations a junction. nose: 0 = the nasal branch holds this
 * step (MODE 2X N1: the nose at half the oral rate; its junction input is averaged over the two steps).
 * Runs from RAM (.ram_text, no calls): the hot loop off the XIP cache */
#if defined(__APPLE__) || defined(__EMSCRIPTEN__) || defined(__linux__)
#define TR_RAMFN static __attribute__((noinline))
#else
#define TR_RAMFN static __attribute__((section(".ram_text"), noinline, used))
#endif
TR_RAMFN int32_t tr_step(tract_t *tr, int32_t g, int32_t nzs, uint32_t nose_on)
{
    int32_t *R = tr->R, *L = tr->L, *k = tr->k;
    uint32_t i, n = tr->n, ns = tr->ns, nn = tr->nn;
    int32_t rp = R[0], li, w, rn, lip, nose;
    R[0] = ((L[0] * tr->gref) >> TR_Q) + g;
    for (i = 1; i < ns; i++) {                             /* glottis -> velum */
        li = L[i];
        w = (k[i] * (rp + li)) >> TR_Q;
        rn = R[i];
        R[i] = rp - w;
        L[i - 1] = li + w;
        rp = rn;
    }
    {   /* the velum: oral left (ns - 1), oral right (ns), nose (0) */
        int32_t nl0 = tr->nL[0], jn;
        li = L[ns];
        rn = R[ns];
        L[ns - 1] = ((tr->jl * rp) >> TR_Q) + (((16384 + tr->jl) * (nl0 + li)) >> TR_Q);
        R[ns] = ((tr->jr * li) >> TR_Q) + (((16384 + tr->jr) * (rp + nl0)) >> TR_Q);
        jn = ((tr->jn * nl0) >> TR_Q) + (((16384 + tr->jn) * (li + rp)) >> TR_Q);
        rp = rn;
        if (!nose_on) {                                    /* half rate, first step: the nose holds */
            tr->jacc = jn;
            nose = tr->nlast;
        } else {   /* the nose, from its junction to the nostrils */
            int32_t *nR = tr->nR, *nL = tr->nL, *nk = tr->nk, np = nR[0];
            nR[0] = nose_on == 1u ? (jn + tr->jacc) >> 1 : jn;
            for (i = 1; i < nn; i++) {
                int32_t ni = nL[i], nw = (nk[i] * (np + ni)) >> TR_Q, nx = nR[i];
                nR[i] = np - nw;
                nL[i - 1] = ni + nw;
                np = nx;
            }
            nL[nn - 1] = (np * tr->nref) >> TR_Q;
            nose = tr->nlast = nR[nn - 1];
        }
    }
    for (i = ns + 1u; i < n; i++) {                        /* velum -> lips */
        li = L[i];
        w = (k[i] * (rp + li)) >> TR_Q;
        rn = R[i];
        R[i] = rp - w;
        L[i - 1] = li + w;
        rp = rn;
    }
    L[n - 1] = (rp * tr->lref) >> TR_Q;
    lip = R[n - 1];
    if (nzs) {                                             /* turbulence just past the constriction */
        uint32_t s = tr->nsite;
        R[s + 1u] += nzs >> 1;
        L[s + 1u] += nzs >> 1;
    }
    return lip + nose;
}

/* MODE 0 1X 22 (all at 44.1 kHz), 1 2X 44 (all at 88.2 kHz), 2 2X N1 (the mouth at 88.2, the nose at 44.1) */
static void tract_setup(tract_t *tr, uint32_t mode)
{
    uint32_t i, os = mode ? 2u : 1u, nos = mode == 1u ? 2u : 1u;
    tr->os = (uint8_t)(mode + 1u);
    tr->n = (uint8_t)(os == 2u ? TR_NMAX : TR_NMAX / 2u);
    tr->nn = (uint8_t)(nos == 2u ? TR_NNMAX : TR_NNMAX / 2u);
    tr->ns = (uint8_t)(tr->n * 17u / 44u);                 /* the velum: 17 of 44 sections from the glottis */
    /* the losses: 0.999 a section at 2x (Pink Trombone's), as ~(1 - n / 1024) at each end of a pass */
    tr->gref = TR_GREF - ((TR_GREF * (int32_t)tr->n * (int32_t)(3u - os)) >> 10);
    tr->lref = TR_LREF - ((TR_LREF * (int32_t)tr->n * (int32_t)(3u - os)) >> 10);
    tr->nref = TR_NREF - ((TR_NREF * (int32_t)tr->nn * (int32_t)(3u - nos)) >> 9);
    for (i = 0; i < TR_NMAX; i++)
        tr->R[i] = tr->L[i] = 0;
    for (i = 0; i < TR_NNMAX; i++)
        tr->nR[i] = tr->nL[i] = 0;
    if (!tr->nz)
        tr->nz = 0x2545F491;
    tr->sub_ph = 0;
    if (tr->artic.state == SYLL_IDLE)
        nv_artic_init(&tr->artic);
}

static void tract_sync_creature(tract_t *tr, uint32_t eng)
{
    uint32_t e = eng < NENGINES ? eng : 0u;
    if (e == 6)
        tr->creature = CREATURE_TROLL;
    else if (e == 7)
        tr->creature = CREATURE_ELF;
    else if (e == 8)
        tr->creature = CREATURE_BEAST;
    else
        tr->creature = CREATURE_HUMAN;
    tr->artic.creature = tr->creature;
}

static void tract_note_on(track_t *t, voice_t *v)
{
    tract_t *tr = tr_of(t);
    uint32_t i;
    (void)v;
    if (tr->os != 1u || !tr->n)
        tract_setup(tr, 0u);
    tract_sync_creature(tr, t->engine);
    tr->artic.onset_idx = (uint8_t)clamp(t->p[P_E5], 0, NV_NONSETS);
    tr->artic.coda_idx = (uint8_t)clamp(t->p[P_E6], 0, NV_NCODAS);
    nv_artic_note_on(&tr->artic, 0);
    for (i = 0; i < TR_NGLOT; i++) {
        tr->gph[i] = 0;
        tr->tilt[i] = 0;
    }
    tr->sub_ph = 0;
}

/* CHOIR's intervals (Q16): the root, a major third, a fifth */
static const uint32_t TR_CHOIR_Q16[TR_NGLOT] = {65536u, 82570u, 98193u};

static void tract_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    tract_t *tr = tr_of(t);
    const int16_t *p = t->p;
    uint32_t ng = 1u + (uint32_t)clamp(p[P_E7], 0, 2), i, j;
    uint32_t base_inc, inc[TR_NGLOT], te, tp, rp, rn;
    int32_t oq, tiltk, gv, gn, nst, brth;

    if (tr->os != 1u || !tr->n)
        tract_setup(tr, 0u);

    tract_sync_creature(tr, t->engine);

    /* Update articulator parameters: Start Mouth Shape */
    tr->artic.tongue = (uint8_t)clamp(p[P_E0], 0, 127);
    tr->artic.jaw = (uint8_t)clamp(p[P_E1], 0, 127);
    tr->artic.lips = (uint8_t)clamp(p[P_E2], 0, 127);
    tr->artic.velum = (uint8_t)clamp(p[P_E3], 0, 127);
    tr->artic.body = (uint8_t)clamp(p[P_E4], 0, 127);
    tr->artic.onset_idx = (uint8_t)clamp(p[P_E5], 0, NV_NONSETS);
    tr->artic.coda_idx = (uint8_t)clamp(p[P_E6], 0, NV_NCODAS);
    tr->artic.key_pressure = (t->at > 0) ? (uint8_t)t->at : (v->vel > 0 ? (uint8_t)v->vel : 64);

    /* Target Mouth Shape (End Shape on ENV Page 2) */
    tr->artic.end_tongue = (uint8_t)clamp(p[P_END_TONGUE], 0, 127);
    tr->artic.end_jaw = (uint8_t)clamp(p[P_END_JAW], 0, 127);
    tr->artic.end_lips = (uint8_t)clamp(p[P_END_LIPS], 0, 127);
    tr->artic.end_velum = (uint8_t)clamp(p[P_END_VELUM], 0, 127);

    /* Articulatory Envelope Dynamics (ENV Page 1: SPEED, HOLD, RETURN, MODE) */
    {
        uint32_t spd = (uint32_t)clamp(p[P_ENV_SPEED], 0, 127);
        uint32_t hld = (uint32_t)clamp(p[P_ENV_HOLD], 0, 127);
        uint32_t ret = (uint32_t)clamp(p[P_ENV_RET], 0, 127);
        tr->artic.speed_ms = 5u + ((spd * spd * 2995u) >> 14);     /* 5 .. 3000 ms */
        tr->artic.hold_ms = ((hld * hld * 3000u) >> 14);           /* 0 .. 3000 ms */
        tr->artic.return_ms = 5u + ((ret * ret * 2995u) >> 14);   /* 5 .. 3000 ms */
        tr->artic.morph_mode = (uint8_t)clamp(p[P_ENV_MODE], 0, 3); /* 0: GLIDE, 1: RETURN, 2: BOUNCE, 3: TOUCH */
    }

    /* Breath / Phonation Envelope (ENV Page 3: ATK, REL, TENSION, BREATH) */
    {
        uint32_t atk = (uint32_t)clamp(p[P_ATK], 0, 127);
        uint32_t rel = (uint32_t)clamp(p[P_REL], 0, 127);
        tr->artic.attack_ms = 2u + ((atk * atk * 1998u) >> 14);    /* 2 .. 2000 ms */
        tr->artic.release_ms = 5u + ((rel * rel * 2995u) >> 14);   /* 5 .. 3000 ms */
        tr->artic.tension = (uint8_t)clamp(p[P_SUS], 0, 127);      /* TENSION */
        tr->artic.breath_base = (uint8_t)clamp(p[P_DEC], 0, 127);  /* BREATH */
    }

    if (!v->gate && !tr->artic.hold_on)
        nv_artic_note_off(&tr->artic);

    nv_artic_update(&tr->artic, n);
    tr_control(tr, p);

    /* Pitch scaling per creature */
    base_inc = m->inc;
    if (tr->creature == CREATURE_TROLL) {
        base_inc = base_inc >> 1;                                  /* 1 octave down: subterranean chest fundamental */
    } else if (tr->creature == CREATURE_ELF) {
        base_inc = (base_inc < 0x7FFFFFFFu) ? (base_inc << 1) : base_inc; /* 1 octave up: singing fundamental */
    }

    for (j = 0; j < ng; j++)
        inc[j] = (uint32_t)(((uint64_t)base_inc * TR_CHOIR_Q16[j]) >> 16);

    if (tr->creature == CREATURE_BEAST) {
        /* Predatory cycle-to-cycle frequency flutter / shimmer */
        int32_t jit = (int32_t)(noise32(&tr->nz) & 255u) - 128;
        for (j = 0; j < ng; j++)
            inc[j] = (uint32_t)((int32_t)inc[j] + (((int32_t)inc[j] * jit) >> 13));
    }

    /* Acoustic Cavity: Wall damping / Q */
    {
        int32_t wall_q = clamp(p[P_WALL_Q], 0, 127);
        int32_t gref_base = 10000 + ((wall_q * 6300) >> 7); /* 10000 (~0.61) .. 16300 (~0.995) */

        if (tr->creature == CREATURE_TROLL) {
            oq = 54000 - (((int32_t)tr->artic.tension * 200));
            tiltk = 1100 + (int32_t)tr->artic.tension * 60;
            tr->gref = (gref_base * 7) >> 3;                    /* subglottal heavy tissue absorption */
            tr->lref = -(gref_base + 600);
        } else if (tr->creature == CREATURE_ELF) {
            oq = 62000 - (((int32_t)tr->artic.tension * 160));
            tiltk = 3600 + (int32_t)tr->artic.tension * 130;
            tr->gref = gref_base > 12000 ? gref_base : 12000;   /* ringing silver vocal tract */
            tr->lref = -(tr->gref + 800);
        } else if (tr->creature == CREATURE_BEAST) {
            oq = 51000 - (((int32_t)tr->artic.tension * 260));
            tiltk = 3100 + (int32_t)tr->artic.tension * 100;
            tr->gref = gref_base;
            tr->lref = -(gref_base + 800);
        } else { /* CREATURE_HUMAN */
            oq = 58982 - (((int32_t)tr->artic.tension * 284)); /* TENSION: open quotient */
            tiltk = 2500 + (int32_t)tr->artic.tension * 109;
            tr->gref = gref_base;
            tr->lref = -(gref_base + 800);
        }
        if (tr->lref < -16300) tr->lref = -16300;
        if (tr->gref > 16340) tr->gref = 16340;
    }

    te = (uint32_t)oq << 16;
    tp = (te >> 8) * 166u;
    rp = (1u << 30) / ((tp >> 16) | 1u);
    rn = (1u << 30) / (((te - tp) >> 16) | 1u);

    brth = tr->artic.cur_breath;                        /* 0..32767 */
    if (brth < 400 || tr->artic.whisper_on) {
        gv = 0;                                         /* unvoiced or below phonation threshold */
        gn = (brth * 6) >> 15;
    } else {
        if (tr->creature == CREATURE_TROLL) {
            gv = (brth * 3400) >> 15;
            gn = (brth * (int32_t)tr->artic.noise_amt) >> 13;
        } else if (tr->creature == CREATURE_ELF) {
            gv = (brth * 2600) >> 15;
            gn = (brth * ((int32_t)tr->artic.noise_amt + 32)) >> 11; /* silver breath */
        } else if (tr->creature == CREATURE_BEAST) {
            gv = (brth * 3200) >> 15;
            gn = (brth * ((int32_t)tr->artic.noise_amt + 48)) >> 11; /* snarling breath */
        } else {
            gv = (brth * 3000) >> 15;                       /* voicing level */
            gn = (brth * (int32_t)tr->artic.noise_amt) >> 12;/* aspiration */
        }
    }

    /* Voice Onset Time (VOT) & Voiceless Consonant Phonation Attenuation */
    if (tr->artic.state == SYLL_ONSET && tr->artic.onset_idx > 0u && tr->artic.onset_idx <= NV_NONSETS) {
        const nv_consonant_t *oc = &NV_ONSETS[tr->artic.onset_idx - 1u];
        if (!oc->voiced) {
            if (tr->artic.onset_idx >= 39u || tr->artic.state_samples < (tr->artic.onset_samples * 3u) / 4u) {
                gv >>= 3;                                   /* suppress fold voicing during closure */
                gn += (brth * 35) >> 12;                    /* voiceless aspiration turbulence */
            }
        }
    }

    {
        int32_t bb_kick = 0, bb_click = 0;
        if (tr->artic.state == SYLL_ONSET && tr->artic.onset_idx >= 39u && tr->artic.onset_idx <= NV_NONSETS) {
            uint32_t ss = tr->artic.state_samples;
            if (tr->artic.onset_idx == 39u) {
                /* KICK: 110 Hz -> 55 Hz sub-bass thump in first 35 ms */
                if (ss < 1500u)
                    bb_kick = 1500 - (int32_t)ss;
            } else if (tr->artic.onset_idx >= 42u && tr->artic.onset_idx <= 44u) {
                /* RIM / CLICK / POP: sharp cavitation impulse */
                if (ss < 260u)
                    bb_click = (260 - (int32_t)ss) * 8;
            }
        }

        int32_t trach_coupling = clamp(p[P_TRACHEA], 0, 127);
        int32_t growl_dist = clamp(p[P_DIST], 0, 127);
        nst = tr->nz;
        for (i = 0; i < n; i++) {
            int32_t g = 0, a, s, nzs = 0, y;
            for (j = 0; j < ng; j++) {                      /* the sources: derivative of a Rosenberg pulse */
                uint32_t ph = tr->gph[j];
                int32_t e;
                if (ph < tp)
                    e = (sine_i(((ph >> 16) * rp) << 1) * 17644) >> 15;
                else if (ph < te)
                    e = -sine_i(((ph - tp) >> 16) * rn);
                else
                    e = 0;
                e += blep(ph - te, inc[j]) >> 1;

                if (trach_coupling > 0 && ph < te) {
                    /* Subglottal tracheal coupling resonance rings through open glottis */
                    e += (sine_i((ph >> 14) * 3) * (trach_coupling * 40)) >> 15;
                }

                if (tr->creature == CREATURE_BEAST) {
                    /* Non-linear vocal fold overdrive / asymmetric saturation */
                    int32_t sat = (e * 7) >> 2;
                    if (sat > 13000)
                        e = 13000 + ((sat - 13000) >> 3);
                    else if (sat < -13000)
                        e = -13000 + ((sat + 13000) >> 3);
                    else
                        e = sat;
                }

                tr->tilt[j] += ((e - tr->tilt[j]) * tiltk) >> 14;
                g += tr->tilt[j];
                tr->gph[j] = ph + inc[j];
            }

            if (tr->creature == CREATURE_TROLL) {
                /* Subharmonic ventricular fold growl at f0/2 */
                uint32_t sph = tr->sub_ph;
                int32_t sub_pulse = (sine_i(sph >> 15) * 5500) >> 15;
                g += sub_pulse;
                tr->sub_ph = sph + (inc[0] >> 1);
            }

            if (growl_dist > 0) {
                /* Fold overdrive / saturation */
                int32_t drive = 128 + growl_dist * 3;
                int32_t sat = (g * drive) >> 7;
                g = soft_knee(clamp(sat, -30000, 30000), 16000);
            }

            g = (g * gv) >> 15;

            if (bb_kick) {
                uint32_t ss_i = tr->artic.state_samples + i;
                uint32_t kph = ss_i * 10500000u - (ss_i * 1200u) * ss_i;
                g += (sine_i(kph) * bb_kick) >> 10;
            }

            {
                int32_t w = (int32_t)(noise32(&nst) >> 17) - 16384;
                g += (w * gn) >> 15;                        /* aspiration at the glottis */
                if (tr->ngain) {                            /* turbulence: a band-pass on the same noise */
                    tr->nf1 += (w - tr->nf1) >> 2;
                    tr->nf2 += (tr->nf1 - tr->nf2) >> 3;
                    nzs = ((tr->nf1 - tr->nf2) * tr->ngain) >> 15;
                    if (tr->creature == CREATURE_BEAST)
                        nzs = (nzs * 5) >> 2;               /* predator raspy turbulence */
                }
                if (bb_click) {
                    uint32_t ss_i = tr->artic.state_samples + i;
                    uint32_t cph = ss_i * 120000000u;
                    nzs += (sine_i(cph) * bb_click) >> 9;
                }
            }
            {
                int32_t (*volatile vstep)(tract_t *, int32_t, int32_t, uint32_t) = tr_step;
                int32_t (*step)(tract_t *, int32_t, int32_t, uint32_t) = vstep;
                a = step(tr, g, nzs, 2u);
            }
            s = soft_knee(clamp(a << 3, -32000, 32000), 20000);
            y = s - tr->dc_x + ((tr->dc_y * 32604 + 16384) >> 15);   /* DC blocker, ~10 Hz */
            tr->dc_x = s;
            tr->dc_y = y;
            out[i] += voice_amp(y, m, i);
        }
    }
    tr->nz = nst;
}

/* ------------------------------------------------ Human Presets & Engine --- */
static const preset_t HUMAN_PRESETS[] = {
    /* TONGUE JAW LIPS VELUM | BODY ONSET CODA CHOIR; env: {ATK, DEC, SUS, REL}; fenv: TARGET */
    {"AH MALE",    {70,  115, 100, 0,  0,   0,  0,  0}, {4, 70, 110, 60}, 0, 1, FX(0, 0, 0, 40), 0},
    {"WEE",        {115, 15,  20,  0,  0,   23, 0,  0}, {4, 70, 110, 60}, 1, 1, FX(0, 0, 0, 40), 0},
    {"YOU",        {10,  10,  100, 0,  0,   24, 0,  0}, {4, 70, 110, 60}, 2, 1, FX(0, 0, 0, 40), 0},
    {"WHY",        {70,  115, 100, 0,  0,   23, 0,  0}, {4, 70, 110, 60}, 1, 1, FX(0, 0, 0, 40), 0},
    {"NOISE",      {115, 64,  35,  0,  0,   8,  10, 0}, {4, 70, 110, 60}, 1, 1, FX(0, 0, 0, 40), 0},
    {"MA-MA",      {64,  50,  0,   80, 0,   7,  1,  0}, {4, 70, 110, 60}, 3, 1, FX(0, 0, 0, 40), 0},
    {"LA-LA",      {70,  115, 100, 0,  0,   25, 0,  0}, {4, 70, 110, 60}, 0, 1, FX(0, 0, 0, 40), 0},
    {"CHOIR 3",    {70,  80,  100, 0,  0,   0,  0,  2}, {4, 70, 110, 60}, 0, 1, FX(0, 0, 0, 40), 0},
    {"RETRO-R",    {50,  70,  30,  0,  0,   27, 7,  0}, {4, 70, 110, 60}, 7, 1, FX(0, 0, 0, 40), 0},
    {"TH-FLOW",    {110, 40,  40,  0,  0,   13, 14, 0}, {5, 60, 105, 50}, 2, 1, FX(0, 0, 0, 40), 0},
    {"CH-BURST",   {90,  55,  45,  0,  0,   20, 24, 0}, {2, 65, 100, 45}, 1, 1, FX(0, 0, 0, 40), 0},
    {"BEATBOX",    {64,  40,  10,  0,  0,   39, 28, 0}, {1, 30, 80,  20}, 5, 1, FX(10, 0, 0, 25), 0},
};

/* ------------------------------------------------ Troll Presets & Engine --- */
static const preset_t TROLL_PRESETS[] = {
    {"DEEP GROWL", {50,  120, 80,  0,  0,   0,  0,  0}, {10, 80, 115, 70}, 3, 1, FX(20, 0, 0, 50), 0},
    {"CAVE ROAR",  {90,  127, 110, 0,  0,   33, 0,  0}, {8,  90, 120, 80}, 3, 1, FX(30, 0, 20, 60), 0},
    {"GUTTURAL",   {30,  90,  60,  40, 0,   37, 0,  0}, {6,  70, 110, 60}, 3, 1, FX(15, 0, 0, 40), 0},
    {"BOULDER",    {20,  60,  40,  0,  0,   2,  21, 0}, {12, 80, 120, 90}, 2, 1, FX(25, 0, 10, 50), 0},
    {"OGRE WA",    {40,  40,  30,  0,  0,   23, 0,  0}, {6,  70, 110, 60}, 3, 1, FX(10, 0, 0, 40), 0},
    {"TOMB",       {64,  80,  70,  60, 0,   38, 0,  0}, {20, 90, 120, 100},0, 1, FX(0, 0, 40, 80), 0},
    {"GIANT LA",   {80,  120, 90,  0,  0,   25, 0,  0}, {8,  70, 110, 60}, 0, 1, FX(20, 0, 0, 50), 0},
    {"WAR CHANT",  {64,  100, 90,  0,  0,   0,  0,  2}, {8,  80, 115, 70}, 0, 1, FX(20, 15, 0, 60), 0},
    {"UVULAR KH",  {25,  70,  45,  0,  0,   34, 20, 0}, {8,  75, 115, 65}, 3, 1, FX(25, 0, 0, 45), 0},
    {"PHARYNX",    {30,  85,  55,  0,  0,   36, 0,  0}, {7,  80, 120, 70}, 3, 1, FX(30, 0, 10, 45), 0},
    {"THUD",       {50,  50,  10,  0,  0,   39, 21, 0}, {1,  45, 90,  30}, 5, 1, FX(35, 0, 0, 30), 0},
    {"GROWL TR",   {60,  90,  60,  0,  0,   26, 6,  0}, {5,  70, 110, 60}, 3, 1, FX(20, 0, 0, 50), 0},
};

/* ------------------------------------------------ Elf Presets & Engine --- */
static const preset_t ELF_PRESETS[] = {
    {"SILVER",     {120, 20,  30,  0,  100, 17, 5,  0}, {6,  60, 100, 50}, 2, 1, FX(0, 30, 15, 60), 0},
    {"AIRY WE",    {30,  15,  25,  0,  80,  23, 0,  0}, {8,  70, 105, 60}, 2, 1, FX(0, 25, 20, 70), 0},
    {"SPELL",      {100, 30,  60,  0,  90,  15, 0,  0}, {4,  60, 100, 50}, 4, 1, FX(0, 40, 25, 65), 0},
    {"WHISPER",    {64,  40,  50,  20, 127, 19, 0,  0}, {10, 50, 90,  50}, 2, 1, FX(0, 20, 30, 80), 0},
    {"CHIME",      {125, 10,  20,  0,  110, 0,  0,  0}, {2,  40, 80,  40}, 0, 1, FX(0, 50, 20, 50), 0},
    {"FAIRY",      {110, 25,  40,  0,  120, 11, 0,  0}, {3,  50, 90,  40}, 4, 1, FX(0, 30, 10, 60), 0},
    {"MOONLIGHT",  {70,  35,  60,  0,  90,  7,  2,  0}, {15, 80, 110, 80}, 2, 1, FX(0, 35, 40, 90), 0},
    {"HIGH CHOIR", {115, 25,  40,  0,  100, 0,  0,  2}, {8,  70, 110, 70}, 2, 1, FX(0, 40, 30, 80), 0},
    {"LIQUID L",   {110, 20,  35,  0,  95,  25, 5,  0}, {5,  60, 100, 55}, 4, 1, FX(0, 25, 15, 65), 0},
    {"FLAPTAP",    {95,  25,  30,  0,  85,  29, 9,  0}, {2,  45, 85,  40}, 2, 1, FX(0, 20, 10, 55), 0},
    {"NY-SONG",    {115, 30,  45,  40, 105, 10, 4,  0}, {6,  65, 105, 60}, 4, 1, FX(0, 30, 20, 70), 0},
    {"SH-WIND",    {85,  45,  35,  0,  115, 17, 12, 0}, {12, 70, 100, 70}, 2, 1, FX(0, 35, 25, 75), 0},
};

/* ------------------------------------------------ Beast Presets & Engine --- */
static const preset_t BEAST_PRESETS[] = {
    {"SNARL",      {110, 80,  50,  0,  0,   15, 10, 0}, {2,  60, 110, 40}, 5, 1, FX(40, 0, 0, 30), 0},
    {"PREDATOR",   {90,  50,  30,  0,  0,   17, 12, 0}, {4,  70, 115, 50}, 5, 1, FX(50, 0, 10, 40), 0},
    {"HOWL",       {40,  30,  20,  0,  0,   19, 0,  0}, {12, 90, 120, 80}, 3, 1, FX(30, 0, 25, 60), 0},
    {"GUTTURAL",   {20,  100, 70,  50, 0,   37, 0,  0}, {6,  80, 115, 60}, 5, 1, FX(45, 0, 0, 35), 0},
    {"BITE",       {100, 10,  10,  0,  0,   2,  21, 0}, {1,  40, 70,  20}, 6, 1, FX(35, 0, 0, 25), 0},
    {"FRENZY",     {115, 70,  40,  0,  0,   32, 0,  0}, {3,  50, 100, 30}, 5, 1, FX(60, 0, 0, 40), 0},
    {"CHITIN",     {125, 40,  20,  0,  0,   43, 28, 0}, {1,  30, 60,  20}, 6, 1, FX(30, 0, 10, 30), 0},
    {"PACK",       {80,  70,  60,  0,  0,   0,  0,  2}, {5,  70, 110, 50}, 5, 1, FX(40, 20, 15, 50), 0},
    {"CLICK-POP",  {90,  50,  20,  0,  0,   43, 29, 0}, {1,  25, 70,  15}, 6, 1, FX(20, 0, 0, 20), 0},
    {"EJECT-K",    {50,  40,  15,  0,  0,   39, 18, 0}, {1,  30, 80,  20}, 5, 1, FX(45, 0, 0, 20), 0},
    {"RIMSHOT",    {30,  50,  40,  0,  0,   42, 20, 0}, {1,  25, 75,  15}, 5, 1, FX(35, 0, 0, 20), 0},
    {"SNARE-X",    {70,  60,  20,  0,  0,   40, 16, 0}, {1,  40, 85,  25}, 5, 1, FX(55, 0, 5, 30), 0},
};

typedef struct {
    uint8_t end_tongue, end_jaw, end_lips, end_velum;
    uint8_t speed, hold, ret, mode;
    uint8_t wall_q, tract_len, trachea, nasal_q;
} tract_preset_ext_t;

static const tract_preset_ext_t TRACT_PRESET_EXT[4][12] = {
    /* CREATURE_HUMAN */
    {
        {85, 55, 30, 0,  50, 20, 45, 1,  64, 64, 30, 0},
        {40, 10, 15, 0,  40, 10, 40, 0,  70, 60, 10, 0},
        {90, 20, 15, 0,  45, 15, 45, 0,  64, 64, 20, 0},
        {70, 115, 100, 0,35, 25, 40, 1,  64, 64, 25, 0},
        {70, 115, 100, 0,30, 10, 30, 2,  64, 64, 40, 80},
        {70, 115, 100, 0,25, 15, 25, 2,  64, 64, 20, 0},
        {90, 110, 90, 0, 50, 0,  50, 3,  80, 64, 30, 0},
        {80, 70, 80, 0,  60, 30, 60, 2,  90, 64, 35, 0},
        {30, 80, 60, 0,  45, 20, 45, 1,  70, 64, 25, 0},
        {60, 90, 80, 0,  40, 15, 40, 0,  64, 64, 20, 0},
        {75, 100, 85, 0, 25, 10, 25, 1,  64, 64, 25, 0},
        {45, 80, 60, 0,  15, 5,  15, 2,  50, 64, 50, 0},
    },
    /* CREATURE_TROLL */
    {
        {30, 90, 40, 0,  60, 20, 50, 1,  30, 110, 90, 0},
        {40, 60, 30, 0,  70, 40, 60, 1,  40, 120, 100, 0},
        {20, 40, 20, 20, 40, 10, 40, 2,  35, 115, 80, 40},
        {10, 20, 10, 0,  80, 30, 70, 1,  25, 127, 110, 0},
        {80, 120, 100, 0,45, 20, 45, 2,  45, 105, 70, 0},
        {50, 60, 50, 80, 90, 50, 80, 1,  50, 125, 85, 90},
        {30, 50, 20, 0,  50, 15, 50, 2,  40, 115, 95, 0},
        {85, 90, 70, 0,  55, 25, 55, 2,  55, 100, 75, 0},
        {40, 95, 65, 0,  50, 20, 50, 1,  30, 115, 90, 0},
        {20, 70, 45, 0,  45, 15, 45, 2,  25, 120, 100, 0},
        {65, 90, 70, 0,  20, 5,  20, 1,  35, 110, 95, 0},
        {45, 110, 85, 0, 55, 20, 55, 2,  40, 110, 85, 0},
    },
    /* CREATURE_ELF */
    {
        {100, 15, 60, 0, 40, 20, 40, 2,  110, 20, 0, 0},
        {110, 30, 70, 0, 50, 10, 45, 0,  100, 25, 5, 0},
        {40, 20, 20, 0,  35, 25, 35, 1,  115, 15, 0, 0},
        {90, 30, 40, 10, 45, 15, 45, 2,  90,  30, 0, 70},
        {115, 15, 40, 0, 30, 10, 30, 1,  125, 10, 0, 0},
        {60, 15, 15, 0,  40, 20, 40, 3,  105, 20, 0, 0},
        {105, 20, 30, 0, 70, 30, 60, 1,  120, 22, 5, 0},
        {100, 20, 30, 0, 50, 30, 50, 2,  110, 18, 0, 0},
        {95, 35, 55, 0,  45, 15, 45, 1,  115, 20, 0, 0},
        {120, 15, 50, 0, 30, 10, 30, 0,  110, 22, 0, 0},
        {80, 40, 60, 20, 40, 15, 40, 2,  100, 25, 5, 60},
        {105, 25, 45, 0, 60, 25, 60, 1,  115, 20, 0, 0},
    },
    /* CREATURE_BEAST */
    {
        {90, 40, 20, 0,  30, 15, 30, 2,  60, 50, 30, 0},
        {115, 90, 70, 0, 40, 20, 35, 1,  70, 55, 40, 0},
        {10, 15, 100, 0, 80, 40, 70, 1,  80, 65, 50, 0},
        {40, 60, 30, 20, 35, 10, 35, 2,  50, 75, 60, 60},
        {80, 110, 90, 0, 20, 10, 20, 1,  65, 45, 20, 0},
        {70, 90, 60, 0,  25, 5,  25, 2,  75, 50, 45, 0},
        {100, 20, 10, 0, 20, 10, 20, 3,  95, 35, 10, 0},
        {60, 40, 30, 0,  50, 25, 50, 2,  70, 60, 35, 0},
        {40, 80, 60, 0,  15, 5,  15, 2,  75, 45, 20, 0},
        {80, 90, 70, 0,  15, 5,  15, 1,  60, 55, 40, 0},
        {75, 85, 55, 0,  20, 5,  20, 1,  70, 50, 30, 0},
        {50, 95, 75, 0,  20, 10, 20, 2,  65, 55, 45, 0},
    },
};

static void tract_preset_apply(track_t *t, uint32_t pi)
{
    uint32_t eng = t->eng_req < NENGINES ? t->eng_req : 0u;
    uint32_t ci;
    const tract_preset_ext_t *ext;
    if (eng < 5u || eng > 8u) return;
    ci = eng - 5u;
    if (pi >= 12u) pi = 0u;
    ext = &TRACT_PRESET_EXT[ci][pi];
    t->p[P_END_TONGUE] = ext->end_tongue;
    t->p[P_END_JAW]    = ext->end_jaw;
    t->p[P_END_LIPS]   = ext->end_lips;
    t->p[P_END_VELUM]  = ext->end_velum;
    t->p[P_ENV_SPEED]  = ext->speed;
    t->p[P_ENV_HOLD]   = ext->hold;
    t->p[P_ENV_RET]    = ext->ret;
    t->p[P_ENV_MODE]   = ext->mode;
    t->p[P_WALL_Q]     = ext->wall_q;
    t->p[P_TRACT_LEN]  = ext->tract_len;
    t->p[P_TRACHEA]    = ext->trachea;
    t->p[P_NASAL_Q]    = ext->nasal_q;
}

static const param_desc_t *tract_desc(const track_t *t, uint32_t slot)
{
    (void)t;
    static const param_desc_t D_ONSET = {"ONSET", F_ENUM, 0, NV_NONSETS, 0, NV_ONSET_DISP, 0};
    static const param_desc_t D_CODA  = {"CODA",  F_ENUM, 0, NV_NCODAS,  0, NV_CODA_DISP,  0};
    static const char *const BODY_NAMES[] = {"MALE", "FEMALE", "CHILD", 0};
    static const param_desc_t D_BODY  = {"BODY",  F_ENUM, 0, 2,           0, BODY_NAMES,    0};
    static const char *const CHOIR_NAMES[] = {"1 SOLO", "2 DUET", "3 CHOIR", 0};
    static const param_desc_t D_CHOIR = {"CHOIR", F_ENUM, 0, 2,           0, CHOIR_NAMES,   0};

    if (slot == 5u) return &D_ONSET;
    if (slot == 6u) return &D_CODA;
    if (slot == 4u) return &D_BODY;
    if (slot == 7u) return &D_CHOIR;
    return 0;
}

static const engine_t ENG_HUMAN = {
    .name = "HUMAN",
    .page_title = {"MOUTH", "VOICE"},
    .edit = {
        {"TONGUE", F_PCT, 0, 127, 64, 0, 0},
        {"JAW", F_PCT, 0, 127, 64, 0, 0},
        {"LIPS", F_PCT, 0, 127, 100, 0, 0},
        {"VELUM", F_PCT, 0, 127, 0, 0, 0},
        {"BODY", F_PCT, 0, 127, 0, 0, 0},
        {"ONSET", F_INT, 0, NV_NONSETS, 0, 0, 0},
        {"CODA", F_INT, 0, NV_NCODAS, 0, 0, 0},
        {"CHOIR", F_INT, 0, 2, 0, 0, 0},
    },
    .desc = tract_desc,
    .presets = HUMAN_PRESETS,
    .npresets = NELEM(HUMAN_PRESETS),
    .note_on = tract_note_on,
    .render = tract_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = 1,
    .mem = sizeof(tract_t),
};

static const engine_t ENG_TROLL = {
    .name = "TROLL",
    .page_title = {"MOUTH", "VOICE"},
    .edit = {
        {"TONGUE", F_PCT, 0, 127, 64, 0, 0},
        {"JAW", F_PCT, 0, 127, 64, 0, 0},
        {"LIPS", F_PCT, 0, 127, 100, 0, 0},
        {"VELUM", F_PCT, 0, 127, 0, 0, 0},
        {"BODY", F_PCT, 0, 127, 0, 0, 0},
        {"ONSET", F_INT, 0, NV_NONSETS, 0, 0, 0},
        {"CODA", F_INT, 0, NV_NCODAS, 0, 0, 0},
        {"CHOIR", F_INT, 0, 2, 0, 0, 0},
    },
    .desc = tract_desc,
    .presets = TROLL_PRESETS,
    .npresets = NELEM(TROLL_PRESETS),
    .note_on = tract_note_on,
    .render = tract_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = 1,
    .mem = sizeof(tract_t),
};

static const engine_t ENG_ELF = {
    .name = "ELF",
    .page_title = {"MOUTH", "VOICE"},
    .edit = {
        {"TONGUE", F_PCT, 0, 127, 64, 0, 0},
        {"JAW", F_PCT, 0, 127, 64, 0, 0},
        {"LIPS", F_PCT, 0, 127, 100, 0, 0},
        {"VELUM", F_PCT, 0, 127, 0, 0, 0},
        {"BODY", F_PCT, 0, 127, 0, 0, 0},
        {"ONSET", F_INT, 0, NV_NONSETS, 0, 0, 0},
        {"CODA", F_INT, 0, NV_NCODAS, 0, 0, 0},
        {"CHOIR", F_INT, 0, 2, 0, 0, 0},
    },
    .desc = tract_desc,
    .presets = ELF_PRESETS,
    .npresets = NELEM(ELF_PRESETS),
    .note_on = tract_note_on,
    .render = tract_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = 1,
    .mem = sizeof(tract_t),
};

static const engine_t ENG_BEAST = {
    .name = "BEAST",
    .page_title = {"MOUTH", "VOICE"},
    .edit = {
        {"TONGUE", F_PCT, 0, 127, 64, 0, 0},
        {"JAW", F_PCT, 0, 127, 64, 0, 0},
        {"LIPS", F_PCT, 0, 127, 100, 0, 0},
        {"VELUM", F_PCT, 0, 127, 0, 0, 0},
        {"BODY", F_PCT, 0, 127, 0, 0, 0},
        {"ONSET", F_INT, 0, NV_NONSETS, 0, 0, 0},
        {"CODA", F_INT, 0, NV_NCODAS, 0, 0, 0},
        {"CHOIR", F_INT, 0, 2, 0, 0, 0},
    },
    .desc = tract_desc,
    .presets = BEAST_PRESETS,
    .npresets = NELEM(BEAST_PRESETS),
    .note_on = tract_note_on,
    .render = tract_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = 1,
    .mem = sizeof(tract_t),
};

#define ENG_TRACT ENG_HUMAN
