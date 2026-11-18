/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Nick Culbertson */
/* toys.h: two music toys for Mighty Arcade's menu, games you play as much as hear (from Nick's CYD MIDI Controller's
 * DROP and BOUNCE modes). Every collision is a note, and every note lands on the next sixteenth of the groove
 * (grid_next, app.c): the physics is free, the music always in time. Notes are the chord's tones (h_tone), so always
 * in tune. Included once from app.c, after seq.h; their state lives in the shared arena.
 *
 *  NOTE DROP   every key drops a ball from its own place along the top (low keys left, high keys right). It tumbles
 *              down the board's platforms, each a note (the higher, the higher), into the floor's eight bars, each a
 *              note too. A ball landing on a floor bar lights it: light all eight to clear the board (aim with the
 *              keys). Three hits light a platform full, and lit platforms bring in the drums. A ball hitting four or
 *              more on its way down is a chain. K1..K4 tilt the platforms, each its own (K1 the first, K2 the second ..;
 *              on a board of more than four, the fifth turns with K1, and so on); SELECT picks one of twelve boards,
 *              starting with two platforms and building up to eight
 *  DODGE       (was BOUNCE BOX) a rolling ball, dodge-ball in a box whose walls are 24 note tiles: every bounce off a wall is a note (one an
 *              eighth). The arrows move you; don't get hit. Every four bars you last, another ball comes in, on the
 *              bar line (up to six), so the music builds the longer you survive, and the drums with it. A hit: back
 *              to one ball, the music drops. A near miss is an accent and points; stars are points. Light all 24
 *              tiles: a full circle. Three balls unlock an exit ring; roll into it for the next level, with solid
 *              musical bumpers to navigate. An earned exit stays open after a hit. K2 sets the speed
 * Both: the more you've lit, the more of the drums come in (seq.h). Their knobs: K1 TEMPO, K2 BOARD or SPEED,
 * K3 SOUND, K4 EFFECTS, PRESET the scale, ALGORITHM the key. */

enum { TY_SEQ, TY_LOW, TY_BASS, TY_HAT, TY_KICK, TY_SNARE, TY_STAR, TY_WIN, TY_TICK };
static void toy_sounds(int g)
{
    k_slot_set(TY_LOW, &KSND[S_MARIMBA]);
    k_slot_set(TY_BASS, &KSND[S_FMBASS]);
    k_slot_set(TY_HAT, &KSND[S_HAT]);
    k_slot_set(TY_KICK, &KSND[S_KICK]);
    k_slot_set(TY_SNARE, &KSND[S_SNARE]);
    k_slot_set(TY_STAR, &KSND[S_CELESTA]);
    k_slot_set(TY_WIN, &KSND[S_TUBE]);
    k_slot_set(TY_TICK, &KSND[S_HAT]);
    k_slot_rel(TY_LOW, 0.6f);
    k_slot_rel(TY_BASS, 0.2f);
    k_slot_rel(TY_STAR, 1.0f);
    k_slot_rel(TY_WIN, 1.2f);
    k_main_slot = TY_SEQ;
    k_main.rev_mix = 0.3f, k_main.rev_secs = 3.2f, k_main.shimmer = 0.04f;
    k_main.dly_mix = 0.16f, k_main.dly_fb = 0.35f;
    sq_slot = TY_SEQ;
    sq_sound(g);
}
/* a note on the next sixteenth (grid 1) or eighth (2) (as the music plays it, not an effect: EFFECTS leaves it);
 * when it sounds (ms) */
static uint32_t toy_note_g(uint32_t *last_t, int32_t note, uint32_t vel, uint32_t slot, float secs, uint32_t grid)
{
    uint32_t t = grid_next_n(grid), was = k_in_music;
    if (last_t && *last_t == t)          /* (this one already sounds on that sixteenth) */
        return sq_ms(t);
    if (last_t)
        *last_t = t;
    k_in_music = 1;
    k_on(t, note, vel, slot, secs);
    k_in_music = (uint8_t)was;
    return sq_ms(t);
}
static uint32_t toy_note(uint32_t *last_t, int32_t note, uint32_t vel, uint32_t slot, float secs)
{
    return toy_note_g(last_t, note, vel, slot, secs, 1u);
}
static uint8_t toy_board = 0, toy_speed = 5;   /* K2: NOTE DROP's board, DODGE's speed (they last) */

/* ============================================================= NOTE DROP === */
#define DP_NB 10                         /* balls at once */
#define DP_NP 8                          /* platforms on a board, at most */
#define DP_FLOOR 214                     /* the floor's bars, from here down */
#define DP_R 4.0f
#define DP_NBOARD 12
#define DP_FULL 3                        /* hits to light a platform full */
static const char *const DP_NAME[DP_NBOARD] = {"FIRST STEPS", "TRIO", "SWITCHBACK", "SPLIT", "ZIGZAG", "FUNNEL",
                                             "CROSSING", "STAIRS", "CASCADE", "WEAVE", "PINS", "SEESAW"};
/* Two platforms to start; more routes to steer through as the boards progress. Zero rows are unused. */
static const int16_t DP_BOARD[DP_NBOARD][DP_NP][4] = {
    {{48, 88, 96, 98}, {144, 140, 192, 130}},
    {{38, 70, 90, 80}, {150, 100, 202, 90}, {94, 146, 146, 146}},
    {{30, 66, 100, 80}, {140, 112, 210, 98}, {62, 160, 132, 174}},
    {{30, 64, 88, 78}, {152, 78, 210, 64}, {54, 132, 110, 148}, {130, 148, 186, 132}},
    {{30, 62, 110, 80}, {130, 102, 210, 84}, {40, 134, 120, 152}, {120, 186, 200, 168}},
    {{18, 60, 100, 92}, {222, 60, 140, 92}, {54, 130, 108, 150}, {186, 130, 132, 150}, {92, 186, 148, 186}},
    {{28, 70, 110, 100}, {212, 70, 130, 100}, {110, 132, 28, 162}, {130, 132, 212, 162}, {98, 192, 142, 192}},
    {{30, 62, 100, 76}, {140, 76, 210, 62}, {70, 110, 140, 124}, {100, 152, 170, 138}, {20, 176, 80, 190},
     {160, 190, 220, 176}},
    {{24, 58, 78, 72}, {128, 72, 182, 58}, {68, 104, 122, 118}, {164, 118, 218, 104},
     {28, 164, 86, 180}, {128, 180, 186, 164}},
    {{28, 58, 82, 70}, {158, 70, 212, 58}, {92, 96, 148, 96}, {28, 124, 82, 138},
     {158, 138, 212, 124}, {66, 174, 112, 188}, {128, 188, 174, 174}},
    {{40, 70, 66, 70}, {107, 70, 133, 70}, {174, 70, 200, 70}, {72, 112, 98, 112}, {142, 112, 168, 112},
     {40, 154, 66, 154}, {107, 154, 133, 154}, {174, 154, 200, 154}},
    {{28, 58, 78, 70}, {162, 70, 212, 58}, {86, 96, 154, 96}, {24, 126, 74, 114},
     {166, 114, 216, 126}, {88, 152, 152, 152}, {30, 190, 84, 176}, {156, 176, 210, 190}},
};
typedef struct {
    float x, y, vx, vy;
    int16_t tx[5], ty[5];                /* its trail */
    uint8_t on, chain, col_k;            /* col_k: the key that dropped it (its colour) */
    int8_t last_p;                       /* the platform it last hit (-1 none): one note a bounce */
    uint32_t last_ms, born;
} dp_ball_t;
typedef struct {
    dp_ball_t b[DP_NB];
    uint8_t lit[DP_NP], np, board, tier, cleared;
    uint32_t hit_ms[DP_NP], note_ms[DP_NP], note_t[DP_NP];   /* touched; its note sounds; (that sixteenth) */
    uint32_t floor_ms[8], floor_t[8];
    uint8_t floor_lit[8];                /* the floor's bars lit: all eight clear the board */
    uint32_t keys_was, key_ms[NKEYS];   /* the keys down last frame; when each last dropped one */
    uint32_t score, last, clear_at, start_at;
    uint8_t oldest;
    int16_t pl[DP_NP][4];                /* the platforms as they are: the board's, tilted by the knobs */
    int8_t tilt[DP_NP];                  /* each one's tilt, in steps of DP_TILT */
    uint32_t tilt_ms[DP_NP];             /* turned (ms): its knob's number shows */
} dp_t;
#define DP_TILT 0.10472f                 /* a detent's tilt: 6 degrees */
#define DP_TMAX 10                       /* at most 60 degrees either way */
#define ndrop (*(dp_t *)g_arena)
_Static_assert(sizeof(dp_t) <= sizeof g_arena, "ndrop: the arena is too small");

static int dp_lit_n(void)                /* the platforms lit full */
{
    int i, n = 0;
    for (i = 0; i < ndrop.np; i++)
        n += ndrop.lit[i] >= DP_FULL;
    return n;
}
static int dp_floor_n(void)              /* the floor's bars lit */
{
    int i, n = 0;
    for (i = 0; i < 8; i++)
        n += ndrop.floor_lit[i];
    return n;
}
static int dp_hits_n(void)               /* how far along: every platform's hits, up to full */
{
    int i, n = 0;
    for (i = 0; i < ndrop.np; i++)
        n += ndrop.lit[i];
    return n;
}
static int32_t dp_pnote(int p)           /* a platform's note: its height up the chord */
{
    const int16_t *s = ndrop.pl[p];
    int32_t mid = (s[1] + s[3]) / 2;
    return h_tone((int)clamp((DP_FLOOR - mid) / 20, 0, 8) + 2);
}
static uint16_t dp_pcol(int p)
{
    const int16_t *s = ndrop.pl[p];
    return deg_col((DP_FLOOR - (s[1] + s[3]) / 2) / 16);
}
static void dp_start(void)               /* the board as new */
{
    int i;
    ndrop.np = 0;
    for (i = 0; i < DP_NP; i++) {
        const int16_t *s = DP_BOARD[ndrop.board][i];
        ndrop.lit[i] = 0;
        ndrop.tilt[i] = 0, ndrop.tilt_ms[i] = 0;
        ndrop.pl[i][0] = s[0], ndrop.pl[i][1] = s[1], ndrop.pl[i][2] = s[2], ndrop.pl[i][3] = s[3];
        ndrop.hit_ms[i] = ndrop.note_ms[i] = ndrop.note_t[i] = 0;
        if (s[0] || s[2])
            ndrop.np = (uint8_t)(i + 1);
    }
    for (i = 0; i < DP_NB; i++)
        ndrop.b[i].on = 0;
    ndrop.tier = 0;
    ndrop.cleared = 0;
    for (i = 0; i < 8; i++)
        ndrop.floor_lit[i] = 0, ndrop.floor_ms[i] = 0, ndrop.floor_t[i] = 0;
    ndrop.start_at = plat_ms() | 1u;
    juice_pop(120, 100, DP_NAME[ndrop.board], C_CYAN);
    juice_text(120, 136, "PLAY ANY KEY", C_GHOST);
}
static void dp_new(void)
{
    ndrop.board = toy_board % DP_NBOARD;
    ndrop.score = 0;
    ndrop.last = 0;
    ndrop.keys_was = 0xFFFFFFFFu;        /* (a key still down from the menu's A drops nothing) */
    dp_start();
}
static void dp_tilt(int knob, int d)     /* K1..K4: its platforms tilt, about their middles */
{
    int i;
    for (i = knob; i < ndrop.np; i += 4) {
        const int16_t *s = DP_BOARD[ndrop.board][i];
        float mx = 0.5f * (float)(s[0] + s[2]), my = 0.5f * (float)(s[1] + s[3]);
        float hx = 0.5f * (float)(s[2] - s[0]), hy = 0.5f * (float)(s[3] - s[1]), a, c, n;
        ndrop.tilt[i] = (int8_t)clamp(ndrop.tilt[i] + d, -DP_TMAX, DP_TMAX);
        a = DP_TILT * (float)ndrop.tilt[i];
        c = fm_cosf(a), n = fm_sinf(a);
        ndrop.pl[i][0] = (int16_t)(mx - (hx * c - hy * n));
        ndrop.pl[i][1] = (int16_t)(my - (hx * n + hy * c));
        ndrop.pl[i][2] = (int16_t)(mx + (hx * c - hy * n));
        ndrop.pl[i][3] = (int16_t)(my + (hx * n + hy * c));
        ndrop.tilt_ms[i] = plat_ms() | 1u;
    }
}
static void dp_set_board(int d)          /* SELECT */
{
    toy_board = (uint8_t)clamp((int32_t)toy_board + d, 0, DP_NBOARD - 1);
    if (toy_board != ndrop.board) {
        ndrop.board = toy_board;
        dp_start();
    }
}
static int32_t dp_key_x(uint32_t k)      /* a key's place along the top: over where it is on the FM-1's keyboard (the
                                          * white keys evenly across, each black key between its two whites) */
{
    static const uint8_t WK[16] = PLAT_WHITE_KEYS;
    int w;
    for (w = 0; w < 16; w++)
        if (WK[w] == k)
            return 12 + w * 216 / 15;
    for (w = 0; w < 15; w++)
        if (WK[w] < k && WK[w + 1] > k)
            return 12 + (2 * w + 1) * 216 / 30;
    return 120;
}
static void dp_drop(uint32_t k)          /* a key: a ball from its place along the top */
{
    dp_ball_t *b = 0;
    int i;
    for (i = 0; i < DP_NB && !b; i++)
        if (!ndrop.b[i].on)
            b = &ndrop.b[i];
    if (!b) {                            /* all falling: the oldest makes way */
        b = &ndrop.b[ndrop.oldest % DP_NB];
        ndrop.oldest++;
    }
    b->x = (float)dp_key_x(k);
    b->y = 28.0f;
    b->vx = (float)((int32_t)(kr() % 21u) - 10) * 0.003f;
    b->vy = 0.0f;
    b->on = 1;
    b->chain = 0;
    b->col_k = (uint8_t)k;
    b->last_p = -1;
    b->born = plat_ms();
    for (i = 0; i < 5; i++)
        b->tx[i] = (int16_t)b->x, b->ty[i] = (int16_t)b->y;
    ndrop.key_ms[k] = plat_ms() | 1u;
    juice_burst((int32_t)b->x, 28, 5, 1, deg_col((int32_t)k), C_INK, 0);
    k_on(0, 96, 26, TY_TICK, 0.02f);     /* (a tick: an effect) */
}
static void dp_nav(int dx, int dy, int sel) { (void)dx, (void)dy, (void)sel; }   /* (every key drops: dp_tick) */
static void dp_bounce(dp_ball_t *b, int p, uint32_t ms)
{
    const int16_t *s = ndrop.pl[p];
    float ax = s[0], ay = s[1], ex = (float)(s[2] - s[0]), ey = (float)(s[3] - s[1]);
    float len2 = ex * ex + ey * ey, t, qx, qy, dx, dy, d, nx, ny, vn;
    if (len2 <= 0.0f)
        return;
    t = fm_clampf(((b->x - ax) * ex + (b->y - ay) * ey) / len2, 0.0f, 1.0f);
    qx = ax + ex * t, qy = ay + ey * t;
    dx = b->x - qx, dy = b->y - qy;
    d = fm_sqrtf(dx * dx + dy * dy);
    if (d >= DP_R + 2.0f || d < 0.001f)
        return;
    nx = dx / d, ny = dy / d;
    vn = b->vx * nx + b->vy * ny;
    b->x = qx + nx * (DP_R + 2.0f);
    b->y = qy + ny * (DP_R + 2.0f);
    if (vn >= 0.0f)
        return;
    b->vx -= 1.6f * vn * nx;             /* bounces off, losing a little */
    b->vy -= 1.6f * vn * ny;
    if (-vn > 0.035f && (b->last_p != p || ms - b->last_ms > 160u)) {   /* a hit: its note, on the next sixteenth */
        uint16_t col = dp_pcol(p);
        uint32_t vel = (uint32_t)clamp((int32_t)(60.0f + -vn * 160.0f), 60, 118);
        char cs[16];
        b->last_p = (int8_t)p;
        b->last_ms = ms;
        b->chain++;
        ndrop.note_ms[p] = toy_note(&ndrop.note_t[p], dp_pnote(p), vel, TY_SEQ, 0.5f);
        ndrop.hit_ms[p] = ms | 1u;
        ndrop.score += 10u * b->chain;
        juice_burst((int32_t)qx, (int32_t)qy, 6 + b->chain * 2, 2, col, C_INK, 1);
        juice_heat(3);
        if (-vn > 0.16f)
            juice_shake(2, 120);
        if (ndrop.lit[p] < DP_FULL) {      /* brighter; at full, a ring */
            ndrop.lit[p]++;
            if (ndrop.lit[p] == DP_FULL) {
                juice_ring((int32_t)qx, (int32_t)qy, 30, col, 320);
                juice_embers((int32_t)qx, (int32_t)qy, 6, col);
            }
        }
        if (b->chain >= 4) {             /* a chain */
            tcpy(cs, "CHAIN X");
            itos(b->chain, cs + 7);
            juice_text((int32_t)qx, (int32_t)qy - 10, cs, C_YEL);
            juice_heat(6);
        }
    }
}
static void dp_tick(void)
{
    uint32_t dt = arc_dt(&ndrop.last), ms = plat_ms(), keys = ui.keys, k;
    int i, p, j;
    for (k = 0; k < NKEYS; k++)          /* each key pressed: a ball */
        if ((keys >> k & 1u) && !(ndrop.keys_was >> k & 1u))
            dp_drop(k);
    ndrop.keys_was = keys;
    if (ndrop.cleared) {                    /* the board cleared: a moment, then the next */
        if (ms - ndrop.clear_at > 2400u) {
            ndrop.board = (uint8_t)((ndrop.board + 1u) % DP_NBOARD);
            toy_board = ndrop.board;
            dp_start();
        }
    }
    for (i = 0; i < DP_NB; i++) {
        dp_ball_t *b = &ndrop.b[i];
        float f = (float)dt;
        if (!b->on)
            continue;
        for (j = 4; j > 0; j--)
            b->tx[j] = b->tx[j - 1], b->ty[j] = b->ty[j - 1];
        b->tx[0] = (int16_t)b->x, b->ty[0] = (int16_t)b->y;
        b->vy += 0.00055f * f;           /* gravity */
        b->vx = fm_clampf(b->vx, -0.35f, 0.35f);
        b->vy = fm_clampf(b->vy, -0.5f, 0.5f);
        b->x += b->vx * f;
        b->y += b->vy * f;
        if (b->x < 6.0f + DP_R)
            b->x = 6.0f + DP_R, b->vx = fm_fabsf(b->vx) * 0.7f;
        if (b->x > 234.0f - DP_R)
            b->x = 234.0f - DP_R, b->vx = -fm_fabsf(b->vx) * 0.7f;
        for (p = 0; p < ndrop.np; p++)
            dp_bounce(b, p, ms);
        if (b->y >= (float)DP_FLOOR - DP_R) {   /* the floor: its bar's note, a splash, gone */
            int t = (int)clamp((int32_t)(b->x / 30.0f), 0, 7);
            uint16_t col = deg_col(t * 2);
            ndrop.floor_ms[t] = toy_note(&ndrop.floor_t[t], h_tone(t) - 12, 92, TY_LOW, 0.6f);
            juice_burst((int32_t)b->x, DP_FLOOR, 10, 2, col, C_INK, 1);
            juice_ring((int32_t)b->x, DP_FLOOR + 4, 20, col, 260);
            if (!ndrop.floor_lit[t]) {      /* a bar lit for the first time */
                ndrop.floor_lit[t] = 1;
                ndrop.score += 50;
                juice_ring(t * 30 + 15, DP_FLOOR + 12, 34, col, 360);
                juice_embers(t * 30 + 15, DP_FLOOR + 6, 8, col);
                juice_text(t * 30 + 15, DP_FLOOR - 10, "+50", col);
            }
            if (b->chain >= 4) {
                char cs[16] = "+";
                uint32_t bonus = 25u * b->chain;
                itos((int32_t)bonus, cs + 1);
                ndrop.score += bonus;
                juice_pop((int32_t)fm_clampf(b->x, 50.0f, 190.0f), 170, cs, C_YEL);
                juice_sting(1);
            }
            b->on = 0;
        } else if (ms - b->born > 15000u)   /* (one stuck on a ledge: let go) */
            b->on = 0;
    }
    if (!ndrop.cleared && dp_floor_n() == 8) {   /* every floor bar lit: the board cleared */
        ndrop.cleared = 1;
        ndrop.clear_at = ms;
        ndrop.score += 500;
        juice_calm();                    /* (the words before it gone) */
        juice_pop(120, 100, "BOARD CLEAR!", C_YEL);
        juice_freeze(80);
        juice_flash(0xFFFF, 160, 260);
        juice_ring(120, 120, 140, C_YEL, 600);
        juice_ring(120, 120, 90, C_CYAN, 450);
        juice_confetti(120, 110, 50);
        juice_shake(5, 300);
        juice_sting(2);
        J.heat = 100;
    }
}
static void dp_groove(uint32_t t, uint32_t step)
{
    uint32_t p = step % 16u;
    int tier;
    if (p == 0u && (step / 16u) % 2u == 0u)
        h_deg_at(step / 32u);            /* a new chord every two bars */
    if (p == 0u || p == 8u)
        k_on(t, h_bass(), 70, TY_BASS, 0.2f);
    tier = sq_tier_of(dp_hits_n(), ndrop.np * DP_FULL);
    sq_tier_say(tier, &ndrop.tier, -1);
    sq_drums(t, p, tier, TY_HAT, TY_KICK, TY_SNARE, 80);
}
static int32_t dp_glow(uint32_t at, uint32_t ms, uint32_t len)   /* 256 just as at came, gone len ms later */
{
    uint32_t q = ms - at;
    if (!at || (int32_t)q < 0 || q >= len)
        return 0;
    return 256 - (int32_t)(q * 256u / len);
}
static void dp_scene(void)
{
    uint32_t ms = plat_ms(), k;
    int i, j;
    char n[12];
    itos((int32_t)ndrop.score, n);
    {
        char level[12] = "L";
        itos(ndrop.board + 1, level + 1);
        tcat(level, "/12");
        header("DROP", C_CYAN, level, C_PINK, n);
    }
    for (k = 0; k < NKEYS; k++) {        /* the keys along the top: where each drops (a ball just dropped: lit) */
        static const uint8_t WK[16] = PLAT_WHITE_KEYS;
        int32_t x = dp_key_x(k), g = dp_glow(ndrop.key_ms[k], ms, 300), white = 0, w;
        uint16_t c = (ui.keys >> k & 1u) ? C_INK : g ? mix565(C_LINE2, deg_col((int32_t)k), g) : C_LINE2;
        for (w = 0; w < 16; w++)
            white |= WK[w] == k;
        cv_rect(x - 1, white ? 22 : 21, 3, (white ? 3 : 2) + g / 64, white ? c : mix565(C_BG, c, 160));   /* (black
                                                                                     * keys' marks shorter, dimmer) */
    }
    for (i = 0; i < ndrop.np; i++) {        /* the platforms: outlined until lit; their note flashes them */
        const int16_t *s = ndrop.pl[i];
        uint16_t col = dp_pcol(i);
        int32_t g = dp_glow(ndrop.note_ms[i], ms, 220), h = dp_glow(ndrop.hit_ms[i], ms, 160);
        int32_t steps = 16, w = 2 + g / 100;
        int32_t lv = ndrop.lit[i] * 166 / DP_FULL + 90;   /* dim, brighter with each hit, full colour lit */
        uint16_t c = g ? mix565(mix565(C_LINE2, col, lv), C_INK, g * 3 / 4) : mix565(C_LINE2, col, lv);
        for (j = 0; j <= steps; j++) {
            int32_t x = s[0] + (s[2] - s[0]) * j / steps, y = s[1] + (s[3] - s[1]) * j / steps + (h ? (h / 90) : 0);
            cv_rect(x - w, y - w, 2 * w + 1, 2 * w + 1, c);
        }
        {                                /* its knob's number: under its middle, bright as it turns */
            int32_t tg = dp_glow(ndrop.tilt_ms[i], ms, 900);
            char kn[3] = {'K', (char)('1' + i % 4), 0};
            text_c((s[0] + s[2]) / 2, (s[1] + s[3]) / 2 + 6, &FONT_XS, kn,
                   tg ? mix565(C_LINE2, C_YEL, tg) : mix565(C_BG, C_LINE2, 150));
        }
        if (ndrop.lit[i] >= DP_FULL && g)   /* a glow above it as it sounds */
            for (j = 0; j <= steps; j += 4)
                cv_rect(s[0] + (s[2] - s[0]) * j / steps, s[1] + (s[3] - s[1]) * j / steps - 6 - g / 64, 1, 2,
                        mix565(C_BG, col, g));
    }
    for (i = 0; i < 8; i++) {            /* the floor: eight bars, each a note */
        int32_t g = dp_glow(ndrop.floor_ms[i], ms, 260), lit = ndrop.floor_lit[i];
        uint16_t col = deg_col(i * 2), base = lit ? col : mix565(C_BG, col, 60);   /* dark until lit */
        cv_rect(i * 30 + 1, DP_FLOOR + 4 - g / 64, 28, 18 + g / 64, g ? mix565(base, C_INK, g * 2 / 3) : base);
        cv_rect(i * 30 + 1, DP_FLOOR + 4 - g / 64, 28, 2, lit ? mix565(col, C_INK, 140) : mix565(C_BG, col, 100));
        if (lit && (ms / 400u + (uint32_t)i) % 8u == 0u)   /* lit ones twinkle */
            cv_rect(i * 30 + 12, DP_FLOOR + 10, 4, 4, C_INK);
    }
    for (i = 0; i < DP_NB; i++) {        /* the balls, their trails */
        dp_ball_t *b = &ndrop.b[i];
        uint16_t col;
        if (!b->on)
            continue;
        col = deg_col(b->col_k);
        for (j = 4; j >= 0; j--)
            disc(b->tx[j], b->ty[j], 3 - j / 2, mix565(C_BG, col, 180 - j * 34));
        disc((int32_t)b->x, (int32_t)b->y, (int32_t)DP_R, col);
        disc((int32_t)b->x - 1, (int32_t)b->y - 1, 1, C_INK);
    }
    {                                    /* the floor's bars lit, small, under the header */
        char t[12];
        itos(dp_floor_n(), t);
        tcat(t, "/8");
        text_c(222, 28, &FONT_T, t, ndrop.cleared ? C_YEL : C_GHOST);
    }
}

/* ================================================================= DODGE === */
#define BX_X0 10
#define BX_X1 230
#define BX_Y0 26
#define BX_Y1 230
#define BX_T 6                           /* the tiles' thickness */
#define BX_NB 6                          /* balls at most */
#define BX_R 4.0f
#define BX_ME 10.0f                      /* you: a rolling ball, this its radius */
#define BX_NO 5                          /* solid circular bumpers at most */
#define BX_EXIT_R 12
#define BX_EXIT_BALLS 3                  /* the exit opens early; hazards keep building until you leave */
#define BX_EVERY 4u                      /* bars you last for another ball */
typedef struct {
    float x, y, vx, vy;
    int16_t tx[6], ty[6];
    uint8_t on, col;
    uint32_t near_at;                    /* its last near miss (ms) */
} bx_ball_t;
typedef struct {
    bx_ball_t b[BX_NB];
    uint8_t lit[24], tier, round, nballs;
    uint32_t note_ms[24], note_t[24], hit_ms[24];
    float px, py, pvx, pvy, roll;       /* you: your middle, how you're rolling (px/ms), how far round (rad) */
    uint32_t last, score, star_at, full_at, wall_t, near_t;   /* wall_t: the eighth a wall note last took */
    uint32_t hurt_at, safe_until;        /* the last hit; after it, a moment no ball can hit you */
    uint32_t spawn_at;                   /* a ball due (ms: as its bar sounds), 0 none */
    uint16_t bars, best;                 /* bars lasted since the last hit; the most balls you've had */
    float sx, sy;                        /* the star (star_on 0: none) */
    uint8_t star_on, exit_on;
    uint16_t level;                      /* separate from the wall-colour round */
    float ex, ey;
    uint32_t obstacle_ms[BX_NO], obstacle_t;
} bx_t;
#define bbox (*(bx_t *)g_arena)
_Static_assert(sizeof(bx_t) <= sizeof g_arena, "bbox: the arena is too small");
/* the tiles: 0..7 the top (left to right), 8..15 the bottom, 16..19 the left (top down), 20..23 the right */
static int32_t bx_tnote(int i)
{
    if (i < 8)
        return h_tone(i + 4);
    if (i < 16)
        return h_tone(i - 8);
    if (i < 20)
        return h_tone(19 - i) - 12 + 12 * (i == 16);
    return h_tone(i - 20 + 8);
}
static uint16_t bx_tcol(int i)
{
    static const uint8_t SHIFT[4] = {0, 3, 6, 9};
    return deg_col((i % 8 + (i >= 16 ? 4 : 0) + SHIFT[bbox.round % 4u]) % 14);
}
static void bx_trect(int i, int32_t *x, int32_t *y, int32_t *w, int32_t *h)
{
    int32_t W = BX_X1 - BX_X0, H = BX_Y1 - BX_Y0;
    if (i < 16) {
        *x = BX_X0 + (i % 8) * W / 8, *w = W / 8 - 1, *h = BX_T;
        *y = i < 8 ? BX_Y0 : BX_Y1 - BX_T;
    } else {
        *y = BX_Y0 + BX_T + ((i - 16) % 4) * (H - 2 * BX_T) / 4, *h = (H - 2 * BX_T) / 4 - 1, *w = BX_T;
        *x = i < 20 ? BX_X0 : BX_X1 - BX_T;
    }
}
/* Every layout leaves the centre, corner spawn points, and a route around the bumpers clear. */
static const uint8_t BX_OBSTACLES[5][BX_NO][3] = {
    {{0, 0, 0}},
    {{80, 100, 16}, {160, 156, 16}},
    {{75, 91, 14}, {165, 91, 14}, {120, 177, 18}},
    {{75, 85, 15}, {165, 85, 15}, {75, 171, 15}, {165, 171, 15}},
    {{72, 83, 14}, {168, 83, 14}, {70, 165, 14}, {170, 165, 14}, {120, 198, 12}},
};
static int bx_layout(void)
{
    return bbox.level < 5u ? bbox.level : 3 + (bbox.level & 1u);
}
static int bx_obstacles(void)
{
    int i = 0;
    while (i < BX_NO && BX_OBSTACLES[bx_layout()][i][2])
        i++;
    return i;
}
static int bx_clear_spot(float x, float y, float r)
{
    int i;
    for (i = 0; i < bx_obstacles(); i++) {
        const uint8_t *o = BX_OBSTACLES[bx_layout()][i];
        float dx = x - o[0], dy = y - o[1], rr = r + o[2];
        if (dx * dx + dy * dy < rr * rr)
            return 0;
    }
    return 1;
}
/* Resolve penetration and reflect only incoming velocity. The player's marble bounces softly;
 * enemy balls keep their speed. Physics takes <=8 ms steps so neither can tunnel through a bumper. */
static void bx_bump(float *x, float *y, float *vx, float *vy, float r, int player)
{
    int i;
    for (i = 0; i < bx_obstacles(); i++) {
        const uint8_t *o = BX_OBSTACLES[bx_layout()][i];
        float dx = *x - o[0], dy = *y - o[1], rr = r + o[2], d2 = dx * dx + dy * dy, d, nx, ny, vn;
        if (d2 >= rr * rr)
            continue;
        d = fm_sqrtf(d2);
        nx = d > 0.001f ? dx / d : 1.0f;
        ny = d > 0.001f ? dy / d : 0.0f;
        *x = o[0] + nx * rr, *y = o[1] + ny * rr;
        vn = *vx * nx + *vy * ny;
        if (vn < 0.0f) {
            float bounce = player ? 1.4f : 2.0f;
            *vx -= bounce * vn * nx, *vy -= bounce * vn * ny;
            bbox.obstacle_ms[i] = toy_note_g(&bbox.obstacle_t, h_tone(i + 2), player ? 45 : 70, TY_LOW, 0.2f, 2u);
        }
    }
}
static void bx_open_exit(void)
{
    if (bbox.exit_on)
        return;
    /* The opposite corner: appearing under the player must not instantly skip the level. */
    bbox.ex = bbox.px < 120.0f ? 200.0f : 40.0f;
    bbox.ey = bbox.py < 128.0f ? 196.0f : 60.0f;
    bbox.exit_on = 1;
    juice_pop(120, 100, "EXIT OPEN!", C_CYAN);
    juice_text(120, 124, "ROLL INTO THE RING", C_GHOST);
    juice_ring((int32_t)bbox.ex, (int32_t)bbox.ey, 28, C_CYAN, 500);
    juice_sting(1);
}
static float bx_speed(void) { return 0.04f + 0.014f * (float)toy_speed; }   /* px/ms */
static void bx_add(void)                 /* another ball: from the corner farthest from you, away from you */
{
    int i;
    for (i = 0; i < BX_NB; i++)
        if (!bbox.b[i].on) {
            bx_ball_t *b = &bbox.b[i];
            float sx = bbox.px < 120.0f ? 1.0f : -1.0f, sy = bbox.py < 128.0f ? 1.0f : -1.0f;
            float a = 0.5f + (float)(kr() % 60u) * 0.01f;   /* (a steep-ish angle, never flat) */
            int j;
            b->x = sx > 0.0f ? 200.0f : 40.0f;
            b->y = sy > 0.0f ? 196.0f : 60.0f;
            b->vx = sx * fm_cosf(a) * bx_speed();
            b->vy = sy * fm_sinf(a) * bx_speed();
            b->on = 1;
            b->col = (uint8_t)(kr() % 14u);
            b->near_at = 0;
            for (j = 0; j < 6; j++)
                b->tx[j] = (int16_t)b->x, b->ty[j] = (int16_t)b->y;
            bbox.nballs++;
            if (bbox.nballs == BX_EXIT_BALLS)
                bx_open_exit();
            if (bbox.nballs > bbox.best)
                bbox.best = bbox.nballs;
            juice_ring((int32_t)b->x, (int32_t)b->y, 30, C_PINK, 320);
            juice_burst((int32_t)b->x, (int32_t)b->y, 12, 2, C_PINK, C_INK, 0);
            return;
        }
}
static void bx_start_level(void)
{
    int i;
    for (i = 0; i < 24; i++)
        bbox.lit[i] = 0, bbox.note_ms[i] = bbox.note_t[i] = bbox.hit_ms[i] = 0;
    for (i = 0; i < BX_NB; i++)
        bbox.b[i].on = 0;
    bbox.nballs = 0;
    bbox.tier = 0;
    bbox.round = 0;
    bbox.last = 0;
    bbox.bars = 0;
    bbox.hurt_at = 0;
    bbox.safe_until = plat_ms() + 1500u;
    bbox.spawn_at = 0;
    bbox.px = 120.0f, bbox.py = 128.0f;
    bbox.pvx = bbox.pvy = bbox.roll = 0.0f;
    bbox.exit_on = 0;
    bbox.wall_t = bbox.near_t = bbox.obstacle_t = 0;
    for (i = 0; i < BX_NO; i++)
        bbox.obstacle_ms[i] = 0;
    bbox.star_on = 0;
    bbox.star_at = plat_ms() + 5000u;
    bbox.full_at = 0;
    bx_add();
    {
        char label[20] = "LEVEL ";
        itos(bbox.level + 1, label + 6);
        juice_pop(120, 96, label, C_CYAN);
        juice_text(120, 132, "3 BALLS OPEN THE EXIT", C_GHOST);
    }
}
static void bx_new(void)
{
    bbox.level = 0;
    bbox.score = 0;
    bbox.best = 0;
    bx_start_level();
}
static void bx_next_level(void)
{
    bbox.score += 500;
    if (bbox.level < 65534u)
        bbox.level++;
    juice_calm();
    bx_start_level();
    juice_confetti(120, 110, 36);
    juice_sting(2);
}
static void bx_nav(int dx, int dy, int sel) { (void)dx, (void)dy, (void)sel; }   /* (you move on the held keys) */
static int bx_lit_n(void)
{
    int i, n = 0;
    for (i = 0; i < 24; i++)
        n += bbox.lit[i];
    return n;
}
static void bx_tile(int i, float x, float y, uint32_t ms)   /* a ball hit tile i at x, y */
{
    uint16_t col = bx_tcol(i);
    uint32_t t = grid_next_n(2u);
    if (bbox.wall_t != t) {              /* sparse: one wall note an eighth, the first ball there plays it */
        bbox.wall_t = t;
        bbox.note_ms[i] = toy_note_g(&bbox.note_t[i], bx_tnote(i), 84, TY_SEQ, 0.45f, 2u);
    }
    bbox.hit_ms[i] = ms | 1u;
    juice_burst((int32_t)x, (int32_t)y, 6, 2, col, C_INK, 0);
    if (!bbox.lit[i]) {
        bbox.lit[i] = 1;
        bbox.score += 10;
        juice_ring((int32_t)x, (int32_t)y, 18, col, 240);
    }
}
static void bx_light_some(int n)         /* a star: some tiles not yet lit, lit */
{
    int i, tries;
    for (tries = 0; n > 0 && tries < 40; tries++) {
        i = (int)(kr() % 24u);
        if (!bbox.lit[i]) {
            int32_t x, y, w, h;
            bbox.lit[i] = 1;
            bbox.hit_ms[i] = plat_ms() | 1u;
            bx_trect(i, &x, &y, &w, &h);
            juice_ring(x + w / 2, y + h / 2, 16, bx_tcol(i), 300);
            n--;
        }
    }
}
static void bx_hurt(int keep)            /* a ball got you: back to one ball (keep), the music drops */
{
    int i;
    uint32_t ms = plat_ms();
    for (i = 0; i < BX_NB; i++)
        if (i != keep && bbox.b[i].on) {
            juice_burst((int32_t)bbox.b[i].x, (int32_t)bbox.b[i].y, 12, 3, C_PINK, C_INK, 1);
            bbox.b[i].on = 0;
        }
    bbox.nballs = 1;
    bbox.bars = 0;
    bbox.spawn_at = 0;
    bbox.hurt_at = ms | 1u;
    bbox.safe_until = ms + 1500u;
    k_on(0, h_bass() - 12, 110, TY_LOW, 0.8f);
    juice_pop(120, 110, "HIT!", C_PINK);
    juice_shake(8, 400);
    juice_flash(RGB(255, 40, 80), 140, 350);
    juice_glitch(220);
    juice_ring((int32_t)bbox.px, (int32_t)bbox.py, 60, C_PINK, 400);
    juice_cool();
}
static void bx_tick(void)
{
    uint32_t dt = arc_dt(&bbox.last), ms = plat_ms();
    float f = (float)dt, sp = bx_speed();
    int i, j;
    {                                    /* you roll: the keys push, the ball keeps going and slows by itself (rolling
                                          * friction), so a turn takes a moment; off the walls it bounces, softly */
        float ax = (HELD_R ? 1.0f : 0.0f) - (HELD_L ? 1.0f : 0.0f), ay = (HELD_D ? 1.0f : 0.0f) - (HELD_U ? 1.0f : 0.0f);
        float v, lo, hi;
        bbox.pvx += ax * 0.0009f * f, bbox.pvy += ay * 0.0009f * f;
        bbox.pvx *= 1.0f - 0.0028f * f, bbox.pvy *= 1.0f - 0.0028f * f;   /* (the friction) */
        v = fm_sqrtf(bbox.pvx * bbox.pvx + bbox.pvy * bbox.pvy);
        if (v > 0.2f)
            bbox.pvx *= 0.2f / v, bbox.pvy *= 0.2f / v, v = 0.2f;
        for (uint32_t left = dt; left; ) {
            uint32_t step = left > 8u ? 8u : left;
            bbox.px += bbox.pvx * (float)step, bbox.py += bbox.pvy * (float)step;
            bx_bump(&bbox.px, &bbox.py, &bbox.pvx, &bbox.pvy, BX_ME, 1);
            left -= step;
        }
        bbox.roll += v * f / BX_ME;
        lo = (float)BX_X0 + BX_T + BX_ME, hi = (float)BX_X1 - BX_T - BX_ME;
        if (bbox.px < lo || bbox.px > hi) {
            bbox.px = fm_clampf(bbox.px, lo, hi), bbox.pvx = -bbox.pvx * 0.5f;
            if (v > 0.08f)
                k_on(0, 40, 50, TY_KICK, 0.05f);   /* (a thump) */
        }
        lo = (float)BX_Y0 + BX_T + BX_ME, hi = (float)BX_Y1 - BX_T - BX_ME;
        if (bbox.py < lo || bbox.py > hi) {
            bbox.py = fm_clampf(bbox.py, lo, hi), bbox.pvy = -bbox.pvy * 0.5f;
            if (v > 0.08f)
                k_on(0, 40, 50, TY_KICK, 0.05f);
        }
    }
    if (bbox.spawn_at && (int32_t)(ms - bbox.spawn_at) >= 0) {   /* another ball, as its bar begins */
        bbox.spawn_at = 0;
        if (bbox.nballs < BX_NB) {
            char bs[16];
            bx_add();
            itos(bbox.nballs, bs);
            tcat(bs, " BALLS");
            juice_text(120, 60, bs, C_PINK);
        }
    }
    if (!bbox.star_on && (int32_t)(ms - bbox.star_at) >= 0) {   /* a star appears */
        bbox.star_on = 1;
        int tries;
        for (tries = 0; tries < 32; tries++) {
            bbox.sx = 40.0f + (float)(kr() % 160u);
            bbox.sy = 50.0f + (float)(kr() % 140u);
            if (bx_clear_spot(bbox.sx, bbox.sy, BX_ME + 4.0f))
                break;
        }
        if (tries == 32)                 /* bounded search; the centre is clear in every layout */
            bbox.sx = 120.0f, bbox.sy = 128.0f;
        juice_ring((int32_t)bbox.sx, (int32_t)bbox.sy, 20, C_YEL, 300);
    }
    for (i = 0; i < BX_NB; i++) {
        bx_ball_t *b = &bbox.b[i];
        float v, k, dx, dy;
        if (!b->on)
            continue;
        for (j = 5; j > 0; j--)
            b->tx[j] = b->tx[j - 1], b->ty[j] = b->ty[j - 1];
        b->tx[0] = (int16_t)b->x, b->ty[0] = (int16_t)b->y;
        v = fm_sqrtf(b->vx * b->vx + b->vy * b->vy);   /* its speed: SPEED's */
        k = v > 0.0001f ? sp / v : 1.0f;
        b->vx *= k, b->vy *= k;
        if (fm_fabsf(b->vy) < 0.25f * sp)   /* (never flat along a wall forever) */
            b->vy = b->vy < 0.0f ? -0.25f * sp : 0.25f * sp;
        if (fm_fabsf(b->vx) < 0.25f * sp)
            b->vx = b->vx < 0.0f ? -0.25f * sp : 0.25f * sp;
        for (uint32_t left = dt; left; ) {
            uint32_t step = left > 8u ? 8u : left;
            b->x += b->vx * (float)step;
            b->y += b->vy * (float)step;
            bx_bump(&b->x, &b->y, &b->vx, &b->vy, BX_R, 0);
            left -= step;
            if (b->y < (float)BX_Y0 + BX_T + BX_R) {   /* the walls: each a tile, each a note */
                b->y = (float)BX_Y0 + BX_T + BX_R, b->vy = fm_fabsf(b->vy);
                bx_tile((int)clamp((int32_t)((b->x - BX_X0) * 8.0f / (float)(BX_X1 - BX_X0)), 0, 7), b->x, b->y, ms);
            } else if (b->y > (float)BX_Y1 - BX_T - BX_R) {
                b->y = (float)BX_Y1 - BX_T - BX_R, b->vy = -fm_fabsf(b->vy);
                bx_tile(8 + (int)clamp((int32_t)((b->x - BX_X0) * 8.0f / (float)(BX_X1 - BX_X0)), 0, 7), b->x, b->y, ms);
            }
            if (b->x < (float)BX_X0 + BX_T + BX_R) {
                b->x = (float)BX_X0 + BX_T + BX_R, b->vx = fm_fabsf(b->vx);
                bx_tile(16 + (int)clamp((int32_t)((b->y - BX_Y0 - BX_T) * 4.0f / (float)(BX_Y1 - BX_Y0 - 2 * BX_T)), 0, 3),
                        b->x, b->y, ms);
            } else if (b->x > (float)BX_X1 - BX_T - BX_R) {
                b->x = (float)BX_X1 - BX_T - BX_R, b->vx = -fm_fabsf(b->vx);
                bx_tile(20 + (int)clamp((int32_t)((b->y - BX_Y0 - BX_T) * 4.0f / (float)(BX_Y1 - BX_Y0 - 2 * BX_T)), 0, 3),
                        b->x, b->y, ms);
            }
        }
        dx = fm_fabsf(b->x - bbox.px), dy = fm_fabsf(b->y - bbox.py);
        if (dx * dx + dy * dy < (BX_ME + BX_R) * (BX_ME + BX_R)) {   /* it got you */
            if ((int32_t)(ms - bbox.safe_until) >= 0) {
                bx_hurt(i);
                return;
            }
        } else if (dx < BX_ME + BX_R + 9.0f && dy < BX_ME + BX_R + 9.0f && ms - b->near_at > 700u &&
                   (int32_t)(ms - bbox.safe_until) >= 0) {   /* a near miss: an accent, points */
            b->near_at = ms;
            bbox.score += 10u * bbox.nballs;
            toy_note_g(&bbox.near_t, h_tone(9), 96, TY_STAR, 0.3f, 2u);
            juice_text((int32_t)bbox.px, (int32_t)bbox.py - 14, "CLOSE!", C_YEL);
            juice_burst((int32_t)bbox.px, (int32_t)bbox.py, 6, 2, C_YEL, C_INK, 0);
            juice_heat(6);
        }
        if (bbox.star_on && fm_fabsf(b->x - bbox.sx) < 9.0f && fm_fabsf(b->y - bbox.sy) < 9.0f) {
            bbox.star_on = 0;                /* (a ball through the star: it's gone, nothing won) */
            bbox.star_at = ms + 6000u + kr() % 5000u;
            juice_burst((int32_t)bbox.sx, (int32_t)bbox.sy, 6, 1, C_GHOST, C_BG, 0);
        }
    }
    if (bbox.exit_on) {
        float dx = bbox.px - bbox.ex, dy = bbox.py - bbox.ey;
        if (dx * dx + dy * dy <= (float)(BX_EXIT_R * BX_EXIT_R)) {
            bx_next_level();
            return;
        }
    }
    if (bbox.star_on && fm_fabsf(bbox.px - bbox.sx) < 11.0f && fm_fabsf(bbox.py - bbox.sy) < 11.0f) {
        bbox.star_on = 0;                /* a star caught: points, a phrase, and some tiles lit */
        bbox.star_at = ms + 6000u + kr() % 5000u;
        bbox.score += 50;
        juice_pop((int32_t)fm_clampf(bbox.sx, 50.0f, 190.0f), (int32_t)bbox.sy - 14, "+50", C_YEL);
        juice_confetti((int32_t)bbox.sx, (int32_t)bbox.sy, 24);
        juice_ring((int32_t)bbox.sx, (int32_t)bbox.sy, 40, C_YEL, 400);
        juice_sting(1);
        bx_light_some(3);
    }
    if (bx_lit_n() == 24) {              /* a full circle: the walls go round again in new colours */
        int k2;
        bbox.round++;
        bbox.full_at = ms | 1u;
        bbox.score += 300;
        for (k2 = 0; k2 < 24; k2++)
            bbox.lit[k2] = 0;
        juice_calm();
        juice_pop(120, 100, "FULL CIRCLE!", C_YEL);
        juice_freeze(80);
        juice_flash(0xFFFF, 150, 260);
        juice_ring(120, 128, 150, C_YEL, 650);
        juice_ring(120, 128, 100, C_CYAN, 480);
        juice_confetti(120, 120, 50);
        juice_shake(5, 300);
        juice_sting(2);
    }
}
static void bx_groove(uint32_t t, uint32_t step)
{
    uint32_t p = step % 16u;
    int tier;
    if (p == 0u && (step / 16u) % 2u == 0u)
        h_deg_at(step / 32u);            /* a new chord every two bars: the walls' notes change with it */
    if (p == 0u) {                       /* a bar lasted: points, and every four another ball (on the next bar line) */
        k_on(t, h_bass(), 64, TY_BASS, 0.3f);
        if (!bbox.hurt_at || sq_ms(t) - bbox.hurt_at >= 1500u) {   /* (not while getting over a hit) */
            bbox.bars++;
            bbox.score += bbox.nballs;
            if (bbox.bars % BX_EVERY == 0u && bbox.nballs < BX_NB) {
                bbox.spawn_at = sq_ms(t);
                k_on(t, h_tone(5), 90, TY_STAR, 0.4f);   /* (here it comes: on the bar line) */
                k_on(t + (uint32_t)(SR_F * 0.06f), h_tone(7), 90, TY_STAR, 0.4f);
            }
        }
    }
    tier = bbox.nballs > 5 ? 5 : bbox.nballs - 1;   /* the drums: one part more for each ball */
    sq_tier_say(tier, &bbox.tier, -1);
    if (tier < bbox.tier)
        bbox.tier = (uint8_t)tier;
    sq_drums(t, p, tier, TY_HAT, TY_KICK, TY_SNARE, 70);
}
/* a glass marble, drawn a pixel at a time (17 Nov 2026, Nick: "more like a high def marble"): clear blue glass, dark
 * at its rim and bright through its middle, a cat's-eye ribbon of pink and gold inside that turns as it rolls (the
 * sphere rotated by roll about the axis across ux, uy), the window's highlight and a smaller one opposite (still: the
 * light doesn't roll), a thin bright rim where the light comes through, and on the floor its shadow with the light it
 * gathers (a caustic) on the far side. hurt: pink glass. shadow 0: none (the menu's icon) */
static void bx_marble(int32_t x, int32_t y, int32_t r, float roll, float ux, float uy, int hurt, int shadow)
{
    float cr = fm_cosf(roll), sr = -fm_sinf(roll), kx = -uy, ky = ux, R = (float)r;   /* (sr negated: it turns the way it rolls) */
    uint16_t core = hurt ? RGB(255, 140, 190) : RGB(80, 190, 255), rim = hurt ? RGB(70, 6, 30) : RGB(4, 14, 60);
    uint16_t rib1 = hurt ? RGB(255, 240, 200) : RGB(255, 60, 150), rib2 = hurt ? RGB(255, 255, 255) : RGB(255, 200, 60);
    int32_t dx, dy;
    if (shadow) {
        disc(x + 3, y + 4, r, RGB(4, 2, 10));                              /* its shadow */
        disc(x + 5, y + 6 + r / 2, r / 3, mix565(RGB(4, 2, 10), core, 50));    /* the light it gathers */
    }
    for (dy = -r; dy <= r; dy++)
        for (dx = -r; dx <= r; dx++) {
            float nx = ((float)dx + 0.5f) / R, ny = ((float)dy + 0.5f) / R, n2 = nx * nx + ny * ny, nz, t, px, py, pz, kv;
            float band, hl;
            uint16_t c;
            if (n2 > 1.0f)
                continue;
            nz = fm_sqrtf(1.0f - n2);
            t = nz;                      /* 1 the middle, 0 the rim */
            c = mix565(rim, core, (int32_t)(256.0f * (0.08f + 0.92f * t * t * (1.4f - 0.4f * t)) * (0.7f + 0.3f * (-nx - ny + 1.0f) * 0.5f)));
            /* the ribbon inside: the point rotated with the roll (Rodrigues, about k), a thin twisted sheet through
             * the middle, seen a little shrunk (it's inside the glass) and faded toward the rim */
            kv = kx * nx + ky * ny;
            px = nx * cr + (ky * nz) * sr + kx * kv * (1.0f - cr);
            py = ny * cr + (-kx * nz) * sr + ky * kv * (1.0f - cr);
            pz = nz * cr + (kx * ny - ky * nx) * sr;
            band = px * 0.9f + 0.35f * fm_sinf(3.0f * py + 1.5f * pz);
            if (t > 0.25f) {
                float a = band < 0.0f ? -band : band;
                if (a < 0.2f)
                    c = mix565(c, rib1, (int32_t)((1.0f - a / 0.2f * 0.6f) * 245.0f * t));
                else {
                    float b2 = pz * 0.9f - 0.3f * fm_sinf(3.0f * px), a2 = b2 < 0.0f ? -b2 : b2;
                    if (a2 < 0.12f)
                        c = mix565(c, rib2, (int32_t)((1.0f - a2 / 0.12f * 0.6f) * 220.0f * t));
                }
            }
            if (t < 0.32f && nx + ny > 0.45f)   /* the rim lit where the light comes through */
                c = mix565(c, core, (int32_t)((0.32f - t) / 0.32f * 220.0f));
            else if (t < 0.18f)
                c = mix565(c, RGB(2, 6, 20), 90);
            hl = (nx + 0.38f) * (nx + 0.38f) + (ny + 0.42f) * (ny + 0.42f);   /* the window's highlight */
            if (hl < 0.035f)
                c = RGB(255, 255, 255);
            else if (hl < 0.12f)
                c = mix565(c, RGB(255, 255, 255), (int32_t)((0.12f - hl) / 0.085f * 150.0f));
            hl = (nx - 0.42f) * (nx - 0.42f) + (ny - 0.5f) * (ny - 0.5f);    /* a smaller one, opposite */
            if (hl < 0.018f)
                c = mix565(c, RGB(255, 255, 255), 140);
            cv_pset(x + dx, y + dy, c);
        }
}
static void bx_scene(void)
{
    uint32_t ms = plat_ms();
    int i, j;
    char n[12];
    itos((int32_t)bbox.score, n);
    {
        char level[12] = "L";
        itos(bbox.level + 1, level + 1);
        header("DODGE", C_CYAN, level, C_PINK, n);
    }
    for (i = 0; i < bbox.nballs && i < BX_NB; i++)   /* the balls in play, in the header */
        disc(124 + i * 8, 10, 2, C_PINK);
    for (i = 0; i < 24; i++) {           /* the tiles: dim until lit; a note flashes them, bulging in */
        int32_t x, y, w, h, g = dp_glow(bbox.note_ms[i], ms, 240), bulge = g / 64;
        uint16_t col = bx_tcol(i), c;
        bx_trect(i, &x, &y, &w, &h);
        c = bbox.lit[i] ? (g ? mix565(col, C_INK, g * 3 / 4) : col) : mix565(C_BG, col, 70 + g / 2);
        if (i < 8)
            h += bulge;
        else if (i < 16)
            y -= bulge, h += bulge;
        else if (i < 20)
            w += bulge;
        else
            x -= bulge, w += bulge;
        cv_rect(x, y, w, h, c);
    }
    for (i = 0; i < bx_obstacles(); i++) {
        const uint8_t *o = BX_OBSTACLES[bx_layout()][i];
        int32_t g = dp_glow(bbox.obstacle_ms[i], ms, 240);
        uint16_t col = mix565(C_LINE2, C_CYAN, 100 + g / 2);
        disc(o[0] + 2, o[1] + 3, o[2], RGB(4, 2, 10));
        disc(o[0], o[1], o[2], col);
        disc(o[0], o[1], o[2] - 3, mix565(C_BG, col, 80));
        disc(o[0] - 3, o[1] - 4, 3, mix565(col, C_INK, 120));
    }
    if (bbox.exit_on) {
        int32_t x = (int32_t)bbox.ex, y = (int32_t)bbox.ey, r = BX_EXIT_R + (int32_t)(ms / 180u % 3u);
        disc(x, y, r, C_CYAN);
        disc(x, y, r - 2, C_BG);
        disc(x, y, 2, C_INK);
        text_c(x, y - 24, &FONT_XS, "EXIT", C_CYAN);
    }
    if (bbox.star_on) {                  /* the star: spinning, pulsing */
        int32_t sx = (int32_t)bbox.sx, sy = (int32_t)bbox.sy, r = 5 + (int32_t)((ms / 120u) % 3u);
        uint32_t a = ms / 90u % 4u;
        disc(sx, sy, r + 3, mix565(C_BG, C_YEL, 60));
        cv_rect(sx - r, sy - (a & 1u), 2 * r + 1, 2, C_YEL);
        cv_rect(sx - (a & 1u), sy - r, 2, 2 * r + 1, C_YEL);
        disc(sx, sy, 3, C_INK);
    }
    for (i = 0; i < BX_NB; i++) {        /* the balls, comet tails */
        bx_ball_t *b = &bbox.b[i];
        uint16_t col;
        if (!b->on)
            continue;
        col = deg_col(b->col);
        for (j = 5; j >= 0; j--)
            disc(b->tx[j], b->ty[j], 3 - j / 2, mix565(C_BG, col, 190 - j * 30));
        disc((int32_t)b->x, (int32_t)b->y, (int32_t)BX_R, col);
        disc((int32_t)b->x - 1, (int32_t)b->y - 1, 1, C_INK);
    }
    {                                    /* you: a glass marble rolling the way it goes (it blinks while safe after
                                          * a hit, flushed pink a moment when hit) */
        int32_t x = (int32_t)bbox.px, y = (int32_t)bbox.py;
        int safe = (int32_t)(ms - bbox.safe_until) < 0, hurt = bbox.hurt_at && ms - bbox.hurt_at < 400u;
        float v = fm_sqrtf(bbox.pvx * bbox.pvx + bbox.pvy * bbox.pvy), ux = v > 0.001f ? bbox.pvx / v : 1.0f,
              uy = v > 0.001f ? bbox.pvy / v : 0.0f;
        if (!safe || (ms / 90u) & 1u)
            bx_marble(x, y, (int32_t)BX_ME, bbox.roll, ux, uy, hurt, 1);
    }
}
