/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 NOICE VOICE contributors */
/* TRACT engine test (firmware/src/eng_tract.c) on the Mac, through hostsim.c as noise_test.c.
 *   build/host/tract_test [DEMODIR]          (run_tests.sh: build/tract_demo)
 * 1. it speaks: a held note is not silent, has no DC, and never reaches full scale, in both modes, with CHOIR,
 *    at the corners of every parameter (a 3 s sweep each).
 * 2. it is a vowel tract: the first resonance of /a/ (VOWEL 0) sits above that of /i/ (VOWEL 127).
 * 3. stability: the last second of a 4 s worst-case hold is no louder than the second one (no runaway).
 * 4. cost: host instructions per sample (one note, the idle mix taken off) for 1X 22 and 2X 44, CHOIR 1 and 3,
 *    the device estimate (1.7 % per 100: noise_test's ratio); fails above TRACT_COST_MAX.
 * 5. demos into DEMODIR: vowel sweeps in each mode, lips closing to a hum, CHOIR 3. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif

#define TRACT_COST_MAX 2600.0     /* host instructions per sample, the worst corner (~44 % of core 0) */
#define NFFT 8192

static int bad;
static void check(const char *what, int ok)
{
    printf("tract: %-86s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static uint32_t eng_tract(void)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (ENGINES[e] == &ENG_TRACT)
            return e;
    fprintf(stderr, "tract: TRACT is not in ENGINES[] (NOICE_PROBE=0?)\n");
    exit(2);
}

/* VOWEL MODE CHOIR NOISE | TENSE BREATH VELUM LIPS */
static void setup(const int16_t *e)
{
    uint32_t i;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(&trk[0], eng_tract(), 0);
    for (i = 0; i < 8u; i++)
        trk[0].p[P_E0 + i] = e[i];
    trk[0].p[P_END_TONGUE] = e[0];
    trk[0].p[P_END_JAW] = e[1];
    trk[0].p[P_END_LIPS] = e[2];
    trk[0].p[P_END_VELUM] = e[3];
    trk[0].p[P_WALL_Q] = 64;
    trk[0].p[P_TRACT_LEN] = 64;
    trk[0].p[P_ATK] = 0;
    trk[0].p[P_SUS] = 127;
    trk[0].p[P_REL] = 10;
    trk[0].p[P_ED_FLT] = 0;
    trk[0].p[P_VOICE] = V_MONO;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
}

/* the part's output (track_render) of a held note, n samples after 0.1 s; sweep: E[k] goes from e[k] to to over n */
static void render(const int16_t *e, uint32_t note, double *y, uint32_t n, int k, int to)
{
    static int32_t b[CTL];
    uint32_t i, j, skip = FS / 10u / CTL;
    setup(e);
    trk_note_on(&trk[0], note, 100);
    for (i = 0; i < skip; i++) {
        memset(b, 0, sizeof b);
        track_render(&trk[0], b, CTL);
    }
    for (i = 0; i < n; i += CTL) {
        if (k >= 0)
            trk[0].p[P_E0 + k] = (int16_t)(e[k] + (to - e[k]) * (int32_t)i / (int32_t)n);
        memset(b, 0, sizeof b);
        track_render(&trk[0], b, CTL);
        for (j = 0; j < CTL && i + j < n; j++)
            y[i + j] = b[j] / 32768.0;
    }
}

static double stats(const double *y, uint32_t n, double *mean, double *peak)
{
    double s = 0, s2 = 0, p = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        s += y[i];
        s2 += y[i] * y[i];
        p = fabs(y[i]) > p ? fabs(y[i]) : p;
    }
    *mean = s / n;
    *peak = p;
    return sqrt(s2 / n - (*mean) * (*mean));
}

/* ------------------------------------------------------------ spectra --- */
static void fft(double *re, double *im, uint32_t n)
{
    uint32_t i, j = 0, k, m;
    for (i = 1; i < n; i++) {
        for (k = n >> 1; j & k; k >>= 1)
            j ^= k;
        j |= k;
        if (i < j) {
            double t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (m = 2; m <= n; m <<= 1) {
        double a = -2 * M_PI / m;
        for (i = 0; i < n; i += m)
            for (k = 0; k < m / 2; k++) {
                double wr = cos(a * k), wi = sin(a * k);
                double xr = re[i + k + m / 2] * wr - im[i + k + m / 2] * wi;
                double xi = re[i + k + m / 2] * wi + im[i + k + m / 2] * wr;
                re[i + k + m / 2] = re[i + k] - xr;
                im[i + k + m / 2] = im[i + k] - xi;
                re[i + k] += xr;
                im[i + k] += xi;
            }
    }
}

/* the spectral envelope's peak between lo and hi Hz: power averaged over frames, then smoothed over 150 Hz so
 * the harmonics (a low note, 110 Hz) merge into the resonance they sample */
static double env_peak(const double *y, uint32_t n, double lo, double hi)
{
    static double re[NFFT], im[NFFT], ps[NFFT / 2], sm[NFFT / 2];
    uint32_t f, i, nf = n / NFFT, w = (uint32_t)(150.0 * NFFT / FS);
    double best = 0, at = 0;
    memset(ps, 0, sizeof ps);
    for (f = 0; f < nf; f++) {
        for (i = 0; i < NFFT; i++) {
            re[i] = y[f * NFFT + i] * (0.5 - 0.5 * cos(2 * M_PI * i / NFFT));
            im[i] = 0;
        }
        fft(re, im, NFFT);
        for (i = 0; i < NFFT / 2; i++)
            ps[i] += re[i] * re[i] + im[i] * im[i];
    }
    for (i = 0; i < NFFT / 2; i++) {                        /* +6 dB/oct tilt smoothed over 150 Hz */
        uint32_t a = i > w ? i - w : 0, b = i + w < NFFT / 2 ? i + w : NFFT / 2 - 1, j;
        double s = 0;
        for (j = a; j <= b; j++)
            s += ps[j] * (double)j * j;
        sm[i] = s / (b - a + 1);
    }
    for (i = (uint32_t)(lo * NFFT / FS); i < (uint32_t)(hi * NFFT / FS); i++)
        if (sm[i] > best) {
            best = sm[i];
            at = (double)i * FS / NFFT;
        }
    if (getenv("TRACT_DEBUG"))
        printf("tract: debug: env_peak [%.0f..%.0f] at=%.1f best=%e\n", lo, hi, at, best);
    return at;
}

#define NS (FS * 4)
static double Y[NS];

/* ------------------------------------------------------------- checks --- */
static void speaks(void)
{
    static const int16_t BASE[8] = {64, 64, 100, 0, 0, 0, 0, 0};
    double mean, peak, r, worst_peak = 0, worst_dc = 0, quiet = 1e9;
    uint32_t k;
    char what[160];
    for (k = 0; k < 8u; k++) {
        int16_t e[8];
        uint32_t c;
        int16_t pmin = (int16_t)ENG_TRACT.edit[k].min;
        int16_t pmax = (int16_t)ENG_TRACT.edit[k].max;
        if (k == 2u)
            pmin = 20; /* LIPS at 0 clamps the tract shut (no sound escape); test phonation above 20 */
        for (c = 0; c < 2u; c++) { /* each parameter swept up, then down */
            memcpy(e, BASE, sizeof e);
            e[k] = c ? pmax : pmin;
            render(e, 45, Y, FS * 3, (int)k, c ? pmin : pmax);
            r = stats(Y, FS * 3, &mean, &peak);
            if (r < 0.002)
                printf("tract: debug: quiet param %u (%s) c=%u r=%.4f mean=%.4f peak=%.4f\n", k, ENG_TRACT.edit[k].label, c, r, mean, peak);
            if (getenv("TRACT_DEBUG") && fabs(mean) > 0.005)
                printf("tract: debug: dc %.4f param %u (%s) %s\n", mean, k, ENG_TRACT.edit[k].label, c ? "down" : "up");
            worst_peak = peak > worst_peak ? peak : worst_peak;
            worst_dc = fabs(mean) > worst_dc ? fabs(mean) : worst_dc;
            quiet = r < quiet ? r : quiet;
        }
    }
    snprintf(what, sizeof what, "every parameter swept: peak %.3f of full scale (< 0.99)", worst_peak);
    check(what, worst_peak < 0.99);
    snprintf(what, sizeof what, "no DC: |mean| at most %.4f (< 0.01)", worst_dc);
    check(what, worst_dc < 0.01);
    snprintf(what, sizeof what, "never silent while held: RMS at least %.4f (> 0.002)", quiet);
    check(what, quiet > 0.002);
}

static void vowels(void)
{
    static const struct { const char *name; int16_t body; } BODIES[2] = {
        {"Male", 0}, {"Female", 64}
    };
    uint32_t b;
    char what[160];
    for (b = 0; b < 2u; b++) {
        int16_t a[8] = {100, 110, 100, 0, BODIES[b].body, 0, 0, 0};
        int16_t i[8] = {0,   0,   100, 0, BODIES[b].body, 0, 0, 0};
        double fa, fi;
        render(a, 45, Y, NFFT * 8, -1, 0);
        fa = env_peak(Y, NFFT * 8, 200, 1100);
        render(i, 45, Y, NFFT * 8, -1, 0);
        fi = env_peak(Y, NFFT * 8, 200, 1100);
        snprintf(what, sizeof what, "%s: F1 of /a/ %.0f Hz above F1 of /i/ %.0f Hz", BODIES[b].name, fa, fi);
        check(what, fa > fi + 150);
    }
}

static void stability(void)
{
    static const int16_t WORST[8] = {64, 64, 100, 64, 0, 0, 0, 2}; /* CHOIR 3, velum open */
    double mean, peak, r1, r3;
    char what[160];
    render(WORST, 57, Y, NS, -1, 0);
    r1 = stats(Y + FS, FS, &mean, &peak);
    r3 = stats(Y + 3 * FS, FS, &mean, &peak);
    snprintf(what, sizeof what, "no runaway: RMS of the 4th second %.4f vs the 2nd %.4f", r3, r1);
    check(what, r3 <= r1 * 1.5 + 1e-4);
}

/* ---------------------------------------------------------------- cost --- */
static uint64_t instr_now(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri))
        return ri.ri_instructions;
#endif
    return 0;
}

static double mix_cost(const int16_t *e, uint32_t notes)
{
    static int32_t o[2 * CTL];
    uint32_t i, nb = FS * 2u / CTL;
    uint64_t i0;
    setup(e);
    for (i = 0; i < notes; i++)
        trk_note_on(&trk[0], 48 + 3 * i, 100);
    for (i = 0; i < 64u; i++)
        mix_block(o, CTL);
    i0 = instr_now();
    for (i = 0; i < nb; i++)
        mix_block(o, CTL);
    if (getenv("TRACT_DEBUG"))
        printf("tract: debug: busy %u engine %u choir %d\n", busy_now(), trk[0].engine, trk[0].p[P_E7]);
    return (double)(instr_now() - i0) / (nb * CTL);
}

static void cost(void)
{
    static const char *const NAME[2] = {"1X 22, CHOIR 1", "1X 22, CHOIR 3"};
    double idle, c, worst = 0;
    uint32_t v;
    char what[160];
    if (!instr_now()) {
        printf("tract: cost: no instruction counter on this host (proc_pid_rusage); not measured\n");
        return;
    }
    {
        int16_t e[8] = {64, 64, 100, 0, 0, 0, 0, 0};
        idle = mix_cost(e, 0);
    }
    printf("tract: cost, host instructions per sample (one note; device estimate 1.7 %% per 100):\n");
    for (v = 0; v < 2u; v++) {
        int16_t e[8] = {64, 64, 100, 0, 0, 0, 0, (int16_t)(v * 2u)};
        c = mix_cost(e, 1) - idle;
        printf("tract:   %-16s %6.0f  ~%.1f %%\n", NAME[v], c, c * 0.017);
        worst = c > worst ? c : worst;
    }
    snprintf(what, sizeof what, "cost: at most %.0f instructions a sample (~%.1f %% on the device; limit %.0f)", worst,
             worst * 0.017, TRACT_COST_MAX);
    check(what, worst <= TRACT_COST_MAX);
}

/* --------------------------------------------------------------- demos --- */
static void demo(const char *dir, const char *name, const int16_t *e, uint32_t note, int k, int to)
{
    char path[512];
    uint32_t i, n = FS * 3;
    FILE *f;
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    if (!(f = fopen(path, "wb")))
        return;
    render(e, note, Y, n, k, to);
    wav_hdr(f, n);
    for (i = 0; i < n; i++) {
        int32_t s = (int32_t)(Y[i] * 32767.0);
        wav_put(f, s, s);
    }
    fclose(f);
}

static void demos(const char *dir)
{
    static const int16_t SW_AH[8] = {100, 110, 100, 0, 0, 0, 0, 0};
    static const int16_t EE[8]    = {10,  10,  100, 0, 0, 0, 0, 0};
    static const int16_t HUM[8]   = {64,  64,  100, 64, 0, 0, 0, 0};
    static const int16_t CH[8]    = {80,  80,  100, 0, 0, 0, 0, 2};
    static const int16_t LA[8]    = {100, 100, 100, 0, 0, 7, 0, 0};
    demo(dir, "vowel_ah", SW_AH, 45, -1, 0);
    demo(dir, "vowel_ee", EE, 45, -1, 0);
    demo(dir, "vowel_sweep_ah_ee", SW_AH, 45, 0, 10);
    demo(dir, "lips_close_hum", HUM, 45, 2, 0);
    demo(dir, "choir3", CH, 48, -1, 0);
    demo(dir, "syllable_la_la", LA, 45, -1, 0);
    printf("tract: demos in %s\n", dir);
}

int main(int argc, char **argv)
{
    speaks();
    vowels();
    stability();
    cost();
    if (argc > 1)
        demos(argv[1]);
    printf(bad ? "tract: %d FAILED\n" : "tract: all ok\n", bad);
    return bad != 0;
}
