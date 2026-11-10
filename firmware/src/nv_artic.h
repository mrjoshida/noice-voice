/* SPDX-License-Identifier: GPL-3.0-only */
/* NOICE VOICE: Articulator & Syllable Engine */
#ifndef NV_ARTIC_H
#define NV_ARTIC_H

#include <stdint.h>
#include "nv_tables.h"

#define NV_SECTIONS 22u
#define NV_PHARYNX  8u
#define NV_ORAL     14u

typedef enum {
    SYLL_IDLE = 0,
    SYLL_ONSET,
    SYLL_NUCLEUS,
    SYLL_CODA,
} nv_syll_state_t;

typedef enum {
    CREATURE_HUMAN = 0,
    CREATURE_TROLL = 1,
    CREATURE_ELF   = 2,
    CREATURE_BEAST = 3,
} nv_creature_t;

typedef struct {
    /* Articulator controls: Start Mouth Shape (0..127) */
    uint8_t tongue;      /* K1: 0 (front) .. 127 (back) */
    uint8_t jaw;         /* K2: 0 (close) .. 127 (open) */
    uint8_t lips;        /* K3: 0 (closed) .. 127 (spread) */
    uint8_t velum;       /* K4: 0 (shut) .. 127 (open) */
    uint8_t body;        /* ALGORITHM: 0 (male) .. 64 (female) .. 127 (child) */
    uint8_t breath_base; /* SELECT: base lung pressure */
    uint8_t tension;     /* THROAT: 0 (breathy) .. 127 (pressed) */
    uint8_t noise_amt;   /* THROAT: turbulence / aspiration */
    uint8_t creature;    /* CREATURE_HUMAN, TROLL, ELF, BEAST */

    /* Articulator Envelope: End Mouth Shape (0..127) */
    uint8_t end_tongue;  /* Target Tongue */
    uint8_t end_jaw;     /* Target Jaw */
    uint8_t end_lips;    /* Target Lips */
    uint8_t end_velum;   /* Target Velum */

    /* Envelope Dynamics */
    uint16_t speed_ms;   /* Transition duration: Start -> End (5 .. 3000 ms) */
    uint16_t hold_ms;    /* Dwell duration at End shape (0 .. 3000 ms) */
    uint16_t return_ms;  /* Return duration: End -> Start (5 .. 3000 ms) */
    uint8_t morph_mode;  /* 0: GLIDE, 1: RETURN, 2: BOUNCE, 3: TOUCH */
    uint32_t morph_phase;/* Q16 progress: 0 (at Start) .. 65536 (at End) */
    int8_t morph_dir;    /* +1 (toward End), -1 (toward Start) */
    uint32_t hold_timer_samples;

    /* Breath / Phonation envelope */
    uint16_t attack_ms;  /* 2 .. 2000 ms breath ramp */
    uint16_t release_ms; /* 5 .. 3000 ms breath fade */

    /* Consonants (optional onset/coda) */
    uint8_t onset_idx;   /* 0: none, 1..16: consonant */
    uint8_t coda_idx;    /* 0: none, 1..10: coda */
    uint16_t onset_hold_ms; /* onset hold time: 20 .. 300 ms */

    /* Live Articulator Position (tracks knobs when idle, morphs during note) */
    uint8_t cur_tongue;     /* 0..127 */
    uint8_t cur_jaw;        /* 0..127 */

    /* Performance modifiers */
    uint8_t key_pressure;/* aftertouch from keytouch.c (0..127) */
    uint8_t whisper_on;  /* SEQ held: unvoiced aspiration mode */
    uint8_t hold_on;     /* PLAY latched: note sustains */
    int8_t vowel_jump;   /* -1: knobs, 0..21: vowel key held on SCL */

    /* State machine */
    nv_syll_state_t state;
    uint32_t state_samples;
    uint32_t onset_samples;
    uint32_t coda_samples;

    /* Smoothed outputs (Q12 cm^2 areas, Q15 breath) */
    int32_t cur_breath;  /* 0..32767 lung pressure */
    int32_t cur_areas[NV_SECTIONS];
    int32_t cur_velum;   /* Q12 cm^2 port area */
    int32_t cur_lips;    /* Q12 factor */
    int32_t pitch_scoop; /* Q16 semitones offset from scoop */
} nv_artic_t;

void nv_artic_init(nv_artic_t *a);
void nv_artic_note_on(nv_artic_t *a, uint8_t legato);
void nv_artic_note_off(nv_artic_t *a);
void nv_artic_respeak(nv_artic_t *a);

/* Called every control block (32 samples = ~0.725 ms @ 44.1 kHz) */
void nv_artic_update(nv_artic_t *a, uint32_t n_samples);

#endif /* NV_ARTIC_H */
