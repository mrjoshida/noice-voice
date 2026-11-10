/* SPDX-License-Identifier: GPL-3.0-only */
/* NOICE VOICE: Vector View Implementation */
#include "nv_view.h"

void nv_view_render_strip(uint16_t *buf, int32_t strip_y, int32_t strip_h,
                          const nv_artic_t *a, const int32_t *areas,
                          const int32_t *pres, uint32_t n_sections,
                          const int16_t *params, const char *msg,
                          uint8_t view_mode)
{
    int32_t w = 240, h = strip_h;
    uint32_t i;
    (void)areas;

    /* 1. Clear strip to black */
    memset(buf, 0, (uint32_t)w * (uint32_t)h * sizeof(uint16_t));

    /* ------------------------------------------------ Top & Middle: 0..179 */
    if (strip_y < 180) {
        /* Creature theming */
        uint8_t creature = a ? a->creature : (uint8_t)CREATURE_HUMAN;
        uint16_t skull_col = NV_COL_CYAN;
        const char *cname = "HUMAN";
        if (creature == (uint8_t)CREATURE_TROLL) {
            skull_col = NV_COL_AMBER;
            cname = "TROLL";
        } else if (creature == (uint8_t)CREATURE_ELF) {
            skull_col = NV_COL_PINK;
            cname = "ELF";
        } else if (creature == (uint8_t)CREATURE_BEAST) {
            skull_col = NV_COL_CORAL;
            cname = "BEAST";
        }

        /* A. Cranium / Skull (creature outline) */
        nv_vec_poly(buf, w, h, NV_SKULL, NV_NSKULL, strip_y, NV_SWAP16(skull_col));

        /* In RESONANCE: Wall Q tissue damping contour */
        if (view_mode == NV_VIEW_RESONANCE && params) {
            int32_t wall_q = params[P_WALL_Q];
            if (wall_q > 20) {
                uint16_t wcol = NV_SWAP16(NV_COL_CYAN);
                for (i = 0; i < NV_NSKULL - 1; i += 2) {
                    int32_t sx = (int32_t)NV_SKULL[i].x - 2;
                    int32_t sy = (int32_t)NV_SKULL[i].y - strip_y;
                    int32_t ex = (int32_t)NV_SKULL[i + 1].x - 2;
                    int32_t ey = (int32_t)NV_SKULL[i + 1].y - strip_y;
                    nv_vec_line(buf, w, h, sx, sy, ex, ey, wcol);
                }
            }
        }

        /* B. Palate / Hard roof of mouth (white outline) */
        nv_vec_poly(buf, w, h, NV_PALATE, NV_NPALATE, strip_y, NV_SWAP16(NV_COL_WHITE));

        /* In RESONANCE: Subglottal Trachea & Upper Nasal Cavity */
        if (view_mode == NV_VIEW_RESONANCE && params) {
            /* Trachea cartilage rings below glottis */
            int32_t trach = params[P_TRACHEA];
            uint16_t tr_col = trach > 0 ? NV_SWAP16(NV_COL_CORAL) : NV_SWAP16(NV_COL_DIM_AMB);
            for (int32_t ty = 162; ty <= 178; ty += 5) {
                nv_vec_line(buf, w, h, 34, ty - strip_y, 48, ty - strip_y, tr_col);
            }
            nv_vec_line(buf, w, h, 34, 155 - strip_y, 34, 180 - strip_y, tr_col);
            nv_vec_line(buf, w, h, 48, 155 - strip_y, 48, 180 - strip_y, tr_col);

            /* Nasal cavity branch */
            int32_t nas_q = params[P_NASAL_Q];
            uint16_t nas_col = nas_q > 0 ? NV_SWAP16(NV_COL_WHITE) : NV_SWAP16(NV_COL_DIM_AMB);
            nv_vec_line(buf, w, h, 65, 75 - strip_y, 90, 68 - strip_y, nas_col);
            nv_vec_line(buf, w, h, 90, 68 - strip_y, 120, 72 - strip_y, nas_col);
            nv_vec_line(buf, w, h, 120, 72 - strip_y, 135, 95 - strip_y, nas_col);
            for (int32_t nx = 76; nx <= 116; nx += 10) {
                nv_vec_line(buf, w, h, nx, 71 - strip_y, nx + 3, 80 - strip_y, nas_col);
            }
        }

        /* C. Amber Waveguide Pressure Rungs */
        if (pres && n_sections) {
            for (i = 0; i < NV_NRAYS; i++) {
                uint32_t sec = (i * n_sections) / NV_NRAYS;
                int32_t p = pres[sec];
                int32_t mag = p < 0 ? -p : p;
                if (mag > 200) {
                    uint16_t col = mag > 2000 ? NV_SWAP16(NV_COL_AMBER) : NV_SWAP16(NV_COL_DIM_AMB);
                    nv_vec_line(buf, w, h,
                                NV_RAYS[i].inner.x, (int32_t)NV_RAYS[i].inner.y - strip_y,
                                NV_RAYS[i].outer.x, (int32_t)NV_RAYS[i].outer.y - strip_y,
                                col);
                }
            }
        }

        /* D. Live Tongue (interpolated green outline) */
        if (a) {
            nv_pt_t live_tongue[NV_NTONGUE];
            int32_t tx = ((int32_t)a->cur_tongue * 512) / 127;
            int32_t ty = ((int32_t)a->cur_jaw * 512) / 127;
            uint32_t ix = (uint32_t)(tx >> 8), fx = (uint32_t)(tx & 255);
            uint32_t iy = (uint32_t)(ty >> 8), fy = (uint32_t)(ty & 255);

            if (ix > 1u) { ix = 1u; fx = 256u; }
            if (iy > 1u) { iy = 1u; fy = 256u; }

            for (i = 0; i < NV_NTONGUE; i++) {
                int32_t p00_x = NV_TONGUE[iy][ix][i].x,       p00_y = NV_TONGUE[iy][ix][i].y;
                int32_t p01_x = NV_TONGUE[iy][ix + 1u][i].x,  p01_y = NV_TONGUE[iy][ix + 1u][i].y;
                int32_t p10_x = NV_TONGUE[iy + 1u][ix][i].x,  p10_y = NV_TONGUE[iy + 1u][ix][i].y;
                int32_t p11_x = NV_TONGUE[iy + 1u][ix + 1u][i].x, p11_y = NV_TONGUE[iy + 1u][ix + 1u][i].y;

                int32_t top_x = p00_x + (((p01_x - p00_x) * (int32_t)fx) >> 8);
                int32_t top_y = p00_y + (((p01_y - p00_y) * (int32_t)fx) >> 8);
                int32_t bot_x = p10_x + (((p11_x - p10_x) * (int32_t)fx) >> 8);
                int32_t bot_y = p10_y + (((p11_y - p10_y) * (int32_t)fx) >> 8);

                live_tongue[i].x = (int16_t)(top_x + (((bot_x - top_x) * (int32_t)fy) >> 8));
                live_tongue[i].y = (int16_t)(top_y + (((bot_y - top_y) * (int32_t)fy) >> 8));
            }
            nv_vec_poly(buf, w, h, live_tongue, NV_NTONGUE, strip_y, NV_SWAP16(NV_COL_GREEN));

            /* Highlight consonant constriction in PHONEMES mode */
            if (view_mode == NV_VIEW_PHONEMES && params && params[P_E5] > 0 && params[P_E5] <= NV_NONSETS) {
                uint32_t cs = NV_ONSETS[params[P_E5] - 1u].constr_sec;
                uint32_t r_idx = (cs * NV_NRAYS) / NV_SECTIONS;
                if (r_idx < NV_NRAYS) {
                    int32_t cx = ((int32_t)NV_RAYS[r_idx].inner.x + (int32_t)NV_RAYS[r_idx].outer.x) / 2;
                    int32_t cy = ((int32_t)NV_RAYS[r_idx].inner.y + (int32_t)NV_RAYS[r_idx].outer.y) / 2 - strip_y;
                    nv_vec_line(buf, w, h, cx - 4, cy, cx + 4, cy, NV_SWAP16(NV_COL_AMBER));
                    nv_vec_line(buf, w, h, cx, cy - 4, cx, cy + 4, NV_SWAP16(NV_COL_AMBER));
                }
            }

            /* E. Jaw Bone (coral line) */
            int32_t jaw_drop = ((int32_t)a->cur_jaw * 18) / 127;
            nv_pt_t live_jaw[NV_NJAW];
            for (i = 0; i < NV_NJAW; i++) {
                live_jaw[i].x = NV_JAW[iy][i].x;
                live_jaw[i].y = (int16_t)(NV_JAW[iy][i].y + jaw_drop);
            }
            nv_vec_poly(buf, w, h, live_jaw, NV_NJAW, strip_y, NV_SWAP16(NV_COL_CORAL));

            /* F. Vermilion Lips (pink outline) */
            int32_t lip_ap = ((int32_t)a->cur_lips * 14) / 127;
            nv_pt_t live_ulip[NV_NUPPER_LIP];
            nv_pt_t live_llip[NV_NLOWER_LIP];
            for (i = 0; i < NV_NUPPER_LIP; i++) {
                live_ulip[i].x = (int16_t)(NV_UPPER_LIP[iy][ix][i].x + (lip_ap >> 1));
                live_ulip[i].y = NV_UPPER_LIP[iy][ix][i].y;
            }
            for (i = 0; i < NV_NLOWER_LIP; i++) {
                live_llip[i].x = (int16_t)(NV_LOWER_LIP[iy][ix][i].x + (lip_ap >> 1));
                live_llip[i].y = (int16_t)(NV_LOWER_LIP[iy][ix][i].y + jaw_drop);
            }
            nv_vec_poly(buf, w, h, live_ulip, NV_NUPPER_LIP, strip_y, NV_SWAP16(NV_COL_PINK));
            nv_vec_poly(buf, w, h, live_llip, NV_NLOWER_LIP, strip_y, NV_SWAP16(NV_COL_PINK));

            /* G. Velum / Soft palate port */
            int32_t vel_open = (a->cur_velum * 12) / 3276;
            nv_pt_t live_velum[NV_NVELUM];
            for (i = 0; i < NV_NVELUM; i++) {
                live_velum[i].x = NV_VELUM[i].x;
                live_velum[i].y = (int16_t)(NV_VELUM[i].y - (i > 8 ? vel_open : 0));
            }
            nv_vec_poly(buf, w, h, live_velum, NV_NVELUM, strip_y, NV_SWAP16(NV_COL_WHITE));

            /* H. 2D Formant Vowel Quadrant Crosshair Box */
            int32_t bx = 186, by = 28, bw = 48, bh = 48;
            nv_vec_line(buf, w, h, bx, by - strip_y, bx + bw, by - strip_y, NV_SWAP16(NV_COL_DIM_AMB));
            nv_vec_line(buf, w, h, bx + bw, by - strip_y, bx + bw, by + bh - strip_y, NV_SWAP16(NV_COL_DIM_AMB));
            nv_vec_line(buf, w, h, bx + bw, by + bh - strip_y, bx, by + bh - strip_y, NV_SWAP16(NV_COL_DIM_AMB));
            nv_vec_line(buf, w, h, bx, by + bh - strip_y, bx, by - strip_y, NV_SWAP16(NV_COL_DIM_AMB));

            /* Vowel quadrant corner labels */
            nv_vec_text(buf, w, h, bx + 2, by + 10, strip_y, 45, "i", NV_SWAP16(NV_COL_AMBER));
            nv_vec_text(buf, w, h, bx + bw - 8, by + 10, strip_y, 45, "u", NV_SWAP16(NV_COL_AMBER));
            nv_vec_text(buf, w, h, bx + (bw / 2) - 3, by + bh - 2, strip_y, 45, "a", NV_SWAP16(NV_COL_AMBER));

            /* Moving '+' cursor dot */
            int32_t dot_x = bx + 4 + (((int32_t)a->cur_tongue * (bw - 8)) / 127);
            int32_t dot_y = by + 4 + (((int32_t)a->cur_jaw * (bh - 8)) / 127) - strip_y;
            nv_vec_line(buf, w, h, dot_x - 3, dot_y, dot_x + 3, dot_y, NV_SWAP16(NV_COL_WHITE));
            nv_vec_line(buf, w, h, dot_x, dot_y - 3, dot_x, dot_y + 3, NV_SWAP16(NV_COL_WHITE));

            /* In ENV mode, draw trajectory line from start mouth to target mouth */
            if (view_mode == NV_VIEW_ENV && params) {
                int32_t s_x = bx + 4 + (((int32_t)params[P_E0] * (bw - 8)) / 127);
                int32_t s_y = by + 4 + (((int32_t)params[P_E1] * (bw - 8)) / 127) - strip_y;
                int32_t e_x = bx + 4 + (((int32_t)params[P_END_TONGUE] * (bw - 8)) / 127);
                int32_t e_y = by + 4 + (((int32_t)params[P_END_JAW] * (bw - 8)) / 127) - strip_y;
                nv_vec_line(buf, w, h, s_x, s_y, e_x, e_y, NV_SWAP16(NV_COL_PINK));
            }
        }

        /* Top Banner: Header Text */
        nv_vec_text(buf, w, h, 6, 12, strip_y, 90, cname, NV_SWAP16(skull_col));

        if (msg) {
            char sbuf[32];
            unsigned len;
            sbuf[0] = '[';
            sbuf[1] = 0;
            str_cpy(sbuf + 1, msg, sizeof(sbuf) - 2);
            len = str_len(sbuf);
            sbuf[len] = ']';
            sbuf[len + 1] = 0;
            nv_vec_text(buf, w, h, 60, 12, strip_y, 90, sbuf, NV_SWAP16(NV_COL_AMBER));
        } else if (view_mode == NV_VIEW_TARGET) {
            const char *st = (a && a->state != SYLL_IDLE) ? "MORPH" : "TARGET";
            char sbuf[32];
            unsigned len;
            sbuf[0] = '[';
            sbuf[1] = 0;
            str_cpy(sbuf + 1, st, sizeof(sbuf) - 2);
            len = str_len(sbuf);
            sbuf[len] = ']';
            sbuf[len + 1] = 0;
            nv_vec_text(buf, w, h, 60, 12, strip_y, 90, sbuf, NV_SWAP16(NV_COL_PINK));
        } else if (view_mode == NV_VIEW_ENV && params) {
            static const char *const MODES[] = {"GLIDE", "RETURN", "BOUNCE", "TOUCH"};
            uint8_t m = (uint8_t)clamp(params[P_ENV_MODE], 0, 3);
            char sbuf[32];
            unsigned len;
            sbuf[0] = '[';
            sbuf[1] = 0;
            str_cpy(sbuf + 1, MODES[m], sizeof(sbuf) - 2);
            len = str_len(sbuf);
            sbuf[len] = ']';
            sbuf[len + 1] = 0;
            nv_vec_text(buf, w, h, 60, 12, strip_y, 90, sbuf, NV_SWAP16(NV_COL_PINK));
        } else if (view_mode == NV_VIEW_RESONANCE) {
            nv_vec_text(buf, w, h, 60, 12, strip_y, 90, "[RESONANCE]", NV_SWAP16(NV_COL_CYAN));
        } else if (view_mode == NV_VIEW_PHONEMES && params) {
            char sbuf[32];
            uint8_t oi = (uint8_t)clamp(params[P_E5], 0, NV_NONSETS);
            uint8_t ci = (uint8_t)clamp(params[P_E6], 0, NV_NCODAS);
            const char *on_str = NV_ONSET_DISP[oi];
            const char *co_str = NV_CODA_DISP[ci];
            sbuf[0] = '[';
            sbuf[1] = 0;
            str_cpy(sbuf + 1, on_str, sizeof(sbuf) - 2);
            unsigned len = str_len(sbuf);
            str_cpy(sbuf + len, " -> ", sizeof(sbuf) - len - 1);
            len = str_len(sbuf);
            str_cpy(sbuf + len, co_str, sizeof(sbuf) - len - 1);
            len = str_len(sbuf);
            sbuf[len] = ']';
            sbuf[len + 1] = 0;
            nv_vec_text(buf, w, h, 60, 12, strip_y, 90, sbuf, NV_SWAP16(NV_COL_PINK));
        } else if (a) {
            const char *st = "VOWEL";
            char sbuf[32];
            unsigned len;
            if (a->whisper_on) {
                st = "WHISPER";
            } else if (a->state == SYLL_ONSET && a->onset_idx > 0 && a->onset_idx <= NV_NONSETS) {
                st = NV_ONSET_DISP[a->onset_idx];
            } else if (a->state == SYLL_CODA && a->coda_idx > 0 && a->coda_idx <= NV_NCODAS) {
                st = NV_CODA_DISP[a->coda_idx];
            } else if (a->state != SYLL_IDLE) {
                if (a->morph_mode == 3) {
                    st = "TOUCH";
                } else if (a->morph_dir > 0 && a->morph_phase < 65536u) {
                    st = "GLIDE";
                } else if (a->morph_phase >= 65536u && a->hold_timer_samples > 0) {
                    st = "HOLD";
                } else if (a->morph_dir < 0) {
                    st = "RETURN";
                } else {
                    st = "VOICING";
                }
            } else if (a->hold_on) {
                st = "HOLD";
            } else {
                st = "IDLE";
            }
            sbuf[0] = '[';
            sbuf[1] = 0;
            str_cpy(sbuf + 1, st, sizeof(sbuf) - 2);
            len = str_len(sbuf);
            sbuf[len] = ']';
            sbuf[len + 1] = 0;
            nv_vec_text(buf, w, h, 60, 12, strip_y, 90, sbuf, NV_SWAP16(NV_COL_AMBER));
        }
    }

    /* ------------------------------------------------ Bottom Cards: 180..239 */
    if (strip_y + strip_h > 180) {
        static const struct {
            const char *name;
            uint16_t color;
        } CARDS_TABLE[5][4] = {
            /* NV_VIEW_START */
            { {"TONGUE", NV_COL_GREEN}, {"JAW", NV_COL_CORAL}, {"LIPS", NV_COL_PINK}, {"VELUM", NV_COL_WHITE} },
            /* NV_VIEW_TARGET */
            { {"END TNG", NV_COL_GREEN}, {"END JAW", NV_COL_CORAL}, {"END LIP", NV_COL_PINK}, {"END VEL", NV_COL_WHITE} },
            /* NV_VIEW_ENV */
            { {"SPEED", NV_COL_GREEN}, {"HOLD", NV_COL_AMBER}, {"RETURN", NV_COL_CORAL}, {"MODE", NV_COL_PINK} },
            /* NV_VIEW_RESONANCE */
            { {"WALL Q", NV_COL_CYAN}, {"LENGTH", NV_COL_AMBER}, {"TRACHEA", NV_COL_CORAL}, {"NASAL Q", NV_COL_WHITE} },
            /* NV_VIEW_PHONEMES */
            { {"ONSET", NV_COL_PINK}, {"CODA", NV_COL_AMBER}, {"BODY", NV_COL_CYAN}, {"CHOIR", NV_COL_CORAL} },
        };

        uint8_t mode_idx = view_mode < 5u ? view_mode : 0u;

        for (i = 0; i < 4u; i++) {
            int32_t cx = 4 + (int32_t)i * 59;
            int32_t cy = 182;
            int32_t cw = 55;
            int32_t ch = 54;
            const char *cname = CARDS_TABLE[mode_idx][i].name;
            uint16_t col = NV_SWAP16(CARDS_TABLE[mode_idx][i].color);
            int32_t val = 64;
            char val_str[16] = "";

            if (mode_idx == NV_VIEW_START) {
                val = params ? params[P_E0 + i] : 64;
                if (a && a->state != SYLL_IDLE) {
                    if (i == 0) val = a->cur_tongue;
                    else if (i == 1) val = a->cur_jaw;
                    else if (i == 2) val = a->lips;
                    else if (i == 3) val = (a->cur_velum * 127) / 3276;
                }
                fmt_int(val_str, val);
            } else if (mode_idx == NV_VIEW_TARGET) {
                val = params ? params[P_END_TONGUE + i] : 64;
                fmt_int(val_str, val);
            } else if (mode_idx == NV_VIEW_ENV) {
                val = params ? params[P_ENV_SPEED + i] : 0;
                if (i == 3) {
                    static const char *const M_STR[] = {"GLID", "RET", "BNC", "TCH"};
                    str_cpy(val_str, M_STR[clamp(val, 0, 3)], sizeof(val_str));
                } else {
                    fmt_int(val_str, val);
                }
            } else if (mode_idx == NV_VIEW_RESONANCE) {
                val = params ? params[P_WALL_Q + i] : 64;
                fmt_int(val_str, val);
            } else if (mode_idx == NV_VIEW_PHONEMES) {
                if (i == 0) {
                    val = params ? params[P_E5] : 0;
                    str_cpy(val_str, NV_ONSET_DISP[clamp(val, 0, NV_NONSETS)], sizeof(val_str));
                } else if (i == 1) {
                    val = params ? params[P_E6] : 0;
                    str_cpy(val_str, NV_CODA_DISP[clamp(val, 0, NV_NCODAS)], sizeof(val_str));
                } else if (i == 2) {
                    val = params ? params[P_E4] : 0;
                    const char *b_str = (val < 43) ? "MALE" : (val < 86 ? "FEML" : "CHLD");
                    str_cpy(val_str, b_str, sizeof(val_str));
                } else {
                    val = params ? params[P_E7] : 0;
                    fmt_int(val_str, clamp(val, 0, 2) + 1);
                }
            }

            /* Card frame box */
            nv_vec_line(buf, w, h, cx, cy - strip_y, cx + cw, cy - strip_y, col);
            nv_vec_line(buf, w, h, cx + cw, cy - strip_y, cx + cw, cy + ch - strip_y, col);
            nv_vec_line(buf, w, h, cx + cw, cy + ch - strip_y, cx, cy + ch - strip_y, col);
            nv_vec_line(buf, w, h, cx, cy + ch - strip_y, cx, cy - strip_y, col);

            /* Title label centered */
            int32_t tw = nv_vec_text_width(cname, 60);
            int32_t tx = cx + (cw - tw) / 2;
            nv_vec_text(buf, w, h, tx, cy + 12, strip_y, 60, cname, col);

            /* Value text in center */
            int32_t scale = str_len(val_str) > 3 ? 75 : 95;
            int32_t vw = nv_vec_text_width(val_str, scale);
            int32_t vx = cx + (cw - vw) / 2;
            nv_vec_text(buf, w, h, vx, cy + 28, strip_y, scale, val_str, NV_SWAP16(NV_COL_WHITE));

            /* Bottom level bar gauge */
            int32_t gauge_val = clamp(val, 0, 127);
            int32_t bar_w = ((cw - 8) * gauge_val) / 127;
            if (bar_w > 0) {
                nv_vec_line(buf, w, h, cx + 4, cy + ch - 6 - strip_y, cx + 4 + bar_w, cy + ch - 6 - strip_y, col);
                nv_vec_line(buf, w, h, cx + 4, cy + ch - 5 - strip_y, cx + 4 + bar_w, cy + ch - 5 - strip_y, col);
            }
        }
    }
}
