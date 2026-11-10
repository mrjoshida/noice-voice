/* SPDX-License-Identifier: GPL-3.0-only */
/* NOICE VOICE: Vector Drawing Primitives */
#ifndef NV_VEC_H
#define NV_VEC_H

#include <stdint.h>
#include "nv_tables.h"

/* Fast RGB565 byte swap for ST7789 */
#define NV_SWAP16(c) ((uint16_t)((((uint16_t)(c) >> 8) & 0xFFu) | (((uint16_t)(c) & 0xFFu) << 8)))

/* Canonical colors (RGB565 native) */
#define NV_COL_BLACK   0x0000
#define NV_COL_WHITE   0xFFFF
#define NV_COL_CYAN    0x1CDF /* Skull outline */
#define NV_COL_GREEN   0x5F10 /* Tongue (#5be381) */
#define NV_COL_CORAL   0xFAE8 /* Jaw (#ff5d40) */
#define NV_COL_PINK    0xFB99 /* Lips (#ff73cf) */
#define NV_COL_AMBER   0xFD00 /* Waveguide rungs (#ffb000) */
#define NV_COL_DIM_AMB 0x8280 /* Faint rung */
#define NV_COL_DARK    0x18C3 /* Card borders */
#define NV_COL_GRID    0x2945 /* Vowel grid lines */

/* Clip and draw Bresenham line into RGB565 buffer */
void nv_vec_line(uint16_t *buf, int32_t w, int32_t h, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t color_swapped);

/* Draw polyline */
void nv_vec_poly(uint16_t *buf, int32_t w, int32_t h, const nv_pt_t *pts, uint32_t n, int32_t y_offset, uint16_t color_swapped);

/* Render Hershey single-stroke text */
int32_t nv_vec_text(uint16_t *buf, int32_t w, int32_t h, int32_t x, int32_t y, int32_t y_offset, int32_t scale_fp8, const char *str, uint16_t color_swapped);

/* Measure text width in pixels */
int32_t nv_vec_text_width(const char *str, int32_t scale_fp8);

#endif /* NV_VEC_H */
