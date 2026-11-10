/* SPDX-License-Identifier: GPL-3.0-only */
/* NOICE VOICE: Vector View Header */
#ifndef NV_VIEW_H
#define NV_VIEW_H

#include <stdint.h>
#include "nv_vec.h"
#include "nv_artic.h"

/* Forward declarations */
struct track;

enum {
    NV_VIEW_START     = 0,  /* MOUTH / HOME: Start mouth shape */
    NV_VIEW_TARGET    = 1,  /* END MOUTH / END SHAPE: Target mouth shape */
    NV_VIEW_ENV       = 2,  /* ENV TIME: SPEED, HOLD, RETURN, MODE */
    NV_VIEW_RESONANCE = 3,  /* RESONANCE: WALL Q, LENGTH, TRACHEA, NASAL Q */
    NV_VIEW_PHONEMES  = 4,  /* PHONEMES: ONSET, CODA, BODY, CHOIR */
};

/* Render one horizontal strip (e.g. 240 x strip_h at screen offset strip_y)
 * buf: pointer to 240 * strip_h uint16_t RGB565 pixels (byte-swapped for ST7789) */
void nv_view_render_strip(uint16_t *buf, int32_t strip_y, int32_t strip_h,
                          const nv_artic_t *a, const int32_t *areas,
                          const int32_t *pres, uint32_t n_sections,
                          const int16_t *params, const char *msg,
                          uint8_t view_mode);

#endif /* NV_VIEW_H */
