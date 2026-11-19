/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Nick Culbertson */
/* games3.h: DARK (was LIGHTS: Nick, 17 Nov 2026) for Mighty Arcade's menu (SUDOKU and SEA BATTLE were cut, 17 Nov 2026). Included once from app.c, after its
 * helpers. Their state lives in the shared arena (g_arena), one game at a time.
 *
 *  DARK        press a light: it and its neighbours flip. Turn them all off. The grid is a step sequencer: a playhead
 *              sweeps the columns and the lights still on sound, so the puzzle is quieting a melody down to silence
 */

/* ================================================================ LIGHTS === */
enum { NL_PRESS, NL_SEQ, NL_WIN, NL_PAD };
typedef struct {
    uint8_t on[25];
    int8_t cx, cy;
    uint16_t moves, level, par;          /* par: the presses that made the board (it can be solved in that many) */
    uint8_t won, was[25];                /* was: a light's state before its last flip (for the flip's turn) */
    uint32_t won_at, flash_t[25], flip_at[25];   /* flip_at: when its flip turns (ms; a ripple: the press first) */
} li_t;
#define li (*(li_t *)g_arena)
_Static_assert(sizeof(li_t) <= sizeof g_arena, "li: the arena is too small");
static int li_note(int i) { return h_tone(i % 5 + (4 - i / 5) + 2); }   /* a light's note: a tone of the chord now */
static void li_flip(int i)
{
    static const int8_t D[5][2] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int k, r = i / 5, c = i % 5;
    for (k = 0; k < 5; k++) {
        int rr = r + D[k][1], cc = c + D[k][0];
        if (rr >= 0 && rr < 5 && cc >= 0 && cc < 5)
            li.on[rr * 5 + cc] ^= 1u;
    }
}
static void li_level(void)
{
    int k, n = 3 + (int)li.level;
    uint8_t pressed[25] = {0};
    for (k = 0; k < 25; k++)
        li.on[k] = 0, li.flip_at[k] = 0;
    for (k = 0; k < n; k++) {            /* presses from dark: always solvable, in as many as were pressed (twice on
                                          * one undoes it) */
        int i = (int)(kr() % 25u);
        li_flip(i);
        pressed[i] ^= 1u;
    }
    for (k = 0; k < 25 && !li.on[k]; k++)
        ;
    if (k == 25)
        li_flip(12), pressed[12] ^= 1u;
    li.par = 0;
    for (k = 0; k < 25; k++)
        li.par += pressed[k];
    li.moves = 0;
    li.won = 0;
}
static void li_new(void)
{
    li.level = 0;
    li.cx = li.cy = 2;
    li_level();
}
static void li_nav(int dx, int dy, int sel)
{
    int i, k, any = 0;
    if (li.won) {
        if (sel && plat_ms() - li.won_at > 1200u) {
            li.level++;
            li_level();
        }
        return;
    }
    if (dx || dy) {
        li.cx = (int8_t)clamp(li.cx + dx, 0, 4);
        li.cy = (int8_t)clamp(li.cy - dy, 0, 4);
    }
    if (!sel)
        return;
    i = li.cy * 5 + li.cx;
    {                                    /* the flip ripples out: the press, then its neighbours, each turning over
                                          * with its note (a light going out, softer and lower) */
        static const int8_t D[5][2] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        uint32_t ms = plat_ms();
        for (k = 0; k < 5; k++) {
            int rr = i / 5 + D[k][1], cc = i % 5 + D[k][0], j = rr * 5 + cc;
            if (rr < 0 || rr > 4 || cc < 0 || cc > 4)
                continue;
            li.was[j] = li.on[j];
            li.flip_at[j] = (ms + (k ? 70u : 0u)) | 1u;
            k_on(k_at(k ? 0.07f : 0.0f), li_note(j) - (li.on[j] ? 12 : 0), li.on[j] ? 54u : (k ? 80u : 100u),
                 NL_PRESS, 0.4f);
        }
        juice_ring(22 + li.cx * 40 + 18, 26 + li.cy * 40 + 18, 34, C_YEL, 240);
        juice_kick(0, 1);
    }
    li_flip(i);
    li.moves++;
    for (k = 0; k < 25; k++)
        any |= li.on[k];
    if (!any) {                          /* all dark: the melody resolves, the key's own chord, rising, then whole */
        uint32_t ms = plat_ms();
        li.won = 1;
        li.won_at = ms;
        h_reset();
        for (k = 0; k < 5; k++)
            k_on(k_at(0.25f + 0.11f * (float)k), h_note(k) + 12, 92, NL_WIN, 1.6f);
        h_pad(k_at(0.25f + 0.11f * 5.0f), 70, NL_PAD, 4.0f, 0);
        juice_flash(RGB(255, 255, 255), 150, 400);
        juice_shake(4, 300);
        juice_confetti(120, 110, 50);
        juice_ring(120, 120, 150, C_YEL, 600);
        juice_pop(120, 70, li.moves < li.par ? "UNDER PAR!" : li.moves == li.par ? "PAR!" : "ALL DARK!", C_YEL);
        for (k = 0; k < 25; k++)          /* a wave across the board, corner to corner */
            li.was[k] = 1, li.flip_at[k] = (ms + 120u + 45u * (uint32_t)(k % 5 + k / 5)) | 1u;
    }
}
/* the grid, a step sequencer: a column an eighth note, the lights on in it sound */
/* the music, ambient (Nick, 17 Nov 2026: as the others are): a slow chord every two bars, soft and long; a playhead
 * walks the five columns, a quarter each, and a lit light there may sound (one or two, by chance), long and quiet,
 * in a big room: quieting the board quiets the music, down to the chord alone */
static void li_groove(uint32_t t, uint32_t step)
{
    int c = (int)((step / 4u) % 5u), r, n = 0;
    if (step % 32u == 0u) {              /* a new chord every two bars: its voicing, slowly spread */
        h_deg_at(step / 32u);
        h_pad(t, 30, NL_PAD, 4.0f, 0);
    }
    if (step % 4u || li.won)
        return;
    for (r = 0; r < 5 && n < 2; r++)
        if (li.on[r * 5 + c] && kr() % 100u < 60u) {
            k_on(t + (uint32_t)n * (uint32_t)(SR_F * 0.09f), li_note(r * 5 + c), 26u + kr() % 18u, NL_SEQ, 1.8f);
            li.flash_t[r * 5 + c] = t;
            n++;
        }
}
static void li_sounds(void)
{
    k_slot_set(NL_PRESS, &KSND[S_MARIMBA]);
    k_slot_set(NL_SEQ, &KSND[S_DREAM]);
    k_slot_set(NL_WIN, &KSND[S_TUBE]);
    k_slot_set(NL_PAD, &KSND[S_GLASSPAD]);
    k_slot_rel(NL_SEQ, 1.4f);
    k_main.rev_mix = 0.5f, k_main.rev_secs = 8.0f, k_main.shimmer = 0.04f;   /* a big room, the keys hanging in it */
    k_main.dly_mix = 0.25f, k_main.dly_fb = 0.45f;
    k_slot_rel(NL_WIN, 1.2f);
    k_slot_rel(NL_PAD, 1.4f);
    k_main_slot = NL_SEQ;
}
static void li_scene(void)
{
    int i;
    char t[40], n[8];                    /* ("LEVEL 12  MOVES 123 / PAR 14": was 24, too small: GPT's review,
                                          * 19 Nov 2026) */
    uint32_t now = k_now();
    tcpy(t, "LEVEL ");
    itos((int32_t)li.level + 1, n);
    tcat(t, n);
    tcat(t, "  MOVES ");
    itos(li.moves, n);
    tcat(t, n);
    tcat(t, " / PAR ");
    itos(li.par, n);
    tcat(t, n);
    header("DARK", C_CYAN, "", C_CYAN, t);
    for (i = 0; i < 25; i++) {
        int32_t x = 22 + (i % 5) * 40, y = 26 + (i / 5) * 40;
        uint32_t ft = li.flash_t[i];
        uint32_t fq = plat_ms() - li.flip_at[i];
        int on = li.on[i], h = 36;
        uint16_t c, lit_c = deg_col(i % 5 + 2 * (4 - i / 5));
        if (li.flip_at[i] && (int32_t)(plat_ms() - li.flip_at[i]) < 0)
            on = li.was[i];              /* (its ripple hasn't reached it yet) */
        else if (li.flip_at[i] && fq < 180u) {   /* turning over: squashed flat at the middle, the new face after */
            float p = (float)fq / 180.0f, cs = fm_cosf(p * 3.1415927f);
            h = (int)(36.0f * (cs < 0.0f ? -cs : cs));
            if (h < 2)
                h = 2;
            on = p < 0.5f ? li.was[i] : li.on[i];
            if (li.won)                  /* (the winning wave: a flash of the light it was) */
                on = p < 0.5f;
        }
        c = on ? lit_c : RGB(26, 19, 46);
        if (on && ft && (int32_t)(now - ft) >= 0 && now - ft < 8000u)
            c = mix565(c, C_INK, 160 - (int32_t)((now - ft) / 50u));
        cv_rect(x, y + (36 - h) / 2, 36, h, c);
        if (on && h > 20)
            cv_rect(x + 4, y + (36 - h) / 2 + 4, 8, 4, mix565(c, C_INK, 120));
    }
    if (!li.won) {
        int32_t x = 22 + li.cx * 40, y = 26 + li.cy * 40;
        frame_rect(x - 2, y - 2, 40, 40, C_YEL);
        frame_rect(x - 3, y - 3, 42, 42, C_YEL);
    } else {
        cv_rect(40, 96, 160, 50, C_BG);
        frame_rect(40, 96, 160, 50, C_YEL);
        tcpy(t, "MOVES ");
        itos(li.moves, n);
        tcat(t, n);
        tcat(t, "  PAR ");
        itos(li.par, n);
        tcat(t, n);
        text_c(120, 100, &FONT_M, "ALL DARK!", C_YEL);
        text_c(120, 122, &FONT_T, t, li.moves <= li.par ? C_YEL : C_INK);
        text_c(120, 134, &FONT_T, "TOP KEY: NEXT LEVEL", C_GHOST);
    }
    text_c(120, 228, &FONT_T, "TOP KEY FLIPS A LIGHT AND NEIGHBOURS", C_GHOST);
}
