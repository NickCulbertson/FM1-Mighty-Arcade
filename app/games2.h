/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Nick Culbertson */
/* games2.h: three score games for Mighty Arcade's menu, each a sequencer of its own. Included once from app.c, after its
 * helpers (the colours, header(), degree_note(), cube()).
 *
 *  SNAKE  a sequencer: the snake moves on the groove's eighth notes and, on each, its body plays (tail to head) the
 *         notes it has eaten: the longer it grows, the longer its melody, the quicker the beat, and the more of the
 *         drums. Its knobs: TEMPO (where it starts), CHANCE, SOUND, scale, key
 *  DEFUSE (was MINES, renamed 17 Nov 2026; its mines drawn as bombs)  12 x 9, 16 mines: A reveals, A held flags; a square sounds its number, an open field sweeps out as a run.
 *         A sequencer too: a playhead walks the columns, an eighth each, and each plays its highest number opened
 *         (1 low .. 8 high): the field you clear is the tune, and the drums come in as you clear it
 *  2048   slide the tiles; equal ones merge and climb the scale, several merges at once a chord. A sequencer too: a
 *         playhead sweeps the four columns twice a bar, each one's tiles a chord (the bigger, the higher); the
 *         bigger your best tile, the more of the drums
 * Each has: X_sounds, X_new, X_nav (dx, dy, sel), X_tick, X_groove (t, step), X_scene, and its score and its end. */

/* ================================================================= SNAKE === */
enum { NS_HAT, NS_BASS, NS_BODY, NS_EAT, NS_DIE, NS_KICK, NS_SNARE };   /* NS_BODY: the playhead's (K3) */
static uint16_t deg_col(int d);
#define SN_W 15
#define SN_H 13
#define SN_C 16
#define SN_Y 24
typedef struct {
    int8_t x[SN_W * SN_H], y[SN_W * SN_H];
    int8_t deg[SN_W * SN_H];             /* each piece of it: a note (a scale degree) */
    int16_t len;
    int8_t dx, dy, ndx, ndy;             /* where it goes, and where it will go next step */
    int8_t fx, fy, fdeg;                 /* the food, and its note */
    uint32_t next_t;                     /* the next step (sample) */
    uint32_t beat;                       /* steps taken: which piece sings */
    uint8_t over, started;
    uint32_t over_at;
    int8_t ox[SN_W * SN_H], oy[SN_W * SN_H];   /* where each piece was a step ago: drawn sliding between */
    uint32_t ate_at;                     /* a bite (ms): it swallows, the lump goes down it */
    int16_t dead_n;                      /* the end: pieces burst head to tail */
    uint32_t mq[4], start_t;             /* its moves, as the groove schedules them (samples); the first may be */
    uint8_t mq_w, mq_r, tier;
} sn_t;
static int16_t sn_best;
#define sn (*(sn_t *)g_arena)   /* in the games' shared memory (app.c) */
_Static_assert(sizeof(sn_t) <= sizeof g_arena, "sn: the arena is too small");
static uint32_t sn_bpm(void) { return (uint32_t)sq_cfg[SQ_SNAKE].bpm + 3u * (uint32_t)(sn.len > 3 ? (sn.len - 3 > 25 ? 25 : sn.len - 3) : 0); }
static uint32_t sn_step(void) { return (uint32_t)(SR_F * 30.0f / (float)sn_bpm()); }   /* an eighth note */
static void sn_food(void)
{
    int tries, i, ok;
    for (tries = 0; tries < 400; tries++) {
        sn.fx = (int8_t)(kr() % SN_W);
        sn.fy = (int8_t)(kr() % SN_H);
        for (i = 0, ok = 1; i < sn.len; i++)
            if (sn.x[i] == sn.fx && sn.y[i] == sn.fy)
                ok = 0;
        if (ok)
            break;
    }
    sn.fdeg = (int8_t)(kr() % 10u);
}
static void sn_new(void)
{
    int i;
    sn.len = 3;
    for (i = 0; i < 3; i++) {
        sn.x[i] = (int8_t)(5 - i);
        sn.y[i] = 6;
        sn.deg[i] = (int8_t)(i * 2);
    }
    sn.dx = sn.ndx = 1;
    sn.dy = sn.ndy = 0;
    sn.over = 0;
    sn.started = 1;
    sn.beat = 0;
    sn.ate_at = 0;
    sn.dead_n = 0;
    for (i = 0; i < 3; i++)
        sn.ox[i] = sn.x[i], sn.oy[i] = sn.y[i];
    sn_food();
    sn.next_t = sn.start_t = k_at(0.6f);
    sn.mq_w = sn.mq_r = 0;
    sn.tier = 0;
}
static void sn_nav(int dx, int dy, int sel)
{
    (void)sel;
    if (sn.over)
        return;
    dy = -dy;                            /* (up is up the screen) */
    if (dx && sn.dx == 0)
        sn.ndx = (int8_t)dx, sn.ndy = 0;
    if (dy && sn.dy == 0)
        sn.ndx = 0, sn.ndy = (int8_t)dy;
}
static void sn_tick(void)
{
    int i, hx, hy, ate;
    uint32_t mt;
    if (sn.over || sn.mq_r == sn.mq_w || (int32_t)(k_now() - sn.mq[sn.mq_r % 4u]) < 0)
        return;                          /* (it moves as the groove plays its eighth) */
    mt = sn.mq[sn.mq_r++ % 4u];
    sn.next_t = mt + sn_step();
    sn.dx = sn.ndx, sn.dy = sn.ndy;
    hx = sn.x[0] + sn.dx, hy = sn.y[0] + sn.dy;
    ate = hx == sn.fx && hy == sn.fy;
    for (i = 0; i < sn.len - (ate ? 0 : 1); i++)
        if (sn.x[i] == hx && sn.y[i] == hy)
            hx = -1;                     /* into itself */
    if (hx < 0 || hx >= SN_W || hy < 0 || hy >= SN_H) {
        sn.over = 1;
        sn.over_at = plat_ms();
        for (i = 0; i < sn.len; i++)     /* (it stops where it is) */
            sn.ox[i] = sn.x[i], sn.oy[i] = sn.y[i];
        if (sn.len - 3 > sn_best)
            sn_best = (int16_t)(sn.len - 3);
        juice_freeze(220);
        juice_shake(10, 700);
        juice_flash(RGB(255, 40, 80), 180, 600);
        juice_ring(sn.x[0] * SN_C + SN_C / 2, SN_Y + sn.y[0] * SN_C + SN_C / 2, 80, C_PINK, 500);
        juice_glitch(500);
        juice_cool();
        for (i = 0; i < 4; i++)
            k_on(k_at(0.15f * (float)i), degree_note(7 - 2 * i), 90, NS_DIE, 0.6f);
        return;
    }
    for (i = 0; i < sn.len; i++)         /* where they were */
        sn.ox[i] = sn.x[i], sn.oy[i] = sn.y[i];
    if (ate && sn.len < SN_W * SN_H) {
        sn.len++;
        sn.ox[sn.len - 1] = sn.x[sn.len - 2], sn.oy[sn.len - 1] = sn.y[sn.len - 2];
    }
    for (i = sn.len - 1; i > 0; i--) {   /* the body follows; its notes stay where they were eaten */
        sn.x[i] = sn.x[i - 1];
        sn.y[i] = sn.y[i - 1];
    }
    sn.x[0] = (int8_t)hx, sn.y[0] = (int8_t)hy;
    if (ate) {
        for (i = sn.len - 1; i > 0; i--)
            sn.deg[i] = sn.deg[i - 1];
        sn.deg[0] = sn.fdeg;
        k_on(0, degree_note(sn.fdeg + 9), 110, NS_EAT, 0.5f);
        {
            int32_t fx = sn.fx * SN_C + SN_C / 2, fy = SN_Y + sn.fy * SN_C + SN_C / 2, e = sn.len - 3;
            juice_burst(fx, fy, 14, 3, deg_col(sn.fdeg), C_INK, 0);
            juice_ring(fx, fy, 22, deg_col(sn.fdeg), 260);
            juice_shake(3, 140);
            juice_freeze(30);
            juice_heat(8 + (e > 20 ? 20 : e));   /* the longer you are, the hotter the music */
            juice_embers(fx, fy, 4, deg_col(sn.fdeg));
            sn.ate_at = plat_ms() | 1u;
            if (e % 5 == 0) {
                char es[12];
                itos(e, es);
                tcat(es, "!");
                juice_pop(120, 100, es, C_YEL);
                juice_confetti(fx, fy, 20);
                juice_impact(50);
                if (e % 10 == 0) {
                    juice_fireworks(1500);
                    juice_sting(2);
                }
            } else
                juice_text(fx, fy - 10, "+1", deg_col(sn.fdeg));
        }
        sn_food();
    }
}
static void sn_groove(uint32_t t, uint32_t step)
{
    uint32_t p = step % 16u, bar = step / 16u;
    int tier;
    if (sn.over)
        return;
    h_deg_at(bar);                       /* a new chord each bar, at random in the key */
    if (p == 0u || p == 8u)
        k_on(t, h_bass(), 72, NS_BASS, 0.2f);
    tier = sq_tier_of(sn.len - 3, 24);   /* the drums, as it grows */
    sq_tier_say(tier, &sn.tier, 200);
    sq_drums(t, p, tier, NS_HAT, NS_KICK, NS_SNARE, 85);
    if (!(p & 1u) && (int32_t)(t - sn.start_t) >= 0) {   /* an eighth: it moves, and its body sings */
        int i = (int)(sn.beat % (uint32_t)sn.len);
        if ((uint8_t)(sn.mq_w - sn.mq_r) < 4u)
            sn.mq[sn.mq_w++ % 4u] = t;
        if (sq_roll(SQ_SNAKE))           /* the body, a piece a step, tail to head: its melody */
            k_on(t, degree_note(sn.deg[sn.len - 1 - i] + 7), 84, NS_BODY, 0.25f);
        sn.beat++;
    }
}
static void sn_sounds(void)
{
    k_slot_set(NS_HAT, &KSND[S_HAT]);
    k_slot_set(NS_BASS, &KSND[S_FMBASS]);
    k_slot_set(NS_BODY, &KSND[S_CHIP]);
    k_slot_set(NS_EAT, &KSND[S_CRYSTAL]);
    k_slot_set(NS_DIE, &KSND[S_SONAR]);
    k_slot_set(NS_KICK, &KSND[S_KICK]);
    k_slot_set(NS_SNARE, &KSND[S_SNARE]);
    sq_slot = NS_BODY;
    sq_sound(SQ_SNAKE);
    k_slot_rel(NS_BASS, 0.15f);
    k_slot_rel(NS_BODY, 0.2f);
    k_slot_rel(NS_EAT, 0.6f);
    k_main_slot = NS_BODY;
}
static uint16_t deg_col(int d)
{
    static const uint16_t C[10] = {RGB(95, 227, 238), RGB(110, 240, 140), RGB(255, 210, 63), RGB(255, 150, 60),
                                   RGB(255, 46, 151), RGB(176, 124, 255), RGB(90, 140, 255), RGB(95, 227, 238),
                                   RGB(110, 240, 140), RGB(255, 210, 63)};
    return C[(d % 10 + 10) % 10];
}
static void sn_scene(void)
{
    int i;
    char n[16];
    int sing = sn.len ? sn.len - 1 - (int)((sn.beat + (uint32_t)sn.len - 1u) % (uint32_t)sn.len) : -1;
    uint32_t ms = plat_ms();
    itos(sn.len - 3, n);
    tcat(n, " EATEN");
    header("WORM", C_CYAN, "", C_PINK, n);
    {                                    /* the desert at dusk: sand, its ripples drifting in the wind, a few stones */
        int32_t y, x;
        cv_rect(0, SN_Y, SN_W * SN_C, SN_H * SN_C, RGB(92, 60, 42));
        for (y = 0; y < SN_H * SN_C; y += 6) {
            int32_t ph = (int32_t)(ms / 70u) + y * 7;
            for (x = 0; x < SN_W * SN_C; x += 4) {
                int32_t w = (int32_t)(fm_sinf((float)(x + ph) * 0.05f + (float)y * 0.31f) * 2.0f);
                cv_rect(x, SN_Y + y + w, 4, 1, (y / 6) & 1 ? RGB(120, 82, 56) : RGB(108, 72, 50));
            }
        }
        for (i = 0; i < 9; i++) {
            uint32_t h = (uint32_t)(i + 3) * 2654435761u;
            int32_t sx = (int32_t)(h >> 8 & 255u) % (SN_W * SN_C - 8), sy = SN_Y + (int32_t)(h >> 16 & 255u) % (SN_H * SN_C - 6);
            disc(sx + 1, sy + 1, 2, RGB(70, 44, 32));
            disc(sx, sy, 2, RGB(140, 104, 76));
        }
    }
    {                                    /* the food: a glowing seed, half buried, breathing; a sparkle on it */
        int32_t fx = sn.fx * SN_C + SN_C / 2, fy = SN_Y + sn.fy * SN_C + SN_C / 2 + (int32_t)(fm_sinf((float)ms * 0.008f) * 2.0f);
        int32_t r = 5 + (int32_t)((ms / 120u) % 4u == 0u);
        disc(fx, fy + 5, 5, RGB(70, 44, 32));
        disc(fx, fy, r + 2, mix565(RGB(92, 60, 42), deg_col(sn.fdeg), 90));
        disc(fx, fy, r, deg_col(sn.fdeg));
        disc(fx - 2, fy - 2, 1, C_INK);
        if ((ms / 90u) % 12u < 3u) {
            cv_rect(fx + 3, fy - 7, 1, 5, C_INK);
            cv_rect(fx + 1, fy - 5, 5, 1, C_INK);
        }
    }
    {
        uint32_t now = k_now(), st = sn_step();
        float f = sn.over ? 1.0f : 1.0f - (float)(int32_t)(sn.next_t - now) / (float)st;
        int32_t lump = sn.ate_at ? (int32_t)((plat_ms() - sn.ate_at) * 1000u / (st * 1000u / 44u)) : 999;
        uint32_t dq = sn.over ? plat_ms() - sn.over_at : 0;
        int16_t px[SN_W * SN_H], py[SN_W * SN_H];
        int32_t nseg = sn.len, chew = (int32_t)(juice_pulse() / 64);
        if (f < 0.0f)
            f = 0.0f;
        if (f > 1.0f)
            f = 1.0f;
        for (i = 0; i < nseg; i++) {     /* where each ring is, sliding between steps */
            px[i] = (int32_t)(((float)sn.ox[i] + (float)(sn.x[i] - sn.ox[i]) * f) * SN_C) + SN_C / 2;
            py[i] = SN_Y + (int32_t)(((float)sn.oy[i] + (float)(sn.y[i] - sn.oy[i]) * f) * SN_C) + SN_C / 2;
        }
        for (i = nseg - 1; i >= 0; i--) {   /* the burrow it leaves: churned sand under it */
            if (sn.over && dq > 250u && (int32_t)dq >= 250 + i * 50)
                continue;
            if (i + 1 < nseg && (px[i] - px[i + 1]) * (px[i] - px[i + 1]) + (py[i] - py[i + 1]) * (py[i] - py[i + 1]) < 400)
                disc((px[i] + px[i + 1]) / 2, (py[i] + py[i + 1]) / 2 + 2, 6, RGB(66, 42, 30));
            disc(px[i], py[i] + 2, 7, RGB(66, 42, 30));
        }
        for (i = nseg - 1; i >= 0; i--) {   /* the rings, tail to head: thick, tapering, each banded with its note */
            int32_t r = i == 0 ? 8 : 7 - (i > nseg - 4 ? (i - (nseg - 4)) : 0);
            uint16_t skin = (i & 1) ? RGB(196, 140, 110) : RGB(176, 122, 96);
            uint16_t band = i == sing ? C_INK : deg_col(sn.deg[i]);
            int32_t g = i == lump ? 2 : i == sing ? 1 : 0;   /* the lump going down; the ring singing, a size up */
            if (r < 3)
                r = 3;
            if (sn.over && dq > 250u) {  /* the end: it bursts, head to tail */
                int32_t at = 250 + i * 50;
                if ((int32_t)dq >= at) {
                    if ((int32_t)dq < at + 34)
                        juice_burst(px[i], py[i], 2, 3, RGB(196, 140, 110), band, 1);
                    continue;
                }
            }
            if (i + 1 < nseg && (px[i] - px[i + 1]) * (px[i] - px[i + 1]) + (py[i] - py[i + 1]) * (py[i] - py[i + 1]) < 400)
                disc((px[i] + px[i + 1]) / 2, (py[i] + py[i + 1]) / 2, r - 1, mix565(skin, RGB(60, 36, 28), 60));
            disc(px[i], py[i], r + g, skin);
            disc(px[i], py[i], r + g - 3 > 1 ? r + g - 3 : 1, mix565(skin, band, 170));   /* its note, a band */
            disc(px[i] - 2, py[i] - 3, 1 + g / 2, mix565(skin, C_INK, 150));               /* the light on it */
            if (i == 0) {                /* the maw: round, a ring of teeth, opening on the beat */
                int32_t mx = px[0] + sn.dx * 3, my = py[0] + sn.dy * 3, mr = 4 + chew + (sn.over ? 0 : 0), t;
                disc(px[0], py[0], 9 + g, RGB(150, 96, 80));
                disc(mx, my, mr, RGB(40, 12, 20));
                disc(mx, my, mr - 2 > 0 ? mr - 2 : 1, RGB(90, 20, 40));
                for (t = 0; t < 8; t++) {
                    float a = (float)t * 0.7854f + (float)ms * 0.001f;
                    cv_rect(mx + (int32_t)(fm_cosf(a) * (float)(mr + 1)) - 1, my + (int32_t)(fm_sinf(a) * (float)(mr + 1)) - 1,
                            2, 2, RGB(250, 236, 200));
                }
                if (sn.over)
                    cv_rect(mx - 3, my, 7, 1, C_PINK);
                else if ((ms / 50u) % 3u == 0u && sn.started)   /* sand thrown up as it ploughs on */
                    juice_burst(px[0] + sn.dx * 9, py[0] + sn.dy * 9, 1, 1, RGB(200, 156, 104), RGB(150, 110, 76), 1);
            }
        }
    }
    if (sn.over && plat_ms() - sn.over_at > 250u + (uint32_t)sn.len * 50u) {
        char bs[24], bn[8];
        cv_rect(40, 90, 160, 62, C_BG);
        frame_rect(40, 90, 160, 62, C_YEL);
        text_c(120, 94, &FONT_M, "THE END", C_YEL);
        itos(sn_best, bn);
        tcpy(bs, "BEST ");
        tcat(bs, bn);
        text_c(120, 120, &FONT_XS, bs, C_CYAN);
        text_c(120, 136, &FONT_T, "TOP KEY: AGAIN   HOME: MENU", C_GHOST);
    }
}

/* ================================================================= MINES === */
enum { NM_OPEN, NM_FLAG, NM_BOOM, NM_DRUM, NM_WIN, NM_PAD, NM_SEQ, NM_HAT, NM_SNARE };   /* NM_SEQ: the playhead's */
#define MI_W 12
#define MI_H 9
#define MI_C 19
#define MI_X 6
#define MI_Y 26
/* the fields get harder: clear one and the next has more mines (10, 13, 16 .. 31); a boom replays the same field */
static uint8_t mi_field = 1;             /* the field you're on (1 ..), kept until the FM-1 restarts */
#define MI_N (mi_field < 8u ? 7 + 3 * (int)mi_field : 31)
typedef struct {
    uint8_t mine[MI_H][MI_W], open[MI_H][MI_W], flag[MI_H][MI_W];
    int8_t cx, cy;
    uint8_t placed, over, won;
    uint16_t opened;
    uint32_t over_at, a_down;            /* a_down: when A went down (ms), 0 up */
    uint8_t a_flagged;                   /* this press flagged already */
    uint32_t ot[MI_H][MI_W];             /* when each square opens (ms): a reveal ripples out, in time with its notes */
    int8_t bx, by;                       /* the mine that went off */
    float fx, fy;                        /* the cursor, gliding */
    uint32_t start_at, flag_at;
    int8_t flx, fly;                     /* the flag just planted */
    uint32_t seq_ms[MI_W];               /* when the playhead's note in each column last sounded */
    uint8_t tier;
} mi_t;
static uint32_t mi_best;                 /* the quickest clear (ms) */
#define mi (*(mi_t *)g_arena)   /* in the games' shared memory (app.c) */
_Static_assert(sizeof(mi_t) <= sizeof g_arena, "mi: the arena is too small");
static int mi_count(int x, int y)
{
    int dx, dy, n = 0;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++) {
            int a = x + dx, b = y + dy;
            if (a >= 0 && a < MI_W && b >= 0 && b < MI_H && mi.mine[b][a])
                n++;
        }
    return n;
}
static void mi_new(void)
{
    int x, y;
    for (y = 0; y < MI_H; y++)
        for (x = 0; x < MI_W; x++)
            mi.mine[y][x] = mi.open[y][x] = mi.flag[y][x] = 0;
    mi.cx = MI_W / 2, mi.cy = MI_H / 2;
    mi.placed = mi.over = mi.won = 0;
    mi.opened = 0;
    mi.tier = 0;
    {
        int k;
        for (k = 0; k < MI_W; k++)
            mi.seq_ms[k] = 0;
    }
    mi.a_down = 0;
    mi.fx = (float)mi.cx, mi.fy = (float)mi.cy;
    mi.start_at = 0;
    mi.flag_at = 0;
}
static void mi_place(int sx, int sy)     /* after the first press: never under it, nor next to it */
{
    int n = 0;
    while (n < MI_N) {
        int x = (int)(kr() % MI_W), y = (int)(kr() % MI_H);
        if (mi.mine[y][x] || (x >= sx - 1 && x <= sx + 1 && y >= sy - 1 && y <= sy + 1))
            continue;
        mi.mine[y][x] = 1;
        n++;
    }
    mi.placed = 1;
}
static void mi_reveal(int sx, int sy)
{
    static int8_t qx[MI_W * MI_H], qy[MI_W * MI_H];
    int h = 0, t = 0, notes = 0;
    if (mi.open[sy][sx] || mi.flag[sy][sx])
        return;
    if (!mi.placed) {
        mi_place(sx, sy);
        mi.start_at = plat_ms();
    }
    if (mi.mine[sy][sx]) {               /* boom: every mine shows, and goes off, nearest first */
        int x, y;
        int32_t px = MI_X + sx * MI_C + MI_C / 2, py = MI_Y + sy * MI_C + MI_C / 2;
        mi.over = 1;
        mi.over_at = plat_ms();
        mi.bx = (int8_t)sx, mi.by = (int8_t)sy;
        juice_freeze(220);
        juice_shake(12, 900);
        juice_flash(RGB(255, 255, 255), 220, 500);
        juice_ring(px, py, 120, C_PINK, 600);
        juice_ring(px, py, 70, C_YEL, 400);
        juice_burst(px, py, 40, 6, C_PINK, C_YEL, 1);
        juice_glitch(900);
        juice_embers(px, py, 20, RGB(255, 120, 40));
        juice_cool();
        for (y = 0; y < MI_H; y++)
            for (x = 0; x < MI_W; x++)
                if (mi.mine[y][x])
                    mi.open[y][x] = 1;
        k_on(0, 33, 127, NM_DRUM, 0.5f);
        k_on(0, degree_note(-7), 110, NM_BOOM, 1.5f);
        k_on(k_at(0.2f), degree_note(-8), 90, NM_BOOM, 1.5f);
        return;
    }
    qx[t] = (int8_t)sx, qy[t++] = (int8_t)sy;
    mi.open[sy][sx] = 1;
    while (h < t) {                      /* opened outwards; each square sounds its number, in the order it opens */
        int x = qx[h], y = qy[h++], n = mi_count(x, y), dx, dy;
        mi.ot[y][x] = plat_ms() + (uint32_t)(notes < 24 ? notes : 24) * 30u + (uint32_t)(h > 24 ? (h - 24) * 6 : 0);
        mi.opened++;
        if (notes < 24) {
            k_on(k_at(0.03f * (float)notes), degree_note(n * 2 + 7), n ? 92 : 56, NM_OPEN, 0.3f);
            notes++;
        }
        if (n)
            continue;
        for (dy = -1; dy <= 1; dy++)
            for (dx = -1; dx <= 1; dx++) {
                int a = x + dx, b = y + dy;
                if (a >= 0 && a < MI_W && b >= 0 && b < MI_H && !mi.open[b][a] && !mi.flag[b][a]) {
                    mi.open[b][a] = 1;
                    qx[t] = (int8_t)a, qy[t++] = (int8_t)b;
                }
            }
    }
    if (t > 6) {                         /* a big opening: it whooshes */
        juice_ring(MI_X + sx * MI_C + MI_C / 2, MI_Y + sy * MI_C + MI_C / 2, 40 + t, C_CYAN, 400);
        juice_shake(2 + (t > 30 ? 3 : t / 10), 200);
        juice_heat(t / 2);
        if (t > 25) {
            juice_pop(120, 80, "NICE!", C_CYAN);
            juice_ring(MI_X + sx * MI_C + MI_C / 2, MI_Y + sy * MI_C + MI_C / 2, 120, RGB(255, 255, 255), 600);
        }
    }
    if (mi.opened == MI_W * MI_H - MI_N) {   /* every safe square: won */
        int i;
        uint32_t took = plat_ms() - mi.start_at;
        mi.over = mi.won = 1;
        mi.over_at = plat_ms();
        if (!mi_best || took < mi_best)
            mi_best = took;
        juice_flash(RGB(255, 255, 255), 160, 600);
        juice_shake(6, 500);
        juice_confetti(120, 100, 60);
        juice_ring(120, 110, 140, C_YEL, 700);
        juice_pop(120, 70, "CLEARED!", C_YEL);
        juice_celebrate(120, 110);
        for (i = 0; i < 8; i++)
            k_on(k_at(0.3f + 0.1f * (float)i), degree_note(7 + i), 100, NM_WIN, 1.2f);
    }
}
static void mi_nav(int dx, int dy, int sel)
{
    if (mi.over) {
        if (sel && plat_ms() - mi.over_at > 800u) {
            if (mi.won && mi_field < 99u)
                mi_field++, mi_best = 0;   /* (the best time is the field's) */
            mi_new();
            if (mi_field > 1u) {
                char f[16], n[6];
                itos((int32_t)mi_field, n);
                tcpy(f, "FIELD ");
                tcat(f, n);
                juice_pop(120, 100, f, C_CYAN);
            }
        }
        return;
    }
    if (dx || dy) {
        mi.cx = (int8_t)clamp(mi.cx + dx, 0, MI_W - 1);
        mi.cy = (int8_t)clamp(mi.cy - dy, 0, MI_H - 1);
    }
    if (sel) {                           /* A down: a tap reveals (on its release), a hold flags */
        mi.a_down = plat_ms() | 1u;
        mi.a_flagged = 0;
    }
}
static void mi_release(void)            /* A up */
{
    if (mi.a_down && !mi.a_flagged && !mi.over)
        mi_reveal(mi.cx, mi.cy);
    mi.a_down = 0;
}
static void mi_tick(void)
{
    mi.fx += ((float)mi.cx - mi.fx) * 0.45f;   /* the cursor glides */
    mi.fy += ((float)mi.cy - mi.fy) * 0.45f;
    if (mi.over && !mi.won) {            /* the other mines: one by one, nearest first */
        int x, y;
        uint32_t q = plat_ms() - mi.over_at;
        for (y = 0; y < MI_H; y++)
            for (x = 0; x < MI_W; x++)
                if (mi.mine[y][x] && !(x == mi.bx && y == mi.by)) {
                    int32_t d = (x - mi.bx) * (x - mi.bx) + (y - mi.by) * (y - mi.by);
                    uint32_t at = 350u + (uint32_t)fm_sqrtf((float)d) * 110u;
                    if (q >= at && q < at + 34u) {
                        int32_t px = MI_X + x * MI_C + MI_C / 2, py = MI_Y + y * MI_C + MI_C / 2;
                        juice_burst(px, py, 12, 3, C_PINK, C_YEL, 1);
                        juice_ring(px, py, 28, C_PINK, 300);
                        juice_shake(5, 200);
                        juice_kick(x < mi.bx ? -5 : 5, y < mi.by ? -5 : 5);   /* each blast pushes away from the first */
                        juice_embers(px, py, 3, RGB(255, 120, 40));
                        k_on(0, 33 + (x + y) % 5, 110, NM_DRUM, 0.3f);
                    }
                }
    }
    if (mi.a_down && !mi.a_flagged && !mi.over && (int32_t)(plat_ms() - mi.a_down) >= 420) {   /* (signed: a_down is
                                          * stamped | 1, a ms ahead at an even time; unsigned, that read as held for
                                          * ages and flagged at once, the flag Nick kept getting: 19 Nov 2026) */
        if (!mi.open[mi.cy][mi.cx]) {
            int32_t px = MI_X + mi.cx * MI_C + MI_C / 2, py = MI_Y + mi.cy * MI_C + MI_C / 2;
            int fx2, fy2, nf = 0;
            mi.flag[mi.cy][mi.cx] ^= 1u;
            for (fy2 = 0; fy2 < MI_H; fy2++)
                for (fx2 = 0; fx2 < MI_W; fx2++)
                    nf += mi.flag[fy2][fx2];
            /* the flags climb the chord: the nth flag its nth tone up (an arpeggio rising as the bombs are
             * marked); one taken back, the tone a step down (Nick, 19 Nov 2026) */
            k_on(0, h_tone(1 + nf % 10), mi.flag[mi.cy][mi.cx] ? 84u : 60u, NM_FLAG, 0.25f);   /* (two octaves up,
                                          * then round again: never shrill) */
            mi.flag_at = plat_ms() | 1u;
            mi.flx = mi.cx, mi.fly = mi.cy;
            if (mi.flag[mi.cy][mi.cx]) {
                juice_ring(px, py, 16, C_YEL, 220);
                juice_burst(px, py + 6, 6, 2, C_YEL, RGB(170, 160, 200), 0);
                juice_shake(2, 90);
            } else
                juice_burst(px, py, 5, 1, RGB(170, 160, 200), C_GHOST, 0);
        }
        mi.a_flagged = 1;
    }
}
static void mi_groove(uint32_t t, uint32_t step)
{
    uint32_t p = step % 16u;
    int tier;
    if (mi.over)
        return;
    if (step % 32u == 0u) {
        h_deg_at(step / 32u);
        h_pad(t, 34, NM_PAD, 3.0f, 0);
    }
    tier = sq_tier_of(mi.opened, MI_W * MI_H - MI_N);   /* the drums, as you clear the field */
    sq_tier_say(tier, &mi.tier, 220);
    sq_drums(t, p, tier, NM_HAT, NM_DRUM, NM_SNARE, 80);
    if (!(p & 1u)) {                     /* the playhead: a column an eighth, its highest number */
        int c = (int)(step / 2u % (uint32_t)MI_W), y, best = 0;
        for (y = 0; y < MI_H; y++)
            if (mi.open[y][c] && !mi.mine[y][c]) {
                int n = mi_count(c, y);
                if (n > best)
                    best = n;
            }
        if (best && sq_roll(SQ_MINES)) {
            k_on(t, h_tone(best + 1), 70u + (uint32_t)best * 5u, NM_SEQ, 0.35f);
            mi.seq_ms[c] = sq_ms(t);
        }
    }
}
static void mi_sounds(void)
{
    k_slot_set(NM_OPEN, &KSND[S_MARIMBA]);
    k_slot_set(NM_FLAG, &KSND[S_KOTO]);
    k_slot_set(NM_BOOM, &KSND[S_DRONE]);
    k_slot_set(NM_DRUM, &KSND[S_KICK]);
    k_slot_set(NM_WIN, &KSND[S_TUBE]);
    k_slot_set(NM_PAD, &KSND[S_GLASSPAD]);
    k_slot_set(NM_HAT, &KSND[S_HAT]);
    k_slot_set(NM_SNARE, &KSND[S_SNARE]);
    sq_slot = NM_SEQ;
    sq_sound(SQ_MINES);
    k_slot_rel(NM_OPEN, 0.3f);
    k_slot_rel(NM_BOOM, 1.5f);
    k_slot_rel(NM_WIN, 1.2f);
    k_slot_rel(NM_PAD, 1.4f);
    k_main_slot = NM_OPEN;
}
static void mi_scene(void)
{
    int x, y, flags = 0;
    char t[32], n[8];                    /* ("FIELD 12  16 BOMBS": 16 was too small, GPT's review 19 Nov 2026) */
    for (y = 0; y < MI_H; y++)
        for (x = 0; x < MI_W; x++)
            flags += mi.flag[y][x];
    itos((int32_t)mi_field, n);
    tcpy(t, "FIELD ");
    tcat(t, n);
    tcat(t, "  ");
    itos(MI_N - flags, n);
    tcat(t, n);
    tcat(t, " BOMBS");
    header("DEFUSE", C_CYAN, "", C_CYAN, t);
    for (y = 0; y < MI_H; y++)
        for (x = 0; x < MI_W; x++) {
            int32_t px = MI_X + x * MI_C, py = MI_Y + y * MI_C;
            uint32_t ms = plat_ms();
            int shut = !mi.open[y][x] || (int32_t)(ms - mi.ot[y][x]) < 0;
            if (mi.open[y][x] && mi.mine[y][x])
                shut = 0;
            if (shut) {                  /* closed: raised */
                cv_rect(px, py, MI_C - 1, MI_C - 1, RGB(70, 52, 120));
                cv_rect(px, py, MI_C - 1, 2, RGB(110, 86, 170));
                cv_rect(px, py + MI_C - 3, MI_C - 1, 2, RGB(48, 34, 86));
                if (mi.flag[y][x]) {     /* a flag: planted with a bounce, waving */
                    uint32_t fq = ms - mi.flag_at;
                    int32_t hop = x == mi.flx && y == mi.fly && fq < 200u ? (int32_t)(200u - fq) / 40 : 0;
                    int32_t wave = (int32_t)((ms / 160u + (uint32_t)(x + y)) & 1u);
                    uint16_t fc = mi.won ? ((ms / 100u) & 1u ? C_YEL : C_CYAN) : C_YEL;
                    cv_rect(px + 7, py + 4 - hop, 2, 11, C_INK);
                    cv_rect(px + 9, py + 4 - hop, 6 - wave, 5, fc);
                    cv_rect(px + 4, py + 14, 8, 2, RGB(48, 34, 86));
                }
            } else if (mi.mine[y][x]) {
                int32_t d = (x - mi.bx) * (x - mi.bx) + (y - mi.by) * (y - mi.by);
                uint32_t at = x == mi.bx && y == mi.by ? 0u : 350u + (uint32_t)fm_sqrtf((float)d) * 110u;
                int gone = !mi.won && ms - mi.over_at >= at;
                cv_rect(px, py, MI_C - 1, MI_C - 1, gone ? RGB(30, 6, 16) : RGB(60, 10, 30));
                if (gone) {              /* a crater, smoking */
                    disc(px + 9, py + 9, 4, RGB(14, 4, 8));
                    if ((ms / 140u + (uint32_t)x) % 3u == 0u)
                        cv_rect(px + 8, py + 2 - (int32_t)((ms / 50u) % 4u), 2, 2, RGB(110, 90, 120));
                } else {
                    int32_t pulse = (ms / 90u) & 1u;   /* a bomb, its fuse sparking */
                    disc(px + 8, py + 10, 6, RGB(20, 14, 30));
                    disc(px + 8, py + 10, 5, RGB(70, 62, 100));
                    disc(px + 6, py + 8, 1, RGB(170, 160, 200));
                    cv_line(px + 11, py + 5, px + 14, py + 2, RGB(200, 170, 120));
                    disc(px + 15, py + 2, pulse ? 2 : 1, pulse ? C_YEL : C_PINK);
                }
            } else {
                int c = mi_count(x, y);
                uint32_t oq = ms - mi.ot[y][x];
                cv_rect(px, py, MI_C - 1, MI_C - 1, oq < 90u ? mix565(RGB(22, 16, 38), C_INK, 200 - (int32_t)oq * 2) : RGB(22, 16, 38));
                if (c && oq >= 40u) {
                    char d[2] = {(char)('0' + c), 0};
                    text_c(px + 9, py - (oq < 120u ? (int32_t)(120u - oq) / 30 : 0), &FONT_B, d, deg_col(c * 2));
                }
            }
        }
    if (!mi.over) {                      /* the cursor: gliding; held, a bar fills toward the flag */
        int32_t px = MI_X + (int32_t)(mi.fx * MI_C + 0.5f), py = MI_Y + (int32_t)(mi.fy * MI_C + 0.5f);
        int32_t g = (plat_ms() / 150u) % 4u == 0u;
        frame_rect(px - 1 - g, py - 1 - g, MI_C + 1 + 2 * g, MI_C + 1 + 2 * g, C_YEL);
        frame_rect(px - 2 - g, py - 2 - g, MI_C + 3 + 2 * g, MI_C + 3 + 2 * g, C_YEL);
        if (mi.a_down && !mi.a_flagged) {
            int32_t f = (int32_t)(plat_ms() - mi.a_down) * (MI_C + 1) / 420;
            cv_rect(px - 1, py + MI_C + 2, f > MI_C + 1 ? MI_C + 1 : f, 3, C_CYAN);
        }
    }
    if (mi.over && plat_ms() - mi.over_at > (mi.won ? 300u : 1500u)) {
        cv_rect(40, 92, 160, 50, C_BG);
        frame_rect(40, 92, 160, 50, mi.won ? C_YEL : C_PINK);
        {
            text_c(120, 96, &FONT_M, mi.won ? "CLEARED!" : "BOOM", mi.won ? C_YEL : C_PINK);
            if (mi.won) {
                char ts[24], tn[8];
                itos((int32_t)((mi.over_at - mi.start_at) / 1000u), tn);
                tcpy(ts, tn);
                tcat(ts, "S   BEST ");
                itos((int32_t)(mi_best / 1000u), tn);
                tcat(ts, tn);
                tcat(ts, "S");
                text_c(120, 116, &FONT_T, ts, C_CYAN);
            }
            text_c(120, 128, &FONT_T, mi.won ? "TOP KEY: NEXT FIELD   HOME: MENU" : "TOP KEY: AGAIN   HOME: MENU", C_GHOST);
        }
    } else if (!mi.over)
        text_c(120, 202, &FONT_T, "TOP KEY OPENS    HOLD IT: FLAG", C_GHOST);
    {                                    /* the playhead's column, lit as it sounds */
        int c;
        for (c = 0; c < MI_W; c++)
            sq_col_glow(MI_X + c * MI_C - 1, MI_Y - 1, MI_C + 1, MI_H * MI_C + 1, mi.seq_ms[c], C_YEL);
    }
}
static uint32_t mi_score(void) { return mi.opened + (mi.won ? 100u : 0u); }

/* ================================================================== 2048 === */
enum { NT_MERGE, NT_SLIDE, NT_PAD, NT_WIN, NT_SEQ, NT_HAT, NT_KICK, NT_SNARE, NT_BASS };   /* NT_SEQ: the playhead's */
typedef struct {
    uint8_t g[4][4];                     /* each tile: its power of two (0 empty) */
    uint8_t pulse[4][4];                 /* merged just now */
    uint32_t score, pulse_at;
    uint8_t over, won;
    uint32_t over_at;
    struct { int8_t fx, fy, tx, ty; uint8_t v; } mv[16];   /* the tiles sliding (the old ones, where from, where to) */
    uint8_t nmv;
    int8_t sx, sy;                       /* the new tile, growing in */
    uint32_t best_seen;                  /* the biggest tile made yet (its power): a milestone pops */
    uint32_t seq_ms[4];                  /* when the playhead's chord in each column last sounded */
    uint8_t tier;
} tw_t;
static uint32_t tw_best;
#define TW_SLIDE 110u
#define tw (*(tw_t *)g_arena)   /* in the games' shared memory (app.c) */
_Static_assert(sizeof(tw_t) <= sizeof g_arena, "tw: the arena is too small");
static void tw_spawn(void)
{
    int e[16], n = 0, x, y, k;
    for (y = 0; y < 4; y++)
        for (x = 0; x < 4; x++)
            if (!tw.g[y][x])
                e[n++] = y * 4 + x;
    if (!n)
        return;
    k = e[kr() % (uint32_t)n];
    tw.g[k / 4][k % 4] = (uint8_t)(kr() % 10u ? 1 : 2);
    tw.sx = (int8_t)(k % 4), tw.sy = (int8_t)(k / 4);
}
static int tw_can(void)
{
    int x, y;
    for (y = 0; y < 4; y++)
        for (x = 0; x < 4; x++) {
            if (!tw.g[y][x])
                return 1;
            if (x < 3 && tw.g[y][x] == tw.g[y][x + 1])
                return 1;
            if (y < 3 && tw.g[y][x] == tw.g[y + 1][x])
                return 1;
        }
    return 0;
}
static void tw_new(void)
{
    int x, y;
    for (y = 0; y < 4; y++)
        for (x = 0; x < 4; x++)
            tw.g[y][x] = tw.pulse[y][x] = 0;
    tw.score = 0;
    tw.over = tw.won = 0;
    tw.nmv = 0;
    tw.best_seen = 2;
    tw.tier = 0;
    for (x = 0; x < 4; x++)
        tw.seq_ms[x] = 0;
    tw_spawn();
    tw_spawn();
    tw.sx = -1;
    tw.pulse_at = plat_ms();
}
static void tw_nav(int dx, int dy, int sel)
{
    int i, j, k, moved = 0, merges[8], nm = 0;
    if (tw.over) {
        if (sel && plat_ms() - tw.over_at > 800u)
            tw_new();
        return;
    }
    if (!dx && !dy)
        return;
    dy = -dy;
    tw.nmv = 0;
    for (i = 0; i < 4; i++) {            /* each line along the move, from its far end */
        uint8_t line[4], out[4] = {0, 0, 0, 0};
        int n = 0, o = 0, src[4];
        for (j = 0; j < 4; j++) {
            int p = dx > 0 || dy > 0 ? 3 - j : j;
            line[j] = dx ? tw.g[i][p] : tw.g[p][i];
        }
        for (j = 0; j < 4; j++)
            if (line[j])
                src[n] = j, line[n++] = line[j];
        for (j = 0; j < n; j++) {        /* (each tile's slide: from its place to its new one) */
            int dst = o + (j + 1 < n && line[j] == line[j + 1] ? 0 : 0), q;
            for (q = 0; q < 1 + (j + 1 < n && line[j] == line[j + 1]); q++) {
                int ps = dx > 0 || dy > 0 ? 3 - src[j + q] : src[j + q], pd = dx > 0 || dy > 0 ? 3 - dst : dst;
                if (tw.nmv < 16) {
                    tw.mv[tw.nmv].fx = (int8_t)(dx ? ps : i), tw.mv[tw.nmv].fy = (int8_t)(dx ? i : ps);
                    tw.mv[tw.nmv].tx = (int8_t)(dx ? pd : i), tw.mv[tw.nmv].ty = (int8_t)(dx ? i : pd);
                    tw.mv[tw.nmv].v = line[j];
                    tw.nmv++;
                }
            }
            if (j + 1 < n && line[j] == line[j + 1]) {
                out[o] = (uint8_t)(line[j] + 1);
                tw.score += 1u << out[o];
                if (nm < 8)
                    merges[nm++] = out[o];
                if (out[o] == 11 && !tw.won)
                    tw.won = 1;
                j++;
            } else
                out[o] = line[j];
            o++;
        }
        for (j = 0; j < 4; j++) {
            int p = dx > 0 || dy > 0 ? 3 - j : j;
            uint8_t *c = dx ? &tw.g[i][p] : &tw.g[p][i];
            if (*c != out[j])
                moved = 1;
            *c = out[j];
            tw.pulse[dx ? i : p][dx ? p : i] = (uint8_t)(j < o && nm && out[j] > line[j < n ? j : 0] ? 1 : 0);
        }
    }
    if (!moved) {                        /* it won't go: a nudge */
        juice_shake(2, 100);
        juice_kick(dx * 4, dy * 4);
        tw.nmv = 0;
        return;
    }
    tw.pulse_at = plat_ms();
    {
        int y, x, top = 0;
        for (y = 0; y < 4; y++)
            for (x = 0; x < 4; x++)
                if (tw.pulse[y][x]) {    /* a merge: sparks of its colour, after the slide */
                    int32_t e = tw.g[y][x];
                    juice_burst(18 + x * 52 + 24, 30 + y * 52 + 24, 6 + (int32_t)e, 2 + (int32_t)e / 3, deg_col((int)e - 1), C_INK, 0);
                    if (e > top)
                        top = (int)e;
                }
        if (top) {
            char ps[12] = {'+', 0};
            uint32_t got = 0;
            for (k = 0; k < nm; k++)
                got += 1u << merges[k];
            itos((int32_t)got, ps + 1);
            juice_text(120, 120, ps, deg_col(top - 1));
            juice_shake(top / 2 + nm, 120u + 15u * (uint32_t)top);
            juice_kick(dx * (2 + top / 2), dy * (2 + top / 2));   /* the board slams the way you swiped */
            juice_heat(3 * nm + top);
            if (top >= 7)
                juice_freeze(20u * (uint32_t)(top - 6));
        }
        if ((uint32_t)top > tw.best_seen && top >= 7) {   /* 128 and up: a milestone */
            char ms_[8];
            tw.best_seen = (uint32_t)top;
            itos(1 << top, ms_);
            juice_pop(120, 100, ms_, deg_col(top - 1));
            juice_confetti(120, 120, 20 + top * 3);
            juice_ring(120, 130, 60 + top * 8, deg_col(top - 1), 500);
            juice_flash(deg_col(top - 1), 60 + top * 6, 400);
            juice_impact(top >= 10 ? 90u : 50u);
            juice_sting(top >= 10 ? 2 : 1);
            if (top >= 9)
                juice_fireworks(500u * (uint32_t)(top - 7));
        }
    }
    for (k = 0; k < nm; k++)             /* the merges: their notes, up the scale with the tiles, at once a chord */
        k_on(k_at(0.02f * (float)k), degree_note(merges[k] - 1 + 3), 100, NT_MERGE, 0.6f);
    if (!nm)
        k_on(0, degree_note(dx < 0 ? 0 : dx > 0 ? 2 : dy < 0 ? 4 : 1), 40, NT_SLIDE, 0.08f);
    if (tw.won == 1) {
        tw.won = 2;
        for (k = 0; k < 6; k++)
            k_on(k_at(0.2f + 0.1f * (float)k), degree_note(7 + 2 * k), 100, NT_WIN, 1.0f);
        app_say("2048!", "KEEP GOING");
        juice_celebrate(120, 120);
    }
    tw_spawn();
    if (!tw_can()) {
        tw.over = 1;
        tw.over_at = plat_ms();
        if (tw.score > tw_best)
            tw_best = tw.score;
        juice_shake(8, 600);
        juice_freeze(150);
        juice_flash(RGB(255, 40, 80), 150, 500);
        juice_glitch(600);
        juice_cool();
        k_on(k_at(0.1f), degree_note(-7), 90, NT_PAD, 1.0f);
    }
}
static void tw_groove(uint32_t t, uint32_t step)
{
    uint32_t p = step % 16u;
    int tier, best = 0, x, y;
    if (tw.over)
        return;
    if (step % 32u == 0u) {
        h_deg_at(step / 32u);
        h_pad(t, 30, NT_PAD, 3.0f, 0);
    }
    for (y = 0; y < 4; y++)
        for (x = 0; x < 4; x++)
            best = tw.g[y][x] > best ? tw.g[y][x] : best;
    tier = sq_tier_of(best - 3, 8);      /* the drums, as your best tile grows (16 .. 2048) */
    sq_tier_say(tier, &tw.tier, 236);
    if (p == 0u)
        k_on(t, h_bass(), 64, NT_BASS, 0.3f);
    sq_drums(t, p, tier, NT_HAT, NT_KICK, NT_SNARE, 80);
    if (!(p & 1u)) {                     /* the playhead: a column an eighth, twice a bar; its tiles a chord */
        int c = (int)(p / 2u % 4u), any = 0;
        for (y = 0; y < 4; y++)
            if (tw.g[y][c] && sq_roll(SQ_2048)) {
                k_on(t, h_tone(tw.g[y][c] - 1), 60u + (uint32_t)tw.g[y][c] * 3u, NT_SEQ, 0.4f);
                any = 1;
            }
        if (any)
            tw.seq_ms[c] = sq_ms(t);
    }
}
static void tw_sounds(void)
{
    k_slot_set(NT_MERGE, &KSND[S_CELESTA]);
    k_slot_set(NT_SLIDE, &KSND[S_NYLON]);
    k_slot_set(NT_PAD, &KSND[S_GLASSPAD]);
    k_slot_set(NT_WIN, &KSND[S_TUBE]);
    k_slot_set(NT_HAT, &KSND[S_HAT]);
    k_slot_set(NT_KICK, &KSND[S_KICK]);
    k_slot_set(NT_SNARE, &KSND[S_SNARE]);
    k_slot_set(NT_BASS, &KSND[S_FMBASS]);
    k_slot_rel(NT_BASS, 0.3f);
    sq_slot = NT_SEQ;
    sq_sound(SQ_2048);
    k_slot_rel(NT_MERGE, 0.6f);
    k_slot_rel(NT_PAD, 1.4f);
    k_slot_rel(NT_WIN, 1.0f);
    k_main_slot = NT_MERGE;
}
static void tw_tile(int32_t px, int32_t py, uint8_t e, int32_t grow, char *n)   /* a tile: bevelled, its number */
{
    uint16_t c = deg_col(e - 1);
    int32_t x = px - grow, y = py - grow, w = 48 + 2 * grow;
    if (w < 2)
        return;
    cv_rect(x + 2, y + 3, w, w, RGB(8, 5, 16));   /* its shadow */
    cv_rect(x + 1, y, w - 2, w, c);      /* (corners cut: rounded, at this size) */
    cv_rect(x, y + 1, w, w - 2, c);
    cv_rect(x + 2, y + 1, w - 4, 3, mix565(c, C_INK, 120));
    cv_rect(x + 2, y + w - 4, w - 4, 3, mix565(c, C_BG, 90));
    if (e >= 7 && (plat_ms() / 100u + e) % 20u == 0u)   /* big ones glint */
        cv_rect(x + 4, y + 4, w - 8, 2, C_INK);
    if (w >= 30) {
        itos(1 << e, n);
        text_c(px + 24, py + (e < 10 ? 10 : 15), e < 10 ? &FONT_M : e < 14 ? &FONT_B : &FONT_XS, n, C_BG);
    }
}
static void tw_scene(void)
{
    int x, y;
    char n[12];
    uint32_t ms = plat_ms();
    itos((int32_t)tw.score, n);
    header("2048", C_PINK, "", C_PINK, n);
    cv_rect(14, 26, 212, 212, RGB(18, 13, 32));
    for (y = 0; y < 4; y++)              /* the empty slots */
        for (x = 0; x < 4; x++)
            cv_rect(18 + x * 52, 30 + y * 52, 48, 48, RGB(30, 22, 52));
    for (x = 0; x < 4; x++)              /* the playhead's column, lit under the tiles as it sounds */
        sq_col_glow(15 + x * 52, 27, 54, 210, tw.seq_ms[x], C_PINK);
    if (tw.nmv && ms - tw.pulse_at < TW_SLIDE) {   /* sliding: the old tiles, on their way */
        float f = (float)(ms - tw.pulse_at) / (float)TW_SLIDE;
        int k;
        f = 1.0f - (1.0f - f) * (1.0f - f);
        for (k = 0; k < tw.nmv; k++) {
            int32_t px = 18 + tw.mv[k].fx * 52 + (int32_t)((float)((tw.mv[k].tx - tw.mv[k].fx) * 52) * f);
            int32_t py = 30 + tw.mv[k].fy * 52 + (int32_t)((float)((tw.mv[k].ty - tw.mv[k].fy) * 52) * f);
            tw_tile(px, py, tw.mv[k].v, 0, n);
        }
    } else
        for (y = 0; y < 4; y++)
            for (x = 0; x < 4; x++) {
                int32_t px = 18 + x * 52, py = 30 + y * 52, e = tw.g[y][x], grow = 0;
                uint32_t q = ms - tw.pulse_at - TW_SLIDE;
                if (!e)
                    continue;
                if (tw.pulse[y][x] && q < 160u)   /* merged: it pops up big, settles */
                    grow = (int32_t)(fm_sinf((float)q / 160.0f * 3.14159f) * 7.0f);
                if (x == tw.sx && y == tw.sy && q < 140u)   /* new: it grows in */
                    grow = -24 + (int32_t)q * 24 / 140;
                if (tw.over) {           /* the end: the tiles go grey, one by one */
                    uint32_t oq = ms - tw.over_at;
                    if (oq > 200u + (uint32_t)(y * 4 + x) * 50u) {
                        cv_rect(px, py, 48, 48, RGB(50, 44, 70));
                        itos(1 << e, n);
                        text_c(px + 24, py + (e < 10 ? 10 : 15), e < 10 ? &FONT_M : e < 14 ? &FONT_B : &FONT_XS, n, RGB(90, 80, 120));
                        continue;
                    }
                }
                tw_tile(px, py, (uint8_t)e, grow, n);
            }
    if (tw.over && ms - tw.over_at > 1100u) {
        char bs[24], bn[12];
        cv_rect(40, 100, 160, 62, C_BG);
        frame_rect(40, 100, 160, 62, C_YEL);
        text_c(120, 104, &FONT_M, "NO MOVES", C_YEL);
        itos((int32_t)tw_best, bn);
        tcpy(bs, "BEST ");
        tcat(bs, bn);
        text_c(120, 128, &FONT_XS, bs, C_CYAN);
        text_c(120, 146, &FONT_T, "TOP KEY: AGAIN   HOME: MENU", C_GHOST);
    }
}
