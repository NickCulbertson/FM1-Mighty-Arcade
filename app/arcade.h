/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Nick Culbertson */
/* arcade.h: three action games for Mighty Arcade's menu, played on held keys (left and right, up, down, A). Included
 * once from app.c, after its helpers; their state lives in the shared arena. Each its own design and name (the ideas
 * are old and free; the famous looks and names are not ours to use):
 *
 *  BRICK BREAK  a paddle, a ball, a wall of bricks: each row a pitch, each column a step of the scale
 *  SWARM        a rocket against a swarm of creatures, each kind flying and firing its own way; each one hit
 *               goes in the jar, an eight-step loop that plays them, each sway a note of the bass (quicker as it thins); shoot them,
 *               each one its note. (17 Nov 2026: no longer rows marching to the edge and stepping down over a cannon,
 *               the famous invaders game's design; the swarm weaves, circles and swoops in the upper sky, so the first
 *               creatures, its own designs, came back at Nick's call)
 *  ROCK DRIFT   a ship among drifting rocks: left and right turn, up thrusts, A fires; a rock breaks into smaller,
 *               higher notes. A sequencer: a radar sweep plays the rocks it crosses, once a bar
 * (DOT MUNCHER was cut, 16 Nov 2026: a maze of dots with chasers is Pac-Man's design, cf. Atari v. Philips 1982) */

/* held like a game controller (Nick, 18 Nov 2026): the pad under the left hand at the keyboard's bottom end, F3 left,
 * F#3 up, G3 down, A3 right; the select key under the right hand at its top end, F5 (one key, Nick said: F,
 * as the pad's left) */
#define PAD_L 0u
#define PAD_U 1u
#define PAD_D 2u
#define PAD_R 4u
#define PAD_S0 24u                       /* F5 */
#define PAD_S1 26u                       /* G5: CUBES' drop, the second button on the right */
#define PAD_SEL_KEYS (1u << PAD_S0)
#define HELD_L ((ui.keys >> PAD_L) & 1u)
#define HELD_R ((ui.keys >> PAD_R) & 1u)
#define HELD_U ((ui.keys >> PAD_U) & 1u)
#define HELD_D ((ui.keys >> PAD_D) & 1u)
#define HELD_S ((ui.keys & PAD_SEL_KEYS) != 0u)
#define HELD_S1 ((ui.keys >> PAD_S1) & 1u)   /* G5, the second button on the right */
static uint32_t arc_dt(uint32_t *last)   /* ms since the last frame, at most 40 (a stall never jumps far) */
{
    uint32_t ms = plat_ms(), d = *last ? ms - *last : 16u;
    *last = ms;
    return d > 40u ? 40u : d;
}
static void over_box(const char *a, const char *b)
{
    cv_rect(30, 92, 180, 52, C_BG);
    frame_rect(30, 92, 180, 52, C_YEL);
    text_c(120, 96, &FONT_M, a, C_YEL);
    text_c(120, 126, &FONT_T, b, C_GHOST);
}
static void lives_draw(int n)
{
    int i;
    for (i = 0; i < n && i < 5; i++)
        disc(120 + i * 9, 10, 3, C_PINK);
}
enum { NA_HIT, NA_BASS, NA_HAT, NA_KICK, NA_BOOM, NA_WIN, NA_SNARE, NA_PAD, NA_SEQ };   /* NA_SEQ: the playhead's
                                                                                     * (BRICK BREAK, STAR SWARM) */
static void br_room(void)                /* BOUNCE: more room, and a dotted-eighth echo at the tempo (Nick, 17 Nov 2026) */
{
    k_main.rev_mix = 0.28f, k_main.rev_secs = 2.6f;
    k_main.dly_samples = SR_F * 45.0f / (float)sq_cfg[SQ_BRICK].bpm;
    k_main.dly_mix = 0.24f, k_main.dly_fb = 0.42f;
}
static void arc_sounds(int hit)
{
    static const uint8_t HIT[4] = {S_MARIMBA, S_CELESTA, S_VIBES, S_CHIP};
    k_slot_set(NA_HIT, &KSND[HIT[hit & 3]]);
    k_slot_set(NA_BASS, &KSND[S_FMBASS]);
    k_slot_set(NA_HAT, &KSND[S_HAT]);
    k_slot_set(NA_KICK, &KSND[S_KICK]);
    k_slot_set(NA_BOOM, &KSND[S_SONAR]);
    k_slot_set(NA_WIN, &KSND[S_TUBE]);
    k_slot_set(NA_SNARE, &KSND[S_SNARE]);
    k_slot_set(NA_PAD, &KSND[S_BRASSPAD]);
    k_slot_rel(NA_HIT, 0.4f);
    k_slot_rel(NA_BASS, 0.15f);
    k_slot_rel(NA_BOOM, 0.8f);
    k_slot_rel(NA_WIN, 1.2f);
    k_slot_rel(NA_PAD, 1.4f);
    k_main_slot = NA_HIT;
    k_main.rev_mix = 0.14f, k_main.rev_secs = 1.6f, k_main.shimmer = 0.0f, k_main.dly_mix = 0.08f;
    if (hit <= 2) {                      /* the sequencer games: their playhead's sound (K3) */
        sq_slot = NA_SEQ;
        sq_sound(hit == 0 ? SQ_BRICK : hit == 1 ? SQ_SWARM : SQ_DRIFT);
    }
    if (hit == 0)
        br_room();
    if (hit == 2)                        /* ROCK DRIFT: space, and room for the sweep's notes to ring */
        k_main.rev_mix = 0.26f, k_main.rev_secs = 3.2f, k_main.shimmer = 0.04f, k_main.dly_mix = 0.16f;
}
static void arc_groove(uint32_t t, uint32_t step, uint32_t lv, int on)
{
    uint32_t p = step % 16u;
    h_deg_at(step / 16u);                /* a new chord each bar, at random in the key */
    if (!on)
        return;
    if (p % 4u == 2u)
        k_on(t, 92, 40, NA_HAT, 0.04f);
    if (p == 0u || p == 8u || (lv >= 2u && p == 6u))
        k_on(t, h_bass(), 80, NA_BASS, 0.12f);
    if (lv >= 2u && (p == 0u || p == 8u))
        k_on(t, 36, 90, NA_KICK, 0.2f);
    if (lv >= 3u && (p == 4u || p == 12u))
        k_on(t, 50, 70, NA_SNARE, 0.2f);
}
static void arc_win(void)
{
    int k;
    for (k = 0; k < 5; k++)
        k_on(k_at(0.1f * (float)k), degree_note(7 + 2 * k), 100, NA_WIN, 1.0f);
}

/* =========================================================== BRICK BREAK === */
typedef struct {
    uint8_t b[6][10];
    float px, bx, by, vx, vy;
    uint8_t stuck, lives, over, level;
    uint32_t score, last, over_at;
    uint8_t combo;                       /* bricks since the paddle: each one worth more, higher */
    uint32_t squash_at;                  /* the paddle hit (ms): it squashes */
    int16_t tx[6], ty[6];                /* the ball's trail */
    uint8_t hp[6][10];                   /* a brick's hits left: the top rows take two (cracked after one) */
    uint32_t hit_at[6][10];              /* a brick hit (ms): it flashes, jolts */
    float cap_x, cap_y;                  /* a power-up falling (cap_k 0: none): 1 wide, 2 a life, 3 slow */
    uint8_t cap_k;
    uint32_t wide_until, slow_until;
    float top, top_from;                 /* the wall's first row (px): it creeps down the screen, a row at a time */
    uint32_t drop_at;                    /* the last step down (ms): it slides */
    uint8_t bar;                         /* the music's bar it last counted */
    uint8_t bars;                        /* bars since it last stepped */
    /* the sequencer: the playhead walks the columns, an eighth each, and plays each one's lowest brick */
    uint32_t seq_ms[10];                 /* when a column's brick last sounded (it lights) */
    uint8_t total, tier;                 /* the wall's bricks at its start; the drums so far (named once) */
} br_t;
static uint32_t br_best;
#define br (*(br_t *)g_arena)
_Static_assert(sizeof(br_t) <= sizeof g_arena, "br: the arena is too small");
static float br_speed(void) { return (0.15f + 0.015f * (float)br.level) * (plat_ms() < br.slow_until ? 0.7f : 1.0f); }
static float br_w(void) { return plat_ms() < br.wide_until ? 58.0f : 38.0f; }
/* each level its own wall ('.' none, '1' a brick, '2' a tough one), in turn; round again, every brick tougher */
#define BR_NL 8
static const char *const BR_NAME[BR_NL] = {"CLASSIC", "CHECKERS", "PYRAMID", "TOWERS", "ZIGZAG", "DIAMOND",
                                            "FORTRESS", "HEART"};
static const char *const BR_LAYOUT[BR_NL][6] = {
    {"2222222222", "1111111111", "1111111111", "1111111111", "..........", ".........."},
    {"2.2.2.2.2.", ".1.1.1.1.1", "1.1.1.1.1.", ".1.1.1.1.1", "1.1.1.1.1.", ".1.1.1.1.1"},
    {"....22....", "...1111...", "..111111..", ".11111111.", "1111111111", "2222222222"},
    {"22......22", "11.2222.11", "11.1111.11", "11.1..1.11", "11.1..1.11", "11......11"},
    {"22..22..22", ".11..11..1", "1..11..11.", "11..11..11", ".11..11..1", "1..11..11."},
    {"....22....", "...1221...", "..112211..", ".11122111.", "..112211..", "...1221..."},   /* (was INVADER: a famous game's alien, cut 17 Nov 2026) */
    {"2222222222", "2........2", "2.111111.2", "2.1....1.2", "2.111111.2", "2........2"},
    {".111..111.", "1111111111", "1111111111", ".11111111.", "..111111..", "...1111..."}};
static int br_bars_per_step(void)        /* how often the wall steps down: not on the first wall; then every 4
                                          * bars, 3, then 2 */
{
    return br.level == 0 ? 0 : br.level < 3 ? 4 : br.level < 6 ? 3 : 2;
}
static void br_wall(void)
{
    int r, c, L = br.level % BR_NL, hard = br.level >= BR_NL;
    for (r = 0; r < 6; r++)
        for (c = 0; c < 10; c++) {
            char ch = BR_LAYOUT[L][r][c];
            br.b[r][c] = (uint8_t)(ch == '.' ? 0 : 1 + r);
            br.hp[r][c] = (uint8_t)(ch == '2' || (hard && ch == '1' && r < 3) ? 2 : ch == '.' ? 0 : 1);
            br.hit_at[r][c] = 0;
        }
    br.total = 0;
    for (r = 0; r < 6; r++)
        for (c = 0; c < 10; c++)
            br.total = (uint8_t)(br.total + (br.b[r][c] ? 1 : 0));
    br.tier = 0;
    for (c = 0; c < 10; c++)
        br.seq_ms[c] = 0;
    br.stuck = 1;
    br.cap_k = 0;
    br.top = br.top_from = 40.0f;
    br.drop_at = 0;
    br.bars = 0;
    br.bar = J.bar;
    {
        char ls[12] = "LEVEL ";
        itos(br.level + 1, ls + 6);
        juice_pop(120, 96, ls, C_CYAN);
        juice_text(120, 132, BR_NAME[L], C_YEL);
    }
}
static int32_t br_top(void)              /* the wall's first row now (sliding to its new place) */
{
    uint32_t q = plat_ms() - br.drop_at;
    float p = br.drop_at && q < 260u ? (float)q / 260.0f : 1.0f;
    p = 1.0f - (1.0f - p) * (1.0f - p);
    return (int32_t)(br.top_from + (br.top - br.top_from) * p);
}
static int br_low_row(void)              /* the lowest row with a brick in it (-1 none) */
{
    int r, c;
    for (r = 5; r >= 0; r--)
        for (c = 0; c < 10; c++)
            if (br.b[r][c])
                return r;
    return -1;
}
static int br_seq_row(int c)             /* the brick a column plays: its lowest, the one facing you (-1 none) */
{
    int r;
    for (r = 5; r >= 0 && !br.b[r][c]; r--)
        ;
    return r;
}
/* the music: the wall is the sequence. A playhead walks its ten columns, an eighth note each (ten eighths round,
 * so it slips against the four-four drums and the bar), and each column plays its lowest brick: the row its note,
 * up the chord (the top rows highest), a tough one louder; CHANCE (K2) how many sound. Clear the wall and the
 * drums come in: hi-hats, the kick, the snare, the full kit, double time */
static void br_groove(uint32_t t, uint32_t step)
{
    uint32_t p = step % 16u;
    int tier, left = 0, r, c;
    h_deg_at(step / 16u);                /* a new chord each bar, at random in the key */
    if (br.over)
        return;
    for (r = 0; r < 6; r++)
        for (c = 0; c < 10; c++)
            left += br.b[r][c] ? 1 : 0;
    tier = sq_tier_of(br.total - left, br.total);
    sq_tier_say(tier, &br.tier, 200);
    if (p == 0u || p == 8u)
        k_on(t, h_bass(), 72, NA_BASS, 0.12f);
    sq_drums(t, p, tier, NA_HAT, NA_KICK, NA_SNARE, 85);
    if (!(p & 1u)) {
        c = (int)(step / 2u % 10u);
        r = br_seq_row(c);
        if (r >= 0 && sq_roll(SQ_BRICK)) {
            k_on(t, h_tone(5 - r + 3), (br.hp[r][c] > 1 ? 96u : 78u) + (p % 8u == 0u ? 10u : 0u), NA_SEQ, 0.25f);
            br.seq_ms[c] = sq_ms(t);
        }
    }
}
static void br_creep(void)               /* on the music's bars: the wall steps down; reaching you, it crushes */
{
    int every = br_bars_per_step();
    if (br.bar == J.bar)
        return;
    br.bar = J.bar;
    if (!every || br.stuck || br.over)
        return;
    if (++br.bars < every)
        return;
    br.bars = 0;
    br.top_from = (float)br_top();
    br.top += 12.0f;
    br.drop_at = plat_ms() | 1u;
    k_on(0, 33, 110, NA_KICK, 0.3f);     /* a thud, on the downbeat */
    k_on(0, h_bass(), 90, NA_BASS, 0.3f);
    juice_shake(3, 200);
    juice_kick(0, 4);
    if (br.top + (float)(br_low_row() * 12 + 10) >= 214.0f) {   /* it reached you: a life, and back up it goes */
        k_on(0, degree_note(-5), 110, NA_BOOM, 0.8f);
        juice_pop(120, 150, "CRUSHED!", C_PINK);
        juice_shake(9, 500);
        juice_freeze(120);
        juice_flash(RGB(255, 40, 80), 150, 500);
        juice_glitch(350);
        J.heat /= 3;
        br.top_from = (float)br_top();
        br.top = 40.0f;
        br.stuck = 1;
        if (--br.lives == 0) {
            if (br.score > br_best)
                br_best = br.score;
            br.over = 1;
            br.over_at = plat_ms();
        }
    } else if (br.top + (float)(br_low_row() * 12 + 10) >= 190.0f)
        juice_text(120, 200, "LOOK OUT!", C_PINK);
}
static void br_new(void)
{
    br.level = 0;
    br.lives = 3;
    br.score = 0;
    br.over = 0;
    br.px = 120.0f;
    br.last = 0;
    br_wall();
}
static void br_nav(int dx, int dy, int sel)
{
    (void)dx, (void)dy;
    if (br.over) {
        if (sel && plat_ms() - br.over_at > 800u)
            br_new();
        return;
    }
    if (sel && br.stuck) {               /* away */
        float s = br_speed();
        br.stuck = 0;
        br.vx = s * 0.4f;
        br.vy = -s * 0.92f;
        juice_ring((int32_t)br.bx, (int32_t)br.by, 18, C_YEL, 220);
    }
}
static void br_tick(void)
{
    uint32_t dt = arc_dt(&br.last), i;
    float w = br_w();
    if (br.over)
        return;
    br_creep();
    if (HELD_L)
        br.px -= 0.24f * (float)dt;
    if (HELD_R)
        br.px += 0.24f * (float)dt;
    br.px = fm_clampf(br.px, w / 2.0f, 240.0f - w / 2.0f);
    if (br.cap_k) {                      /* the power-up falls; the paddle catches it */
        br.cap_y += 0.07f * (float)dt;
        if (br.cap_y >= 224.0f && br.cap_y <= 234.0f && fm_fabsf(br.cap_x - br.px) <= w / 2.0f + 6.0f) {
            static const char *const CN[4] = {"", "WIDE!", "+1 LIFE", "SLOW-MO"};
            juice_pop((int32_t)br.cap_x < 60 ? 60 : (int32_t)br.cap_x > 180 ? 180 : (int32_t)br.cap_x, 150, CN[br.cap_k], C_YEL);
            juice_ring((int32_t)br.cap_x, 228, 40, C_YEL, 350);
            juice_burst((int32_t)br.cap_x, 228, 16, 3, C_YEL, C_CYAN, 1);
            k_on(0, degree_note(14), 100, NA_HIT, 0.5f);
            k_on(k_at(0.08f), degree_note(18), 100, NA_HIT, 0.5f);
            if (br.cap_k == 1)
                br.wide_until = plat_ms() + 12000u;
            else if (br.cap_k == 2 && br.lives < 5)
                br.lives++;
            else if (br.cap_k == 3)
                br.slow_until = plat_ms() + 8000u;
            br.cap_k = 0;
        } else if (br.cap_y > 245.0f)
            br.cap_k = 0;
    }
    for (i = 5u; i > 0u; i--)            /* the trail: where it was */
        br.tx[i] = br.tx[i - 1], br.ty[i] = br.ty[i - 1];
    br.tx[0] = (int16_t)br.bx, br.ty[0] = (int16_t)br.by;
    if (br.stuck) {
        br.bx = br.px;
        br.by = 222.0f;
        for (i = 0; i < 6u; i++)
            br.tx[i] = (int16_t)br.bx, br.ty[i] = (int16_t)br.by;
        return;
    }
    for (i = 0; i < 4u; i++) {           /* in quarter steps: never through a brick */
        int r, c;
        br.bx += br.vx * (float)dt / 4.0f;
        br.by += br.vy * (float)dt / 4.0f;
        if (br.bx < 3.0f)
            br.vx = fm_fabsf(br.vx), br.bx = 3.0f;
        if (br.bx > 237.0f)
            br.vx = -fm_fabsf(br.vx), br.bx = 237.0f;
        if (br.by < 27.0f)
            br.vy = fm_fabsf(br.vy), br.by = 27.0f;
        if (br.vy > 0.0f && br.by >= 222.0f && br.by <= 230.0f && fm_fabsf(br.bx - br.px) <= w / 2.0f + 3.0f) {
            float rel = (br.bx - br.px) / (w / 2.0f), s = br_speed();
            br.vx = s * 0.85f * fm_clampf(rel, -1.0f, 1.0f);
            br.vy = -fm_sqrtf(s * s - br.vx * br.vx);
            {                            /* the paddle: the bricks' own sound, a new tone of the chord
                                          * each hit, up and round (Nick, 19 Nov 2026) */
                static uint8_t pn;
                k_on(0, h_tone(1 + pn++ % 6u), 90, NA_HIT, 0.3f);
            }
            br.combo = 0;
            br.squash_at = plat_ms() | 1u;
            juice_burst((int32_t)br.bx, 226, 4, 1, C_CYAN, C_INK, 0);
        }
        if (br.by > 242.0f) {            /* missed */
            k_on(0, degree_note(-5), 100, NA_BOOM, 0.6f);
            juice_shake(8, 500);
            juice_freeze(120);
            juice_flash(RGB(255, 40, 80), 150, 500);
            juice_ring((int32_t)br.bx, 236, 70, C_PINK, 450);
            juice_burst((int32_t)br.bx, 234, 24, 4, C_PINK, C_YEL, 1);
            juice_kick(0, 10);
            juice_glitch(350);
            J.heat /= 3;                 /* the music sags with you */
            br.combo = 0;
            br.wide_until = br.slow_until = 0;
            if (br.lives > 1)
                juice_pop(120, 150, oops_word(), C_PINK);
            if (--br.lives == 0) {
                if (br.score > br_best)
                    br_best = br.score;
                br.over = 1;
                br.over_at = plat_ms();
            }
            br.stuck = 1;
            return;
        }
        r = ((int)br.by - (int)br.top) / 12;
        c = ((int)br.bx - 5) / 23;
        if (br.by >= br.top && r < 6 && c >= 0 && c < 10 && br.b[r][c] && br.hp[r][c] > 1) {   /* a tough one: cracks */
            br.hp[r][c]--;
            br.hit_at[r][c] = plat_ms() | 1u;
            br.vy = -br.vy;
            k_on(0, degree_note(c + 2 * (5 - r) + 2) - 12, 100, NA_HIT, 0.3f);
            juice_burst(6 + c * 23 + 10, (int32_t)br.top + r * 12 + 5, 5, 1, C_INK, RGB(150, 140, 180), 1);
            juice_shake(2, 80);
            continue;
        }
        if (br.by >= br.top && r < 6 && c >= 0 && c < 10 && br.b[r][c]) {
            br.b[r][c] = 0;
            br.vy = -br.vy;
            if (!br.cap_k && kr() % 7u == 0u) {   /* now and then: a power-up drops */
                br.cap_k = (uint8_t)(1u + kr() % 3u);
                br.cap_x = (float)(6 + c * 23 + 10);
                br.cap_y = (float)((int32_t)br.top + r * 12 + 5);
            }
            br.combo = (uint8_t)(br.combo < 9 ? br.combo + 1 : 9);
            br.score += (uint32_t)(10 * (6 - r) * br.combo);
            k_on(0, degree_note(c + 2 * (5 - r) + 2 + br.combo - 1), 96, NA_HIT, 0.4f);   /* a combo climbs the scale */
            juice_burst(6 + c * 23 + 10, (int32_t)br.top + r * 12 + 5, 7 + br.combo, 2, deg_col(r * 2 + 1), C_INK, 1);
            juice_shake(2 + br.combo / 2, 110);
            juice_heat(4 + 2 * br.combo);
            juice_kick(br.vx > 0 ? 2 + br.combo / 2 : -2 - br.combo / 2, -2 - br.combo / 2);   /* the ball's way */
            if (br.combo >= 6)
                juice_embers(6 + c * 23 + 10, (int32_t)br.top + r * 12 + 5, 3, C_YEL);
            if (br.combo == 9)
                juice_impact(40);
            if (br.combo >= 4) {
                juice_freeze(25);
                juice_ring(6 + c * 23 + 10, (int32_t)br.top + r * 12 + 5, 16 + 3 * br.combo, C_YEL, 220);
            }
            if (br.combo >= 2) {
                char cs[6] = {'X', 0};
                itos(br.combo, cs + 1);
                juice_text(6 + c * 23 + 10, (int32_t)br.top + r * 12 + 12, cs, C_YEL);
            }
            {
                int left = 0, rr, cc;
                for (rr = 0; rr < 6; rr++)
                    for (cc = 0; cc < 10; cc++)
                        left += br.b[rr][cc] != 0;
                if (!left) {
                    juice_flash(RGB(255, 255, 255), 180, 600);
                    juice_shake(9, 600);
                    juice_freeze(160);
                    juice_confetti(120, 80, 50);
                    juice_ring(120, 90, 150, C_YEL, 700);
                    juice_pop(120, 110, "WALL DOWN!", C_YEL);
                    juice_celebrate(120, 90);
                    br.level++;
                    arc_win();
                    br_wall();
                    return;
                }
            }
        }
    }
}
static void br_scene(void)
{
    int r, c;
    char n[12];
    itos((int32_t)br.score, n);
    header("BOUNCE", C_CYAN, "", C_PINK, n);
    lives_draw(br.lives);
    for (r = 0; r < 6; r++)
        for (c = 0; c < 10; c++)
            if (br.b[r][c]) {
                uint16_t col = deg_col(r * 2 + 1);
                uint32_t hq = plat_ms() - br.hit_at[r][c];
                int32_t jx = br.hit_at[r][c] && hq < 120u ? ((hq / 30u) & 1u ? 2 : -2) : 0;
                int32_t bx = 6 + c * 23 + jx, by = br_top() + r * 12;
                if (br.hit_at[r][c] && hq < 90u)
                    col = C_INK;
                {                        /* the playhead's brick: it lights and hops as it sounds */
                    uint32_t sq = plat_ms() - br.seq_ms[c];
                    if (br.seq_ms[c] && (int32_t)sq >= 0 && sq < 140u && br_seq_row(c) == r) {
                        col = mix565(col, C_INK, 150);
                        by -= 2 - (int32_t)sq / 70;
                    }
                }
                cv_rect(bx, by, 21, 10, col);
                cv_rect(bx, by, 21, 2, mix565(col, C_INK, 120));
                cv_rect(bx, by + 8, 21, 2, mix565(col, C_BG, 120));
                if (br.hp[r][c] > 1) {   /* tough: a steel band */
                    frame_rect(bx + 1, by + 1, 19, 8, mix565(col, C_INK, 200));
                } else if (br.hit_at[r][c]) {   /* cracked */
                    cv_rect(bx + 6, by + 2, 1, 3, C_BG);
                    cv_rect(bx + 7, by + 5, 1, 3, C_BG);
                    cv_rect(bx + 14, by + 3, 1, 4, C_BG);
                }
            }
    if (br.cap_k) {                      /* the power-up: a capsule, blinking */
        static const char *const CL[4] = {"", "W", "+", "S"};
        int32_t x = (int32_t)br.cap_x, y = (int32_t)br.cap_y;
        uint16_t cc = br.cap_k == 1 ? C_CYAN : br.cap_k == 2 ? C_PINK : C_PURP;
        cv_rect(x - 9, y - 5, 18, 10, (plat_ms() / 120u) & 1u ? cc : C_INK);
        frame_rect(x - 9, y - 5, 18, 10, C_YEL);
        text_c(x, y - 5, &FONT_T, CL[br.cap_k], C_BG);
    }
    {
        uint32_t q = plat_ms() - br.squash_at;
        int32_t sq = br.squash_at && q < 150u ? (int32_t)(150u - q) / 30 : 0;   /* squashed wide and flat, back */
        int32_t hw = (int32_t)br_w() / 2;
        uint16_t pcl = plat_ms() < br.slow_until ? C_PURP : C_CYAN;
        cv_rect((int32_t)br.px - hw - sq, 228 + sq / 2, 2 * hw + 2 * sq, 5 - sq / 2, C_INK);
        cv_rect((int32_t)br.px - hw - sq, 228 + sq / 2, 2 * hw + 2 * sq, 2, pcl);
    }
    for (r = 5; r > 0; r--)              /* the trail, fading */
        disc(br.tx[r], br.ty[r], r > 3 ? 1 : 2, mix565(C_BG, C_YEL, 200 - r * 32));
    disc((int32_t)br.bx, (int32_t)br.by, 3, C_YEL);
    disc((int32_t)br.bx - 1, (int32_t)br.by - 1, 1, C_INK);
    if (br.stuck && !br.over)
        text_c(120, 160, &FONT_T, "TOP KEY: LAUNCH", C_GHOST);
    if (br.over) {
        char bs[24], bn[12];
        over_box("THE END", "TOP KEY: AGAIN   HOME: MENU");
        itos((int32_t)br_best, bn);
        tcpy(bs, "BEST ");
        tcat(bs, bn);
        text_c(120, 150, &FONT_XS, bs, C_YEL);
    }
}

/* ================================================================= SWARM === */
/* A space shooter of its own (reworked 17 Nov 2026: nothing like the famous invaders game, nor the one where they fly
 * in): no rows marching, no formations diving. A swarm of droid ships holds the upper sky, never in step, each kind
 * flying and firing its own way, its shots on the beat. You fly a rocket in the lower sky (left and right, a little
 * forward and back; A fires). The twist is the music: every droid you hit is caught in the JAR, eight slots across
 * the top that loop as a bar of eighths, each playing its droid's note, so the tune is what you shot, in the order you
 * shot them, each into a random slot (a full jar: a random one goes). Three hits on you and it's the end. The whole swarm: the next, bigger
 * and quicker. The first five waves are one kind of droid each, then two, then all.
 *   SCOUT       weaves fast;            fires one shot straight down
 *   GUNSHIP     heavy, slow;            fires a spread of two
 *   INTERCEPTOR sweeps side to side;    fires one at you
 *   ORBITER     circles its post;       drops a slow mine that wanders as it falls
 *   SENTINEL    steps on the beat;      fires a burst of two in a line */
#define SW_NB 24                         /* droids at most */
#define SW_SLOTS 8                       /* the jar's slots: a bar of eighths */
#define SW_NKIND 5
#define SW_NSHOT 12                      /* their shots in the air, at most */
#define SW_JAR_Y 24                      /* the jar: a strip of eight slots under the header */
#define SW_LOW 116                       /* the droids keep above this: the upper sky is theirs */
#define SW_SHIP_Y 214                    /* your rocket's nose, at the back; it may come forward to SW_SHIP_Y - 36 */
static const uint16_t SW_KCOL[SW_NKIND] = {RGB(130, 170, 200), RGB(150, 150, 160), RGB(200, 140, 90), RGB(150, 130, 200),
                                          RGB(150, 170, 110)};   /* steel, gunmetal, copper, violet steel, olive */
static const uint16_t SW_KLIGHT[SW_NKIND] = {RGB(95, 227, 238), RGB(255, 210, 63), RGB(255, 90, 70), RGB(200, 140, 255),
                                            RGB(140, 255, 160)};   /* their lights */
typedef struct {
    float x, y, vx, vy, ox, oy;          /* where; its post in the swarm (from the swarm's middle) */
    uint8_t on, kind, note, step;        /* step: a sentinel's post this beat */
    float hx, hy;                        /* a sentinel's hold */
    uint8_t dive;                        /* diving at you (on the beat, out of the swarm and back: Nick, 19 Nov 2026) */
    uint32_t dive_at;
    float dx0, dy0, dtx;                 /* where it dove from, where at */
} sw_bug_t;
typedef struct {
    float x, y, vx, vy;
    uint8_t on, kind;
    uint32_t born;
} sw_shot_t;
typedef struct {
    sw_bug_t b[SW_NB];
    sw_shot_t e[SW_NSHOT];               /* theirs */
    uint8_t n_alive, nbugs, lives, over, wave, frame, tier, streak;
    float px, py, cx, cy, ph;            /* you; the swarm's middle; its drift */
    int8_t lean;
    uint8_t jar[SW_SLOTS], jkind[SW_SLOTS], jnext;   /* the jar: each slot's note (0 empty), its droid */
    uint32_t seq_ms[SW_SLOTS], jar_ms[SW_SLOTS];   /* a slot sounded (lit); a droid landed in it */
    float shx[2], shy[2];                /* your shots (shy < 0: none) */
    uint32_t last, score, over_at, hurt_until, wave_at, catches, shot_at;
    float fly_x[4], fly_y[4];            /* the hit ones on their way up to the jar */
    uint8_t fly_k[4], fly_slot[4];
    uint32_t fly_at[4];
    uint32_t mq[4];                      /* the beats, as the music schedules them (ms): the swarm acts then */
    uint8_t mq_w, mq_r;
    uint32_t burst_at[3];                /* a sentinel's burst still to come (ms), its x */
    float burst_x[3], burst_y[3];
    uint8_t pu_on, pu_kind, pow;         /* a power-up falling (its kind); the power you have (1 DOUBLE, 2 RAPID) */
    float pu_x, pu_y;
    uint32_t pow_until;
    uint16_t beats;
} sw_t;
#define SW_DIVE_MS 1800u                 /* a dive: down at you and back up to its post */
#define SW_POW_MS 9000u                  /* a power lasts this long */
static uint32_t sw_best;
#define sw (*(sw_t *)g_arena)
_Static_assert(sizeof(sw_t) <= sizeof g_arena, "sw: the arena is too small");
static void sw_boom(int32_t x, int32_t y, uint16_t c, int big)   /* something bursts */
{
    juice_burst(x, y, big ? 40 : 14, big ? 6 : 3, c, C_INK, 1);
    juice_ring(x, y, big ? 70 : 20, c, big ? 500 : 220);
    juice_shake(big ? 11 : 3, big ? 700u : 130u);
    if (big) {
        juice_freeze(160);
        juice_flash(RGB(255, 40, 80), 170, 500);
        juice_glitch(400);
        J.heat /= 3;
    } else {
        juice_freeze(14);
        juice_heat(5);
    }
}
static void sw_wave(void)                /* a new swarm: its droids warp in at their posts */
{
    int k, w = sw.wave;
    sw.nbugs = (uint8_t)(w < 6 ? 12 + 2 * w : SW_NB);
    for (k = 0; k < SW_NB; k++) {
        sw_bug_t *b = &sw.b[k];
        float a = (float)k * 2.39996f, rr = 12.0f + 5.0f * fm_sqrtf((float)k);   /* a sunflower's spiral: the posts */
        b->on = k < sw.nbugs;
        b->ox = fm_cosf(a) * rr * 1.7f, b->oy = fm_sinf(a) * rr * 0.6f;
        b->x = 112.0f + b->ox, b->y = 70.0f + b->oy;
        b->hx = b->x, b->hy = b->y;
        b->vx = b->vy = 0.0f;
        b->kind = (uint8_t)(w < SW_NKIND ? w : w < 8 ? ((k & 1) ? (w + 2) % SW_NKIND : w % SW_NKIND)
                                                  : (int)((kr() >> 16) % SW_NKIND));
        b->note = (uint8_t)(2 + (kr() >> 16) % 10u);
        b->step = 0;
        b->dive = 0;
    }
    sw.n_alive = sw.nbugs;
    sw.cx = 112.0f, sw.cy = 70.0f, sw.ph = 0.0f;
    for (k = 0; k < SW_NSHOT; k++)
        sw.e[k].on = 0;
    for (k = 0; k < 4; k++)
        sw.fly_at[k] = 0;
    for (k = 0; k < 3; k++)
        sw.burst_at[k] = 0;
    sw.wave_at = plat_ms();
    sw.tier = 0;
    sw.mq_w = sw.mq_r = 0;
    {
        char ws[12] = "WAVE ";
        itos(sw.wave + 1, ws + 5);
        juice_pop(120, 150, ws, C_CYAN);
    }
}
static uint32_t sw_bpm(void)             /* TEMPO (K1), a little quicker each wave */
{
    return (uint32_t)sq_cfg[SQ_SWARM].bpm + (uint32_t)(sw.wave > 8 ? 8 : sw.wave) * 3u;
}
static void sw_new(void)
{
    int k;
    sw.wave = 0;
    sw.lives = 3;
    sw.score = 0;
    sw.over = 0;
    sw.px = 120.0f, sw.py = (float)SW_SHIP_Y;
    sw.shy[0] = sw.shy[1] = -1.0f;
    sw.last = 0;
    sw.streak = 0;
    sw.catches = 0;
    for (k = 0; k < SW_SLOTS; k++)
        sw.jar[k] = 0, sw.seq_ms[k] = sw.jar_ms[k] = 0;
    sw.jnext = 0;
    sw.pu_on = 0, sw.pow = 0, sw.beats = 0;
    sw_wave();
}
/* the music: the jar is the sequence. A playhead walks its eight slots, an eighth each, each playing its droid's note on
 * the chord now (CHANCE how many sound); an empty slot rests. Under it a bass on the beat, and the drums coming in as
 * the swarm is shot down */
static void sw_groove(uint32_t t, uint32_t step)
{
    uint32_t p = step % 16u, ms = sq_ms(t);
    int tier;
    h_deg_at(step / 16u);                /* a new chord each bar, at random in the key */
    if (sw.over)
        return;
    tier = sq_tier_of(sw.nbugs - sw.n_alive, sw.nbugs);
    if (tier < (sw.wave > 2 ? 2 : sw.wave))   /* a later wave starts with the kick */
        tier = sw.wave > 2 ? 2 : sw.wave;
    sq_tier_say(tier, &sw.tier, 160);
    sq_drums(t, p, tier, NA_HAT, NA_KICK, NA_SNARE, 85);
    if (p % 4u == 0u) {                  /* the bass, on the beat; the swarm acts with it (sw_tick) */
        k_on(t, h_bass() + (p == 8u ? 7 : 0), 84, NA_BASS, 0.14f);
        if ((uint8_t)(sw.mq_w - sw.mq_r) < 4u)
            sw.mq[sw.mq_w++ % 4u] = ms;
    }
    if (!(p & 1u)) {                     /* the jar's slot, this eighth */
        int c = (int)(p / 2u);
        if (sw.jar[c] && sq_roll(SQ_SWARM)) {
            k_on(t, h_tone(sw.jar[c]), 80u + (p % 8u == 0u ? 12u : 0u), NA_SEQ, 0.25f);
            sw.seq_ms[c] = ms;
        }
    }
}
static void sw_catch(int k)              /* droid k hit: up into the jar */
{
    sw_bug_t *b = &sw.b[k];
    int f, slot = seq_free_slot(sw.jar, SW_SLOTS);   /* (a random slot: a random rhythm) */
    char ps[12] = {'+', 0};
    uint32_t pts;
    b->on = 0;
    sw.n_alive--;
    sw.catches++;
    sw.streak = (uint8_t)(sw.streak < 99 ? sw.streak + 1 : 99);
    pts = 10u * (1u + sw.streak / 4u) * (1u + sw.wave) * (b->kind == 1 ? 2u : 1u) * (b->dive ? 3u : 1u);   /* (a
                                          * gunship: double; one caught mid-dive: triple) */
    sw.score += pts;
    if (b->dive) {
        juice_pop((int32_t)fm_clampf(b->x + 8.0f, 60.0f, 180.0f), (int32_t)b->y - 6, "NICE SHOT!", C_YEL);
        juice_kick(0, -3);
    }
    if (!sw.pu_on && !sw.pow && (kr() >> 16) % 100u < 14u) {   /* now and then it drops a power-up */
        sw.pu_on = 1;
        sw.pu_kind = (uint8_t)((kr() >> 16) % 2u);
        sw.pu_x = b->x + 8.0f, sw.pu_y = b->y + 8.0f;
    }
    b->dive = 0;
    for (f = 0; f < 4 && sw.fly_at[f]; f++)
        ;
    if (f < 4) {
        sw.fly_x[f] = b->x, sw.fly_y[f] = b->y, sw.fly_k[f] = b->kind, sw.fly_slot[f] = (uint8_t)slot;
        sw.fly_at[f] = plat_ms() | 1u;
    }
    sw.jar[slot] = b->note;              /* (the music has it at once) */
    sw.jkind[slot] = b->kind;
    sw.jar_ms[slot] = plat_ms() | 1u;
    k_on(grid_next(), h_tone(b->note) + 12, 100, NA_HIT, 0.4f);   /* its note, on the next sixteenth */
    sw_boom((int32_t)b->x + 8, (int32_t)b->y + 7, SW_KLIGHT[b->kind], 0);
    itos((int32_t)pts, ps + 1);
    juice_text((int32_t)b->x + 8, (int32_t)b->y + 14, ps, SW_KLIGHT[b->kind]);
    if (!sw.n_alive) {                   /* the whole swarm: the next */
        juice_flash(RGB(255, 255, 255), 150, 500);
        juice_ring(120, 100, 150, C_CYAN, 700);
        juice_celebrate(120, 100);
        sw.wave++;
        arc_win();
        sw_wave();
    }
}
static void sw_nav(int dx, int dy, int sel)
{
    int k;
    (void)dx, (void)dy;
    if (sw.over) {
        if (sel && plat_ms() - sw.over_at > 800u)
            sw_new();
        return;
    }
    if (!sel)
        return;
    if (sw.pow == 1 && sw.shy[0] < 0.0f && sw.shy[1] < 0.0f) {   /* DOUBLE: a pair, side by side */
        sw.shx[0] = sw.px - 6.0f, sw.shx[1] = sw.px + 6.0f;
        sw.shy[0] = sw.shy[1] = sw.py - 4.0f;
        sw.shot_at = plat_ms() | 1u;
        k_on(0, 96, 56, NA_HAT, 0.03f);
        juice_burst((int32_t)sw.px, (int32_t)sw.py - 4, 5, 1, C_YEL, C_INK, 0);
        return;
    }
    for (k = 0; k < 2; k++)              /* a shot (two at most in the air) */
        if (sw.shy[k] < 0.0f) {
            sw.shx[k] = sw.px, sw.shy[k] = sw.py - 4.0f;
            sw.shot_at = plat_ms() | 1u;
            k_on(0, 96, 50, NA_HAT, 0.03f);
            juice_burst((int32_t)sw.px, (int32_t)sw.py - 4, 3, 1, C_YEL, C_INK, 0);
            return;
        }
}
static void sw_hurt(void)
{
    uint32_t ms = plat_ms();
    sw.hurt_until = ms + 2200u;
    sw.streak = 0;
    k_on(0, degree_note(-5), 110, NA_BOOM, 0.8f);
    sw_boom((int32_t)sw.px, (int32_t)sw.py + 6, C_PINK, 1);
    if (--sw.lives == 0) {
        sw.over = 1;
        sw.over_at = ms;
        if (sw.score > sw_best)
            sw_best = sw.score;
    } else
        juice_pop(120, 150, oops_word(), C_PINK);
}
static void sw_fire_one(float x, float y, float vx, float vy, int kind)   /* a droid's shot */
{
    int k;
    for (k = 0; k < SW_NSHOT; k++)
        if (!sw.e[k].on) {
            sw_shot_t *s = &sw.e[k];
            s->on = 1, s->x = x, s->y = y, s->vx = vx, s->vy = vy, s->kind = (uint8_t)kind, s->born = plat_ms();
            return;
        }
}
static void sw_fire(int k)               /* droid k fires, its own way */
{
    sw_bug_t *b = &sw.b[k];
    float x = b->x + 8.0f, y = b->y + 14.0f, sp = 0.065f + 0.003f * (float)(sw.wave > 8 ? 8 : sw.wave);   /* (slow
                                          * enough to see and step aside from) */
    switch (b->kind) {
    case 0: sw_fire_one(x, y, 0.0f, sp * 1.5f, 0); break;                  /* a scout: straight down, quick */
    case 1:                                                                /* a gunship: a spread of three */
        sw_fire_one(x, y, -0.03f, sp, 1), sw_fire_one(x, y, 0.03f, sp, 1);   /* (a spread of two: a gap to sit in) */
        break;
    case 2: {                                                              /* an interceptor: at you */
        float dx = sw.px - x, dy = sw.py - y, d = fm_sqrtf(dx * dx + dy * dy);
        if (d > 1.0f)
            sw_fire_one(x, y, dx / d * sp * 1.2f, dy / d * sp * 1.2f, 2);
        break;
    }
    case 3: sw_fire_one(x, y, 0.0f, sp * 0.55f, 3); break;                 /* an orbiter: a slow mine, wandering */
    default: {                                                             /* a sentinel: a burst of three */
        int j;
        sw_fire_one(x, y, 0.0f, sp * 1.2f, 4);
        for (j = 0; j < 3; j++)
            if (!sw.burst_at[j]) {
                sw.burst_at[j] = plat_ms() + 110u, sw.burst_x[j] = x, sw.burst_y[j] = y;
                break;
            }
        break;                           /* (a burst of two) */
    }
    }
    k_on(0, 40 + b->kind * 3, 50, NA_KICK, 0.06f);
}
static void sw_tick(void)
{
    uint32_t dt = arc_dt(&sw.last), ms = plat_ms();
    float f = (float)dt, sp = 0.05f + 0.006f * (float)(sw.wave > 8 ? 8 : sw.wave);
    int k;
    if (sw.over)
        return;
    if (HELD_L)
        sw.px -= 0.16f * f;
    if (HELD_R)
        sw.px += 0.16f * f;
    if (HELD_U)                          /* a little forward, a little back */
        sw.py -= 0.09f * f;
    if (HELD_D)
        sw.py += 0.09f * f;
    sw.px = fm_clampf(sw.px, 10.0f, 230.0f);
    sw.py = fm_clampf(sw.py, (float)SW_SHIP_Y - 36.0f, (float)SW_SHIP_Y);
    sw.lean = (int8_t)(HELD_L ? -1 : HELD_R ? 1 : 0);
    while (sw.mq_r != sw.mq_w && (int32_t)(ms - sw.mq[sw.mq_r % 4u]) >= 0) {   /* a beat: lights blink, sentinels
                                                                                * step, and some fire */
        int fires = (int)((kr() >> 16) % 100u) < (sw.wave > 6 ? 45 : sw.wave > 3 ? 35 : 25) ? 1 : 0, j;   /* (casual:
                                          * now and then a shot on a beat, not a hail: Nick, 17 Nov 2026) */
        sw.mq_r++;
        sw.frame ^= 1u;
        sw.beats++;
        if (ms - sw.wave_at > 2000u && sw.beats % 8u == 0u && sw.n_alive) {   /* every two bars, on the beat: a dive */
            int diving = 0, most = 1 + sw.wave / 2, pick;
            for (k = 0; k < SW_NB; k++)
                diving += sw.b[k].on && sw.b[k].dive;
            pick = (int)((kr() >> 16) % sw.n_alive);
            for (k = 0; k < SW_NB && diving < most; k++)
                if (sw.b[k].on && pick-- == 0 && !sw.b[k].dive) {
                    sw_bug_t *b = &sw.b[k];
                    b->dive = 1, b->dive_at = ms;
                    b->dx0 = b->x, b->dy0 = b->y;
                    b->dtx = fm_clampf(sw.px - 8.0f, 10.0f, 214.0f);
                    k_on(0, h_tone(8) + 12, 70, NA_HIT, 0.2f);   /* its cry, down the chord */
                    k_on(k_at(0.12f), h_tone(5) + 12, 60, NA_HIT, 0.2f);
                    k_on(k_at(0.24f), h_tone(2) + 12, 50, NA_HIT, 0.3f);
                    juice_ring((int32_t)b->x + 8, (int32_t)b->y + 7, 18, SW_KLIGHT[b->kind], 200);
                    break;
                }
        }
        for (k = 0; k < SW_NB; k++)
            if (sw.b[k].on && sw.b[k].kind == 4) {   /* a sentinel: its next post, a jump on the beat */
                sw_bug_t *b = &sw.b[k];
                b->step = (uint8_t)((b->step + 1) & 3u);
                b->hx = sw.cx + b->ox + (b->step == 1 ? 18.0f : b->step == 3 ? -18.0f : 0.0f);
                b->hy = sw.cy + b->oy + (b->step == 2 ? 10.0f : 0.0f);
            }
        if (ms - sw.wave_at > 1200u)
            for (j = 0; j < fires && sw.n_alive; j++) {
                int pick = (int)((kr() >> 16) % sw.n_alive);
                for (k = 0; k < SW_NB; k++)
                    if (sw.b[k].on && pick-- == 0) {
                        sw_fire(k);
                        break;
                    }
            }
    }
    for (k = 0; k < 3; k++)              /* a sentinel's burst, the rest of it */
        if (sw.burst_at[k] && (int32_t)(ms - sw.burst_at[k]) >= 0) {
            sw.burst_at[k] = 0;
            sw_fire_one(sw.burst_x[k], sw.burst_y[k], 0.0f, 0.08f, 4);
        }
    sw.ph += 0.0005f * f * (1.0f + 0.1f * (float)sw.wave);   /* the swarm's middle drifts in a long figure of eight */
    sw.cx = 112.0f + 64.0f * fm_sinf(sw.ph);
    sw.cy = 70.0f + 10.0f * fm_sinf(sw.ph * 2.0f);
    for (k = 0; k < SW_NB; k++) {        /* each droid: to its post, its own way */
        sw_bug_t *b = &sw.b[k];
        float tx = sw.cx + b->ox, ty = sw.cy + b->oy, ax, ay, v, pull = 0.00002f, top = sp * 2.2f;
        float tt = (float)ms * 0.001f + (float)k;
        if (!b->on)
            continue;
        if (b->dive) {                   /* diving: down at where you were, a swing to the side, and back up */
            float u = (float)(ms - b->dive_at) / (float)SW_DIVE_MS, arc;
            if (u >= 1.0f) {
                b->dive = 0;
            } else {
                arc = fm_sinf(u * 3.1415927f);
                b->x = b->dx0 + (b->dtx - b->dx0) * arc + 26.0f * fm_sinf(u * 6.2831853f);
                b->y = b->dy0 + ((float)SW_SHIP_Y - 6.0f - b->dy0) * arc;
                b->vx = b->vy = 0.0f;
                if (fm_fabsf(b->x + 8.0f - sw.px) < 11.0f && fm_fabsf(b->y + 7.0f - (sw.py + 8.0f)) < 11.0f &&
                    (int32_t)(ms - sw.hurt_until) >= 0) {   /* it got you: on up it goes */
                    b->dive_at = ms - (uint32_t)((1.0f - u) * (float)SW_DIVE_MS);
                    sw_hurt();
                    if (sw.over)
                        return;
                }
                continue;
            }
        }
        switch (b->kind) {
        case 0: tx += 18.0f * fm_sinf(tt * 6.0f), ty += 6.0f * fm_cosf(tt * 4.0f), top = sp * 3.0f; break;   /* weaves */
        case 1: tx += 3.0f * fm_sinf(tt), top = sp * 1.2f; break;                                       /* lumbers */
        case 2: tx += 50.0f * fm_sinf(tt * 1.8f), pull = 0.00004f, top = sp * 3.4f; break;                /* sweeps */
        case 3: tx += 14.0f * fm_cosf(tt * 3.0f), ty += 14.0f * fm_sinf(tt * 3.0f); break;                /* circles */
        default: tx = b->hx, ty = b->hy, pull = 0.00008f, top = sp * 4.0f; break;                        /* steps */
        }
        ax = (tx - b->x) * pull * f, ay = (ty - b->y) * pull * f;
        b->vx = (b->vx + ax) * 0.95f, b->vy = (b->vy + ay) * 0.95f;
        v = fm_sqrtf(b->vx * b->vx + b->vy * b->vy);
        if (v > top)
            b->vx *= top / v, b->vy *= top / v;
        b->x += b->vx * f, b->y += b->vy * f;
        b->x = fm_clampf(b->x, 2.0f, 222.0f);
        b->y = fm_clampf(b->y, (float)SW_JAR_Y + 22.0f, (float)SW_LOW - 14.0f);   /* the upper sky: theirs */
    }
    for (k = 0; k < SW_NSHOT; k++) {     /* their shots */
        sw_shot_t *s = &sw.e[k];
        if (!s->on)
            continue;
        s->x += (s->vx + (s->kind == 3 ? 0.05f * fm_sinf((float)(ms - s->born) * 0.006f) : 0.0f)) * f;
        s->y += s->vy * f;
        if (s->y > 240.0f || s->x < -4.0f || s->x > 244.0f)
            s->on = 0;
        else if (fm_fabsf(s->x - sw.px) < 8.0f && s->y > sw.py - 2.0f && s->y < sw.py + 16.0f &&
                 (int32_t)(ms - sw.hurt_until) >= 0) {
            s->on = 0;
            sw_hurt();
            if (sw.over)
                return;
        }
    }
    if (sw.pow && (int32_t)(ms - sw.pow_until) >= 0)
        sw.pow = 0;
    if (sw.pu_on) {                      /* a power-up falling: catch it */
        sw.pu_y += 0.05f * f;
        if (fm_fabsf(sw.pu_x - sw.px) < 13.0f && fm_fabsf(sw.pu_y - (sw.py + 8.0f)) < 13.0f) {
            sw.pu_on = 0;
            sw.pow = (uint8_t)(sw.pu_kind + 1);
            sw.pow_until = ms + SW_POW_MS;
            juice_pop(120, 150, sw.pow == 1 ? "DOUBLE SHOT!" : "RAPID FIRE!", C_YEL);
            juice_ring((int32_t)sw.px, (int32_t)sw.py + 8, 40, C_YEL, 400);
            juice_flash(RGB(255, 210, 63), 80, 250);
            juice_sting(1);
        } else if (sw.pu_y > 240.0f)
            sw.pu_on = 0;
    }
    for (k = 0; k < 2; k++)              /* your shots: up, and into a droid */
        if (sw.shy[k] >= 0.0f) {
            int j2;
            sw.shy[k] -= (sw.pow == 2 ? 0.66f : 0.36f) * f;   /* (RAPID: quicker, so the next is sooner) */
            if (sw.shy[k] < (float)SW_JAR_Y + 20.0f) {   /* a miss: the streak ends */
                sw.shy[k] = -1.0f;
                sw.streak = 0;
                continue;
            }
            for (j2 = 0; j2 < SW_NB; j2++) {
                sw_bug_t *b = &sw.b[j2];
                if (b->on && sw.shx[k] >= b->x - 1.0f && sw.shx[k] <= b->x + 17.0f && sw.shy[k] >= b->y && sw.shy[k] <= b->y + 14.0f) {
                    sw.shy[k] = -1.0f;
                    sw_catch(j2);
                    break;
                }
            }
        }
    for (k = 0; k < 4; k++)              /* the hit ones, landing in the jar */
        if (sw.fly_at[k] && ms - sw.fly_at[k] > 450u)
            sw.fly_at[k] = 0;
}
/* ---- SWARM's droids (17 Nov 2026, Nick: back to the first swarm's look, "a little different from Space Invaders":
 * the original creatures, 8 x 6, two frames, drawn at twice the size; the way they fly and fire is their own now).
 * a: its body, b: its dark (eyes, mouth) */
static const char *const SW_BUG[SW_NKIND][2][6] = {
    {{".aaaaaa.", "aabaabaa", "aaaaaaaa", "a.a..a.a", "a..aa..a", ".a....a."},     /* a jellyfish (the scout) */
     {".aaaaaa.", "aabaabaa", "aaaaaaaa", ".a.aa.a.", "a.a..a.a", "..a..a.."}},
    {{"a......a", ".a.aa.a.", ".aaaaaa.", "aababbaa", ".aaaaaa.", "a.a..a.a"},     /* a beetle (the gunship) */
     {".a....a.", "..aaaa..", ".aaaaaa.", "aababbaa", ".aaaaaa.", ".a.a.a.a"}},
    {{"...aa...", "...aa...", "aaaaaaaa", ".abaaba.", "..a..a..", ".a....a."},     /* a star (the interceptor) */
     {"...aa...", "a..aa..a", ".aaaaaa.", "..baab..", ".a.aa.a.", "a......a"}},
    {{"..aaaa..", ".aaaaaa.", "aaabbaaa", "aaabbaaa", ".aaaaaa.", "a.a..a.a"},     /* an eye (the orbiter) */
     {"..aaaa..", ".aaaaaa.", "aabbaaaa", "aabbaaaa", ".aaaaaa.", ".a.aa.a."}},
    {{"a..aa..a", ".aaaaaa.", "aab..baa", "aaaaaaaa", ".a.aa.a.", "a......a"},     /* a horned one (the sentinel) */
     {"..a..a..", "aaaaaaaa", "aab..baa", ".aaaaaa.", ".a.aa.a.", "..a..a.."}},
};
static void sw_droid(int k, int32_t x, int32_t y, uint32_t ms, int seed, int lit)   /* droid k in its 16 x 14 box */
{
    uint16_t map[2];
    (void)ms;
    map[0] = lit ? C_INK : SW_KLIGHT[k], map[1] = RGB(20, 16, 40);
    sprite(x, y + 1, SW_BUG[k][(sw.frame + (uint32_t)seed) & 1u], 6, 2, map);
}

static void sw_scene(void)
{
    int k;
    char n[12];
    uint32_t ms = plat_ms();
    itos((int32_t)sw.score, n);
    {                                    /* space: three layers of stars, drifting down, the near ones faster */
        int i;
        for (i = 0; i < 42; i++) {
            uint32_t h = (uint32_t)i * 2654435761u, layer = (uint32_t)i % 3u;
            int32_t x = (int32_t)(h >> 8 & 255u) % 240, y0 = (int32_t)(h >> 16 & 255u);
            int32_t y = 24 + (y0 + (int32_t)(ms / (40u - layer * 12u))) % 216;
            cv_rect(x, y, 1, layer == 2u ? 2 : 1, layer == 2u ? RGB(160, 150, 200) : layer == 1u ? RGB(90, 80, 130) : RGB(50, 44, 80));
        }
    }
    header("SWARM", C_CYAN, "", C_PINK, n);
    lives_draw(sw.lives);
    {                                    /* the jar: eight slots under the header, each its droid, lit as its note sounds */
        int c;
        cv_rect(0, SW_JAR_Y, 240, 20, RGB(16, 12, 28));
        for (c = 0; c < SW_SLOTS; c++) {
            int32_t x = 4 + c * 29, y = SW_JAR_Y + 1;
            uint32_t q = ms - sw.seq_ms[c];
            int lit = sw.seq_ms[c] && (int32_t)q >= 0 && q < 160u;
            frame_rect(x, y, 27, 18, lit ? C_INK : RGB(60, 50, 90));
            if (sw.jar[c]) {
                cv_rect(x + 1, y + 1, 25, 16, mix565(RGB(16, 12, 28), SW_KLIGHT[sw.jkind[c]], lit ? 120 : 40));
                sw_droid(sw.jkind[c], x + 6, y + 2, ms, c, lit);
            }
        }
    }
    for (k = 0; k < 4; k++)              /* the hit ones, flying up into their slots */
        if (sw.fly_at[k]) {
            float t = (float)(ms - sw.fly_at[k]) / 450.0f, x, y;
            t = t > 1.0f ? 1.0f : t;
            x = sw.fly_x[k] + ((float)(4 + sw.fly_slot[k] * 29 + 6) - sw.fly_x[k]) * t;
            y = sw.fly_y[k] + ((float)(SW_JAR_Y + 3) - sw.fly_y[k]) * t * t;
            sw_droid(sw.fly_k[k], (int32_t)x, (int32_t)y, ms, k, 1);
        }
    {
        uint32_t wq = ms - sw.wave_at;
        for (k = 0; k < SW_NB; k++) {    /* the swarm (warping in, at first: a ring closing on each) */
            sw_bug_t *b = &sw.b[k];
            if (!b->on)
                continue;
            if (wq < 700u) {
                int32_t rr = 20 - (int32_t)wq / 35;
                if (rr > 2) {
                    int j;
                    for (j = 0; j < 8; j++)
                        cv_rect((int32_t)b->x + 8 + (int32_t)(fm_cosf((float)j * 0.785f) * (float)rr),
                                (int32_t)b->y + 7 + (int32_t)(fm_sinf((float)j * 0.785f) * (float)rr), 1, 1, SW_KLIGHT[b->kind]);
                }
                if ((wq / 60u) & 1u)
                    continue;
            }
            if (b->dive) {               /* diving: a streak of its light behind it */
                int j;
                for (j = 1; j <= 3; j++)
                    disc((int32_t)b->x + 8, (int32_t)b->y + 7 - j * 5, 5 - j, mix565(C_BG, SW_KLIGHT[b->kind], 140 - 35 * j));
            }
            sw_droid(b->kind, (int32_t)b->x, (int32_t)b->y, ms, k, b->dive && (ms / 80u) & 1u);
        }
    }
    if (sw.pu_on) {                      /* a power-up: a capsule, spinning its letter */
        int32_t x = (int32_t)sw.pu_x, y = (int32_t)sw.pu_y;
        uint16_t c = sw.pu_kind ? C_PINK : C_YEL;
        disc(x, y, 9, mix565(C_BG, c, 90 + (int32_t)((ms / 60u) % 4u) * 30));
        disc(x, y, 6, c);
        text_c(x, y - 6, &FONT_T, sw.pu_kind ? "R" : "D", C_BG);
    }
    for (k = 0; k < 2; k++)              /* your shots: a hot core, a glow, a trail */
        if (sw.shy[k] >= 0.0f) {
            int32_t x = (int32_t)sw.shx[k], y = (int32_t)sw.shy[k];
            cv_rect(x - 1, y + 6, 4, 8, mix565(C_BG, C_CYAN, 90));
            cv_rect(x - 1, y - 1, 4, 8, mix565(C_BG, C_CYAN, 170));
            cv_rect(x, y, 2, 7, C_INK);
        }
    for (k = 0; k < SW_NSHOT; k++) {     /* their shots: each kind its own */
        sw_shot_t *s = &sw.e[k];
        int32_t x = (int32_t)s->x, y = (int32_t)s->y;
        uint16_t c;
        if (!s->on)
            continue;
        c = SW_KLIGHT[s->kind];          /* every kind's the same size as the orbiter's mine (Nick: the rest were too
                                          * small to see), each in its droid's light */
        disc(x, y, 6, mix565(C_BG, c, 70));   /* its glow */
        disc(x, y, 4, (ms / 120u + (uint32_t)k) & 1u ? c : C_INK);
        cv_rect(x - 6, y, 13, 1, c);
        cv_rect(x, y - 6, 1, 13, c);
        disc(x, y, 1, C_INK);
    }
    if (!sw.over && !((int32_t)(ms - sw.hurt_until) < 0 && (ms / 100u) & 1u)) {   /* you: a rocket (it blinks, hurt) */
        int32_t x = (int32_t)sw.px, kick = sw.shot_at && ms - sw.shot_at < 80u ? 2 : 0, l = sw.lean, y = (int32_t)sw.py;
        if (sw.pow) {                    /* a power: a glow round you, and its time running out under you */
            uint32_t left = sw.pow_until - ms;
            uint16_t pc = sw.pow == 1 ? C_YEL : C_PINK;
            disc(x, y + 9, 13, mix565(C_BG, pc, 50 + (int32_t)((ms / 90u) % 3u) * 20));
            cv_rect(x - 12, y + 24, (int32_t)(24u * left / SW_POW_MS), 2, pc);
        }
        int32_t fl = 3 + (int32_t)((ms / 40u) % 3u) + (HELD_U ? 3 : 0);
        cv_rect(x - 2, y + 14 + kick, 5, fl, (ms / 50u) & 1u ? C_YEL : RGB(255, 150, 60));   /* the flame */
        cv_rect(x - 1, y + 14 + kick, 3, fl - 1, C_INK);
        cv_rect(x - 1, y + kick, 3, 6, C_INK);
        cv_rect(x - 4, y + 6 + kick, 9, 6, C_CYAN);
        cv_rect(x - 7, y + 10 + kick + (l < 0 ? -1 : l > 0 ? 1 : 0), 3, 4, C_CYAN);
        cv_rect(x + 5, y + 10 + kick + (l > 0 ? -1 : l < 0 ? 1 : 0), 3, 4, C_CYAN);
        cv_rect(x - 2, y + 7 + kick, 2, 2, C_BG);
        if (kick)                        /* the muzzle flash */
            disc(x, y - 2, 3, C_YEL);
    }
    if (sw.streak >= 4) {                /* the multiplier */
        char xs[8] = {'X', 0};
        itos(1 + sw.streak / 4u, xs + 1);
        text_r(234, 214, &FONT_B, xs, C_YEL);
    }
    if (sw.over && ms - sw.over_at > 700u) {
        char bs[24], bn[12];
        over_box("THE END", "TOP KEY: AGAIN   HOME: MENU");
        itos((int32_t)sw_best, bn);
        tcpy(bs, "BEST ");
        tcat(bs, bn);
        text_c(120, 150, &FONT_XS, bs, C_YEL);
    }
}

/* ============================================================ ROCK DRIFT === */
/* A sequencer too: a radar sweep goes round the field once a bar, a sixteenth a slice, and every rock it crosses
 * plays: big ones low, small ones high, further out higher still, all chord tones. The rocks drift, so the tune does.
 * Your shots play the chord tone of the way you aim, a break lands on the next sixteenth, and the drums come in as the
 * wave is cleared. Its knobs: TEMPO (where it starts; each wave a little quicker), CHANCE, SOUND, scale, key */
#define DR_NR 20
#define DR_CX 120.0f
#define DR_CY 132.0f
typedef struct {
    float x, y, vx, vy, rot, spin;
    uint8_t size;                        /* 0 none, 1 small, 2 middling, 3 big */
    uint8_t shape;                       /* its outline's seed */
    uint32_t lit;                        /* the sweep played it (ms, 0 never) */
} rock_t;
typedef struct {
    float x, y, vx, vy, a;
    rock_t r[DR_NR];
    float shx[4], shy[4], shvx[4], shvy[4];
    uint16_t shlife[4];
    uint8_t lives, over, wave, thrust, tier;
    uint16_t broken, total;              /* the wave's rocks broken, of all it will break into */
    uint32_t score, last, over_at, safe_until, shot_at, wave_at, best_combo;
    uint32_t sweep_ms, sweep_step;       /* the sweep's last slice scheduled (its ms, its step) */
    uint32_t combo_at;
    uint8_t combo;
    float trail_x[8], trail_y[8];        /* the flame's trail */
    uint8_t trail_n;
    uint16_t sweep_notes;                /* the rocks the sweep has played (a probe) */
    uint32_t fld_at, start_at;           /* the field last pushed a rock (ms); the round began (two players) */
    float fld_a;                         /* where round the ship it pushed */
    uint16_t pushes;                     /* rocks the field has turned away (a probe) */
} dr_t;
#define DR_FIELD 13.0f                   /* the ship's field: no rock comes nearer its middle than this past its edge */
#define DR_ROUND_MS 60000u               /* two players: a round each, this long */
#define dr (*(dr_t *)g_arena)
_Static_assert(sizeof(dr_t) <= sizeof g_arena, "dr: the arena is too small");
static const float ROCK_R[4] = {0.0f, 6.0f, 10.0f, 16.0f};
static uint32_t dr_best;
static uint32_t dr_bpm(void) { return (uint32_t)sq_cfg[SQ_DRIFT].bpm + (uint32_t)(dr.wave > 8 ? 8 : dr.wave) * 3u; }
static void dr_rock(float x, float y, uint8_t size, float ang)
{
    int k;
    for (k = 0; k < DR_NR; k++)
        if (!dr.r[k].size) {
            float a = ang < -9.0f ? kr_f() * 6.2831f : ang, s = 0.018f + 0.026f * kr_f() + 0.01f * (float)(3 - size)
                                                                    + 0.002f * (float)(dr.wave > 6 ? 6 : dr.wave);
            dr.r[k].x = x, dr.r[k].y = y;
            dr.r[k].vx = fm_cosf(a) * s, dr.r[k].vy = fm_sinf(a) * s;
            dr.r[k].size = size;
            dr.r[k].shape = (uint8_t)kr();
            dr.r[k].rot = kr_f() * 6.2831f;
            dr.r[k].spin = (kr_f() - 0.5f) * 0.004f * (float)(4 - size);
            dr.r[k].lit = 0;
            return;
        }
}
static void dr_wave(void)
{
    int k, n = 3 + (dr.wave > 4 ? 4 : dr.wave);
    for (k = 0; k < DR_NR; k++)
        dr.r[k].size = 0;
    for (k = 0; k < n; k++)              /* big ones, from the edges */
        dr_rock(kr_f() < 0.5f ? 16.0f : 224.0f, 34.0f + kr_f() * 196.0f, 3, -10.0f);
    dr.total = (uint16_t)(n * 7);        /* each big one: itself, two middling, four small */
    dr.broken = 0;
    dr.tier = 0;
    dr.x = DR_CX, dr.y = DR_CY, dr.vx = dr.vy = 0.0f, dr.a = -1.5708f;
    dr.safe_until = plat_ms() + 2000u;
    dr.wave_at = plat_ms() | 1u;
    dr.trail_n = 0;
}
static void dr_new(void)
{
    int k;
    dr.wave = 0;
    dr.lives = 3;
    dr.score = 0;
    dr.over = 0;
    dr.last = 0;
    dr.combo = 0;
    dr.pushes = 0;
    dr.fld_at = 0;
    dr.start_at = plat_ms();
    for (k = 0; k < 4; k++)
        dr.shlife[k] = 0;
    dr_wave();
}
static int dr_aim_tone(void)             /* the way you aim, as a chord tone: eight ways round, low to high */
{
    float a = dr.a;
    int o;
    while (a < 0.0f)
        a += 6.2831f;
    while (a >= 6.2831f)
        a -= 6.2831f;
    o = (int)((a + 1.5708f + 0.3927f) / 0.7854f) % 8;   /* up is 0 */
    return o <= 4 ? o : 8 - o;           /* (up the scale both ways round to straight down) */
}
static void dr_nav(int dx, int dy, int sel)
{
    int k;
    (void)dx, (void)dy;
    if (dr.over) {
        if (sel && plat_ms() - dr.over_at > 800u)
            dr_new();
        return;
    }
    if (!sel)
        return;
    for (k = 0; k < 4; k++)
        if (!dr.shlife[k]) {
            float c = fm_cosf(dr.a), s = fm_sinf(dr.a);
            dr.shx[k] = dr.x + c * 9.0f, dr.shy[k] = dr.y + s * 9.0f;
            dr.shvx[k] = c * 0.32f + dr.vx, dr.shvy[k] = s * 0.32f + dr.vy;
            dr.shlife[k] = 700;
            dr.shot_at = plat_ms() | 1u;
            dr.vx -= c * 0.006f, dr.vy -= s * 0.006f;   /* a kick back */
            k_on(grid_next(), h_tone(6 + dr_aim_tone()), 52, NA_HIT, 0.08f);
            juice_burst((int32_t)dr.shx[k], (int32_t)dr.shy[k], 3, 1, C_YEL, C_INK, 0);
            return;
        }
}
static float dr_atan2(float y, float x)   /* atan2, to a few thousandths: enough to aim and slice */
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), mx = ax > ay ? ax : ay, mn = ax > ay ? ay : ax, a, z;
    if (mx < 1e-6f)
        return 0.0f;
    z = mn / mx;
    a = z * (0.9998660f + z * z * (-0.3302995f + z * z * (0.1801410f + z * z * (-0.0851330f + z * z * 0.0208351f))));
    if (ay > ax)
        a = 1.5707963f - a;
    if (x < 0.0f)
        a = 3.1415927f - a;
    return y < 0.0f ? -a : a;
}
static void dr_circle(int32_t cx, int32_t cy, int32_t r, uint16_t c)   /* an outline, dotted round */
{
    int j;
    for (j = 0; j < 40; j++) {
        float b = (float)j * 0.15708f;
        cv_rect(cx + (int32_t)(fm_cosf(b) * (float)r), cy + (int32_t)(fm_sinf(b) * (float)r), 1, 1, c);
    }
}
static float wrapf(float v, float lo, float hi) { return v < lo ? v + (hi - lo) : v >= hi ? v - (hi - lo) : v; }

/* the sweep's slice (0..15) a point is in: round from straight up, clockwise */
static int dr_slice(float x, float y)
{
    float a = dr_atan2(x - DR_CX, -(y - DR_CY));   /* 0 up, clockwise positive */
    if (a < 0.0f)
        a += 6.2831f;
    return (int)(a / 6.2831f * 16.0f) & 15;
}
static void dr_groove(uint32_t t, uint32_t step)
{
    uint32_t p = step % 16u, ms = sq_ms(t);
    int k, played = 0;
    h_deg_at(step / 16u);                /* a new chord each bar, at random in the key */
    dr.sweep_ms = ms, dr.sweep_step = step;
    if (dr.over)
        return;
    {
        int tier = sq_tier_of(dr.broken, dr.total);
        if (tier < (dr.wave > 2 ? 2 : dr.wave))   /* a later wave starts with the kick */
            tier = dr.wave > 2 ? 2 : dr.wave;
        sq_tier_say(tier, &dr.tier, 196);
        sq_drums(t, p, tier, NA_HAT, NA_KICK, NA_SNARE, 80);
    }
    if (p == 0u || p == 10u)             /* the bass: the chord's root, and a push into the bar's second half */
        k_on(t, h_bass(), p ? 70 : 88, NA_BASS, 0.25f);
    if (dr.thrust && !(p & 3u))          /* the engine, on the beat while it burns */
        k_on(t, h_bass() + 12, 46, NA_BASS, 0.1f);
    for (k = 0; k < DR_NR && played < 3; k++) {   /* the rocks in the sweep's slice: the biggest first (as found) */
        rock_t *r = &dr.r[k];
        float ddx, ddy, d;
        int band, tone;
        if (!r->size || dr_slice(r->x, r->y) != (int)p || !sq_roll(SQ_DRIFT))
            continue;
        ddx = r->x - DR_CX, ddy = r->y - DR_CY;
        d = fm_sqrtf(ddx * ddx + ddy * ddy);
        band = d < 40.0f ? 0 : d < 80.0f ? 1 : 2;
        tone = (3 - r->size) * 3 + band; /* big near the middle lowest .. small far out highest */
        k_on(t, h_tone(tone + 1), (uint32_t)(70 + r->size * 10 - played * 12), NA_SEQ, r->size == 3 ? 0.5f : 0.2f);
        r->lit = ms;
        played++;
        dr.sweep_notes++;
    }
}

static void dr_debris(float x, float y, uint8_t s, uint16_t col)
{
    int32_t ix = (int32_t)x, iy = (int32_t)y;
    juice_burst(ix, iy, 6 + 5 * s, 1 + s, col, C_INK, 0);
    juice_ring(ix, iy, 10 + 10 * s, col, 220u + 80u * s);
    if (s == 3) {
        juice_shake(4, 220);
        juice_freeze(40);
        juice_flash(col, 50, 120);
    } else if (s == 2)
        juice_shake(2, 140);
}
static uint16_t dr_col(uint8_t s) { return s == 3 ? RGB(170, 120, 230) : s == 2 ? RGB(110, 200, 220) : RGB(250, 170, 90); }

static void dr_tick(void)
{
    uint32_t dt = arc_dt(&dr.last), ms = plat_ms();
    int k, j, left = 0;
    float f = (float)dt;
    if (dr.over)
        return;
    if (players == 2 && ms - dr.start_at >= DR_ROUND_MS) {   /* two players: the round's time is up */
        dr.over = 1;
        dr.over_at = ms;
        juice_cool();
        if (dr.score > dr_best)
            dr_best = dr.score;
        return;
    }
    if (HELD_L)
        dr.a -= 0.0055f * f;
    if (HELD_R)
        dr.a += 0.0055f * f;
    dr.thrust = (uint8_t)HELD_U;
    if (dr.thrust) {
        dr.vx += fm_cosf(dr.a) * 0.00035f * f;
        dr.vy += fm_sinf(dr.a) * 0.00035f * f;
        if ((ms / 30u) & 1u)             /* the flame sheds sparks */
            juice_burst((int32_t)(dr.x - fm_cosf(dr.a) * 9.0f), (int32_t)(dr.y - fm_sinf(dr.a) * 9.0f), 1, 1,
                        C_YEL, C_PINK, 0);
    }
    if (dr.thrust || (ms / 40u) % 2u == 0u) {   /* the trail: where the ship has been, lately */
        for (k = 7; k > 0; k--)
            dr.trail_x[k] = dr.trail_x[k - 1], dr.trail_y[k] = dr.trail_y[k - 1];
        dr.trail_x[0] = dr.x, dr.trail_y[0] = dr.y;
        if (dr.trail_n < 8)
            dr.trail_n++;
    }
    dr.vx *= 1.0f - 0.0006f * f, dr.vy *= 1.0f - 0.0006f * f;
    dr.x = wrapf(dr.x + dr.vx * f, 0.0f, 240.0f);
    dr.y = wrapf(dr.y + dr.vy * f, 24.0f, 240.0f);
    if (dr.combo && ms - dr.combo_at > 1500u)
        dr.combo = 0;
    for (k = 0; k < 4; k++)
        if (dr.shlife[k]) {
            dr.shx[k] = wrapf(dr.shx[k] + dr.shvx[k] * f, 0.0f, 240.0f);
            dr.shy[k] = wrapf(dr.shy[k] + dr.shvy[k] * f, 24.0f, 240.0f);
            dr.shlife[k] = (uint16_t)(dr.shlife[k] > dt ? dr.shlife[k] - dt : 0);
        }
    for (k = 0; k < DR_NR; k++) {
        rock_t *r = &dr.r[k];
        float rr;
        if (!r->size)
            continue;
        left++;
        rr = ROCK_R[r->size];
        r->x = wrapf(r->x + r->vx * f, 0.0f, 240.0f);
        r->y = wrapf(r->y + r->vy * f, 24.0f, 240.0f);
        r->rot += r->spin * f;
        for (j = 0; j < 4; j++)
            if (dr.shlife[j]) {
                float ddx = dr.shx[j] - r->x, ddy = dr.shy[j] - r->y;
                if (ddx * ddx + ddy * ddy < (rr + 1.5f) * (rr + 1.5f)) {   /* it breaks: two smaller, flying apart */
                    uint8_t s = r->size;
                    float sa = dr_atan2(dr.shvy[j], dr.shvx[j]);
                    char pts[12] = {'+', 0};
                    uint32_t got;
                    dr.shlife[j] = 0;
                    r->size = 0;
                    dr.broken++;
                    dr.combo = (uint8_t)(dr.combo < 9 ? dr.combo + 1 : 9);
                    dr.combo_at = ms;
                    got = (uint32_t)(20 * (4 - s)) * dr.combo;
                    dr.score += got;
                    itos((int32_t)got, pts + 1);
                    juice_text((int32_t)r->x, (int32_t)r->y - 8, pts, dr.combo > 2 ? C_YEL : C_INK);
                    k_on(grid_next(), h_tone((3 - s) * 3 + 2 + dr.combo / 3), 100, s == 3 ? NA_PAD : NA_HIT,
                         s == 3 ? 0.9f : 0.4f);
                    if (s == 3)
                        k_on(grid_next(), 36, 96, NA_KICK, 0.2f);
                    dr_debris(r->x, r->y, s, dr_col(s));
                    if (s > 1) {
                        dr_rock(r->x, r->y, (uint8_t)(s - 1), sa + 0.7f);
                        dr_rock(r->x, r->y, (uint8_t)(s - 1), sa - 0.7f);
                    }
                    break;
                }
            }
        if (r->size) {                   /* the ship's field: a rock never touches it (Nick, 17 Nov 2026); one
                                          * coming near is turned away, bouncing off the field with a soft note */
            float ddx = r->x - dr.x, ddy = r->y - dr.y, lim = rr + DR_FIELD, d2 = ddx * ddx + ddy * ddy;
            if (d2 < lim * lim) {
                float d = fm_sqrtf(d2), nx = d > 0.01f ? ddx / d : 1.0f, ny = d > 0.01f ? ddy / d : 0.0f;
                float vn = (r->vx - dr.vx) * nx + (r->vy - dr.vy) * ny;
                r->x = dr.x + nx * lim, r->y = dr.y + ny * lim;   /* out to the field's edge */
                if (vn < 0.0f) {         /* coming in: bounced back out (off the ship's own motion too) */
                    r->vx -= 1.8f * vn * nx, r->vy -= 1.8f * vn * ny;
                    if (ms - dr.fld_at > 120u) {
                        dr.fld_at = ms | 1u;
                        dr.fld_a = dr_atan2(ny, nx);
                        dr.pushes++;
                        k_on(grid_next(), h_tone((3 - r->size) * 3 + 4), 70, NA_HIT, 0.3f);
                        juice_burst((int32_t)(dr.x + nx * DR_FIELD), (int32_t)(dr.y + ny * DR_FIELD), 5, 1, C_CYAN, C_INK, 0);
                    }
                }
            }
        }
    }
    if (!left) {
        char w[16] = "WAVE ";
        dr.wave++;
        arc_win();
        juice_sting(2);
        juice_ring((int32_t)dr.x, (int32_t)dr.y, 140, C_CYAN, 700);
        juice_confetti(120, 120, 30);
        itos((int32_t)dr.wave + 1, w + 5);
        dr_wave();
        juice_pop(120, 100, w, C_YEL);
    }
}

static void dr_rock_draw(const rock_t *r, int k, uint32_t ms)
{
    int32_t j, n = 9, px[9], py[9];
    float rr = ROCK_R[r->size];
    uint16_t col = dr_col(r->size);
    uint32_t q = r->lit ? ms - r->lit : 9999u;
    int32_t glow = (int32_t)q >= 0 && q < 260u ? 256 - (int32_t)(q * 256u / 260u) : 0;
    uint16_t face = mix565(mix565(C_BG, col, 150), C_INK, glow * 3 / 4), edge = mix565(col, C_INK, glow);
    float pop = 1.0f + 0.25f * (float)glow / 256.0f;   /* it swells as it plays */
    (void)k;
    for (j = 0; j < n; j++) {            /* a jagged outline: nine points, each its own distance out */
        uint32_t h = (uint32_t)(r->shape * 31 + j * 97) * 2654435761u;
        float a = r->rot + (float)j * 0.6981f, d = rr * pop * (0.72f + 0.28f * (float)(h >> 28) / 15.0f);
        px[j] = (int32_t)(r->x + fm_cosf(a) * d);
        py[j] = (int32_t)(r->y + fm_sinf(a) * d);
    }
    disc((int32_t)r->x, (int32_t)r->y, (int32_t)(rr * pop * 0.66f), face);
    for (j = 0; j < n; j++) {            /* filled: from the middle out to each edge */
        int32_t i2 = (j + 1) % n, m;
        for (m = 0; m <= 4; m++)
            cv_line((int32_t)r->x, (int32_t)r->y, px[j] + (px[i2] - px[j]) * m / 4, py[j] + (py[i2] - py[j]) * m / 4,
                    face);
    }
    {                                    /* a crater or two, and the shadowed side */
        float ca = r->rot + (float)(r->shape & 7);
        disc((int32_t)(r->x + fm_cosf(ca) * rr * 0.35f), (int32_t)(r->y + fm_sinf(ca) * rr * 0.35f),
             (int32_t)(rr * 0.22f) + 1, mix565(face, C_BG, 110));
        if (r->size >= 2)
            disc((int32_t)(r->x - fm_cosf(ca + 1.0f) * rr * 0.4f), (int32_t)(r->y - fm_sinf(ca + 1.0f) * rr * 0.4f),
                 (int32_t)(rr * 0.14f) + 1, mix565(face, C_BG, 90));
    }
    for (j = 0; j < n; j++)
        cv_line(px[j], py[j], px[(j + 1) % n], py[(j + 1) % n], edge);
}

static void dr_scene(void)
{
    int k, j;
    char n[12];
    uint32_t ms = plat_ms();
    {                                    /* the stars: three layers, drifting against the ship, twinkling */
        int i;
        for (i = 0; i < 48; i++) {
            uint32_t h = (uint32_t)i * 2654435761u, layer = (uint32_t)i % 3u;
            float par = layer == 2u ? 6.0f : layer == 1u ? 3.0f : 1.5f;
            int32_t x = ((int32_t)(h >> 8 & 255u) - (int32_t)(dr.x * par / 40.0f) + 480) % 240;
            int32_t y = 24 + ((int32_t)(h >> 16 & 255u) - (int32_t)(dr.y * par / 40.0f) + 412) % 206;
            int twk = ((ms / 90u + h) % 23u) == 0u;
            uint16_t c = twk ? C_INK : layer == 2u ? RGB(160, 150, 200) : layer == 1u ? RGB(90, 80, 130) : RGB(50, 44, 80);
            cv_rect(x, y, layer == 2u || twk ? 2 : 1, layer == 2u || twk ? 2 : 1, c);
        }
    }
    {                                    /* the sweep: a beam round the field from the middle, once a bar */
        float six = 15000.0f / (float)dr_bpm();
        float pos = (float)dr.sweep_step + 1.0f + (float)(int32_t)(ms - dr.sweep_ms) / six;
        float a = (pos / 16.0f) * 6.2831f - 1.5708f;
        int32_t s;
        for (s = 0; s < 4; s++) {        /* its fading tail */
            float b = a - (float)s * 0.07f;
            cv_line((int32_t)DR_CX, (int32_t)DR_CY, (int32_t)(DR_CX + fm_cosf(b) * 150.0f),
                    (int32_t)(DR_CY + fm_sinf(b) * 150.0f), mix565(C_BG, RGB(60, 200, 170), 90 - s * 22));
        }
        disc((int32_t)DR_CX, (int32_t)DR_CY, 2, mix565(C_BG, RGB(60, 200, 170), 140 + juice_pulse() / 3));
        for (s = 40; s <= 80; s += 40)   /* its rings: the bands a rock's note goes up by */
            for (j = 0; j < 32; j += 2) {
                float b = (float)j * 0.19635f;
                cv_rect((int32_t)(DR_CX + fm_cosf(b) * (float)s), (int32_t)(DR_CY + fm_sinf(b) * (float)s), 1, 1,
                        RGB(40, 46, 70));
            }
    }
    for (k = 0; k < DR_NR; k++)
        if (dr.r[k].size)
            dr_rock_draw(&dr.r[k], k, ms);
    for (k = 0; k < 4; k++)              /* the shots: a bright head, a tail */
        if (dr.shlife[k]) {
            int32_t x = (int32_t)dr.shx[k], y = (int32_t)dr.shy[k];
            cv_line(x, y, (int32_t)(dr.shx[k] - dr.shvx[k] * 18.0f), (int32_t)(dr.shy[k] - dr.shvy[k] * 18.0f),
                    mix565(C_BG, C_YEL, 140));
            disc(x, y, 2, C_YEL);
            cv_rect(x, y, 1, 1, C_INK);
        }
    if (!((int32_t)(ms - dr.safe_until) < 0 && (ms / 120u) & 1u)) {
        float c = fm_cosf(dr.a), s = fm_sinf(dr.a);
        int32_t nx = (int32_t)(dr.x + c * 10.0f), ny = (int32_t)(dr.y + s * 10.0f);
        int32_t lx = (int32_t)(dr.x - c * 7.0f + s * 7.0f), ly = (int32_t)(dr.y - s * 7.0f - c * 7.0f);
        int32_t rx = (int32_t)(dr.x - c * 7.0f - s * 7.0f), ry = (int32_t)(dr.y - s * 7.0f + c * 7.0f);
        int32_t bx = (int32_t)(dr.x - c * 3.0f), by = (int32_t)(dr.y - s * 3.0f);
        for (j = 1; j < dr.trail_n; j++)   /* the trail */
            disc((int32_t)dr.trail_x[j], (int32_t)dr.trail_y[j], j < 3 ? 2 : 1,
                 mix565(C_BG, dr.thrust ? C_PINK : C_PURP, 160 - j * 20));
        if (dr.thrust) {                 /* the flame: long and flickering */
            float fl = 9.0f + (float)((ms / 30u) % 3u) * 3.0f;
            int32_t fx = (int32_t)(dr.x - c * (5.0f + fl)), fy = (int32_t)(dr.y - s * (5.0f + fl));
            for (j = -3; j <= 3; j++)
                cv_line(bx + (int32_t)(s * (float)j), by - (int32_t)(c * (float)j), fx, fy, j == 0 ? C_INK : j * j < 4 ? C_YEL : C_PINK);
        }
        for (j = 0; j <= 14; j++) {      /* the hull: a swept arrowhead, notched at the back (fanned from the nose) */
            int32_t ex = j <= 7 ? lx + (bx - lx) * j / 7 : bx + (rx - bx) * (j - 7) / 7;
            int32_t ey = j <= 7 ? ly + (by - ly) * j / 7 : by + (ry - by) * (j - 7) / 7;
            cv_line(nx, ny, ex, ey, j == 7 ? RGB(60, 150, 200) : C_CYAN);
        }
        cv_line(nx, ny, lx, ly, C_INK);
        cv_line(nx, ny, rx, ry, C_INK);
        cv_line(lx, ly, bx, by, C_INK);
        cv_line(rx, ry, bx, by, C_INK);
        disc((int32_t)(dr.x + c * 2.0f), (int32_t)(dr.y + s * 2.0f), 2, C_PINK);   /* the cockpit */
        if ((int32_t)(ms - dr.safe_until) < 0)   /* the shield, while it's safe */
            dr_circle((int32_t)dr.x, (int32_t)dr.y, 14, mix565(C_BG, C_CYAN, 120 + juice_pulse() / 3));
        if (dr.fld_at && ms - dr.fld_at < 300u) {   /* the field, lit where it turned a rock away */
            int32_t g = 255 - (int32_t)(ms - dr.fld_at) * 255 / 300;
            for (j = -5; j <= 5; j++) {
                float a = dr.fld_a + (float)j * 0.16f;
                cv_rect((int32_t)(dr.x + fm_cosf(a) * DR_FIELD), (int32_t)(dr.y + fm_sinf(a) * DR_FIELD), 2, 2,
                        mix565(C_BG, C_CYAN, g * (6 - (j < 0 ? -j : j)) / 6));
            }
        }
    }
    itos((int32_t)dr.score, n);
    header("DRIFT", C_CYAN, "", C_PINK, n);
    if (players == 2 && !dr.over) {      /* two players: the round's time left */
        char tl[8];
        uint32_t q = plat_ms() - dr.start_at;
        itos((int32_t)((q < DR_ROUND_MS ? DR_ROUND_MS - q : 0u) / 1000u), tl);
        tcat(tl, "S");
        text_c(120, 214, &FONT_T, tl, C_GHOST);
    }
    if (dr.combo > 1) {                  /* the combo: breaks in quick succession multiply */
        char cb[8] = {'x', 0};
        itos(dr.combo, cb + 1);
        text_c(120, 28, &FONT_B, cb, dr.combo > 4 ? C_YEL : C_PINK);
    }
    if (dr.over) {
        char b[24] = "BEST ";
        itos((int32_t)dr_best, b + 5);
        over_box("THE END", "TOP KEY: AGAIN   HOME: MENU");
        text_c(120, 172, &FONT_XS, b, C_GHOST);
    }
}
