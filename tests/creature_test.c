/* SPDX-License-Identifier: GPL-3.0-only */
/* NOICE VOICE: Creature Engines Test (HUMAN, TROLL, ELF, BEAST) */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("creature: %-70s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("creature_test: verifying 4 vocal creature engines...\n");

    /* 1. Verify engine roster */
    check("NENG_SHOWN is 4", NENG_SHOWN == 4u);
    check("ENGINES[5] is HUMAN", ENGINES[5] == &ENG_HUMAN && strcmp(ENG_HUMAN.name, "HUMAN") == 0);
    check("ENGINES[6] is TROLL", ENGINES[6] == &ENG_TROLL && strcmp(ENG_TROLL.name, "TROLL") == 0);
    check("ENGINES[7] is ELF", ENGINES[7] == &ENG_ELF && strcmp(ENG_ELF.name, "ELF") == 0);
    check("ENGINES[8] is BEAST", ENGINES[8] == &ENG_BEAST && strcmp(ENG_BEAST.name, "BEAST") == 0);

    /* 2. Verify visibility and ordering */
    check("eng_vis(0) is HUMAN (5)", eng_vis(0) == 5u);
    check("eng_vis(1) is TROLL (6)", eng_vis(1) == 6u);
    check("eng_vis(2) is ELF (7)", eng_vis(2) == 7u);
    check("eng_vis(3) is BEAST (8)", eng_vis(3) == 8u);

    check("eng_ok(0) is false (legacy ANALOG masked)", !eng_ok(0));
    check("eng_ok(5) is true (HUMAN)", eng_ok(5));
    check("eng_ok(6) is true (TROLL)", eng_ok(6));
    check("eng_ok(7) is true (ELF)", eng_ok(7));
    check("eng_ok(8) is true (BEAST)", eng_ok(8));
    check("eng_ok(10) is false (legacy DRUM masked)", !eng_ok(10));
    check("eng_ok(12) is false (legacy FM6 masked)", !eng_ok(12));

    /* 3. Verify presets */
    check("HUMAN has 8 presets", ENG_HUMAN.npresets == 8u);
    check("TROLL has 8 presets", ENG_TROLL.npresets == 8u);
    check("ELF has 8 presets", ENG_ELF.npresets == 8u);
    check("BEAST has 8 presets", ENG_BEAST.npresets == 8u);

    /* 4. Audio rendering test on each creature */
    for (uint32_t c = 5; c <= 8; c++) {
        memset(trk, 0, sizeof trk);
        host_tracks_init();
        host_preset(&trk[0], c, 0);
        trk[0].p[P_ATK] = 0;
        trk[0].p[P_SUS] = 127;
        trk[0].p[P_REL] = 10;
        trk[0].p[P_VOICE] = V_MONO;

        trk_note_on(&trk[0], 60, 100); /* Middle C */

        /* Render 0.5 seconds */
        int32_t buf[256];
        double energy = 0.0;
        int max_val = 0;
        for (int b = 0; b < 100; b++) {
            track_render(&trk[0], buf, 256);
            for (int s = 0; s < 256; s++) {
                int32_t v = buf[s];
                if (v < 0) v = -v;
                if (v > max_val) max_val = v;
                energy += (double)v * (double)v;
            }
        }
        trk_note_off(&trk[0], 60);

        char desc[80];
        snprintf(desc, sizeof(desc), "Engine %s renders audio (peak %d, RMS > 0)", ENGINES[c]->name, max_val);
        check(desc, max_val > 1000 && max_val < 100000);
    }

    /* 5. Verify default part allocation: Part 1=HUMAN, 2=TROLL, 3=ELF, 4=BEAST */
    check("Part 1 default is HUMAN (5)", trk_def_engine(0) == 5u);
    check("Part 2 default is TROLL (6)", trk_def_engine(1) == 6u);
    check("Part 3 default is ELF (7)", trk_def_engine(2) == 7u);
    check("Part 4 default is BEAST (8)", trk_def_engine(3) == 8u);

    /* 6. Verify articulatory envelope and preset application */
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(&trk[0], 5, 0); /* HUMAN Preset 0 */
    tract_preset_apply(&trk[0], 0);
    check("Preset apply sets P_END_TONGUE", trk[0].p[P_END_TONGUE] == 85);
    check("Preset apply sets P_END_JAW", trk[0].p[P_END_JAW] == 55);
    check("Preset apply sets P_ENV_SPEED", trk[0].p[P_ENV_SPEED] == 50);
    check("Preset apply sets P_WALL_Q", trk[0].p[P_WALL_Q] == 64);
    check("Preset apply sets P_TRACHEA", trk[0].p[P_TRACHEA] == 30);

    /* Test envelope morphing during a note */
    trk[0].p[P_E0] = 0;   /* Start Tongue = 0 */
    trk[0].p[P_END_TONGUE] = 127; /* End Tongue = 127 */
    trk[0].p[P_ENV_SPEED] = 10;   /* fast speed ~5ms */
    trk[0].p[P_ENV_MODE] = 0;    /* GLIDE */
    trk_note_on(&trk[0], 60, 100);

    tract_t *tr = tr_of(&trk[0]);
    int32_t buf[256];
    /* Render first 32 samples */
    track_render(&trk[0], buf, 32);
    check("Morph begins: cur_tongue > 0", tr->artic.cur_tongue > 0);

    /* Render more samples to finish morph */
    for (int b = 0; b < 20; b++)
        track_render(&trk[0], buf, 256);
    check("GLIDE completes: cur_tongue reaches target 127", tr->artic.cur_tongue == 127);
    trk_note_off(&trk[0], 60);

    if (bad) {
        printf("creature_test: FAILED with %d errors!\n", bad);
        return 1;
    }
    printf("creature_test: ALL TESTS PASSED!\n");
    return 0;
}
