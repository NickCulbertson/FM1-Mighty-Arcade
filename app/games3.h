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
    uint16_t moves, level;
    uint8_t won;
    uint32_t won_at, flash_t[25];
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
    for (k = 0; k < 25; k++)
        li.on[k] = 0;
    for (k = 0; k < n; k++)              /* presses from dark: always solvable */
        li_flip((int)(kr() % 25u));
    for (k = 0; k < 25 && !li.on[k]; k++)
        ;
    if (k == 25)
        li_flip(12);
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
    li_flip(i);
    li.moves++;
    k_on(0, li_note(i), 96, NL_PRESS, 0.4f);   /* the press, its note; the ones it lit, theirs */
    for (k = 0; k < 25; k++)
        any |= li.on[k];
    if (!any) {
        li.won = 1;
        li.won_at = plat_ms();
        for (k = 0; k < 6; k++)
            k_on(k_at(0.3f + 0.09f * (float)k), degree_note(7 + 2 * k), 100, NL_WIN, 1.2f);
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
    char t[24], n[8];
    uint32_t now = k_now();
    tcpy(t, "LEVEL ");
    itos((int32_t)li.level + 1, n);
    tcat(t, n);
    tcat(t, "  MOVES ");
    itos(li.moves, n);
    tcat(t, n);
    header("DARK", C_CYAN, "", C_CYAN, t);
    for (i = 0; i < 25; i++) {
        int32_t x = 22 + (i % 5) * 40, y = 26 + (i / 5) * 40;
        uint32_t ft = li.flash_t[i];
        uint16_t c = li.on[i] ? deg_col(i % 5 + 2 * (4 - i / 5)) : RGB(26, 19, 46);
        if (li.on[i] && ft && (int32_t)(now - ft) >= 0 && now - ft < 8000u)
            c = mix565(c, C_INK, 160 - (int32_t)((now - ft) / 50u));
        cv_rect(x, y, 36, 36, c);
        if (li.on[i])
            cv_rect(x + 4, y + 4, 8, 4, mix565(c, C_INK, 120));
    }
    if (!li.won) {
        int32_t x = 22 + li.cx * 40, y = 26 + li.cy * 40;
        frame_rect(x - 2, y - 2, 40, 40, C_YEL);
        frame_rect(x - 3, y - 3, 42, 42, C_YEL);
    } else {
        cv_rect(40, 96, 160, 50, C_BG);
        frame_rect(40, 96, 160, 50, C_YEL);
        text_c(120, 100, &FONT_M, "ALL DARK!", C_YEL);
        text_c(120, 128, &FONT_T, "TOP KEY: NEXT LEVEL", C_GHOST);
    }
    text_c(120, 228, &FONT_T, "TOP KEY FLIPS A LIGHT AND NEIGHBOURS", C_GHOST);
}
