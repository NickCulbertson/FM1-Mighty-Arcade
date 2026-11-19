/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Nick Culbertson */
/* pinball.h: PINBALL, a table down in a dungeon, its art drawn here.
 * Included once from app.c; its state lives in the shared arena.
 *
 *  The table is two screens tall, two chambers, and the view follows the ball. The UPPER chamber: three lights across
 *  the top (roll over all three: the multiplier up; the flippers shift which are lit), three pop bumpers ridden by the
 *  dungeon's creatures (each a voice of the chord the groove is on: hitting them plays the music), a bank of four drop
 *  targets on the right wall (all down: the multiplier up, and back up they come), the chest (three hits: a
 *  treasure and the JACKPOT), and the door the teleport throws the ball out of. Its floor is a funnel with its own two
 *  flippers guarding the gate down. The LOWER chamber: three more pop bumpers (the zombie's in the middle), the
 *  multiplier's lamps (X2 .. X5) above the flippers, chasing arrows toward the shots, the jars on the right wall (break all three:
 *  MULTIBALL), the teleport (it takes the ball up to the top), the slingshots, the two big flippers and the water (a
 *  ball in it is gone; the first seconds of a ball, saved). Up the left wall, a stair lane back to the upper chamber;
 *  up the right, the plunger's lane. Torches on the walls light the floor; the ball is a steel ball, the torches in it.
 *  C: the left flippers, E: the right ones. Hold A (or D) to pull the plunger, let go to launch. C# nudges the table.
 *
 *  Its flow (reworked 17 Nov 2026: Nick found it hard to follow, too few ways to go): three shots up from the big
 *  flippers, each signed on the floor and each worth more in a combo (shot after shot, within five seconds):
 *  STAIRS up the left lane into the upper chamber; the PORTAL in the middle, the ball seen flying up to the door at
 *  the top; and the ORBIT up the right lane, round through the plunger's lane and out over the top lights (it lights
 *  the next one). A line under the header says what's next: the shots, the jars, the chest, the lights. */

enum { PB_BUMP, PB_KICK, PB_HAT, PB_BASS, PB_SNARE, PB_BELL, PB_BOOM, PB_WIN, PB_FLIP, PB_PAD };
#define PB_NB 3                          /* balls at once (multiball) */
#define PB_R 5.0f                        /* the ball's radius */
#define PB_G 0.00072f                    /* the table's slope: px/ms/ms */
#define PB_VMAX 1.05f
#define PB_H 452                         /* the table's height (world px); the view is 240 of it */
#define PB_MID 111                       /* the field's middle (x) */
#define PB_FLY_MS 900u                   /* the portal: the flight up to the door */
#define PB_COMBO_MS 5000u
typedef struct {
    float x, y, vx, vy;
    uint8_t on, lane;                    /* lane: in the plunger's lane */
    uint8_t held, fly;                   /* in the teleport; fly: on its way up, seen (the portal) */
    uint8_t via;                         /* on a shot: 1 the stairs, 3 the orbit (the reward at its top) */
    uint32_t fly_at;
    uint32_t held_until, cool_until;     /* out again at; not taken again before */
    int16_t tx[5], ty[5];                /* its trail (world) */
} pb_ball_t;
typedef struct {
    pb_ball_t b[PB_NB];
    float ang[4], w[4];                  /* the flippers: angle now, and its speed (rad/ms) */
    float cam;                           /* the view: the world y at the screen's top */
    int32_t camy;
    uint32_t bump_at[6], sling_at[2], jar_at[3], light_at, chest_at, tele_at, flip_at[2], ember_at;
    uint8_t drop[4];                     /* the drop targets: 1 down */
    uint32_t drop_at[4], bank_at;        /* each went down; the bank came back up */
    uint8_t lit[3], jar[3];              /* jar: 1 broken */
    uint8_t chest_hits, treasure;
    uint32_t chest_open_at;
    uint8_t mult, balls, over, multiball;
    uint32_t score, last, over_at, save_until, plunge_at, nudge_at, hero_at, drain_at;
    uint8_t hero_mood;                   /* 1 cheering, 2 sad */
    uint8_t combo;                       /* shots in a row, each within PB_COMBO_MS of the last */
    uint32_t shot_at, goal_at;
    uint8_t plunging, save_used, armed;  /* save_used: this ball has had its save; armed: A let go since the game
                                          * began (the press that started it doesn't launch) */
    uint8_t boss_hp, boss_max, boss_lv;  /* THE BOSS (Nick, 19 Nov 2026: one big obvious thing to hit): the lower
                                          * middle bumper, its hit points; down, and back tougher */
    uint32_t boss_hit_at, boss_down_at, slow_until;   /* slow_until: slow motion, after a big moment */
    uint8_t boss_ann;                    /* the next monster announced */
} pb_t;
#define PB_BOSS 3                        /* the bumper that is the boss */
#define PB_BOSS_R 20.0f
#define PB_BOSS_BACK 4000u               /* down this long, then back */
/* the bosses, in turn (Nick, 19 Nov 2026: the old monster movies; drawn our own way, not any film's makeup): each
 * angrier the more it's hit, then its head pops off the table and the next one rises */
#define PB_NMON 5
static const char *const PB_MON_NAME[PB_NMON] = {"VAMPIRE", "MUMMY", "WEREWOLF", "MONSTER", "LAGOON THING"};

static uint32_t pb_best;
static uint8_t pb_cam_lock;
static uint8_t pb_auto;                  /* AUTO FLIP (ARP): the flippers fire on their own as a ball comes down on
                                          * them (Nick, 19 Nov 2026: to spare the keys); it lasts between games */
static uint32_t pb_auto_until[2];         /* each side held by it until then */              /* (a test: the view doesn't follow the ball) */
#define pb (*(pb_t *)g_arena)
_Static_assert(sizeof(pb_t) <= sizeof g_arena, "pb: the arena is too small");
#define SY(wy) ((int32_t)(wy) - pb.camy)   /* world y to the screen's */
static int pb_mon(void) { return pb.boss_lv % PB_NMON; }

/* the table (world px): its walls (x1, y1, x2, y2) */
static const int16_t PB_WALL[][4] = {
    {16, 52, 16, 380},   {16, 52, 40, 28},    {40, 28, 190, 28},   {190, 28, 226, 64},   /* left, the top */
    {226, 64, 226, 452}, {206, 84, 206, 176}, {206, 256, 206, 452},                      /* the plunger's lane */
    {186, 330, 186, 236}, {186, 236, 206, 176},                                          /* the orbit lane, right:
                                                                                     * it bends into the plunger's */
    {36, 150, 36, 300},                                                                  /* the stair lane, left */
    {36, 160, 80, 196},  {206, 160, 142, 196},                                           /* the upper chamber's floor */
    {16, 380, 66, 414},  {206, 380, 156, 414}};                                          /* down to the flippers */
#define PB_NW ((int)(sizeof PB_WALL / sizeof PB_WALL[0]))
static const int16_t PB_SLING[2][4] = {{30, 350, 52, 386}, {192, 350, 170, 386}};   /* the slingshots: they kick */
static const int16_t PB_POST[3][2] = {{90, 62}, {131, 62}, {172, 62}};   /* between the lights: rocks */
static const int16_t PB_LIGHT[3][2] = {{70, 52}, {111, 52}, {152, 52}};
#define PB_NBUMP 6                       /* the pop bumpers: three above, three below */
static const int16_t PB_BUMPER[PB_NBUMP][2] = {{92, 102}, {154, 102}, {123, 142}, {111, 262}, {66, 300}, {150, 304}};
static const uint16_t PB_BUMPER_C[PB_NBUMP] = {RGB(110, 220, 90), RGB(240, 170, 70), RGB(110, 190, 255),
                                               RGB(230, 230, 240), RGB(255, 46, 151), RGB(176, 124, 255)};
/* the drop targets: four along the upper chamber's right wall; all down, the multiplier up */
static const int16_t PB_DROP[4][2] = {{197, 98}, {197, 112}, {197, 126}, {197, 140}};
static const int16_t PB_JAR[3][2] = {{166, 222}, {166, 248}, {166, 274}};
#define PB_CHEST_X 48
#define PB_CHEST_Y 108
#define PB_TELE_X 111
#define PB_TELE_Y 330
#define PB_OUT_X 36
#define PB_OUT_Y 72
#define PB_PLUNGE_Y 440                  /* the plunger's top, at rest */
/* the flippers: pivot, length, at rest, up; 0 1 the big ones below (left, right), 2 3 the upper chamber's gate; the
 * left key moves 0 and 2 */
static const float PB_FL[4][5] = {{68.0f, 416.0f, 30.0f, 0.50f, -0.45f},
                                  {154.0f, 416.0f, 30.0f, 2.6416f, 3.5916f},
                                  {82.0f, 198.0f, 22.0f, 0.50f, -0.45f},
                                  {140.0f, 198.0f, 22.0f, 2.6416f, 3.5916f}};
#define PB_NTORCH 6
static const int16_t PB_TORCH[PB_NTORCH][2] = {{8, 110}, {8, 240}, {8, 360}, {233, 110}, {233, 240}, {233, 360}};

static void pb_sounds(void)
{
    k_slot_set(PB_BUMP, &KSND[S_VIBES]);
    k_slot_set(PB_KICK, &KSND[S_KICK]);
    k_slot_set(PB_HAT, &KSND[S_HAT]);
    k_slot_set(PB_BASS, &KSND[S_FMBASS]);
    k_slot_set(PB_SNARE, &KSND[S_SNARE]);
    k_slot_set(PB_BELL, &KSND[S_CELESTA]);
    k_slot_set(PB_BOOM, &KSND[S_SONAR]);
    k_slot_set(PB_WIN, &KSND[S_TUBE]);
    k_slot_set(PB_FLIP, &KSND[S_LOGDRUM]);   /* (a soft wooden knock: was CHIP, Nick 19 Nov 2026: more subtle) */
    k_slot_set(PB_PAD, &KSND[S_BRASSPAD]);
    k_slot_rel(PB_BUMP, 0.6f);
    k_slot_rel(PB_BASS, 0.2f);
    k_slot_rel(PB_BOOM, 0.8f);
    k_slot_rel(PB_WIN, 1.2f);
    k_slot_rel(PB_PAD, 1.4f);
    k_main_slot = PB_BUMP;
    k_main.rev_mix = 0.22f, k_main.rev_secs = 2.2f, k_main.shimmer = 0.0f, k_main.dly_mix = 0.12f;   /* a stone hall */
}
static int pb_live(void)                 /* balls on the table (or in the teleport) */
{
    int k, n = 0;
    for (k = 0; k < PB_NB; k++)
        n += pb.b[k].on;
    return n;
}
static void pb_groove(uint32_t t, uint32_t step)   /* it builds with the multiplier, and multiball is the whole band */
{
    uint32_t p = step % 16u;
    int mb = pb_live() > 1;
    h_deg_at(step / 16u);
    if (pb.over)
        return;
    if (p % 4u == 2u || (pb.mult >= 3u && p % 2u == 1u))
        k_on(t, 92, 36 + 6 * pb.mult, PB_HAT, 0.04f);
    if (p == 0u || p == 8u || (pb.mult >= 2u && (p == 6u || p == 14u)))
        k_on(t, h_bass(), 84, PB_BASS, 0.14f);
    if (mb || pb.mult >= 3u) {
        if (p == 0u || p == 8u || (mb && p == 10u))
            k_on(t, 36, 96, PB_KICK, 0.2f);
        if (p == 4u || p == 12u)
            k_on(t, 50, 76, PB_SNARE, 0.2f);
    }
    if (p == 0u && step % 64u == 0u)
        h_pad(t, 40, PB_PAD, 2.5f, 0);
}
static void pb_ball_to_lane(int k)
{
    pb_ball_t *b = &pb.b[k];
    int i;
    b->on = 1;
    b->lane = 1;
    b->held = 0;
    b->fly = 0;
    b->via = 0;
    b->x = 216.0f;
    b->y = (float)PB_PLUNGE_Y - PB_R;
    b->vx = b->vy = 0.0f;
    for (i = 0; i < 5; i++)
        b->tx[i] = (int16_t)b->x, b->ty[i] = (int16_t)b->y;
}
static void pb_new(void)
{
    int k;
    for (k = 0; k < PB_NB; k++)
        pb.b[k].on = 0;
    for (k = 0; k < 4; k++)
        pb.ang[k] = PB_FL[k][3], pb.w[k] = 0.0f;
    for (k = 0; k < 3; k++)
        pb.lit[k] = 0, pb.jar[k] = 0;
    for (k = 0; k < 4; k++)
        pb.drop[k] = 0, pb.drop_at[k] = 0;
    pb.bank_at = 0;
    pb.lit[1] = 1;
    pb.chest_hits = 0;
    pb.chest_open_at = 0;
    pb.mult = 1;
    pb.combo = 0;
    pb.shot_at = 0;
    pb.balls = 3;
    pb.over = 0;
    pb.score = 0;
    pb.last = 0;
    pb.multiball = 0;
    pb.save_until = 0;
    pb.save_used = 0;
    pb.armed = 0;
    pb.hero_mood = 0;
    pb.boss_lv = 0, pb.boss_max = pb.boss_hp = 8, pb.boss_hit_at = pb.boss_down_at = pb.slow_until = 0;
    pb.boss_ann = 1;
    pb.cam = (float)(PB_H - 240);
    pb.camy = PB_H - 240;
    pb_ball_to_lane(0);
}
static void pb_add(uint32_t pts, int32_t x, int32_t wy, uint16_t c)   /* points, shown where they were won */
{
    char s[12] = {'+', 0};
    int32_t y = SY(wy);
    pts *= pb.mult;
    pb.score += pts;
    itos((int32_t)pts, s + 1);
    if (pts >= 1000u)                    /* (only the big ones: a bumper's 100s went by in a blur of numbers) */
        juice_text(x < 30 ? 30 : x > 200 ? 200 : x, y < 34 ? 34 : y > 226 ? 226 : y, s, c);
}
static void pb_cheer(void)
{
    pb.hero_mood = 1;
    pb.hero_at = plat_ms() | 1u;
}
static void pb_shot(int which, int32_t x, int32_t wy)   /* a shot made: 1 STAIRS, 2 PORTAL, 3 ORBIT */
{
    static const char *const NAME[4] = {"", "STAIRS", "PORTAL", "ORBIT"};
    uint32_t now = plat_ms();
    char t[20];
    int k;
    pb.combo = (uint8_t)(pb.shot_at && now - pb.shot_at < PB_COMBO_MS && pb.combo < 9 ? pb.combo + 1 : 1);
    pb.shot_at = now | 1u;
    tcpy(t, NAME[which]);
    if (pb.combo > 1) {
        char c[6] = " X";
        itos(pb.combo, c + 2);
        tcat(t, c);
    }
    juice_pop(120, 96, t, which == 2 ? C_PURP : which == 1 ? C_YEL : C_CYAN);
    pb_add(500u * pb.combo, x, wy, C_INK);
    for (k = 0; k < 2 + (pb.combo > 3 ? 3 : pb.combo); k++)   /* up the chord: the longer the combo, the further */
        k_on(grid_next() + (uint32_t)k * (uint32_t)(SR_F * 0.09f), h_note(1 + k) + 12, 96, PB_BELL, 0.4f);
    juice_heat(8 + 4 * pb.combo);
    if (pb.combo >= 3)
        pb_cheer();
}
/* the ball against a segment a..b (thick rad), the segment moving at vs: pushed out, bounced (e), kicked; returns how
 * hard it hit (0: no) */
static float pb_hit_seg(pb_ball_t *b, float ax, float ay, float bx, float by, float rad, float e, float kick,
                        float vsx, float vsy, float *cx_out, float *cy_out)
{
    float dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy, t, cx, cy, nx, ny, d, R = PB_R + rad, rvx, rvy, vn;
    t = l2 > 0.0f ? ((b->x - ax) * dx + (b->y - ay) * dy) / l2 : 0.0f;
    t = fm_clampf(t, 0.0f, 1.0f);
    cx = ax + dx * t;
    cy = ay + dy * t;
    nx = b->x - cx;
    ny = b->y - cy;
    d = nx * nx + ny * ny;
    if (d >= R * R || d < 0.0001f)
        return 0.0f;
    d = fm_sqrtf(d);
    nx /= d;
    ny /= d;
    b->x = cx + nx * R;
    b->y = cy + ny * R;
    if (cx_out)
        *cx_out = cx, *cy_out = cy;
    rvx = b->vx - vsx;
    rvy = b->vy - vsy;
    vn = rvx * nx + rvy * ny;
    if (vn >= 0.0f)
        return 0.001f;
    b->vx = rvx - (1.0f + e) * vn * nx + vsx + kick * nx;
    b->vy = rvy - (1.0f + e) * vn * ny + vsy + kick * ny;
    return -vn + kick;
}
static float pb_hit_circle(pb_ball_t *b, float x, float y, float r, float e, float kick)
{
    return pb_hit_seg(b, x, y, x, y, r, e, kick, 0.0f, 0.0f, 0, 0);
}
static void pb_bumper(int i)
{
    int32_t x = PB_BUMPER[i][0], y = PB_BUMPER[i][1];
    pb.bump_at[i] = plat_ms() | 1u;
    k_on(0, h_note(1 + i) + 12 + (pb.mult >= 3 ? 12 : 0), 110, PB_BUMP, 0.5f);   /* the chord, played by the creatures */
    k_on(0, 36, 70, PB_KICK, 0.1f);
    pb_add(100, x, y - 14, C_YEL);
    juice_burst(x, SY(y), 10, 3, PB_BUMPER_C[i], C_INK, 0);
    juice_ring(x, SY(y), 22, C_INK, 220);
    juice_shake(3, 140);
    juice_freeze(22);
    juice_heat(5);
}
static void pb_multiball(void);
static int pb_boss_up(void)              /* (signed: a time stamped | 1 can be a ms ahead: GPT's review) */
{
    return !pb.boss_down_at || (int32_t)(plat_ms() - pb.boss_down_at) >= (int32_t)PB_BOSS_BACK;
}
static void pb_boss_hit(void)            /* the boss hit: a blow, its chord tone, and down it goes at last */
{
    int32_t x = PB_BUMPER[PB_BOSS][0], y = PB_BUMPER[PB_BOSS][1];
    uint32_t now = plat_ms();
    pb.boss_hit_at = now | 1u;
    if (!pb_boss_up())
        return;
    if (pb.boss_down_at)                 /* back: full again */
        pb.boss_down_at = 0, pb.boss_ann = 1;
    pb.boss_hp--;
    k_on(0, h_note(1 + pb.boss_hp % 4) + 12, 112, PB_BUMP, 0.5f);
    k_on(0, h_bass(), 100, PB_BASS, 0.3f);
    k_on(0, 36, 100, PB_KICK, 0.15f);
    pb_add(500u * (1u + pb.boss_lv), x, y - 26, C_YEL);
    juice_burst(x, SY(y), 16, 3, RGB(140, 230, 90), C_INK, 1);
    juice_ring(x, SY(y), 34, C_INK, 260);
    juice_shake(5, 200);
    juice_freeze(40);
    juice_kick(0, 3);
    juice_heat(8);
    if (pb.boss_hp == 0) {               /* down: slow motion, a jackpot, multiball, and it comes back tougher */
        pb.boss_down_at = now | 1u;
        pb.slow_until = now + 1600u;
        {
            char dn[24];
            tcpy(dn, PB_MON_NAME[pb_mon()]);
            tcat(dn, " DOWN!");
            if (text_w(&FONT_L, dn) > 228)  /* (too wide for the screen: the name small, DOWN! big) */
                juice_pop(120, 96, "DOWN!", C_YEL), juice_text(120, 136, PB_MON_NAME[pb_mon()], C_YEL);
            else
                juice_pop(120, 96, dn, C_YEL);
        }
        pb.boss_lv++;
        pb.boss_ann = 0;
        pb.boss_max = pb.boss_hp = (uint8_t)(pb.boss_max + 4 > 40 ? 40 : pb.boss_max + 4);
        pb_add(10000u * pb.boss_lv, x, y - 30, C_YEL);
        juice_freeze(220);
        juice_flash(RGB(255, 255, 255), 210, 500);
        juice_shake(11, 700);
        juice_confetti(x, SY(y), 60);
        juice_ring(x, SY(y), 120, C_YEL, 600);
        juice_sting(2);
        pb_cheer();
        if (!pb.multiball)
            pb_multiball();
    }
}
static void pb_drain(int k)
{
    pb_ball_t *b = &pb.b[k];
    int32_t x = (int32_t)b->x;
    b->on = 0;
    juice_burst(x, SY(PB_H - 12), 18, 3, RGB(90, 170, 255), RGB(220, 240, 255), 1);
    juice_ring(x, SY(PB_H - 12), 40, RGB(90, 170, 255), 400);
    k_on(0, 88, 90, PB_HAT, 0.3f);
    if (pb_live())
        return;                          /* multiball: the others play on */
    pb.multiball = 0;
    if (plat_ms() < pb.save_until) {     /* the ball save (once a ball): back it comes */
        pb.save_until = 0;
        pb.save_used = 1;
        juice_pop(120, 150, "SAVED!", C_CYAN);
        k_on(0, degree_note(7), 100, PB_BELL, 0.6f);
        pb_ball_to_lane(0);
        return;
    }
    k_on(0, degree_note(-5), 110, PB_BOOM, 0.8f);
    juice_shake(8, 500);
    juice_freeze(120);
    juice_flash(RGB(255, 40, 80), 140, 500);
    juice_glitch(350);
    J.heat /= 3;
    pb.hero_mood = 2;
    pb.hero_at = plat_ms() | 1u;
    pb.mult = 1;
    pb.save_used = 0;
    pb.drain_at = plat_ms() | 1u;
    if (--pb.balls == 0) {
        pb.over = 1;
        pb.over_at = plat_ms();
        if (pb.score > pb_best)
            pb_best = pb.score;
        juice_cool();
        return;
    }
    juice_pop(120, 150, pb.balls == 1 ? "LAST BALL" : "BALL LOST", C_PINK);
    pb_ball_to_lane(0);
}
static void pb_jackpot(void)
{
    pb.chest_open_at = plat_ms() | 1u;
    pb.treasure = (uint8_t)(kr() % 10u);
    pb.chest_hits = 0;
    pb_add(5000, PB_CHEST_X + 30, PB_CHEST_Y, C_YEL);
    pb.slow_until = plat_ms() + 900u;
    juice_pop(120, 120, "JACKPOT!", C_YEL);
    juice_freeze(110);
    juice_flash(RGB(255, 255, 255), 200, 400);
    juice_shake(9, 500);
    juice_ring(PB_CHEST_X, SY(PB_CHEST_Y), 90, C_YEL, 500);
    juice_confetti(PB_CHEST_X, SY(PB_CHEST_Y), 40);
    juice_sting(2);
    pb_cheer();
    {
        int k;
        for (k = 0; k < 6; k++)
            k_on(k_at(0.08f * (float)k), h_note(1 + k) + 12, 100, PB_WIN, 1.0f);
    }
}
static void pb_multiball(void)
{
    int k, n = 0;
    uint32_t now = plat_ms();
    pb.multiball = 1;
    for (k = 0; k < PB_NB; k++)
        if (!pb.b[k].on) {               /* out of the door, one after another */
            pb_ball_t *b = &pb.b[k];
            b->on = 1;
            b->lane = 0;
            b->held = 1;
            b->fly = 0;
            b->held_until = now + 500u + 450u * (uint32_t)n++;
            b->x = PB_OUT_X, b->y = PB_OUT_Y;
        }
    juice_pop(120, 110, "MULTIBALL!", C_PINK);
    juice_freeze(120);
    juice_flash(C_PINK, 160, 500);
    juice_shake(8, 500);
    juice_ring(120, 120, 120, C_PINK, 600);
    juice_sting(2);
    juice_heat(40);
    pb_cheer();
}
static void pb_step(pb_ball_t *b, float h)   /* one small step of one ball */
{
    int i;
    uint32_t now = plat_ms();
    if (b->held) {
        if (b->fly) {                    /* the portal: up through the air to the door, for all to see */
            float f = fm_clampf((float)(now - b->fly_at) / (float)PB_FLY_MS, 0.0f, 1.0f), e = f * f * (3.0f - 2.0f * f);
            b->x = (float)PB_TELE_X + (float)(PB_OUT_X - PB_TELE_X) * e;
            b->y = (float)PB_TELE_Y + (float)(PB_OUT_Y - PB_TELE_Y) * e - fm_sinf(f * 3.1415927f) * 30.0f;
        }
        if ((int32_t)(now - b->held_until) >= 0) {   /* out of the door at the top */
            b->held = 0;
            b->fly = 0;
            b->x = PB_OUT_X, b->y = PB_OUT_Y;
            b->vx = 0.42f, b->vy = 0.12f;
            b->cool_until = now + 600u;
            juice_ring(PB_OUT_X, SY(PB_OUT_Y), 26, C_PURP, 300);
            juice_burst(PB_OUT_X, SY(PB_OUT_Y), 12, 3, C_PURP, C_INK, 0);
            k_on(0, h_note(4) + 12, 90, PB_BELL, 0.4f);
        }
        return;
    }
    b->vy += PB_G * h;
    {
        float sp = b->vx * b->vx + b->vy * b->vy;
        if (sp > PB_VMAX * PB_VMAX) {
            sp = PB_VMAX / fm_sqrtf(sp);
            b->vx *= sp, b->vy *= sp;
        }
    }
    b->x += b->vx * h;
    b->y += b->vy * h;
    if (b->lane) {                       /* the plunger's lane: on the plunger, or going up it */
        float py = (float)PB_PLUNGE_Y - (pb.plunging ? (float)(now - pb.plunge_at > 900u ? 900u : now - pb.plunge_at) / 900.0f * 10.0f : 0.0f);
        if (b->y > py - PB_R) {
            b->y = py - PB_R;
            if (b->vy > 0.0f)
                b->vy = 0.0f;
        }
        if (b->y < 80.0f) {              /* out into the table */
            b->lane = 0;
            if (b->via == 3) {           /* round the orbit: the next top light, lit */
                int i2;
                b->via = 0;
                for (i2 = 0; i2 < 3 && pb.lit[i2]; i2++)
                    ;
                if (i2 < 3)
                    pb.lit[i2] = 1;
                pb_shot(3, 196, 90);
            }
        }
        if (b->x < 205.0f && b->y > 176.0f)   /* (down from the plunger's lane into the orbit's: on the table) */
            b->lane = 0;
    }
    if (!b->lane) {                      /* the shots: into a lane going up, rewarded at its top */
        if (b->x > 186.0f && b->x < 206.0f && b->y > 236.0f && b->y < 330.0f && b->vy < -0.05f)
            b->via = 3;
        else if (b->x < 36.0f && b->y > 150.0f && b->y < 300.0f && b->vy < -0.05f)
            b->via = 1;
        else if (b->via == 1 && b->y < 150.0f) {
            b->via = 0;
            pb_shot(1, 40, 150);
        } else if (b->y > 340.0f)
            b->via = 0;
    }
    for (i = 0; i < PB_NW; i++)
        if (pb_hit_seg(b, PB_WALL[i][0], PB_WALL[i][1], PB_WALL[i][2], PB_WALL[i][3], 1.0f, 0.45f, 0.0f, 0.0f, 0.0f, 0, 0)
            > 0.5f && !b->lane) {        /* a hard hit on the stone: it rings, the screen jolts */
            juice_shake(2, 90);
            juice_burst((int32_t)b->x, SY(b->y), 5, 2, RGB(255, 200, 90), C_INK, 0);
            k_on(0, 52, 60, PB_HAT, 0.05f);
        }
    if (!b->lane && b->x > 209.0f && b->y > 90.0f)   /* back down the lane from the top: to the plunger again */
        b->lane = 1;
    if (b->lane)
        return;
    for (i = 0; i < 2; i++)              /* the slingshots */
        if (pb_hit_seg(b, PB_SLING[i][0], PB_SLING[i][1], PB_SLING[i][2], PB_SLING[i][3], 1.5f, 0.4f, 0.0f, 0.0f, 0.0f,
                       0, 0) > 0.12f) {
            float nx = (float)(PB_SLING[i][3] - PB_SLING[i][1]), ny = -(float)(PB_SLING[i][2] - PB_SLING[i][0]), l;
            l = fm_sqrtf(nx * nx + ny * ny);
            nx /= l, ny /= l;
            if (nx * ((float)(PB_SLING[i][0] + PB_SLING[i][2]) / 2.0f - (float)PB_MID) > 0.0f)   /* (toward the middle) */
                nx = -nx, ny = -ny;
            b->vx += nx * 0.38f;
            b->vy += ny * 0.38f;
            if (now - pb.sling_at[i] > 80u) {
                pb.sling_at[i] = now | 1u;
                k_on(0, h_bass() + 12, 100, PB_BASS, 0.2f);
                pb_add(10, (PB_SLING[i][0] + PB_SLING[i][2]) / 2, (PB_SLING[i][1] + PB_SLING[i][3]) / 2 - 10, C_PINK);
                juice_shake(2, 100);
                juice_kick(i ? -3 : 3, 0);
            }
        }
    for (i = 0; i < 3; i++)
        pb_hit_circle(b, PB_POST[i][0], PB_POST[i][1], 5.0f, 0.5f, 0.0f);
    for (i = 0; i < PB_NBUMP; i++)       /* the bumpers: the creatures (and the boss, bigger) */
        if (!(i == PB_BOSS && !pb_boss_up()) &&   /* (the boss down: nothing there to hit) */
            pb_hit_circle(b, PB_BUMPER[i][0], PB_BUMPER[i][1], i == PB_BOSS ? PB_BOSS_R : 14.0f, 0.6f, 0.32f) > 0.05f &&
            now - pb.bump_at[i] > 60u) {
            if (i == PB_BOSS) {
                pb.bump_at[i] = now | 1u;
                pb_boss_hit();
            } else
                pb_bumper(i);
        }
    for (i = 0; i < 4; i++)              /* the drop targets: down when hit */
        if (!pb.drop[i] && pb_hit_seg(b, PB_DROP[i][0], PB_DROP[i][1] - 5, PB_DROP[i][0], PB_DROP[i][1] + 5, 2.0f, 0.4f,
                                      0.05f, 0.0f, 0.0f, 0, 0) > 0.05f) {
            int k, all = 1;
            pb.drop[i] = 1;
            pb.drop_at[i] = now | 1u;
            k_on(0, h_note(1 + i) + 24, 100, PB_BELL, 0.3f);
            k_on(0, 40, 90, PB_KICK, 0.08f);
            pb_add(200, PB_DROP[i][0] - 18, PB_DROP[i][1], C_CYAN);
            juice_burst(PB_DROP[i][0], SY(PB_DROP[i][1]), 8, 2, C_CYAN, C_INK, 1);
            juice_shake(2, 100);
            for (k = 0; k < 4; k++)
                all &= pb.drop[k];
            if (all) {                   /* the bank: the multiplier up, and back up it comes */
                char ms2[8] = {'X', 0};
                if (pb.mult < 5)
                    pb.mult++;
                itos(pb.mult, ms2 + 1);
                pb_add(2000, 170, 120, C_YEL);
                juice_pop(120, 96, ms2, C_CYAN);
                juice_ring(PB_DROP[1][0], SY(119), 60, C_CYAN, 400);
                juice_shake(5, 260);
                juice_heat(20);
                pb.bank_at = now + 1200u;
                pb_cheer();
            }
        }
    if (pb.bank_at && (int32_t)(now - pb.bank_at) >= 0) {
        int k;
        pb.bank_at = 0;
        for (k = 0; k < 4; k++)
            pb.drop[k] = 0, pb.drop_at[k] = now | 1u;
    }
    for (i = 0; i < 3; i++)              /* the jars */
        if (!pb.jar[i] && pb_hit_circle(b, PB_JAR[i][0], PB_JAR[i][1], 11.0f, 0.5f, 0.05f) > 0.05f) {
            int k, all = 1;
            pb.jar[i] = 1;
            pb.jar_at[i] = now | 1u;
            juice_burst(PB_JAR[i][0], SY(PB_JAR[i][1]), 14, 3, RGB(196, 120, 70), RGB(120, 70, 50), 1);
            juice_shake(4, 160);
            k_on(0, h_note(2 + i) + 24, 100, PB_BELL, 0.4f);
            k_on(0, 90, 100, PB_HAT, 0.1f);
            pb_add(250, PB_JAR[i][0] - 20, PB_JAR[i][1], RGB(240, 170, 90));
            for (k = 0; k < 3; k++)
                all &= pb.jar[k];
            if (all && pb_live() == 1)
                pb_multiball();
        }
    if (!pb.chest_open_at && pb_hit_circle(b, PB_CHEST_X, PB_CHEST_Y, 14.0f, 0.5f, 0.12f) > 0.08f &&
        now - pb.chest_at > 200u) {     /* the chest: three hits */
        pb.chest_at = now | 1u;
        pb.chest_hits++;
        juice_shake(4, 200);
        juice_ring(PB_CHEST_X, SY(PB_CHEST_Y), 30, C_YEL, 260);
        k_on(0, h_note(pb.chest_hits) + 12, 110, PB_WIN, 0.5f);
        pb_add(300, PB_CHEST_X + 20, PB_CHEST_Y - 14, C_YEL);
        if (pb.chest_hits >= 3)
            pb_jackpot();
        else
            juice_text(PB_CHEST_X + 26, SY(PB_CHEST_Y + 8), pb.chest_hits == 1 ? "1 / 3" : "2 / 3", C_INK);
    } else if (pb.chest_open_at)
        pb_hit_circle(b, PB_CHEST_X, PB_CHEST_Y, 14.0f, 0.5f, 0.0f);
    for (i = 0; i < 3; i++)              /* the lights: rolled over */
        if (!pb.lit[i] && (b->x - PB_LIGHT[i][0]) * (b->x - PB_LIGHT[i][0]) + (b->y - PB_LIGHT[i][1]) * (b->y - PB_LIGHT[i][1]) < 120.0f) {
            pb.lit[i] = 1;
            k_on(0, degree_note(7 + 2 * i), 100, PB_BELL, 0.4f);
            pb_add(50, PB_LIGHT[i][0], PB_LIGHT[i][1] + 12, C_YEL);
            juice_ring(PB_LIGHT[i][0], SY(PB_LIGHT[i][1]), 14, C_YEL, 200);
            if (pb.lit[0] && pb.lit[1] && pb.lit[2]) {   /* all three: the multiplier up */
                char ms[8] = {'X', 0};
                pb.light_at = now | 1u;
                if (pb.mult < 5)
                    pb.mult++;
                itos(pb.mult, ms + 1);
                pb_add(1000, PB_MID, 70, C_YEL);
                juice_pop(120, 96, ms, C_YEL);
                juice_shake(5, 300);
                juice_ring(PB_MID, SY(42), 70, C_YEL, 400);
                juice_heat(20);
                pb.lit[0] = pb.lit[1] = pb.lit[2] = 0;
                pb_cheer();
            }
        }
    if (now >= b->cool_until && (b->x - PB_TELE_X) * (b->x - PB_TELE_X) + (b->y - PB_TELE_Y) * (b->y - PB_TELE_Y) < 64.0f) {
        b->held = 1;                     /* the teleport: in, and up to the door at the top */
        b->fly = 1;
        b->fly_at = now;
        b->held_until = now + PB_FLY_MS;
        pb.tele_at = now | 1u;
        pb_shot(2, PB_TELE_X, PB_TELE_Y - 14);
        juice_ring(PB_TELE_X, SY(PB_TELE_Y), 30, C_PURP, 350);
        juice_burst(PB_TELE_X, SY(PB_TELE_Y), 14, 3, C_PURP, C_CYAN, 0);
        juice_shake(3, 200);
        k_on(0, h_note(1) + 24, 90, PB_BELL, 0.4f);
        k_on(k_at(0.1f), h_note(3) + 24, 90, PB_BELL, 0.4f);
        return;
    }
    for (i = 0; i < 4; i++) {            /* the flippers: a moving segment */
        float px = PB_FL[i][0], py = PB_FL[i][1], L = PB_FL[i][2], a = pb.ang[i];
        float tx = px + L * fm_cosf(a), ty = py + L * fm_sinf(a), cx = 0.0f, cy = 0.0f;
        if (pb_hit_seg(b, px, py, tx, ty, 2.5f, 0.15f, 0.0f, 0.0f, 0.0f, &cx, &cy) > 0.0f && pb.w[i] != 0.0f) {
            /* the speed of the flipper where the ball touches it (w x r), into the ball: the shot */
            float rx = cx - px, ry = cy - py, vsx = -pb.w[i] * ry, vsy = pb.w[i] * rx;
            float nx = b->x - cx, ny = b->y - cy, l = fm_sqrtf(nx * nx + ny * ny);
            if (l > 0.001f) {
                float push = (vsx * nx + vsy * ny) / l;
                if (push > 0.0f) {
                    b->vx += nx / l * push * 1.15f;
                    b->vy += ny / l * push * 1.15f;
                }
            }
        }
    }
    if (b->y > (float)PB_H)
        pb_drain((int)(b - pb.b));
}
static void pb_nav(int dx, int dy, int sel)
{
    (void)dx;
    if (pb.over) {
        if (sel && plat_ms() - pb.over_at > 800u)
            pb_new();
        return;
    }
    if (dy > 0 && plat_ms() - pb.nudge_at > 700u) {   /* a nudge (C#, up): the table knocked */
        int k;
        pb.nudge_at = plat_ms();
        for (k = 0; k < PB_NB; k++)
            if (pb.b[k].on && !pb.b[k].lane && !pb.b[k].held) {
                pb.b[k].vy -= 0.18f;
                pb.b[k].vx += (kr() & 1u) ? 0.08f : -0.08f;
            }
        juice_kick(0, -6);
        juice_shake(3, 150);
        k_on(0, 40, 90, PB_KICK, 0.2f);
    }
}
static void pb_tick(void)
{
    uint32_t dt = arc_dt(&pb.last), now = plat_ms();
    int k, i, j, n;
    int keys[2], pull;
    if (pb.over)
        return;
    if ((int32_t)(now - pb.slow_until) < 0)   /* slow motion: a third the speed */
        dt = (dt + 2u) / 3u;
    if (pb.boss_down_at && !pb.boss_ann && (int32_t)(now - pb.boss_down_at) >= (int32_t)PB_BOSS_BACK) {   /* the next
                                          * monster rises */
        char nm[24];
        pb.boss_ann = 1;
        tcpy(nm, "THE ");
        tcat(nm, PB_MON_NAME[pb_mon()]);
        tcat(nm, "!");
        if (text_w(&FONT_L, nm) > 228)      /* (too wide: without THE) */
            tcpy(nm, PB_MON_NAME[pb_mon()]), tcat(nm, "!");
        juice_pop(120, 96, nm, C_PINK);
        k_on(0, h_bass() - 12, 110, PB_BOOM, 1.0f);   /* a low rumble, and its chord, menacing */
        k_on(k_at(0.1f), h_note(1), 90, PB_PAD, 1.6f);
        k_on(k_at(0.1f), h_note(2) + 1, 70, PB_PAD, 1.6f);
        juice_shake(6, 500);
    }
    if (pb_auto) {                       /* AUTO FLIP: a ball coming down near a flipper, that side fires */
        int fl, bk;
        for (bk = 0; bk < PB_NB; bk++) {
            pb_ball_t *bb = &pb.b[bk];
            if (!bb->on || bb->lane || bb->held || bb->fly || bb->vy < 0.03f)
                continue;
            for (fl = 0; fl < 4; fl++) {
                float px = PB_FL[fl][0], py = PB_FL[fl][1], L = PB_FL[fl][2], a = PB_FL[fl][3];
                float tx = px + L * fm_cosf(a), ty = py + L * fm_sinf(a), lx = tx - px, ly = ty - py;
                float t = ((bb->x - px) * lx + (bb->y - py) * ly) / (lx * lx + ly * ly), dx, dy;
                t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
                dx = bb->x - (px + t * lx), dy = bb->y - (py + t * ly);
                if (dy < 2.0f && dx * dx + dy * dy < (PB_R + 9.0f) * (PB_R + 9.0f) && t > 0.25f)   /* above it,
                                          * on its outer part: the sweet spot */
                    pb_auto_until[fl & 1] = now + 160u;
            }
        }
    }
    keys[0] = HELD_L || (pb_auto && (int32_t)(now - pb_auto_until[0]) < 0);
    keys[1] = HELD_R || HELD_S1 || (pb_auto && (int32_t)(now - pb_auto_until[1]) < 0);   /* (the right flipper: A3,
                                          * or G5 under the right hand, so a flipper each side: Nick, 19 Nov 2026) */
    for (k = 0; k < 2; k++)              /* a flipper's key: a click, the flipper's sound */
        if (keys[k] && !pb.flip_at[k]) {
            pb.flip_at[k] = now | 1u;
            k_on(0, k ? scale_fit(h_bass() + 19) : h_bass() + 12, 52, PB_FLIP, 0.08f);   /* its click, on the chord:
                                          * the left its root, the right its fifth (Nick, 19 Nov 2026: musical) */
            juice_kick(k ? 1 : -1, 1);   /* (a snap you feel) */
            if (pb.lit[0] + pb.lit[1] + pb.lit[2] < 3) {   /* the flippers shift which lights are lit */
                uint8_t t = k ? pb.lit[2] : pb.lit[0];
                if (k)
                    pb.lit[2] = pb.lit[1], pb.lit[1] = pb.lit[0], pb.lit[0] = t;
                else
                    pb.lit[0] = pb.lit[1], pb.lit[1] = pb.lit[2], pb.lit[2] = t;
            }
        } else if (!keys[k])
            pb.flip_at[k] = 0;
    pull = HELD_S || HELD_D;            /* the plunger: hold, let go */
    if (!pull)
        pb.armed = 1;
    if (pull && !pb.plunging && pb.armed) {
        pb.plunging = 1;
        pb.plunge_at = now;
    } else if (!pull && pb.plunging) {
        float charge = (float)(now - pb.plunge_at > 900u ? 900u : now - pb.plunge_at) / 900.0f;
        pb.plunging = 0;
        for (k = 0; k < PB_NB; k++)
            if (pb.b[k].on && pb.b[k].lane && pb.b[k].y > (float)(PB_PLUNGE_Y - 20)) {
                pb.b[k].vy = -(0.5f + 0.55f * charge);
                if (!pb.save_used)       /* a new ball: saved, the first seconds */
                    pb.save_until = now + 8000u;
                k_on(0, 36, 110, PB_KICK, 0.2f);
                k_on(0, h_bass() + 24, 90, PB_BASS, 0.2f);
                juice_kick(0, -3 - (int32_t)(charge * 5.0f));
                juice_shake(2 + (int32_t)(charge * 3.0f), 150);
                juice_burst(216, SY(PB_PLUNGE_Y - 4), 8, 2, C_YEL, C_INK, 0);
                if (charge > 0.95f)
                    juice_text(196, SY(PB_PLUNGE_Y - 40), "FULL!", C_YEL);
            }
    }
    n = (int)(dt / 2u) + 1;              /* in steps of 2 ms or less: nothing passes through */
    for (i = 0; i < n; i++) {
        float h = (float)dt / (float)n;
        for (k = 0; k < 4; k++) {        /* the flippers move */
            float target = keys[k & 1] ? PB_FL[k][4] : PB_FL[k][3], a0 = pb.ang[k], sp = keys[k & 1] ? 0.04f : 0.018f;   /* (snappier:
                                          * was 0.028 up, 0.014 down) */
            float d = target - a0;
            if (d > sp * h)
                d = sp * h;
            else if (d < -sp * h)
                d = -sp * h;
            pb.ang[k] = a0 + d;
            pb.w[k] = d / h;
        }
        for (k = 0; k < PB_NB; k++)
            if (pb.b[k].on)
                pb_step(&pb.b[k], h);
        for (k = 0; k < PB_NB; k++)      /* balls against each other */
            for (j = k + 1; j < PB_NB; j++) {
                pb_ball_t *p = &pb.b[k], *q = &pb.b[j];
                float dx, dy, d, nx, ny, vn;
                if (!p->on || !q->on || p->held || q->held)
                    continue;
                dx = q->x - p->x, dy = q->y - p->y;
                d = dx * dx + dy * dy;
                if (d >= 4.0f * PB_R * PB_R || d < 0.0001f)
                    continue;
                d = fm_sqrtf(d);
                nx = dx / d, ny = dy / d;
                p->x -= nx * (2.0f * PB_R - d) / 2.0f, p->y -= ny * (2.0f * PB_R - d) / 2.0f;
                q->x += nx * (2.0f * PB_R - d) / 2.0f, q->y += ny * (2.0f * PB_R - d) / 2.0f;
                vn = (q->vx - p->vx) * nx + (q->vy - p->vy) * ny;
                if (vn < 0.0f) {
                    p->vx += vn * nx, p->vy += vn * ny;
                    q->vx -= vn * nx, q->vy -= vn * ny;
                    k_on(0, 76, 60, PB_HAT, 0.05f);   /* steel on steel */
                }
            }
    }
    {                                    /* the view follows the ball (the lowest, in multiball: the danger) */
        float fy = -1.0f, target;
        for (k = 0; k < PB_NB; k++)
            if (pb.b[k].on && (!pb.b[k].held || pb.b[k].fly) && pb.b[k].y > fy)
                fy = pb.b[k].y;
        if (fy < 0.0f)
            fy = (float)PB_OUT_Y;
        target = fm_clampf(fy - 140.0f, 0.0f, (float)(PB_H - 240));
        if (pb_cam_lock)                 /* (a test: the view held where it was put) */
            target = pb.cam;
        pb.cam += (target - pb.cam) * (dt >= 30u ? 0.3f : 0.16f);
        pb.camy = (int32_t)(pb.cam + 0.5f);
    }
    for (k = 0; k < PB_NB; k++)          /* the trails */
        if (pb.b[k].on) {
            for (i = 4; i > 0; i--)
                pb.b[k].tx[i] = pb.b[k].tx[i - 1], pb.b[k].ty[i] = pb.b[k].ty[i - 1];
            pb.b[k].tx[0] = (int16_t)pb.b[k].x, pb.b[k].ty[0] = (int16_t)pb.b[k].y;
        }
    if (now - pb.ember_at > 260u) {      /* a torch in view throws an ember */
        int t = (int)(kr() % PB_NTORCH);
        int32_t ty = SY(PB_TORCH[t][1]) - 8;
        pb.ember_at = now;
        if (ty > 30 && ty < 236)
            juice_embers(PB_TORCH[t][0], ty, 1, RGB(255, 140, 40));
    }
    if (pb.chest_open_at && now - pb.chest_open_at > 8000u)   /* the chest shuts again */
        pb.chest_open_at = 0;
    if (pb.jar[0] && pb.jar[1] && pb.jar[2] && pb_live() <= 1 && !pb.multiball &&
        now - pb.jar_at[0] > 1500u && now - pb.jar_at[1] > 1500u && now - pb.jar_at[2] > 1500u) {
        pb.jar[0] = pb.jar[1] = pb.jar[2] = 0;   /* the jars back, for the next multiball */
        juice_burst(PB_JAR[1][0], SY(PB_JAR[1][1]), 10, 2, RGB(196, 120, 70), C_INK, 0);
    }
}

/* ------------------------------------------------------------- drawing --- */
/* The arcade's own art, drawn here */
static void ar_boulder(int32_t x, int32_t y, int32_t r)   /* a boulder: its shadow, stone, lit from above */
{
    disc(x + 2, y + 3, r, RGB(10, 7, 18));
    disc(x, y, r, RGB(70, 58, 72));
    disc(x - 1, y - 1, r - 2, RGB(104, 90, 104));
    disc(x - r / 3, y - r / 3, r / 3 + 1, RGB(140, 126, 138));
    cv_line(x + r / 4, y - r / 2, x - r / 6, y + r / 3, RGB(60, 48, 62));
}
static void ar_arch(int32_t cx, int32_t cy, int32_t w, int open, int dim)   /* a stone doorway (bottom at cy + w/2) */
{
    int32_t h = w, x = cx - w / 2, y = cy - h / 2, k;
    uint16_t st = dim ? RGB(60, 50, 70) : RGB(130, 116, 130), dk = dim ? RGB(20, 16, 30) : RGB(60, 50, 64);
    disc(cx, y + w / 2, w / 2, st);      /* the arch's stones */
    cv_rect(x, y + w / 2, w, h - w / 2, st);
    for (k = 0; k < 5; k++)              /* their joints */
        cv_rect(x + (k * w) / 5, y + w / 2 + (k & 1) * 4, 1, 4, dk);
    disc(cx, y + w / 2 + 2, w / 2 - 4, open ? RGB(6, 4, 12) : RGB(110, 70, 40));   /* the way through: dark, or a door */
    cv_rect(x + 4, y + w / 2 + 2, w - 8, h - w / 2 - 2, open ? RGB(6, 4, 12) : RGB(110, 70, 40));
    if (!open)                           /* the door's planks and its ring */
        for (k = 1; k < 4; k++)
            cv_rect(x + 4 + k * (w - 8) / 4, y + 6, 1, h - 6, RGB(80, 50, 30));
}
static void ar_lamp(int32_t x, int32_t y, int on, int dim)   /* a lamp set in the floor */
{
    disc(x, y, 9, RGB(40, 30, 50));
    disc(x, y, 7, on ? (dim ? mix565(C_YEL, RGB(40, 30, 50), dim) : C_YEL) : RGB(70, 56, 40));
    disc(x - 2, y - 2, 2, on ? C_INK : RGB(100, 84, 60));
}
static void ar_swirl(int32_t x, int32_t y, int32_t r, uint32_t ms)   /* the portal: purple rings, turning */
{
    int32_t k;
    disc(x, y, r, RGB(40, 16, 70));
    for (k = 0; k < 18; k++) {
        float a = (float)ms * 0.006f + (float)k * 0.349f;
        int32_t rr = r * (18 - k) / 18;
        cv_rect(x + (int32_t)(fm_cosf(a) * (float)rr), y + (int32_t)(fm_sinf(a) * (float)rr), 2, 2,
                k & 1 ? C_PURP : RGB(220, 180, 255));
    }
    disc(x, y, 3, C_INK);
}
static void ar_jar(int32_t x, int32_t y)   /* a clay pot */
{
    disc(x + 1, y + 4, 9, RGB(10, 7, 18));
    disc(x, y + 2, 9, RGB(170, 96, 56));
    disc(x - 3, y - 1, 3, RGB(220, 150, 100));
    cv_rect(x - 5, y - 9, 11, 4, RGB(130, 70, 40));
    cv_rect(x - 6, y - 10, 13, 2, RGB(200, 130, 80));
    cv_rect(x - 8, y + 3, 17, 2, RGB(130, 70, 40));
}
static void ar_chest(int32_t x, int32_t y, int open)   /* a chest: wood, a gold band and lock */
{
    cv_rect(x - 11, y - 2, 23, 12, RGB(120, 72, 40));
    cv_rect(x - 11, y + 3, 23, 2, RGB(230, 180, 70));
    if (!open) {
        cv_rect(x - 11, y - 10, 23, 8, RGB(150, 92, 50));
        cv_rect(x - 11, y - 10, 23, 2, RGB(190, 120, 70));
        cv_rect(x - 2, y - 4, 5, 6, RGB(250, 210, 90));
    } else
        cv_rect(x - 11, y - 14, 23, 4, RGB(150, 92, 50));
}
static void pb_thick(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t r, uint16_t c)   /* a thick line (screen) */
{
    int32_t n = iabs(x2 - x1) > iabs(y2 - y1) ? iabs(x2 - x1) : iabs(y2 - y1), i;
    if ((y1 < -10 && y2 < -10) || (y1 > 250 && y2 > 250))
        return;
    for (i = 0; i <= n; i++) {
        int32_t x = x1 + (x2 - x1) * i / (n ? n : 1), y = y1 + (y2 - y1) * i / (n ? n : 1);
        cv_rect(x - r, y - r, 2 * r + 1, 2 * r + 1, c);
    }
}
/* the monsters, pixel art (20 x 22, drawn at twice the size; Nick, 19 Nov 2026: drawn, not circles): a outline,
 * b skin, c its shade, d hair or a dark feature, e an accent, f a light accent */
static const char *const PB_MON_ART[PB_NMON][22] = {
    {"......aaaaaaaa......", "....aaddddddddaa....", "...addddddddddddda..", "..adddddddddddddda..", "..addddbbddbbdddda..", "..addbbbbddbbbbdda..", ".adbbbbbbddbbbbbbda.", ".abbbbbbbbbbbbbbbba.", ".abbbbbbbbbbbbbbbba.", ".abbbbbbbbbbbbbbbba.", ".abbbbbbbbbbbbbbbba.", ".abcbbbbbbbbbbbbcba.", ".abcbbbbbccbbbbbcba.", "..abbbbbbbbbbbbbba..", "..acbbbbbbbbbbbbca..", "e..acbbbbbbbbbbca..e", "ee..acbbbbbbbbca..ee", "eee..accbbbbcca..eee", "eeee..aaccccaa..eeee", "eeeee..aaaaaa..eeeee", "eeeeee........eeeeee", ".eeeee........eeeee."},
    {"......aaaaaaaa......", "....aabbbbbbbbaa....", "...abbbbbbbbbbbba...", "..abccccccccccccba..", "..abbbbbbbbbbbbbba..", ".abbbbbbbbbbbbbbbba.", ".accccccbbbbcccccca.", ".abbbbbbbbbbbbbbbba.", ".abbbbbbbbbbbbbbbba.", ".abbbbbbbbbbbbbbbba.", ".acccccccccccbbbbba.", ".abbbbbbbbbbbbbbbba.", ".abbbbbccccccccccca.", "..abbbbbbbbbbbbbba..", "..abbbbbbbbbbbbbba..", "..acccccbbbbbbccca..", "...abbbbbbbbbbbba...", "....abbbccccbbba....", ".....aabbbbbbaabb...", ".......aaaaaa..abb..", "................abb.", ".................ab."},
    {"..aa............aa..", "..ada..........ada..", "..adea........aeda..", "..adeea.aaaa.aeeda..", "..addeaaddddaaedda..", "..adddddddddddddda..", ".adbddddddddddddbda.", "adbbbbbbbbbbbbbbbbda", "adbbbbbbbbbbbbbbbbda", "adbbbbbbbbbbbbbbbbda", "adbbbbbbbbbbbbbbbbda", ".dbbbbbbffffbbbbbbd.", "ddbbbbbffffffbbbbbdd", ".dbbbbffffffffbbbbd.", "ddcbbbffffffffbbbcdd", ".adcbbffffffffbbcda.", "..adcbbffffffbbcda..", "...adccbbbbbbccda...", "....addccccccdda....", ".....aaddddddaa.....", ".......aaaaaa.......", "...................."},
    {"....a...aa....a.....", "...ada.adda..ada.a..", "..adddaddddaadddada.", ".adddddddddddddddda.", ".addbdddddbdddbddda.", ".abebebebebebbbbbba.", ".abbbbbbbbbbbbbbbba.", ".abbbbbbbbbbbbbbbba.", ".abbbbbbbbbbbbbbbba.", ".abbbbbbbbbbbbbbbba.", ".abbbbbbbbbbbbbbbba.", ".abbbbbbbbbbbbbbbba.", ".acbbbbbbbbbbbbbbca.", "..acbbbbbbbbbbbbca..", "..acbbbbbbbbbbbbca..", "..abbbbbbbbbbbbbba..", "...acbbbbbbbbbbca...", "...accbbbbbbbbcca...", "....aaccbebbccaa....", "......aaaaaaaa......", "....................", "...................."},
    {"........aaaa........", "......aaeeeeaa......", ".....aeebbbbeea.....", "....abbbbbbbbbba....", "...abbcbbbbbbcbba...", "a..abbbbbbbbbbbba..a", "ea.abbbbbbbbbbbba.ae", "eeaabbbbbbbbbbbbaaee", "eefabbbbbbbbbbbbafee", "eeeabbbbbbbbbbbbaeee", "efeabbbbbbbbbbbbaefe", "eeeabbcbbbbbbcbbaeee", ".eeabbbbbbbbbbbbaee.", "..eabbbbbbbbbbbbae..", "...acbbbbbbbbbbca...", "...acbbbbbbbbbbca...", "....acbbbbbbbbca....", "....accbbbbbbcca....", ".....aaccccccaa.....", ".......aaaaaa.......", "....................", "...................."},
};
static const uint8_t PB_MON_PAL[PB_NMON][6][3] = {
    {{20, 12, 24}, {226, 220, 236}, {176, 168, 196}, {28, 20, 40}, {170, 24, 48}, {0, 0, 0}},
    {{40, 30, 24}, {222, 206, 160}, {170, 150, 104}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{26, 20, 20}, {120, 110, 104}, {84, 76, 72}, {62, 54, 52}, {176, 112, 112}, {184, 174, 164}},
    {{18, 24, 16}, {146, 170, 126}, {104, 130, 92}, {26, 22, 28}, {70, 40, 52}, {0, 0, 0}},
    {{10, 30, 28}, {62, 146, 124}, {38, 104, 90}, {0, 0, 0}, {36, 96, 84}, {120, 204, 176}},
};
/* the face, by anger: eyes and brows (12 x 5, at 4, 7), the mouth (8 x 4, at 6, 13; the werewolf's a row lower).
 * g an eye's white, h its iris, i a brow, j the mouth, k teeth, l a red glow, m the mouth's inside */
static const char *const PB_MON_EYES[3][5] = {
    {"............", ".iiii..iiii.", ".gghg..ghgg.", ".gggg..gggg.", "............"},
    {"............", ".ii......ii.", "..iii..iii..", ".gghg..ghgg.", "..ggg..ggg.."},
    {"ii........ii", ".iii....iii.", "..iii..iii..", ".lhhl..lhhl.", "..ll....ll.."},
};
static const char *const PB_MON_MOUTH[3][4] = {
    {"........", ".jjjjjj.", "........", "........"},
    {"........", "..jjjj..", ".j....j.", "........"},
    {".kjjjjk.", "jjmmmmjj", "jmmmmmmj", ".jjjjjj."},
};
/* a monster's face, 40 x 44 px, its middle at x, y (screen). anger 0 calm .. 2 furious: the brows come down, the
 * eyes go red and glow, the mouth opens, it flushes, trembles and steams. hit: a white flash */
static void pb_monster(int kind, int32_t x, int32_t y, int anger, int hit, uint32_t ms)
{
    static const uint8_t EYEC[PB_NMON][3] = {{200, 20, 40}, {255, 200, 40}, {255, 190, 40}, {30, 24, 30}, {20, 20, 20}};
    uint16_t pal[13];
    int32_t r, c, ox, oy, my = kind == 2 ? 14 : 13;
    for (c = 0; c < 6; c++) {
        pal[c] = RGB(PB_MON_PAL[kind][c][0], PB_MON_PAL[kind][c][1], PB_MON_PAL[kind][c][2]);
        if (anger >= 2 && (c == 1 || c == 2))   /* flushed with rage */
            pal[c] = mix565(pal[c], RGB(255, 40, 40), 70);
        if (hit && c)
            pal[c] = C_INK;
    }
    pal[6] = RGB(240, 236, 220);
    pal[7] = anger == 0 ? RGB(EYEC[kind][0], EYEC[kind][1], EYEC[kind][2]) : anger == 1 ? RGB(220, 30, 40) : RGB(255, 60, 60);
    pal[8] = RGB(16, 10, 20), pal[9] = RGB(30, 10, 20), pal[10] = RGB(250, 250, 240), pal[11] = RGB(255, 90, 90);
    pal[12] = RGB(150, 20, 40);
    if (anger >= 2) {
        x += (int32_t)((ms / 40u) % 3u) - 1;   /* trembling */
        for (c = 0; c < 3; c++) {        /* steam, in square puffs */
            uint32_t q = (ms / 25u + (uint32_t)c * 13u) % 18u;
            int32_t sz = q < 6u ? 4 : q < 12u ? 2 : 2, sy = y - 26 - (int32_t)q;
            cv_rect(x - 16 + c * 14 - sz / 2, sy, sz, sz, q < 12u ? RGB(190, 180, 200) : RGB(110, 100, 130));
        }
    }
    ox = x - 20, oy = y - 22;
    for (r = 0; r < 22; r++)             /* its shadow, then the face */
        for (c = 0; c < 20; c++)
            if (PB_MON_ART[kind][r][c] != '.')
                cv_rect(ox + 3 + c * 2, oy + 4 + r * 2, 2, 2, RGB(4, 2, 10));
    for (r = 0; r < 22; r++)
        for (c = 0; c < 20; c++) {
            char ch = PB_MON_ART[kind][r][c];
            if (ch != '.')
                cv_rect(ox + c * 2, oy + r * 2, 2, 2, pal[ch - 'a']);
        }
    if (hit)
        return;
    for (r = 0; r < 5; r++)              /* the eyes and brows */
        for (c = 0; c < 12; c++) {
            char ch = PB_MON_EYES[anger][r][c];
            if (ch == '.' || (kind == 1 && anger < 2 && c < 6 && (ch == 'g' || ch == 'h')))
                continue;                /* (the mummy's other eye, under its bandage) */
            cv_rect(ox + (4 + c) * 2, oy + (7 + r) * 2, 2, 2, pal[ch - 'a']);
        }
    for (r = 0; r < 4; r++)              /* the mouth */
        for (c = 0; c < 8; c++) {
            char ch = PB_MON_MOUTH[anger][r][c];
            if (ch != '.')
                cv_rect(ox + (6 + c) * 2, oy + (my + r) * 2, 2, 2, pal[ch - 'a']);
        }
    if ((kind == 0 || kind == 2) && anger < 2) {   /* fangs */
        cv_rect(ox + 7 * 2, oy + (my + 2) * 2, 2, 2, pal[10]);
        cv_rect(ox + 12 * 2, oy + (my + 2) * 2, 2, 2, pal[10]);
    }
}
static void pb_wall_draw(int32_t x1, int32_t y1, int32_t x2, int32_t y2)   /* the dungeon's gold-banded stone */
{
    int32_t n, i;
    pb_thick(x1, SY(y1), x2, SY(y2), 3, RGB(70, 30, 10));
    pb_thick(x1, SY(y1), x2, SY(y2), 2, RGB(200, 110, 30));
    pb_thick(x1, SY(y1), x2, SY(y2), 0, RGB(255, 220, 120));
    n = (iabs(x2 - x1) > iabs(y2 - y1) ? iabs(x2 - x1) : iabs(y2 - y1)) / 14;   /* gold studs along it */
    for (i = 1; i < n; i++)
        cv_rect(x1 + (x2 - x1) * i / n - 1, SY(y1 + (y2 - y1) * i / n) - 1, 3, 3, RGB(255, 240, 170));
}
static int pb_in(int32_t x, int32_t y)   /* a point inside the field (world) */
{
    if (x <= 16 || x >= 206 || y <= 28 || y >= PB_H - 6 || x + y < 76 || x - y > 156)
        return 0;
    if (x < 68 && y * 50 > 380 * 50 + (x - 16) * 34 + 150)
        return 0;
    if (x > 156 && y * 50 > 380 * 50 + (206 - x) * 34 + 150)
        return 0;
    return 1;
}
static int32_t pb_torch_light(int32_t x, int32_t y, uint32_t ms)   /* how lit a place is by the torches (0..140) */
{
    int t;
    int32_t best = 0;
    for (t = 0; t < PB_NTORCH; t++) {
        int32_t dx = x - PB_TORCH[t][0], dy = y - PB_TORCH[t][1], d2 = dx * dx + dy * dy;
        if (d2 < 120 * 120) {
            float f = 0.82f + 0.18f * fm_sinf((float)ms * 0.017f + (float)t * 1.9f) * fm_sinf((float)ms * 0.031f + (float)t);
            int32_t l = (int32_t)((float)(140 - (int32_t)fm_sqrtf((float)d2) * 140 / 120) * f);
            if (l > best)
                best = l;
        }
    }
    return best;
}
static void pb_steel_ball(int32_t cx, int32_t cy, int32_t warm)   /* a steel ball: the vault above in it, the floor
                                                                   * below, a torch's glint, a hard white highlight */
{
    int32_t dx, dy;
    for (dy = -6; dy <= 6; dy++)         /* its shadow */
        for (dx = -6; dx <= 6; dx++)
            if (dx * dx + dy * dy <= 30)
                cv_pset(cx + dx + 2, cy + dy + 3, RGB(4, 2, 10));
    for (dy = -5; dy <= 5; dy++)
        for (dx = -5; dx <= 5; dx++) {
            int32_t d2 = dx * dx + dy * dy, g;
            uint16_t c;
            if (d2 > 27)
                continue;
            if (dy < 0)                  /* the top: the cold light from above, bright to the rim */
                g = 250 - (-dy) * 5 - d2 * 2;
            else                         /* the bottom: the floor in it, brightening to the warm bounce at the rim */
                g = 105 + dy * 16 - d2 * 2;
            g = g < 30 ? 30 : g > 250 ? 250 : g;
            c = RGB(g, g, g + (dy < 0 ? 12 : 0) > 255 ? 255 : g + (dy < 0 ? 12 : 0));
            if (dy > 1 && dx > -2 && d2 > 10)   /* the torches, in the steel */
                c = mix565(c, RGB(255, 150, 60), 60 + warm);
            if (dy == 0)                 /* the horizon line, sharp: chrome */
                c = mix565(c, RGB(30, 30, 50), 120);
            if (d2 >= 22)                /* the rim, darker */
                c = mix565(c, RGB(20, 20, 30), 90);
            cv_pset(cx + dx, cy + dy, c);
        }
    cv_rect(cx - 3, cy - 4, 2, 2, RGB(255, 255, 255));   /* the highlight */
    cv_pset(cx - 1, cy - 4, RGB(255, 255, 255));
    cv_pset(cx + 2, cy + 3, RGB(255, 210, 150));
}
static void pb_torch_draw(int32_t x, int32_t wy, uint32_t ms, int t)
{
    int32_t y = SY(wy), fl = (int32_t)((fm_sinf((float)ms * 0.021f + (float)t * 2.3f) + 1.0f) * 1.5f);
    if (y < 0 || y > 250)
        return;
    cv_rect(x - 3, y, 7, 3, RGB(90, 55, 30));   /* the sconce */
    cv_rect(x - 1, y + 3, 3, 5, RGB(60, 35, 20));
    disc(x, y - 3, 3, RGB(200, 50, 20));         /* the flame, flickering */
    cv_rect(x - 2, y - 6 - fl, 5, 4 + fl, RGB(255, 120, 30));
    cv_rect(x - 1, y - 8 - fl, 3, 5, RGB(255, 210, 80));
    cv_pset(x, y - 9 - fl - (int32_t)((ms / 90u + (uint32_t)t) & 1u), RGB(255, 250, 200));
}
/* the dungeon around the table, drawn fast: each floor and wall tile shaded once (the floor by the torches, which
 * stand still on it; their flames flicker), then copied straight into the canvas a row at a time */
#define PB_NLV 8                         /* the floor's shades */
#define PB_TR 28                         /* the tile rows (16 px) from the table's top */
enum { PBT_LANE = PB_NLV, PBT_W4, PBT_W3, PBT_W6, PBT_W8, PBT_W9, PBT_N };
static uint16_t pb_tilec[PBT_N][256];    /* ready to copy: shaded, and in the canvas's byte order */
static uint8_t pb_kind[PB_TR][15];       /* each tile's: PBT_*, or a floor shade 0 .. PB_NLV - 1 */
static uint8_t pb_tiles_ok;
static void pb_tiles_make(void)
{
    /* its own stone, drawn (not the dungeon game's tiles): the floor's flagstones in the torches' shades, the walls'
     * bricks, the lane dark */
    int t, i, r, c;
    for (t = 0; t < PBT_N; t++) {
        int32_t dim = t < PB_NLV ? 236 - t * 140 / (PB_NLV - 1) : t == PBT_LANE ? 230 : 70;
        for (i = 0; i < 256; i++) {
            int32_t x = i % 16, y = i / 16;
            uint32_t h = (uint32_t)(i * 2654435761u + (uint32_t)t * 97u) >> 28;
            uint16_t px;
            if (t < PB_NLV || t == PBT_LANE)   /* a flagstone: its edge darker, a speck or two */
                px = x == 0 || y == 0 ? RGB(60, 52, 80) : x == 15 || y == 15 ? RGB(40, 34, 56)
                     : h == 0u ? RGB(120, 110, 140) : RGB(92, 84, 116);
            else {                       /* bricks: two courses, offset */
                int32_t row = y / 8, bx = (x + (row & 1) * 8) % 16;
                px = (y % 8 == 7 || bx == 0) ? RGB(40, 26, 30) : h < 2u ? RGB(150, 90, 70) : RGB(128, 74, 58);
            }
            pb_tilec[t][i] = swap16(mix565(px, RGB(6, 4, 14), dim));
        }
    }
    for (r = 0; r < PB_TR; r++)
        for (c = 0; c < 15; c++) {
            int32_t wx = c * 16, wy = 28 + r * 16, mx = wx + 8, my = wy + 8;
            uint8_t k;
            if (c == 0)
                k = PBT_W4;              /* the walls: their gold edge toward the field */
            else if (c == 14)
                k = PBT_W3;
            else if (pb_in(mx, my)) {
                int32_t lit = pb_torch_light(mx, my, 0);   /* (still: at its mean) */
                k = (uint8_t)clamp(lit * PB_NLV / 141, 0, PB_NLV - 1);
            } else if (mx > 206 && mx < 226 && wy >= 64)
                k = PBT_LANE;            /* the lane: in the dark */
            else
                k = (r + c) & 1 ? PBT_W8 : PBT_W9;
            pb_kind[r][c] = k;
        }
    pb_tiles_ok = 1;
}
static void pb_tile(const uint16_t *src, int32_t x, int32_t y)   /* 16 x 16 at x, y (screen): a row at a time */
{
    int32_t r, c, c0 = x < 0 ? -x : 0, c1 = x + 16 > (int32_t)cv_w ? (int32_t)cv_w - x : 16;
    for (r = 0; r < 16; r++) {
        int32_t yy = y + r + cv_oy;
        uint16_t *d;
        if (yy < cv_y0 || yy >= cv_y1)
            continue;
        d = cv_px + (uint32_t)yy * cv_w + (uint32_t)x;
        for (c = c0; c < c1; c++)
            d[c] = src[r * 16 + c];
    }
}
static void pb_scene(void)
{
    uint32_t ms = plat_ms();
    int i, k;
    int32_t cy = pb.camy;
    char s[24], n[12];
    {                                    /* the dungeon: stone and gold all round, the floor lit by the torches */
        int32_t r0 = (cy + 24 - 28) / 16 - 1, r, c, band0 = -cv_oy, band1 = 120 - cv_oy;
        if (!pb_tiles_ok)
            pb_tiles_make();
        for (r = r0; r <= r0 + 16; r++) {
            int32_t wy = 28 + r * 16, sy = wy - cy;
            if (sy > 240 || sy < 8 || sy + 16 <= band0 || sy >= band1)   /* (only this band's rows) */
                continue;
            for (c = 0; c < 15; c++) {
                uint8_t k = r < 0 ? PBT_W6 : r >= PB_TR ? PBT_W8 : pb_kind[r][c];
                pb_tile(pb_tilec[k], c * 16 + (k == PBT_W3 ? 2 : 0), sy);
            }
        }
    }
    for (i = 0; i < PB_NTORCH; i++)
        pb_torch_draw(PB_TORCH[i][0], PB_TORCH[i][1], ms, i);
    ar_arch(PB_MID, SY(212), 30, 1, 1);  /* the gate between the chambers */
    for (k = 0; k < 4; k++)              /* the water below: a ball in it is gone */
    {                                    /* (the water: blue, its waves moving) */
        int32_t wx = 63 + k * 24, wy = SY(PB_H - 2) - 12, j;
        cv_rect(wx, wy, 24, 24, RGB(30, 80, 170));
        for (j = 0; j < 3; j++)
            cv_rect(wx + (int32_t)((ms / 90u + (uint32_t)(j * 7 + k * 5)) % 18u), wy + 4 + j * 7, 6, 1, RGB(140, 200, 255));
    }
    for (i = 0; i < PB_NW; i++)
        pb_wall_draw(PB_WALL[i][0], PB_WALL[i][1], PB_WALL[i][2], PB_WALL[i][3]);
    {                                    /* the plunger: a spring, pulled */
        int32_t pull = pb.plunging ? (int32_t)((ms - pb.plunge_at > 900u ? 900u : ms - pb.plunge_at) * 10u / 900u) : 0;
        int32_t py = SY(PB_PLUNGE_Y) + pull;
        cv_rect(208, py, 16, 3, pb.plunging ? (pull >= 10 && (ms / 80u) & 1u ? C_PINK : C_YEL) : RGB(190, 190, 200));
        for (k = 0; k < 3; k++)
            cv_rect(211 + (k & 1) * 5, py + 3 + k * 2, 6, 1, C_GHOST);
    }
    for (i = 0; i < 2; i++) {            /* the slingshots: switch-wall stone, lit when they kick */
        uint32_t q = ms - pb.sling_at[i];
        int lit = pb.sling_at[i] && q < 120u;
        pb_thick(PB_SLING[i][0], SY(PB_SLING[i][1]), PB_SLING[i][2], SY(PB_SLING[i][3]), 3, RGB(40, 10, 30));
        pb_thick(PB_SLING[i][0], SY(PB_SLING[i][1]), PB_SLING[i][2], SY(PB_SLING[i][3]), 2, lit ? C_INK : C_PINK);
    }
    for (i = 0; i < 3; i++)              /* the posts: rocks */
        ar_boulder(PB_POST[i][0], SY(PB_POST[i][1]), 7);
    for (i = 0; i < 3; i++) {            /* the lights */
        int on = pb.lit[i] || (pb.light_at && ms - pb.light_at < 900u && ((ms / 100u) & 1u));
        int chase = !on && (ms / 180u) % 3u == (uint32_t)i;   /* the unlit ones chase: roll over me */
        ar_lamp(PB_LIGHT[i][0], SY(PB_LIGHT[i][1]), on || chase, chase ? 140 : 0);
    }
    {                                    /* the inserts: lamps under the glass, chasing toward the shots */
        static const int16_t ARROW[6][3] = {{80, 140, 0}, {178, 132, 1}, {138, 248, 1}, {111, 296, 2},
                                            {26, 330, 3}, {196, 344, 3}};   /* x, y, way: 0 up-left, 1 right,
                                                                             * 2 down, 3 up (the stairs, the orbit) */
        int a, d;
        for (a = 0; a < 6; a++) {
            int32_t x = ARROW[a][0], y = SY(ARROW[a][1]);
            if (y < 10 || y > 250)
                continue;
            for (d = 0; d < 3; d++) {    /* three chevrons, lit in turn toward the shot */
                int on = (ms / 140u + (uint32_t)(3 - d)) % 3u == 0u;
                uint16_t c = on ? (a == 3 ? C_PURP : a == 0 || a == 4 ? C_YEL : C_CYAN) : RGB(40, 26, 50);
                int32_t ox = ARROW[a][2] == 1 ? d * 6 : ARROW[a][2] == 0 ? -d * 4 : 0;
                int32_t oy = ARROW[a][2] == 2 ? d * 6 : ARROW[a][2] == 0 ? -d * 4 : ARROW[a][2] == 3 ? -d * 6 : 0;
                if (ARROW[a][2] == 3) {
                    cv_rect(x - 3, y + oy + 2, 2, 2, c), cv_rect(x - 1, y + oy, 2, 2, c), cv_rect(x + 1, y + oy + 2, 2, 2, c);
                } else if (ARROW[a][2] == 1) {
                    cv_rect(x + ox, y - 3, 2, 2, c), cv_rect(x + ox + 2, y - 1, 2, 2, c), cv_rect(x + ox, y + 1, 2, 2, c);
                } else if (ARROW[a][2] == 2) {
                    cv_rect(x - 3, y + oy, 2, 2, c), cv_rect(x - 1, y + oy + 2, 2, 2, c), cv_rect(x + 1, y + oy, 2, 2, c);
                } else
                    cv_rect(x + ox - 1, y + oy - 1, 3, 3, c);
            }
        }
        for (a = 0; a < 4; a++) {        /* the multiplier's lamps above the flippers: X2 .. X5 */
            static const char *const XL[4] = {"2X", "3X", "4X", "5X"};
            int lit = pb.mult >= a + 2;
            int32_t x = 84 + a * 18, y = SY(372);
            if (y < 10 || y > 250)
                continue;
            disc(x, y, 6, lit ? ((ms / 250u + (uint32_t)a) & 1u ? C_YEL : RGB(255, 150, 60)) : RGB(36, 24, 46));
            text_c(x, y - 4, &FONT_T, XL[a], lit ? C_BG : RGB(70, 54, 84));
        }
    }
    {                                    /* the shots' names on the floor: lit while a combo is on */
        static const int16_t LB[3][2] = {{26, 344}, {111, 352}, {196, 358}};
        static const char *const LN[3] = {"STAIRS", "PORTAL", "ORBIT"};
        static const uint16_t LC[3] = {RGB(255, 210, 63), RGB(176, 124, 255), RGB(95, 227, 238)};
        int hot = pb.shot_at && ms - pb.shot_at < PB_COMBO_MS;
        for (i = 0; i < 3; i++) {
            int32_t y = SY(LB[i][1]);
            if (y < 26 || y > 236)
                continue;
            text_c(LB[i][0], y, &FONT_XS, LN[i], hot && (ms / 160u) & 1u ? C_INK : mix565(RGB(30, 20, 40), LC[i], 170));
        }
    }
    for (i = 0; i < 4; i++) {            /* the drop targets: standing (lit), or down (a slot) */
        int32_t x = PB_DROP[i][0], y = SY(PB_DROP[i][1]);
        uint32_t q = ms - pb.drop_at[i];
        if (y < 0 || y > 250)
            continue;
        if (pb.drop[i]) {
            cv_rect(x - 1, y - 5, 3, 11, RGB(20, 12, 28));
        } else {
            int pop = pb.drop_at[i] && q < 160u;   /* (just back up) */
            cv_rect(x - 3, y - 6, 7, 13, RGB(10, 6, 18));
            cv_rect(x - 2, y - 5, 5, 11, pop ? C_INK : C_CYAN);
            cv_rect(x - 2, y - 5, 2, 11, mix565(C_CYAN, C_INK, 140));
            cv_rect(x - 1, y - 1, 3, 3, (ms / 200u + (uint32_t)i) % 4u == 0u ? C_INK : RGB(0, 90, 110));
        }
    }
    {                                    /* the teleport, spinning; the door it throws out of */
        uint32_t tq = ms - pb.tele_at;
        int32_t ts = pb.tele_at && tq < 300u ? 40 - (int32_t)tq / 40 : 32;
        ar_swirl(PB_TELE_X, SY(PB_TELE_Y), ts / 2, ms);
        ar_arch(PB_OUT_X, SY(PB_OUT_Y) - 2, 28, 1, 0);
    }
    for (i = 0; i < 3; i++)              /* the jars */
        if (!pb.jar[i])
            ar_jar(PB_JAR[i][0], SY(PB_JAR[i][1]));
    {                                    /* the chest: shut (it shakes when hit), or open with its treasure */
        uint32_t cq = ms - pb.chest_at;
        int32_t jx = pb.chest_at && cq < 160u ? ((cq / 40u) & 1u ? 2 : -2) : 0;
        if (pb.chest_open_at) {
            uint32_t oq = ms - pb.chest_open_at;
            int32_t lift = oq < 400u ? (int32_t)oq / 20 : 20 + (int32_t)(fm_sinf((float)ms * 0.006f) * 2.0f);
            ar_chest(PB_CHEST_X, SY(PB_CHEST_Y), 1);   /* open: a gem rising out of it */
            cu_gem(PB_CHEST_X - 8, SY(PB_CHEST_Y) - 14 - lift, CUBE_COL[1 + pb.treasure % 4u], ms, 0);
        } else {
            ar_chest(PB_CHEST_X + jx, SY(PB_CHEST_Y), 0);
            for (k = 0; k < pb.chest_hits; k++)
                disc(PB_CHEST_X - 8 + k * 8, SY(PB_CHEST_Y) + 20, 2, C_YEL);
        }
    }
    for (i = 0; i < PB_NBUMP; i++) {     /* the bumpers: the creatures, on gold rings */
        uint32_t q = ms - pb.bump_at[i];
        int hit = pb.bump_at[i] && q < 150u;
        int32_t x = PB_BUMPER[i][0], y = SY(PB_BUMPER[i][1]);
        int32_t bob = (int32_t)(fm_sinf((float)ms * 0.005f + (float)i * 2.0f) * 1.5f);
        if (y < -20 || y > 260)
            continue;
        if (i == PB_BOSS) {              /* THE BOSS: a monster, angrier as it's hit, its health over it */
            uint32_t hq = ms - pb.boss_hit_at, dq = (int32_t)(ms - pb.boss_down_at) < 0 ? 0u : ms - pb.boss_down_at;
            int hit = pb.boss_hit_at && (int32_t)hq >= 0 && hq < 140u, k2, anger;
            if (pb.boss_down_at && dq < PB_BOSS_BACK) {   /* down: its head pops off the table, spinning away */
                if (dq < 900u) {
                    float t = (float)dq;
                    int32_t yy = y - (int32_t)(t * 0.28f + t * t * 0.0004f), xx = x + (int32_t)(fm_sinf(t * 0.02f) * 10.0f);
                    pb_monster((pb.boss_lv + PB_NMON - 1) % PB_NMON, xx, yy, 2, (dq / 60u) & 1u, ms);
                }
                continue;
            }
            if (pb.boss_down_at && dq < PB_BOSS_BACK + 400u)   /* the next rising from the floor */
                y += (int32_t)(PB_BOSS_BACK + 400u - dq) / 10;
            anger = pb.boss_hp * 3 > pb.boss_max * 2 ? 0 : pb.boss_hp * 3 > pb.boss_max ? 1 : 2;
            pb_monster(pb_mon(), x, y + bob, anger, hit, ms);
            {                            /* its health: a bar over it, red when it's nearly done */
                int32_t w = 40, fill = (int32_t)pb.boss_hp * w / (pb.boss_max ? pb.boss_max : 1);
                cv_rect(x - w / 2 - 1, y - 32, w + 2, 6, RGB(10, 6, 18));
                cv_rect(x - w / 2, y - 31, fill, 4, anger >= 2 ? C_PINK : anger ? RGB(255, 150, 60) : C_YEL);
                for (k2 = 1; k2 < pb.boss_max && pb.boss_max <= 16; k2++)
                    cv_rect(x - w / 2 + k2 * w / pb.boss_max, y - 31, 1, 4, RGB(10, 6, 18));
            }
            continue;
        }
        {                                /* a pop bumper: lamps round it, a black rubber skirt, a gold ring, its cap */
            int l;
            uint16_t cc = PB_BUMPER_C[i];
            int32_t sq = hit ? 2 - (int32_t)q / 50 : 0;   /* the cap pushed down as it fires */
            for (l = 0; l < 10; l++) {   /* the lamps: chasing round, all lit as it fires */
                float an = (float)l * 0.628f;
                int on = hit || (int)((ms / 90u + (uint32_t)i * 3u) % 10u) == l;
                disc(x + (int32_t)(fm_cosf(an) * 21.0f), y + (int32_t)(fm_sinf(an) * 21.0f), 1, on ? C_YEL : RGB(60, 40, 30));
            }
            disc(x + 2, y + 3, 18, RGB(4, 2, 10));          /* its shadow */
            disc(x, y, 18, hit ? C_INK : RGB(18, 14, 22));  /* the rubber skirt */
            disc(x, y, 16, hit ? RGB(255, 240, 170) : RGB(210, 150, 50));   /* the gold ring */
            disc(x, y + sq, 13, hit ? C_INK : cc);          /* the cap */
            disc(x - 4, y - 5 + sq, 4, mix565(cc, C_INK, hit ? 255 : 150));   /* its shine */
            (void)bob;
            {                            /* a little monster on its cap: a bat, a ghost, a pumpkin, a skull, a spider */
                int32_t cy = y + sq, ic = i < PB_BOSS ? i : i - 1, k2;
                uint16_t ink = RGB(14, 10, 20), white = RGB(240, 236, 228);
                switch (ic) {
                case 0:                  /* a bat */
                    disc(x, cy, 3, ink);
                    pb_thick(x - 9, cy - 3, x - 2, cy, 1, ink);
                    pb_thick(x + 9, cy - 3, x + 2, cy, 1, ink);
                    pb_thick(x - 9, cy - 3, x - 6, cy + 2, 0, ink);
                    pb_thick(x + 9, cy - 3, x + 6, cy + 2, 0, ink);
                    cv_rect(x - 2, cy - 1, 1, 1, C_PINK), cv_rect(x + 1, cy - 1, 1, 1, C_PINK);
                    break;
                case 1:                  /* a ghost */
                    disc(x, cy - 2, 6, white);
                    cv_rect(x - 6, cy - 2, 13, 7, white);
                    cv_rect(x - 6, cy + 5, 3, 2, white), cv_rect(x - 1, cy + 5, 3, 2, white), cv_rect(x + 4, cy + 5, 3, 2, white);
                    disc(x - 2, cy - 2, 1, ink), disc(x + 2, cy - 2, 1, ink);
                    break;
                case 2:                  /* a pumpkin */
                    disc(x, cy + 1, 7, RGB(255, 140, 30));
                    cv_rect(x - 1, cy - 8, 3, 3, RGB(60, 140, 50));
                    cv_rect(x - 4, cy - 1, 2, 2, ink), cv_rect(x + 3, cy - 1, 2, 2, ink);
                    cv_rect(x - 3, cy + 3, 7, 2, ink);
                    break;
                case 3:                  /* a skull */
                    disc(x, cy - 1, 6, white);
                    cv_rect(x - 3, cy + 3, 7, 4, white);
                    disc(x - 2, cy - 1, 2, ink), disc(x + 3, cy - 1, 2, ink);
                    cv_rect(x - 1, cy + 4, 1, 3, ink), cv_rect(x + 1, cy + 4, 1, 3, ink);
                    break;
                default:                 /* a spider */
                    for (k2 = -1; k2 <= 1; k2 += 2) {
                        pb_thick(x, cy, x + k2 * 9, cy - 5, 0, ink);
                        pb_thick(x, cy, x + k2 * 10, cy, 0, ink);
                        pb_thick(x, cy, x + k2 * 9, cy + 5, 0, ink);
                    }
                    disc(x, cy, 4, ink);
                    cv_rect(x - 2, cy - 2, 1, 1, C_PINK), cv_rect(x + 1, cy - 2, 1, 1, C_PINK);
                    break;
                }
            }
        }
    }
    for (i = 0; i < 4; i++) {            /* the flippers: gold-capped steel */
        float a = pb.ang[i], L = PB_FL[i][2];
        int32_t px = (int32_t)PB_FL[i][0], py = SY(PB_FL[i][1]);
        int32_t tx = px + (int32_t)(L * fm_cosf(a)), ty = py + (int32_t)(L * fm_sinf(a));
        int up = pb.w[i] != 0.0f || (i & 1 ? HELD_R || HELD_S1 : HELD_L) ||
                 (pb_auto && (int32_t)(ms - pb_auto_until[i & 1]) < 0);
        pb_thick(px + 1, py + 2, tx + 1, ty + 2, 3, RGB(4, 2, 10));
        pb_thick(px, py, tx, ty, 3, RGB(30, 30, 50));
        pb_thick(px, py, tx, ty, 2, up ? C_INK : RGB(170, 180, 200));
        pb_thick(px, py - 1, tx, ty - 1, 0, RGB(240, 245, 255));
        disc(px, py, 3, RGB(255, 200, 90));
    }
    {                                    /* the hero on the ledge: watching, cheering, sad */
        uint32_t hq = ms - pb.hero_at;
        int mood = pb.hero_at && hq < 1400u ? pb.hero_mood : 0;
        int32_t hop = mood == 1 ? (int32_t)(fm_fabsf(fm_sinf((float)hq * 0.012f)) * 6.0f) : 0;
        int32_t ly = SY(426) - hop;      /* a lantern on the ledge: it jumps for joy, dims when a ball's lost */
        uint16_t lc = mood == 2 ? RGB(120, 90, 40) : C_YEL;
        cv_rect(30, ly - 10, 9, 14, RGB(60, 50, 70));
        cv_rect(31, ly - 9, 7, 12, mix565(RGB(30, 20, 10), lc, mood == 1 ? 255 : 170 + (int32_t)((ms / 120u) % 3u) * 20));
        cv_rect(32, ly - 13, 5, 3, RGB(90, 80, 100));
        disc(34, ly - 3, mood == 1 ? 9 : 6, mix565(C_BG, lc, 60));
        if (plat_ms() < pb.save_until && (ms / 300u) & 1u)
            text_c(178, SY(430), &FONT_T, "SAVE", C_CYAN);
    }
    for (k = 0; k < PB_NB; k++) {        /* the balls: steel, a trail behind when fast */
        pb_ball_t *b = &pb.b[k];
        int32_t bx = (int32_t)b->x, by = SY(b->y);
        if (!b->on)
            continue;
        if (b->held) {
            if (b->fly) {                /* the portal: a purple spirit with the ball in it, a wake of sparks */
                int32_t r2 = 9 + (int32_t)((ms / 60u) % 3u);
                disc(bx, by, r2 + 3, mix565(RGB(10, 8, 20), C_PURP, 90));
                disc(bx, by, r2, mix565(RGB(10, 8, 20), C_PURP, 200));
                pb_steel_ball(bx, by, 60);
                if ((ms / 30u) & 1u)
                    juice_burst(bx, by, 2, 1, C_PURP, C_INK, 0);
            }
            continue;
        }
        if (b->vx * b->vx + b->vy * b->vy > 0.3f)
            for (i = 4; i > 0; i--)
                disc(b->tx[i], SY(b->ty[i]), i > 2 ? 2 : 3, mix565(RGB(10, 8, 20), RGB(200, 200, 220), 140 - i * 28));
        pb_steel_ball(bx, by, pb_torch_light(bx, b->y, ms) / 2);
    }
    cv_rect(0, 0, 240, 24, C_BG);        /* the header over it all */
    if (!pb.over) {                      /* what's next, under it: a goal at a time, in turn */
        char g[32], v[6];
        int which = (int)((ms / 2600u) % 6u), done = 0, k2;
        uint16_t gc = C_GHOST;
        for (k2 = 0; k2 < 6 && !done; k2++, which = (which + 1) % 6) {
            done = 1;
            if (which == 0) {
                tcpy(g, "SHOOT  STAIRS  PORTAL  ORBIT");
                if (pb.shot_at && ms - pb.shot_at < PB_COMBO_MS) {
                    tcpy(g, "SHOOT AGAIN: X");
                    itos(pb.combo + 1, v);
                    tcat(g, v);
                    gc = C_YEL;
                }
            } else if (which == 1 && !pb.multiball) {
                itos(pb.jar[0] + pb.jar[1] + pb.jar[2], v);
                tcpy(g, "JARS ");
                tcat(g, v);
                tcat(g, "/3  MULTIBALL");
            } else if (which == 2 && !pb.chest_open_at) {
                itos(pb.chest_hits, v);
                tcpy(g, "CHEST ");
                tcat(g, v);
                tcat(g, "/3  JACKPOT");
            } else if (which == 3 && pb.mult < 5) {
                itos(pb.lit[0] + pb.lit[1] + pb.lit[2], v);
                tcpy(g, "TOP LIGHTS ");
                tcat(g, v);
                tcat(g, "/3  X");
                itos(pb.mult + 1, v);
                tcat(g, v);
            } else if (which == 5 && pb_boss_up()) {   /* the boss */
                tcpy(g, PB_MON_NAME[pb_mon()]);
                tcat(g, " ");
                itos(pb.boss_hp, v);
                tcat(g, v);
                tcat(g, "/");
                itos(pb.boss_max, v);
                tcat(g, v);
                tcat(g, "  MULTIBALL");
                gc = C_PINK;
            } else if (which == 4 && pb.mult < 5) {
                itos(pb.drop[0] + pb.drop[1] + pb.drop[2] + pb.drop[3], v);
                tcpy(g, "TARGETS ");
                tcat(g, v);
                tcat(g, "/4  X");
                itos(pb.mult + 1, v);
                tcat(g, v);
            } else
                done = 0;
        }
        cv_rect(0, 24, 240, 10, RGB(10, 7, 18));
        text_c(120, 24, &FONT_XS, g, gc);
    }
    itos((int32_t)pb.score, s);
    header("PINBALL", C_CYAN, "", C_PINK, s);
    for (i = 0; i < pb.balls && i < 5; i++)   /* the balls left, and the multiplier */
        pb_steel_ball(112 + i * 13, 11, 40);
    if (pb.mult > 1) {
        tcpy(n, "X");
        itos(pb.mult, n + 1);
        cv_text(152, 4, &FONT_XS, n, C_YEL);
    }
    if (pb.over) {
        char bs[24], bn[12];
        over_box("THE END", "TOP KEY: AGAIN   HOME: MENU");
        itos((int32_t)pb_best, bn);
        tcpy(bs, "BEST ");
        tcat(bs, bn);
        text_c(120, 150, &FONT_XS, bs, C_YEL);
    } else if (pb_live() == 1 && pb.b[0].on && pb.b[0].lane && !pb.plunging && pb.b[0].y > (float)(PB_PLUNGE_Y - 20))
        text_c(110, SY(400), &FONT_T, "HOLD TOP KEY, LET GO", C_GHOST);
}
#undef SY
