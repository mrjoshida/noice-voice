/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define P_E0 0
#define P_E1 1
#define P_E2 2
#define P_E3 3
#define P_END_TONGUE 4
#define P_END_JAW 5
#define P_END_LIPS 6
#define P_END_VELUM 7
#define P_WALL_Q 8
#define P_TRACT_LEN 9
#define P_TRACHEA 10
#define P_NASAL_Q 11
#define P_ENV_SPEED 12
#define P_ENV_HOLD 13
#define P_ENV_RET 14
#define P_ENV_MODE 15
#define P_E5 16
#define P_E6 17
#define P_E4 18
#define P_E7 19

static inline int32_t clamp(int32_t v, int32_t mn, int32_t mx) {
    return v < mn ? mn : v > mx ? mx : v;
}

/* String and int helpers before nv_view.c */
static unsigned str_len(const char *s) {
    unsigned n = 0;
    while (s[n]) n++;
    return n;
}

static void str_cpy(char *d, const char *s, unsigned max) {
    unsigned i = 0;
    while (s[i] && i + 1 < max) {
        d[i] = s[i];
        i++;
    }
    d[i] = 0;
}

static void fmt_fix(char *b, int32_t v, int decimals) {
    char t[12];
    int n = 0, neg = v < 0, i;
    uint32_t u = (uint32_t)(neg ? -v : v);
    do {
        t[n++] = (char)('0' + u % 10u);
        u /= 10u;
        if (n == decimals)
            t[n++] = '.';
    } while (u || n <= decimals);
    if (neg)
        t[n++] = '-';
    for (i = 0; i < n; i++)
        b[i] = t[n - 1 - i];
    b[n] = 0;
}

static void fmt_int(char *b, int32_t v) { fmt_fix(b, v, 0); }

#include "nv_tables.h"
#include "nv_artic.h"
#include "nv_artic.c"
#include "nv_vec.h"
#include "nv_vec.c"
#include "nv_view.h"
#include "nv_view.c"

/* Write 240x240 RGB565 byte-swapped buffer to PPM file */
static void save_ppm(const char *filename, const uint16_t *buf, int w, int h) {
    FILE *f = fopen(filename, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        /* Unswap ST7789 byte-swapped RGB565 */
        uint16_t px = NV_SWAP16(buf[i]);
        uint8_t r = (uint8_t)(((px >> 11) & 0x1F) * 255 / 31);
        uint8_t g = (uint8_t)(((px >> 5) & 0x3F) * 255 / 63);
        uint8_t b = (uint8_t)((px & 0x1F) * 255 / 31);
        fputc(r, f);
        fputc(g, f);
        fputc(b, f);
    }
    fclose(f);
}

/* Return 1 if swapped RGB565 pixel matches canonical unswapped color */
static int px_is(uint16_t swapped_px, uint16_t canon_col) {
    return swapped_px == NV_SWAP16(canon_col);
}

static int count_color_in_box(const uint16_t *screen, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t canon_col) {
    int cnt = 0;
    for (int32_t y = y0; y <= y1; y++) {
        for (int32_t x = x0; x <= x1; x++) {
            if (px_is(screen[y * 240 + x], canon_col))
                cnt++;
        }
    }
    return cnt;
}

static void render_full(uint16_t *screen, const nv_artic_t *a, const int32_t *pres, uint32_t n_sec, const int16_t *params, const char *msg, uint8_t view_mode) {
    nv_view_render_strip(screen, 0, 90, a, NULL, pres, n_sec, params, msg, view_mode);
    nv_view_render_strip(screen + 90 * 240, 90, 90, a, NULL, pres, n_sec, params, msg, view_mode);
    nv_view_render_strip(screen + 180 * 240, 180, 60, a, NULL, pres, n_sec, params, msg, view_mode);
}

int main(void) {
    printf("nv_view_test: rendering vector display strips...\n");
    static uint16_t screen[240 * 240];
    static uint16_t full_top[240 * 180];
    nv_artic_t artic;
    nv_artic_init(&artic);
    artic.tongue = 64;
    artic.jaw = 64;
    artic.lips = 100;
    artic.velum = 0;
    artic.cur_tongue = 64;
    artic.cur_jaw = 64;
    artic.cur_lips = 512 + artic.lips * 120;
    artic.state = SYLL_NUCLEUS;

    int32_t pres[44] = {0};
    /* Synthetic pressure waves */
    for (int i = 0; i < 22; i++) {
        pres[i] = (i % 4 == 0) ? 3500 : 800;
    }
    int16_t params[20] = {64, 64, 100, 0};
    params[P_END_TONGUE] = 64; params[P_END_JAW] = 64; params[P_END_LIPS] = 100; params[P_END_VELUM] = 0;

    render_full(screen, &artic, pres, 22, params, NULL, 0);

    system("mkdir -p build/test_out");
    save_ppm("build/test_out/nv_view_neutral.ppm", screen, 240, 240);
    printf("nv_view_test: saved build/test_out/nv_view_neutral.ppm\n");

    /* Verify strip seam equivalence across y=89->90 (lower lip spans strip 0..89 and 90..179) */
    nv_view_render_strip(full_top, 0, 180, &artic, NULL, pres, 22, params, NULL, 0);
    if (memcmp(screen, full_top, sizeof(full_top)) != 0) {
        fprintf(stderr, "FAIL: strip seam mismatch between two 90px strips and 180px full render!\n");
        return 1;
    }

    /* Verify lip bounds & aperture across all 27 (tongue, jaw, lips) corners + preset (85,55) */
    {
        const uint8_t vals[4] = {0, 64, 127, 85};
        int16_t p_corner[20] = {0};
        for (int ti = 0; ti < 4; ti++) {
            for (int ji = 0; ji < 4; ji++) {
                uint8_t tv = vals[ti];
                uint8_t jv = (ti == 3 || ji == 3) ? 55 : vals[ji];
                if ((ti == 3) != (ji == 3)) continue; /* check corners {0,64,127}^2 plus (85,55) */
                const uint8_t lip_vals[3] = {0, 64, 127};
                for (int li = 0; li < 3; li++) {
                    nv_artic_t ca;
                    nv_artic_init(&ca);
                    ca.cur_tongue = tv;
                    ca.cur_jaw = jv;
                    ca.lips = lip_vals[li];
                    ca.cur_lips = 512 + (int32_t)ca.lips * 120;
                    ca.cur_velum = 0;
                    ca.state = SYLL_NUCLEUS;
                    p_corner[P_E0] = tv; p_corner[P_E1] = jv; p_corner[P_E2] = lip_vals[li];
                    p_corner[P_END_TONGUE] = tv; p_corner[P_END_JAW] = jv; p_corner[P_END_LIPS] = lip_vals[li];
                    render_full(screen, &ca, NULL, 0, p_corner, NULL, NV_VIEW_TARGET);

                    /* Lips must stay strictly onscreen & clear of vowel quadrant box (bx=186) */
                    int overflow_pink = count_color_in_box(screen, 175, 18, 239, 178, NV_COL_PINK);
                    if (overflow_pink != 0) {
                        fprintf(stderr, "FAIL: lips drew into x>=175 at T=%u J=%u L=%u (%d px)\n",
                                tv, jv, lip_vals[li], overflow_pink);
                        return 1;
                    }
                    /* Verify seam equivalence at jaw=127 corner too */
                    nv_view_render_strip(full_top, 0, 180, &ca, NULL, NULL, 0, p_corner, NULL, NV_VIEW_TARGET);
                    if (memcmp(screen, full_top, sizeof(full_top)) != 0) {
                        fprintf(stderr, "FAIL: strip seam mismatch at T=%u J=%u L=%u!\n", tv, jv, lip_vals[li]);
                        return 1;
                    }
                }
            }
        }
    }

    /* Check bilabial closure at L=0 vs wide opening at L=127 */
    {
        nv_artic_t ca;
        int16_t p_lip[20] = {85, 55, 0, 0};
        p_lip[P_END_TONGUE] = 85; p_lip[P_END_JAW] = 55;

        nv_artic_init(&ca);
        ca.cur_tongue = 85; ca.cur_jaw = 55; ca.state = SYLL_NUCLEUS;
        ca.cur_lips = 512; /* LIPS = 0 */
        p_lip[P_END_LIPS] = 0;
        render_full(screen, &ca, NULL, 0, p_lip, NULL, NV_VIEW_TARGET);
        /* Closed vermilion lips meet around (143..145, 55): must have pink pixels in y=54..57 */
        int closed_pink = count_color_in_box(screen, 141, 54, 147, 57, NV_COL_PINK);
        if (closed_pink < 4) {
            fprintf(stderr, "FAIL: lips did not meet vertically at L=0 (closed_pink=%d)\n", closed_pink);
            return 1;
        }

        ca.cur_lips = 512 + 127 * 120; /* LIPS = 127 */
        p_lip[P_END_LIPS] = 127;
        render_full(screen, &ca, NULL, 0, p_lip, NULL, NV_VIEW_TARGET);
        /* Open lips have >= 14 px vertical gap around y=52..62 between ul(y~50) and ll(y~66) */
        int gap_pink = count_color_in_box(screen, 143, 53, 152, 62, NV_COL_PINK);
        int top_lip = count_color_in_box(screen, 145, 48, 151, 52, NV_COL_PINK);
        int bot_lip = count_color_in_box(screen, 144, 63, 151, 68, NV_COL_PINK);
        if (gap_pink != 0 || top_lip < 4 || bot_lip < 4) {
            fprintf(stderr, "FAIL: lips did not open vertically at L=127 (gap=%d top=%d bot=%d)\n",
                    gap_pink, top_lip, bot_lip);
            return 1;
        }
    }

    /* Check velum rotation: shut at wall (vel=0) vs pivoted down-forward into oropharynx (vel=127) */
    {
        nv_artic_t va;
        int16_t p_vel[20] = {85, 55, 100, 0};
        p_vel[P_END_TONGUE] = 85; p_vel[P_END_JAW] = 55; p_vel[P_END_LIPS] = 100;

        nv_artic_init(&va);
        va.cur_tongue = 85; va.cur_jaw = 55; va.cur_lips = 512 + 100 * 120; va.state = SYLL_NUCLEUS;

        va.cur_velum = 0;
        p_vel[P_END_VELUM] = 0;
        render_full(screen, &va, NULL, 0, p_vel, NULL, NV_VIEW_TARGET);
        if (count_color_in_box(screen, 20, 0, 58, 19, NV_COL_WHITE) != 0) {
            fprintf(stderr, "FAIL: velum=0 drew into banner y<=19!\n");
            return 1;
        }
        if (count_color_in_box(screen, 36, 48, 48, 60, NV_COL_WHITE) != 0) {
            fprintf(stderr, "FAIL: velum=0 encroached into open oropharynx zone!\n");
            return 1;
        }

        va.cur_velum = 3276;
        p_vel[P_END_VELUM] = 127;
        render_full(screen, &va, NULL, 0, p_vel, NULL, NV_VIEW_TARGET);
        save_ppm("build/test_out/nv_view_vel127.ppm", screen, 240, 240);
        if (count_color_in_box(screen, 20, 0, 58, 19, NV_COL_WHITE) != 0) {
            fprintf(stderr, "FAIL: velum=127 translated upward into banner y<=19!\n");
            return 1;
        }
        int rotated_orophx = count_color_in_box(screen, 36, 48, 48, 60, NV_COL_WHITE);
        if (rotated_orophx < 10) {
            fprintf(stderr, "FAIL: velum=127 did not pivot down-forward into oropharynx (px=%d)\n", rotated_orophx);
            return 1;
        }
        /* Ensure phantom shut-velum cyan bridge is gone so airway port is unobstructed */
        int phantom_bridge = count_color_in_box(screen, 30, 32, 48, 41, NV_COL_CYAN);
        if (phantom_bridge != 0) {
            fprintf(stderr, "FAIL: phantom closed-velum cyan line blocking open velopharyngeal port (%d px)\n", phantom_bridge);
            return 1;
        }
    }

    /* Save diagnostic corner frames for visual inspection */
    {
        nv_artic_t ca;
        int16_t p_dbg[20] = {0};
        nv_artic_init(&ca);
        ca.state = SYLL_NUCLEUS;

        /* Jaw wide open (127), lips closed (0) */
        ca.cur_tongue = 64; ca.cur_jaw = 127; ca.cur_lips = 512; ca.cur_velum = 0;
        p_dbg[P_END_TONGUE] = 64; p_dbg[P_END_JAW] = 127; p_dbg[P_END_LIPS] = 0; p_dbg[P_END_VELUM] = 0;
        render_full(screen, &ca, NULL, 0, p_dbg, NULL, NV_VIEW_TARGET);
        save_ppm("build/test_out/nv_view_lip0_openjaw.ppm", screen, 240, 240);

        /* High back /u/ protruded (127, 0), lips wide (127), velum half (64) */
        ca.cur_tongue = 127; ca.cur_jaw = 0; ca.cur_lips = 512 + 127 * 120; ca.cur_velum = 1638;
        p_dbg[P_END_TONGUE] = 127; p_dbg[P_END_JAW] = 0; p_dbg[P_END_LIPS] = 127; p_dbg[P_END_VELUM] = 64;
        render_full(screen, &ca, NULL, 0, p_dbg, NULL, NV_VIEW_TARGET);
        save_ppm("build/test_out/nv_view_lip127_u.ppm", screen, 240, 240);

        /* MONO palette check */
        render_full(screen, &ca, NULL, 0, p_dbg, NULL, NV_VIEW_TARGET | NV_VIEW_BW);
        save_ppm("build/test_out/nv_view_bw.ppm", screen, 240, 240);
    }

    /* Test vowel /i/ corner with message */
    artic.tongue = 0;
    artic.jaw = 0;
    artic.cur_tongue = 0;
    artic.cur_jaw = 0;
    params[0] = 0;
    params[1] = 0;
    render_full(screen, &artic, pres, 22, params, "/i/", 1);
    save_ppm("build/test_out/nv_view_vowel_i.ppm", screen, 240, 240);
    printf("nv_view_test: saved build/test_out/nv_view_vowel_i.ppm\n");

    /* Test ENV trajectory visualizer (mode 2) */
    int16_t env_params[20] = {0};
    env_params[P_E0] = 10; env_params[P_E1] = 15; env_params[P_E2] = 20; env_params[P_E3] = 0;
    env_params[P_END_TONGUE] = 115; env_params[P_END_JAW] = 110; env_params[P_END_LIPS] = 110; env_params[P_END_VELUM] = 40;
    env_params[P_ENV_SPEED] = 48; env_params[P_ENV_HOLD] = 32; env_params[P_ENV_RET] = 64; env_params[P_ENV_MODE] = 2;
    render_full(screen, &artic, pres, 22, env_params, NULL, NV_VIEW_ENV);
    save_ppm("build/test_out/nv_view_env.ppm", screen, 240, 240);
    printf("nv_view_test: saved build/test_out/nv_view_env.ppm\n");

    /* Test RESONANCE visualizer (mode 3) */
    int16_t res_params[20] = {0};
    res_params[P_WALL_Q] = 90;
    res_params[P_TRACT_LEN] = 80;
    res_params[P_TRACHEA] = 100;
    res_params[P_NASAL_Q] = 75;
    render_full(screen, &artic, pres, 22, res_params, NULL, 3);
    save_ppm("build/test_out/nv_view_resonance.ppm", screen, 240, 240);
    printf("nv_view_test: saved build/test_out/nv_view_resonance.ppm\n");

    /* Test PHONEMES visualizer (mode 4) with retroflex R-RET */
    int16_t ph_params[20] = {0};
    ph_params[P_E4] = 40; ph_params[P_E5] = 27; ph_params[P_E6] = 7; ph_params[P_E7] = 1;
    render_full(screen, &artic, pres, 22, ph_params, NULL, NV_VIEW_PHONEMES);
    save_ppm("build/test_out/nv_view_phonemes.ppm", screen, 240, 240);
    printf("nv_view_test: saved build/test_out/nv_view_phonemes.ppm\n");

    printf("nv_view_test: all ok!\n");
    return 0;
}
