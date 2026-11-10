/* SPDX-License-Identifier: GPL-3.0-only */
/* NOICE VOICE: Articulator & Syllable Engine Implementation */
#include "nv_artic.h"

#define CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : (x) > (hi) ? (hi) : (x))

void nv_artic_init(nv_artic_t *a)
{
    uint32_t i;
    a->tongue = 64;
    a->jaw = 64;
    a->lips = 100;
    a->velum = 0;
    a->body = 0;
    a->breath_base = 64;
    a->tension = 64;
    a->noise_amt = 0;
    a->creature = CREATURE_HUMAN;

    a->end_tongue = 64;
    a->end_jaw = 64;
    a->end_lips = 100;
    a->end_velum = 0;

    a->speed_ms = 40;
    a->hold_ms = 0;
    a->return_ms = 40;
    a->morph_mode = 0;
    a->morph_phase = 0;
    a->morph_dir = 1;
    a->hold_timer_samples = 0;

    a->cur_tongue = 64;
    a->cur_jaw = 64;

    a->onset_idx = 0;
    a->coda_idx = 0;
    a->attack_ms = 25;
    a->release_ms = 70;
    a->onset_hold_ms = 60;

    a->key_pressure = 0;
    a->whisper_on = 0;
    a->hold_on = 0;
    a->vowel_jump = -1;

    a->state = SYLL_IDLE;
    a->state_samples = 0;
    a->onset_samples = 0;
    a->coda_samples = 0;

    a->cur_breath = 0;
    a->cur_velum = 0;
    a->cur_lips = 4096;
    a->pitch_scoop = 0;

    /* Initialize to neutral schwa areas */
    for (i = 0; i < NV_SECTIONS; i++)
        a->cur_areas[i] = NV_VOWEL_GRID[0][1][1][i];
}

void nv_artic_note_on(nv_artic_t *a, uint8_t legato)
{
    if (!legato) {
        a->morph_phase = 0;
        a->morph_dir = 1;
        a->hold_timer_samples = 0;
    }

    if (legato && a->state != SYLL_IDLE) {
        /* Legato: keep nucleus, no new consonant onset */
        a->state = SYLL_NUCLEUS;
        return;
    }

    if (a->onset_idx > 0 && a->onset_idx <= NV_NONSETS) {
        uint32_t dur = NV_ONSETS[a->onset_idx - 1u].dur_ms;
        if (dur < 20u) dur = 20u;
        a->state = SYLL_ONSET;
        a->onset_samples = (dur * 44100u) / 1000u;
        a->state_samples = 0;
    } else {
        a->state = SYLL_NUCLEUS;
        a->state_samples = 0;
    }
}

void nv_artic_note_off(nv_artic_t *a)
{
    if (a->hold_on)
        return; /* Note holds indefinitely */

    if (a->coda_idx > 0 && a->coda_idx <= NV_NCODAS) {
        a->state = SYLL_CODA;
        a->coda_samples = (60u * 44100u) / 1000u; /* ~60 ms coda closure */
        a->state_samples = 0;
    } else if (a->morph_mode == 1 && a->morph_dir > 0 && a->morph_phase > 0) {
        /* In RETURN mode: key release triggers return to Start shape */
        a->morph_dir = -1;
        a->hold_timer_samples = 0;
    } else {
        a->state = SYLL_IDLE;
    }
}

void nv_artic_respeak(nv_artic_t *a)
{
    if (a->onset_idx > 0 && a->onset_idx <= NV_NONSETS) {
        a->state = SYLL_ONSET;
        a->onset_samples = ((uint32_t)a->speed_ms * 44100u) / 1000u;
        a->state_samples = 0;
    }
}

/* Bilinear interpolation over 3x3 vowel grid for a given body model */
static void bilerp_vowel_grid(uint32_t body_idx, int32_t tx, int32_t ty, int32_t out[NV_SECTIONS])
{
    /* tx, ty in 0..512 (Q8 where 256 is 1.0) */
    uint32_t ix = (uint32_t)(tx >> 8), fx = (uint32_t)(tx & 255);
    uint32_t iy = (uint32_t)(ty >> 8), fy = (uint32_t)(ty & 255);
    uint32_t i;

    if (ix > 1u) { ix = 1u; fx = 256u; }
    if (iy > 1u) { iy = 1u; fy = 256u; }

    for (i = 0; i < NV_SECTIONS; i++) {
        int32_t a00 = (int32_t)NV_VOWEL_GRID[body_idx][iy][ix][i];
        int32_t a01 = (int32_t)NV_VOWEL_GRID[body_idx][iy][ix + 1u][i];
        int32_t a10 = (int32_t)NV_VOWEL_GRID[body_idx][iy + 1u][ix][i];
        int32_t a11 = (int32_t)NV_VOWEL_GRID[body_idx][iy + 1u][ix + 1u][i];

        int32_t top = a00 + (((a01 - a00) * (int32_t)fx) >> 8);
        int32_t bot = a10 + (((a11 - a10) * (int32_t)fx) >> 8);
        out[i] = top + (((bot - top) * (int32_t)fy) >> 8);
    }
}

void nv_artic_update(nv_artic_t *a, uint32_t n_samples)
{
    int32_t target_A[NV_SECTIONS];
    int32_t a_b0[NV_SECTIONS], a_b1[NV_SECTIONS];
    uint32_t b0, b1, b_frac, i;
    int32_t target_velum, lips_factor;

    /* 1. Syllable state machine transitions */
    if (a->state == SYLL_ONSET) {
        a->state_samples += n_samples;
        if (a->state_samples >= a->onset_samples) {
            a->state = SYLL_NUCLEUS;
            a->state_samples = 0;
        }
    } else if (a->state == SYLL_CODA) {
        a->state_samples += n_samples;
        if (a->state_samples >= a->coda_samples) {
            a->state = SYLL_IDLE;
            a->state_samples = 0;
        }
    }

    /* 2. Breath Envelope tracking */
    {
        /* Base lung pressure modulated by key aftertouch */
        int32_t bp = (int32_t)a->breath_base + ((((127 - (int32_t)a->breath_base) * (int32_t)a->key_pressure)) / 127);
        int32_t target_p = 0;
        uint32_t time_ms;
        int32_t k;

        if (a->state != SYLL_IDLE || a->hold_on) {
            target_p = bp << 8; /* 0..32512 (Q15) */
            time_ms = a->attack_ms < 2u ? 2u : a->attack_ms;
        } else {
            target_p = 0;
            time_ms = a->release_ms < 5u ? 5u : a->release_ms;
        }

        /* 1-pole filter coefficient for n_samples */
        k = (int32_t)((n_samples * 32768u) / ((time_ms * 44100u) / 1000u + 1u));
        if (k > 32768) k = 32768;
        a->cur_breath += ((target_p - a->cur_breath) * k) >> 15;
    }

    /* 3. Mouth Shape Envelope (Transition Start Shape -> End Shape) */
    int32_t eff_t = a->tongue;
    int32_t eff_j = a->jaw;
    int32_t eff_l = a->lips;
    int32_t eff_v = a->velum;

    if (a->state != SYLL_IDLE || a->hold_on) {
        if (a->morph_mode == 3) {
            /* TOUCH: Key aftertouch pressure dynamically drives progress from Start to End shape */
            a->morph_phase = ((uint32_t)a->key_pressure << 16) / 127u;
        } else if (a->speed_ms > 0) {
            uint32_t spd = a->speed_ms < 5u ? 5u : a->speed_ms;
            uint32_t ret = a->return_ms < 5u ? 5u : a->return_ms;
            uint32_t hold_samples = ((uint32_t)a->hold_ms * 44100u) / 1000u;
            uint32_t step = (uint32_t)(((uint64_t)n_samples * 65536u * 1000u) / ((uint64_t)spd * 44100u));
            uint32_t r_step = (uint32_t)(((uint64_t)n_samples * 65536u * 1000u) / ((uint64_t)ret * 44100u));
            if (step < 1u) step = 1u;
            if (r_step < 1u) r_step = 1u;

            if (a->morph_mode == 0) {
                /* GLIDE: Transition from Start to End and stay at End */
                a->morph_phase += step;
                if (a->morph_phase > 65536u) a->morph_phase = 65536u;
            } else if (a->morph_mode == 1) {
                /* RETURN: Move to End, wait for hold_ms, then return to Start */
                if (a->morph_dir > 0) {
                    a->morph_phase += step;
                    if (a->morph_phase >= 65536u) {
                        a->morph_phase = 65536u;
                        a->hold_timer_samples += n_samples;
                        if (a->hold_timer_samples >= hold_samples) {
                            a->morph_dir = -1;
                            a->hold_timer_samples = 0;
                        }
                    }
                } else {
                    if (a->morph_phase <= r_step) {
                        a->morph_phase = 0;
                        if (a->state != SYLL_CODA) a->state = SYLL_IDLE;
                    } else {
                        a->morph_phase -= r_step;
                    }
                }
            } else if (a->morph_mode == 2) {
                /* BOUNCE: Continuous ping-pong talking loop between Start and End */
                if (a->morph_dir > 0) {
                    a->morph_phase += step;
                    if (a->morph_phase >= 65536u) {
                        a->morph_phase = 65536u;
                        a->morph_dir = -1;
                    }
                } else {
                    if (a->morph_phase <= r_step) {
                        a->morph_phase = 0;
                        a->morph_dir = 1;
                    } else {
                        a->morph_phase -= r_step;
                    }
                }
            }
        } else {
            a->morph_phase = 0;
        }

        int32_t p = (int32_t)a->morph_phase; /* 0..65536 */
        eff_t += (((int32_t)a->end_tongue - (int32_t)a->tongue) * p) >> 16;
        eff_j += (((int32_t)a->end_jaw    - (int32_t)a->jaw)    * p) >> 16;
        eff_l += (((int32_t)a->end_lips   - (int32_t)a->lips)   * p) >> 16;
        eff_v += (((int32_t)a->end_velum  - (int32_t)a->velum)  * p) >> 16;
    } else {
        a->morph_phase = 0;
        a->morph_dir = 1;
        a->hold_timer_samples = 0;
    }

    a->cur_tongue = (uint8_t)CLAMP(eff_t, 0, 127);
    a->cur_jaw = (uint8_t)CLAMP(eff_j, 0, 127);
    a->cur_jaw = (uint8_t)CLAMP(eff_j, 0, 127);

    /* 4. Vowel Grid Interpolation with Body Morph */
    if (a->vowel_jump >= 0 && a->vowel_jump < (int8_t)NV_NVOWELS) {
        /* Direct vowel jump from SCL key */
        for (i = 0; i < NV_SECTIONS; i++)
            target_A[i] = NV_VOWEL_AREAS[(uint32_t)a->vowel_jump][i];
    } else {
        /* Map effective Tongue and Jaw to 0..512 grid coordinates */
        int32_t tx = ((int32_t)a->cur_tongue * 512) / 127;
        int32_t ty = ((int32_t)a->cur_jaw * 512) / 127;

        /* Body morph: 0..63 = Male->Female, 64..127 = Female->Child */
        if (a->body < 64u) {
            b0 = 0u; b1 = 1u;
            b_frac = ((uint32_t)a->body * 256u) / 63u;
        } else {
            b0 = 1u; b1 = 2u;
            b_frac = (((uint32_t)a->body - 64u) * 256u) / 63u;
        }

        bilerp_vowel_grid(b0, tx, ty, a_b0);
        bilerp_vowel_grid(b1, tx, ty, a_b1);

        for (i = 0; i < NV_SECTIONS; i++)
            target_A[i] = a_b0[i] + (((a_b1[i] - a_b0[i]) * (int32_t)b_frac) >> 8);
    }

    /* 5. Consonant overlays (ONSET and CODA) */
    target_velum = (eff_v * 3276) / 127; /* 0 .. 0.8 cm^2 */

    if (a->state == SYLL_ONSET && a->onset_idx > 0u && a->onset_idx <= NV_NONSETS) {
        const nv_consonant_t *c = &NV_ONSETS[a->onset_idx - 1u];
        int32_t blend = a->onset_samples > 0 ? (int32_t)((a->state_samples * 256u) / a->onset_samples) : 256;
        if (blend > 256) blend = 256;
        /* Whole-tract consonant articulation profile blending into vowel */
        for (i = 0; i < NV_SECTIONS; i++) {
            int32_t ca = (int32_t)c->areas[i];
            target_A[i] = ca + (((target_A[i] - ca) * blend) >> 8);
        }
        if (c->nasal)
            target_velum = 3276 - ((3276 * blend) >> 8);
        if (c->manner == 2) /* fricative turbulence */
            a->noise_amt = (uint8_t)(96 - ((96 * blend) >> 8));
        else if (c->manner == 0) /* stop release burst */
            a->noise_amt = (blend < 80) ? (uint8_t)(90 - (blend * 90 / 80)) : 0;
        else
            a->noise_amt = 0;
    } else if (a->state == SYLL_CODA && a->coda_idx > 0u && a->coda_idx <= NV_NCODAS) {
        const nv_consonant_t *c = &NV_CODAS[a->coda_idx - 1u];
        int32_t blend = a->coda_samples > 0 ? (int32_t)((a->state_samples * 256u) / a->coda_samples) : 256;
        if (blend > 256) blend = 256;
        /* Vowel blends into coda consonant profile */
        for (i = 0; i < NV_SECTIONS; i++) {
            int32_t ca = (int32_t)c->areas[i];
            target_A[i] = target_A[i] + (((ca - target_A[i]) * blend) >> 8);
        }
        if (c->nasal)
            target_velum = ((3276 * blend) >> 8);
        if (c->manner == 2)
            a->noise_amt = (uint8_t)((96 * blend) >> 8);
        else
            a->noise_amt = 0;
    } else {
        a->noise_amt = 0;
    }

    /* 6. Lips modifier on sections 19..21 */
    lips_factor = 512 + eff_l * 120; /* 0.125 .. 3.8 */
    for (i = 19; i < NV_SECTIONS; i++) {
        target_A[i] = (target_A[i] * lips_factor) >> 14;
        if (target_A[i] < 41)
            target_A[i] = 41; /* Closed floor */
    }

    /* 6. Slew-limiting / 1-pole smoothing of area function (~15 Hz) */
    {
        /* smooth coefficient for ~15 Hz tracking */
        int32_t sm = (int32_t)((n_samples * 32768u) / (44100u / 40u + 1u));
        if (sm > 16384) sm = 16384;

        for (i = 0; i < NV_SECTIONS; i++) {
            a->cur_areas[i] += ((target_A[i] - a->cur_areas[i]) * sm) >> 15;
            if (a->cur_areas[i] < 41)
                a->cur_areas[i] = 41;
        }

        a->cur_velum += ((target_velum - a->cur_velum) * sm) >> 15;
        a->cur_lips += ((lips_factor - a->cur_lips) * sm) >> 15;
    }
}
