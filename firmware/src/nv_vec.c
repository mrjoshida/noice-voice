/* SPDX-License-Identifier: GPL-3.0-only */
/* NOICE VOICE: Vector Drawing Primitives Implementation */
#include "nv_vec.h"

void nv_vec_line(uint16_t *buf, int32_t w, int32_t h, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t color_swapped)
{
    /* Quick reject if wholly outside screen box */
    if ((x0 < 0 && x1 < 0) || (x0 >= w && x1 >= w))
        return;
    if ((y0 < 0 && y1 < 0) || (y0 >= h && y1 >= h))
        return;

    int32_t dx = x1 - x0;
    int32_t dy = y1 - y0;
    int32_t sx = (dx >= 0) ? 1 : -1;
    int32_t sy = (dy >= 0) ? 1 : -1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    int32_t err = dx - dy;

    while (1) {
        if ((uint32_t)x0 < (uint32_t)w && (uint32_t)y0 < (uint32_t)h)
            buf[y0 * w + x0] = color_swapped;
        if (x0 == x1 && y0 == y1)
            break;
        int32_t e2 = err << 1;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void nv_vec_poly(uint16_t *buf, int32_t w, int32_t h, const nv_pt_t *pts, uint32_t n, int32_t y_offset, uint16_t color_swapped)
{
    if (!pts || n < 2)
        return;
    for (uint32_t i = 1; i < n; i++) {
        nv_vec_line(buf, w, h,
                    (int32_t)pts[i - 1].x, (int32_t)pts[i - 1].y - y_offset,
                    (int32_t)pts[i].x,     (int32_t)pts[i].y - y_offset,
                    color_swapped);
    }
}

int32_t nv_vec_text(uint16_t *buf, int32_t w, int32_t h, int32_t x, int32_t y, int32_t y_offset, int32_t scale_fp8, const char *str, uint16_t color_swapped)
{
    if (!str)
        return x;
    int32_t cur_x = x;
    while (*str) {
        uint8_t ch = (uint8_t)*str++;
        if (ch < 32 || ch > 127) {
            cur_x += (12 * scale_fp8) >> 8;
            continue;
        }
        const nv_glyph_info_t *g = &NV_GLYPHS[ch - 32];
        const int8_t (*pts)[2] = &NV_FONT_DATA[g->offset];
        int32_t px = -999, py = -999;
        for (uint32_t i = 0; i < g->len; i++) {
            int8_t gx = pts[i][0];
            int8_t gy = pts[i][1];
            if (gx == -128 && gy == -128) {
                px = -999;
                py = -999;
                continue;
            }
            int32_t sx = cur_x + ((gx * scale_fp8) >> 8);
            int32_t sy = y + ((gy * scale_fp8) >> 8) - y_offset;
            if (px != -999) {
                nv_vec_line(buf, w, h, px, py, sx, sy, color_swapped);
            }
            px = sx;
            py = sy;
        }
        cur_x += ((int32_t)g->width * scale_fp8) >> 8;
    }
    return cur_x;
}

int32_t nv_vec_text_width(const char *str, int32_t scale_fp8)
{
    if (!str)
        return 0;
    int32_t w = 0;
    while (*str) {
        uint8_t ch = (uint8_t)*str++;
        if (ch >= 32 && ch <= 127) {
            w += ((int32_t)NV_GLYPHS[ch - 32].width * scale_fp8) >> 8;
        } else {
            w += (12 * scale_fp8) >> 8;
        }
    }
    return w;
}
