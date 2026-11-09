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

#define TR_NMAX 44u              /* oral sections at 2x (17.5 cm / (c / 88.2 kHz)) */
#define TR_NNMAX 28u             /* nasal sections at 2x */
#define TR_Q 14                  /* reflection coefficients: Q14 */
#define TR_GREF 12288            /* glottal reflection 0.75 (Q14) */
#define TR_LREF (-13926)         /* lip reflection -0.85 */
#define TR_NREF (-14746)         /* nostril reflection -0.9 */
#define TR_NGLOT 3u              /* CHOIR: glottal sources through one tract */

typedef struct {
    int32_t R[TR_NMAX], L[TR_NMAX], k[TR_NMAX];
    int32_t nR[TR_NNMAX], nL[TR_NNMAX], nk[TR_NNMAX];
    int32_t A[TR_NMAX];          /* areas, cm^2 Q12 */
    int32_t jl, jr, jn;          /* three-way junction (Q14): left, right, nose */
    int32_t gref, lref, nref;    /* end reflections with the guide's losses lumped in (Q14) */
    int32_t jacc, nlast;         /* MODE 2X N1: the nose's junction input held over a step, its last output */
    uint32_t gph[TR_NGLOT];      /* glottal phases */
    int32_t tilt[TR_NGLOT];      /* source low-pass */
    int32_t nz, nf1, nf2;        /* noise: xorshift, its band-pass state */
    int32_t dc_x, dc_y;          /* output DC blocker */
    uint8_t n, nn, ns, os;       /* oral sections, nasal sections, nasal junction index, oversampling */
    uint8_t nsite;               /* noise injection section */
    int32_t ngain;               /* noise gain at the site (Q15) */
} tract_t;
ENG_MEM_FITS("TRACT", sizeof(tract_t));

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

/* the control step: shape -> areas -> reflections (called every render, i.e. every 32 samples) */
static void tr_control(tract_t *tr, const int16_t *p)
{
    uint32_t i, n = tr->n, nn = tr->nn;
    int32_t vow = p[P_E0] * 258, lips = 512 + p[P_E7] * 120, vel = p[P_E4] * 64, amin = 0x7FFFFFFF;
    for (i = 0; i < n; i++) {                              /* resample the 22-point shapes onto n sections */
        uint32_t x = (i * 21u << 8) / (n - 1u), j = x >> 8, f = x & 255u, j1 = j < 21u ? j + 1u : 21u;
        int32_t a = TR_SHAPE_A[j] + (((TR_SHAPE_A[j1] - TR_SHAPE_A[j]) * (int32_t)f) >> 8);
        int32_t b = TR_SHAPE_I[j] + (((TR_SHAPE_I[j1] - TR_SHAPE_I[j]) * (int32_t)f) >> 8);
        a += ((b - a) * vow) >> 15;
        if (i + 3u >= n)                                   /* the last three sections: the lips */
            a = (a * lips) >> 14;
        if (a < 41)
            a = 41;                                        /* 0.01 cm^2: closed, but not a hard wall */
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
    {   /* turbulence: louder the narrower the constriction (a Reynolds-number stand-in) */
        int32_t g = amin < 1229 ? (1229 - amin) * 26 : 0;  /* below 0.3 cm^2 */
        tr->ngain = (g * p[P_E3]) >> 7;
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
}

static void tract_note_on(track_t *t, voice_t *v)
{
    tract_t *tr = tr_of(t);
    uint32_t i;
    (void)v;
    if (!v->env && !v->env_out) {                          /* from rest: the folds start closed */
        for (i = 0; i < TR_NGLOT; i++) {
            tr->gph[i] = 0;
            tr->tilt[i] = 0;
        }
    }
}

/* CHOIR's intervals (Q16): the root, a major third, a fifth (the probe; the instrument takes the held notes) */
static const uint32_t TR_CHOIR_Q16[TR_NGLOT] = {65536u, 82570u, 98193u};

static void tract_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    tract_t *tr = tr_of(t);
    const int16_t *p = t->p;
    uint32_t mode = (uint32_t)clamp(p[P_E1], 0, 2), os = mode ? 2u : 1u, ng = 1u + (uint32_t)clamp(p[P_E2], 0, 2), i, j;
    uint32_t inc[TR_NGLOT], te, tp, rp, rn;
    int32_t oq, tiltk, gv, gn, nst;
    (void)v;
    if (tr->os != mode + 1u || !tr->n)
        tract_setup(tr, mode);
    tr_control(tr, p);
    for (j = 0; j < ng; j++)
        inc[j] = (uint32_t)(((uint64_t)m->inc * TR_CHOIR_Q16[j]) >> 16);
    oq = 58982 - ((p[P_E5] * 284));                        /* TENSION: open quotient 0.90 .. 0.35 (Q16) */
    tiltk = 2500 + p[P_E5] * 109;
    te = (uint32_t)oq << 16;
    tp = (te >> 8) * 166u;
    rp = (1u << 30) / ((tp >> 16) | 1u);
    rn = (1u << 30) / (((te - tp) >> 16) | 1u);
    gv = 2048 - p[P_E6] * 12;                              /* voicing level (the input headroom) */
    gn = p[P_E6] * 8;                                      /* BREATH: aspiration */
    nst = tr->nz;
    for (i = 0; i < n; i++) {
        int32_t g = 0, a, s, nzs = 0, y;
        for (j = 0; j < ng; j++) {                         /* the sources: derivative of a Rosenberg pulse */
            uint32_t ph = tr->gph[j];
            int32_t e;
            if (ph < tp)
                e = (sine_i(((ph >> 16) * rp) << 1) * 17644) >> 15;
            else if (ph < te)
                e = -sine_i(((ph - tp) >> 16) * rn);
            else
                e = 0;
            e += blep(ph - te, inc[j]) >> 1;
            tr->tilt[j] += ((e - tr->tilt[j]) * tiltk) >> 14;
            g += tr->tilt[j];
            tr->gph[j] = ph + inc[j];
        }
        g = (g * gv) >> 15;
        {
            int32_t w = (int32_t)(noise32(&nst) >> 17) - 16384;
            g += (w * gn) >> 15;                           /* aspiration at the glottis */
            if (tr->ngain) {                               /* turbulence: a band-pass on the same noise */
                tr->nf1 += (w - tr->nf1) >> 2;
                tr->nf2 += (tr->nf1 - tr->nf2) >> 3;
                nzs = ((tr->nf1 - tr->nf2) * tr->ngain) >> 15;
            }
        }
        {   /* tr_step lives in RAM, beyond a direct jump from XIP: one pointer per block (bsp FL_FAR's way) */
            int32_t (*volatile vstep)(tract_t *, int32_t, int32_t, uint32_t) = tr_step;
            int32_t (*step)(tract_t *, int32_t, int32_t, uint32_t) = vstep;
            if (os == 2u)
                a = (step(tr, g, nzs, mode == 1u ? 2u : 0u) + step(tr, g, nzs, mode == 1u ? 2u : 1u)) >> 1;
            else
                a = step(tr, g, nzs, 2u);
        }
        y = a - tr->dc_x + ((tr->dc_y * 32604 + 16384) >> 15);   /* DC blocker, ~10 Hz (rounded: a floor here */
        tr->dc_x = a;                                            /* leaves a standing offset of 100 LSB) */
        tr->dc_y = y;
        s = soft_knee(clamp(y << 2, -200000, 200000), 24000);
        out[i] += voice_amp(s, m, i) << 1;
    }
    tr->nz = nst;
}

static const char *const TR_MODE[] = {"1X 22", "2X 44", "2X N1"};

static const preset_t TRACT_PRESETS[] = {
    /* VOWEL MODE CHOIR NOISE | TENSE BREATH VELUM LIPS; MIDI Program Change 0..5 (scripts/fm1_probe.py) */
    {"PROBE AH", {0, 1, 0, 0, 64, 10, 0, 100}, {4, 70, 110, 60}, 0, 1, FX(0, 0, 0, 40), 0},
    {"PROBE WORST", {64, 1, 2, 127, 64, 60, 90, 20}, {4, 70, 110, 60}, 0, 1, FX(0, 40, 40, 90), 0},
    {"WORST 1X", {64, 0, 2, 127, 64, 60, 90, 20}, {4, 70, 110, 60}, 0, 1, FX(0, 40, 40, 90), 0},
    {"SOLO 2X", {64, 1, 0, 127, 64, 60, 90, 20}, {4, 70, 110, 60}, 0, 1, FX(0, 0, 0, 40), 0},
    {"SOLO 1X", {64, 0, 0, 127, 64, 60, 90, 20}, {4, 70, 110, 60}, 0, 1, FX(0, 0, 0, 40), 0},
    {"AH 1X", {0, 0, 0, 0, 64, 10, 0, 100}, {4, 70, 110, 60}, 0, 1, FX(0, 0, 0, 40), 0},
    {"SOLO 2XN1", {64, 2, 0, 127, 64, 60, 90, 20}, {4, 70, 110, 60}, 0, 1, FX(0, 0, 0, 40), 0},
    {"WORST 2XN1", {64, 2, 2, 127, 64, 60, 90, 20}, {4, 70, 110, 60}, 0, 1, FX(0, 40, 40, 90), 0},
};

static const engine_t ENG_TRACT = {
    .name = "TRACT",
    .page_title = {"MOUTH", "VOICE"},
    .edit = {
        {"VOWEL", F_INT, 0, 127, 0, 0, 0},
        {"MODE", F_ENUM, 0, 2, 1, TR_MODE, 0},
        {"CHOIR", F_INT, 0, 2, 0, 0, 0},
        {"NOISE", F_PCT, 0, 127, 0, 0, 0},
        {"TENSE", F_PCT, 0, 127, 64, 0, 0},
        {"BRTH", F_PCT, 0, 127, 10, 0, 0},
        {"VELUM", F_PCT, 0, 127, 0, 0, 0},
        {"LIPS", F_PCT, 0, 127, 100, 0, 0},
    },
    .presets = TRACT_PRESETS,
    .npresets = NELEM(TRACT_PRESETS),
    .note_on = tract_note_on,
    .render = tract_render,
    .knob = {P_E0, P_E7, P_E6, P_E2},
    .poly = 1,
    .mem = sizeof(tract_t),
};
