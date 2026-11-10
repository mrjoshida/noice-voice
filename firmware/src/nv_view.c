/* SPDX-License-Identifier: GPL-3.0-only */
/* NOICE VOICE: Vector View Implementation */
#include "nv_view.h"

void nv_view_render_strip(uint16_t *buf, int32_t strip_y, int32_t strip_h,
                          const nv_artic_t *a, const int32_t *areas,
                          const int32_t *pres, uint32_t n_sections,
                          const int16_t *params, const char *msg,
                          int is_target)
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

        /* B. Palate / Hard roof of mouth (white outline) */
        nv_vec_poly(buf, w, h, NV_PALATE, NV_NPALATE, strip_y, NV_SWAP16(NV_COL_WHITE));

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
                int32_t p00_x = (int32_t)NV_TONGUE[iy][ix][i].x,         p00_y = (int32_t)NV_TONGUE[iy][ix][i].y;
                int32_t p01_x = (int32_t)NV_TONGUE[iy][ix + 1u][i].x,    p01_y = (int32_t)NV_TONGUE[iy][ix + 1u][i].y;
                int32_t p10_x = (int32_t)NV_TONGUE[iy + 1u][ix][i].x,    p10_y = (int32_t)NV_TONGUE[iy + 1u][ix][i].y;
                int32_t p11_x = (int32_t)NV_TONGUE[iy + 1u][ix + 1u][i].x,p11_y = (int32_t)NV_TONGUE[iy + 1u][ix + 1u][i].y;

                int32_t top_x = p00_x + (((p01_x - p00_x) * (int32_t)fx) >> 8);
                int32_t top_y = p00_y + (((p01_y - p00_y) * (int32_t)fx) >> 8);
                int32_t bot_x = p10_x + (((p11_x - p10_x) * (int32_t)fx) >> 8);
                int32_t bot_y = p10_y + (((p11_y - p10_y) * (int32_t)fx) >> 8);

                live_tongue[i].x = (int16_t)(top_x + (((bot_x - top_x) * (int32_t)fy) >> 8));
                live_tongue[i].y = (int16_t)(top_y + (((bot_y - top_y) * (int32_t)fy) >> 8));
            }
            nv_vec_poly(buf, w, h, live_tongue, NV_NTONGUE, strip_y, NV_SWAP16(NV_COL_GREEN));
        }

        /* E. Live Jaw bone (coral outline) */
        if (a) {
            nv_pt_t live_jaw[NV_NJAW];
            int32_t ty = ((int32_t)a->cur_jaw * 512) / 127;
            uint32_t iy = (uint32_t)(ty >> 8), fy = (uint32_t)(ty & 255);
            if (iy > 1u) { iy = 1u; fy = 256u; }

            for (i = 0; i < NV_NJAW; i++) {
                int32_t p0_x = (int32_t)NV_JAW[iy][i].x,      p0_y = (int32_t)NV_JAW[iy][i].y;
                int32_t p1_x = (int32_t)NV_JAW[iy + 1u][i].x, p1_y = (int32_t)NV_JAW[iy + 1u][i].y;
                live_jaw[i].x = (int16_t)(p0_x + (((p1_x - p0_x) * (int32_t)fy) >> 8));
                live_jaw[i].y = (int16_t)(p0_y + (((p1_y - p0_y) * (int32_t)fy) >> 8));
            }
            nv_vec_poly(buf, w, h, live_jaw, NV_NJAW, strip_y, NV_SWAP16(NV_COL_CORAL));
        }

        /* F. Velum Flap (white outline hanging from back of hard palate) */
        if (a) {
            nv_pt_t live_velum[NV_NVELUM];
            int32_t v_shift = (a->cur_velum * 6) / 3276; /* Open port lowers uvula toward pharynx */
            for (i = 0; i < NV_NVELUM; i++) {
                live_velum[i].x = NV_VELUM[i].x;
                live_velum[i].y = (int16_t)(NV_VELUM[i].y + (i >= 8 && i <= 14 ? v_shift : 0));
            }
            nv_vec_poly(buf, w, h, live_velum, NV_NVELUM, strip_y, NV_SWAP16(NV_COL_WHITE));
        }

        /* G. Live Lips (pink outline: Upper & Lower vermilion border) */
        if (a) {
            nv_pt_t live_u_lip[NV_NUPPER_LIP];
            nv_pt_t live_l_lip[NV_NLOWER_LIP];
            int32_t tx = ((int32_t)a->cur_tongue * 512) / 127;
            int32_t ty = ((int32_t)a->cur_jaw * 512) / 127;
            uint32_t ix = (uint32_t)(tx >> 8), fx = (uint32_t)(tx & 255);
            uint32_t iy = (uint32_t)(ty >> 8), fy = (uint32_t)(ty & 255);
            if (ix > 1u) { ix = 1u; fx = 256u; }
            if (iy > 1u) { iy = 1u; fy = 256u; }

            for (i = 0; i < NV_NUPPER_LIP; i++) {
                int32_t p00_x = (int32_t)NV_UPPER_LIP[iy][ix][i].x,         p00_y = (int32_t)NV_UPPER_LIP[iy][ix][i].y;
                int32_t p01_x = (int32_t)NV_UPPER_LIP[iy][ix + 1u][i].x,    p01_y = (int32_t)NV_UPPER_LIP[iy][ix + 1u][i].y;
                int32_t p10_x = (int32_t)NV_UPPER_LIP[iy + 1u][ix][i].x,    p10_y = (int32_t)NV_UPPER_LIP[iy + 1u][ix][i].y;
                int32_t p11_x = (int32_t)NV_UPPER_LIP[iy + 1u][ix + 1u][i].x,p11_y = (int32_t)NV_UPPER_LIP[iy + 1u][ix + 1u][i].y;

                int32_t top_x = p00_x + (((p01_x - p00_x) * (int32_t)fx) >> 8);
                int32_t top_y = p00_y + (((p01_y - p00_y) * (int32_t)fx) >> 8);
                int32_t bot_x = p10_x + (((p11_x - p10_x) * (int32_t)fx) >> 8);
                int32_t bot_y = p10_y + (((p11_y - p10_y) * (int32_t)fx) >> 8);

                live_u_lip[i].x = (int16_t)(top_x + (((bot_x - top_x) * (int32_t)fy) >> 8));
                live_u_lip[i].y = (int16_t)(top_y + (((bot_y - top_y) * (int32_t)fy) >> 8));
            }

            for (i = 0; i < NV_NLOWER_LIP; i++) {
                int32_t p00_x = (int32_t)NV_LOWER_LIP[iy][ix][i].x,         p00_y = (int32_t)NV_LOWER_LIP[iy][ix][i].y;
                int32_t p01_x = (int32_t)NV_LOWER_LIP[iy][ix + 1u][i].x,    p01_y = (int32_t)NV_LOWER_LIP[iy][ix + 1u][i].y;
                int32_t p10_x = (int32_t)NV_LOWER_LIP[iy + 1u][ix][i].x,    p10_y = (int32_t)NV_LOWER_LIP[iy + 1u][ix][i].y;
                int32_t p11_x = (int32_t)NV_LOWER_LIP[iy + 1u][ix + 1u][i].x,p11_y = (int32_t)NV_LOWER_LIP[iy + 1u][ix + 1u][i].y;

                int32_t top_x = p00_x + (((p01_x - p00_x) * (int32_t)fx) >> 8);
                int32_t top_y = p00_y + (((p01_y - p00_y) * (int32_t)fx) >> 8);
                int32_t bot_x = p10_x + (((p11_x - p10_x) * (int32_t)fx) >> 8);
                int32_t bot_y = p10_y + (((p11_y - p10_y) * (int32_t)fx) >> 8);

                live_l_lip[i].x = (int16_t)(top_x + (((bot_x - top_x) * (int32_t)fy) >> 8));
                live_l_lip[i].y = (int16_t)(top_y + (((bot_y - top_y) * (int32_t)fy) >> 8));
            }

            /* K3 (Lips) closure modifier: at lips=0, lower lip seals against upper lip */
            if (a->lips < 100u) {
                int32_t close_shift = ((100u - (uint32_t)a->lips) * 8u) / 100u;
                for (i = 0; i < NV_NLOWER_LIP; i++)
                    live_l_lip[i].y -= (int16_t)close_shift;
            }

            nv_vec_poly(buf, w, h, live_u_lip, NV_NUPPER_LIP, strip_y, NV_SWAP16(NV_COL_PINK));
            nv_vec_poly(buf, w, h, live_l_lip, NV_NLOWER_LIP, strip_y, NV_SWAP16(NV_COL_PINK));
        }

        /* H. Vowel Chart Inset (Upper right: X = 172..236, Y = 8..72) */
        {
            int32_t gx = 174, gy = 10, gw = 60, gh = 60;
            /* Outer frame */
            nv_vec_line(buf, w, h, gx, gy - strip_y, gx + gw, gy - strip_y, NV_SWAP16(NV_COL_GRID));
            nv_vec_line(buf, w, h, gx + gw, gy - strip_y, gx + gw, gy + gh - strip_y, NV_SWAP16(NV_COL_GRID));
            nv_vec_line(buf, w, h, gx + gw, gy + gh - strip_y, gx, gy + gh - strip_y, NV_SWAP16(NV_COL_GRID));
            nv_vec_line(buf, w, h, gx, gy + gh - strip_y, gx, gy - strip_y, NV_SWAP16(NV_COL_GRID));

            /* Grid internal cross lines */
            nv_vec_line(buf, w, h, gx + 20, gy - strip_y, gx + 20, gy + gh - strip_y, NV_SWAP16(NV_COL_GRID));
            nv_vec_line(buf, w, h, gx + 40, gy - strip_y, gx + 40, gy + gh - strip_y, NV_SWAP16(NV_COL_GRID));
            nv_vec_line(buf, w, h, gx, gy + 20 - strip_y, gx + gw, gy + 20 - strip_y, NV_SWAP16(NV_COL_GRID));
            nv_vec_line(buf, w, h, gx, gy + 40 - strip_y, gx + gw, gy + 40 - strip_y, NV_SWAP16(NV_COL_GRID));

            /* Vowel labels (small Hershey scale 80 / 256) */
            nv_vec_text(buf, w, h, gx + 3,  gy + 14, strip_y, 75, "I", NV_SWAP16(NV_COL_CYAN));
            nv_vec_text(buf, w, h, gx + 45, gy + 14, strip_y, 75, "U", NV_SWAP16(NV_COL_CYAN));
            nv_vec_text(buf, w, h, gx + 3,  gy + 54, strip_y, 75, "E", NV_SWAP16(NV_COL_CYAN));
            nv_vec_text(buf, w, h, gx + 45, gy + 54, strip_y, 75, "A", NV_SWAP16(NV_COL_CYAN));

            /* Live crosshair cursor '+' */
            if (a) {
                int32_t cx = gx + 4 + (((int32_t)a->cur_tongue * 52) / 127);
                int32_t cy = gy + 4 + (((int32_t)a->cur_jaw * 52) / 127);
                nv_vec_line(buf, w, h, cx - 3, cy - strip_y, cx + 3, cy - strip_y, NV_SWAP16(NV_COL_GREEN));
                nv_vec_line(buf, w, h, cx, cy - 3 - strip_y, cx, cy + 3 - strip_y, NV_SWAP16(NV_COL_GREEN));
            }
        }

        /* I. Top Header & Syllable Status (X = 6, Y = 10..24) */
        nv_vec_text(buf, w, h, 8, 12, strip_y, 90, cname, NV_SWAP16(NV_COL_WHITE));
        if (msg && msg[0]) {
            char sbuf[32];
            unsigned len;
            sbuf[0] = '[';
            sbuf[1] = 0;
            str_cpy(sbuf + 1, msg, sizeof(sbuf) - 2);
            len = str_len(sbuf);
            sbuf[len] = ']';
            sbuf[len + 1] = 0;
            nv_vec_text(buf, w, h, 60, 12, strip_y, 90, sbuf, NV_SWAP16(NV_COL_AMBER));
        } else if (is_target) {
            char sbuf[32];
            unsigned len;
            const char *st = (a && a->state != SYLL_IDLE) ? "MORPH" : "TARGET";
            sbuf[0] = '[';
            sbuf[1] = 0;
            str_cpy(sbuf + 1, st, sizeof(sbuf) - 2);
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
                st = NV_ONSETS[a->onset_idx - 1u].ipa;
            } else if (a->state == SYLL_CODA && a->coda_idx > 0 && a->coda_idx <= NV_NCODAS) {
                st = NV_CODAS[a->coda_idx - 1u].ipa;
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
        } CARDS_START[4] = {
            {"TONGUE", NV_COL_GREEN},
            {"JAW",    NV_COL_CORAL},
            {"LIPS",   NV_COL_PINK},
            {"VELUM",  NV_COL_WHITE},
        };
        static const struct {
            const char *name;
            uint16_t color;
        } CARDS_TARGET[4] = {
            {"END TNG", NV_COL_GREEN},
            {"END JAW", NV_COL_CORAL},
            {"END LIP", NV_COL_PINK},
            {"END VEL", NV_COL_WHITE},
        };

        for (i = 0; i < 4u; i++) {
            int32_t cx = 4 + (int32_t)i * 59;
            int32_t cy = 182;
            int32_t cw = 55;
            int32_t ch = 54;
            const char *cname = is_target ? CARDS_TARGET[i].name : CARDS_START[i].name;
            uint16_t col = NV_SWAP16(is_target ? CARDS_TARGET[i].color : CARDS_START[i].color);
            int32_t val = params ? (is_target ? params[P_END_TONGUE + i] : params[P_E0 + i]) : 64;
            if (a && a->state != SYLL_IDLE) {
                if (i == 0) val = a->cur_tongue;
                else if (i == 1) val = a->cur_jaw;
                else if (i == 2) val = a->lips;
                else if (i == 3) val = (a->cur_velum * 127) / 3276;
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

            /* Value number in center */
            char val_str[16];
            fmt_int(val_str, val);
            int32_t vw = nv_vec_text_width(val_str, 95);
            int32_t vx = cx + (cw - vw) / 2;
            nv_vec_text(buf, w, h, vx, cy + 28, strip_y, 95, val_str, NV_SWAP16(NV_COL_WHITE));

            /* Bottom level bar gauge */
            int32_t bar_w = ((cw - 8) * val) / 127;
            if (bar_w > 0) {
                nv_vec_line(buf, w, h, cx + 4, cy + ch - 6 - strip_y, cx + 4 + bar_w, cy + ch - 6 - strip_y, col);
                nv_vec_line(buf, w, h, cx + 4, cy + ch - 5 - strip_y, cx + 4 + bar_w, cy + ch - 5 - strip_y, col);
            }
        }
    }
}
