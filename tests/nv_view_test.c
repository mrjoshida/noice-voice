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

int main(int argc, char **argv) {
    printf("nv_view_test: rendering vector display strips...\n");
    static uint16_t screen[240 * 240];
    nv_artic_t artic;
    nv_artic_init(&artic);
    artic.tongue = 64;
    artic.jaw = 64;
    artic.lips = 100;
    artic.velum = 0;
    artic.state = SYLL_NUCLEUS;

    int32_t pres[44] = {0};
    /* Synthetic pressure waves */
    for (int i = 0; i < 22; i++) {
        pres[i] = (i % 4 == 0) ? 3500 : 800;
    }
    int16_t params[8] = {64, 64, 100, 0, 0, 0, 0, 0};

    /* Strip 0: 0..89 */
    nv_view_render_strip(screen, 0, 90, &artic, NULL, pres, 22, params, NULL, 0);
    /* Strip 1: 90..179 */
    nv_view_render_strip(screen + 90 * 240, 90, 90, &artic, NULL, pres, 22, params, NULL, 0);
    /* Strip 2: 180..239 */
    nv_view_render_strip(screen + 180 * 240, 180, 60, &artic, NULL, pres, 22, params, NULL, 0);

    system("mkdir -p build/test_out");
    save_ppm("build/test_out/nv_view_neutral.ppm", screen, 240, 240);
    printf("nv_view_test: saved build/test_out/nv_view_neutral.ppm\n");

    /* Test vowel /i/ corner with message */
    artic.tongue = 0;
    artic.jaw = 0;
    params[0] = 0;
    params[1] = 0;
    nv_view_render_strip(screen, 0, 90, &artic, NULL, pres, 22, params, "/i/", 1);
    nv_view_render_strip(screen + 90 * 240, 90, 90, &artic, NULL, pres, 22, params, "/i/", 1);
    nv_view_render_strip(screen + 180 * 240, 180, 60, &artic, NULL, pres, 22, params, "/i/", 1);
    save_ppm("build/test_out/nv_view_vowel_i.ppm", screen, 240, 240);
    printf("nv_view_test: saved build/test_out/nv_view_vowel_i.ppm\n");

    /* Test RESONANCE visualizer (mode 3) */
    int16_t res_params[16] = {0};
    res_params[P_WALL_Q] = 90;
    res_params[P_TRACT_LEN] = 80;
    res_params[P_TRACHEA] = 100;
    res_params[P_NASAL_Q] = 75;
    nv_view_render_strip(screen, 0, 90, &artic, NULL, pres, 22, res_params, NULL, 3);
    nv_view_render_strip(screen + 90 * 240, 90, 90, &artic, NULL, pres, 22, res_params, NULL, 3);
    nv_view_render_strip(screen + 180 * 240, 180, 60, &artic, NULL, pres, 22, res_params, NULL, 3);
    save_ppm("build/test_out/nv_view_resonance.ppm", screen, 240, 240);
    printf("nv_view_test: saved build/test_out/nv_view_resonance.ppm\n");

    printf("nv_view_test: all ok!\n");
    return 0;
}
