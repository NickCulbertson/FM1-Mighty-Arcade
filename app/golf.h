/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Nick Culbertson */
/* golf.h: MINI GOLF, nine holes at night, each its own trick: a dogleg, a windmill, sand, a hill, water, a bumper
 * alley, a sliding wall and a last hole with all of it. Included once from app.c; its state lives in the shared arena.
 *
 *  C and E aim (held: faster). Hold A: the power pulses up and down; let go to putt. The dotted line shows the way to
 *  the first wall and the bounce off it. Every bank off a wall plays the next note up the chord; the bumpers play it
 *  too. Sand slows the ball, the hill pushes it, water costs a stroke (back to where you putted from), a fast ball
 *  lips out of the cup. Sink it: the flag flies, the cup bursts, and the card says how you did against par. */

enum { GF_PUTT, GF_WOOD, GF_BUMP, GF_CUP, GF_WIN, GF_BASS, GF_HAT, GF_KICK, GF_BOOM, GF_PAD };
#define GF_R 3.0f                        /* the ball */
#define GF_CUP_R 5.0f
#define GF_VMAX 0.62f                    /* a full putt, px/ms */
#define GF_MAXSHOTS 8
#define GF_NPL 160                       /* the curved green's points, at most */
#define GF_NWL 24                        /* the bent walls' pieces, at most */
typedef struct {
    const char *name;
    uint8_t par;
    int16_t tx, ty, cx, cy;              /* the tee, the cup */
    const int16_t (*poly)[2];            /* the green's edge (a closed polygon) */
    uint8_t np;
    const int16_t (*w)[4];               /* walls inside */
    uint8_t nw;
    const int16_t (*z)[5];               /* zones: type (1 sand, 2 water, 3..6 a slope up, down, left, right), x, y, w, h */
    uint8_t nz;
    const int16_t (*b)[2];               /* bumpers */
    uint8_t nb;
    int16_t mx, my, ml;                  /* a windmill: its hub, its blades' reach (ml 0: none) */
    int16_t vx0, vx1, vy, vw, vh;        /* a sliding block: its left edge goes vx0..vx1 (vw 0: none) */
} gf_hole_t;

static const int16_t GP1[][2] = {{16, 34}, {110, 34}, {110, 140}, {224, 140}, {224, 232}, {16, 232}};   /* an L */
static const int16_t GP2[][2] = {{16, 34}, {224, 34}, {224, 118}, {104, 118}, {104, 232}, {16, 232}};
static const int16_t GP3[][2] = {{40, 34}, {200, 34}, {200, 232}, {40, 232}};
static const int16_t GPW[][2] = {{16, 34}, {224, 34}, {224, 232}, {16, 232}};
static const int16_t GP5[][2] = {{56, 34}, {184, 34}, {184, 232}, {56, 232}};
static const int16_t GP8[][2] = {{30, 34}, {210, 34}, {210, 232}, {30, 232}};
/* (no hole has a straight line to the cup, Nick 17 Nov 2026; on 18 Nov he asked for it to come from where the tee
 * and cup are, the green's shape, walls and water, not a bumper dropped on the line: the post stands in its own
 * corner now) */
static const int16_t GW4[][4] = {{130, 34, 130, 150}};
static const int16_t GW5[][4] = {{56, 140, 140, 140}};
static const int16_t GW3[][4] = {{40, 132, 94, 132}, {146, 132, 200, 132}};
static const int16_t GZ4[][5] = {{1, 36, 70, 74, 50}, {1, 150, 150, 60, 50}};
static const int16_t GZ5[][5] = {{4, 56, 66, 128, 60}};
static const int16_t GZ6[][5] = {{2, 16, 108, 150, 46}};
static const int16_t GB7[][2] = {{70, 92}, {120, 70}, {170, 92}, {95, 132}, {145, 132}, {120, 172}, {70, 172}, {170, 172}};
static const int16_t GW8[][4] = {{30, 133, 90, 133}, {150, 133, 210, 133}};
static const int16_t GZ8[][5] = {{6, 30, 66, 180, 26}};
static const int16_t GW9[][4] = {{16, 166, 170, 166}, {70, 100, 224, 100}};
static const int16_t GZ9[][5] = {{2, 112, 34, 30, 40}, {1, 176, 182, 44, 46}};
#define GN(a) ((uint8_t)(sizeof a / sizeof a[0]))
static const gf_hole_t GF_HOLES[9] = {   /* course 1: THE GARDEN (the first nine) */
    {"THE POST", 3, 190, 214, 60, 62, GP1, GN(GP1), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"DOGLEG", 3, 60, 208, 196, 72, GP2, GN(GP2), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                         /* (no bumpers on the first two: their L shapes block the straight line
                                          * already, Nick 19 Nov 2026) */
    {"THE WINDMILL", 3, 120, 212, 120, 62, GP3, GN(GP3), GW3, GN(GW3), 0, 0, 0, 0, 120, 132, 24, 0, 0, 0, 0, 0},
    /* (no bumper on the sand trap: its little post went, Nick 23 Nov 2026) */
    {"SAND TRAP", 3, 40, 212, 196, 58, GPW, GN(GPW), GW4, GN(GW4), GZ4, GN(GZ4), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"THE HILL", 3, 76, 212, 164, 58, GP5, GN(GP5), GW5, GN(GW5), GZ5, GN(GZ5), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"WATER HAZARD", 3, 56, 210, 56, 64, GPW, GN(GPW), 0, 0, GZ6, GN(GZ6), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"BUMPER ALLEY", 3, 40, 212, 200, 52, GPW, GN(GPW), 0, 0, 0, 0, GB7, GN(GB7), 0, 0, 0, 0, 0, 0, 0, 0},
    {"SLIDING WALL", 3, 120, 212, 120, 54, GP8, GN(GP8), GW8, GN(GW8), GZ8, GN(GZ8), 0, 0, 0, 0, 0, 90, 118, 128, 32, 10},
    {"THE GAUNTLET", 4, 40, 206, 196, 66, GPW, GN(GPW), GW9, GN(GW9), GZ9, GN(GZ9), 0, 0, 120, 133, 20, 0, 0, 0, 0, 0}};

/* course 2: THE CANYON, a desert at dusk (17 Nov 2026: Nick wanted more than nine holes) */
static const int16_t CP1[][2] = {{16, 34}, {130, 34}, {130, 120}, {224, 120}, {224, 232}, {100, 232}, {100, 150}, {16, 150}};
static const int16_t CZ1[][5] = {{1, 102, 122, 30, 28}};
static const int16_t CW2[][4] = {{90, 110, 150, 110}, {90, 60, 90, 110}, {90, 60, 150, 60}, {150, 60, 150, 70},
                                 {150, 100, 150, 110}};
                                         /* (the mesa: its way in on the right side, Nick 19 Nov 2026) */
static const int16_t CW7[][4] = {{86, 150, 130, 150}, {110, 100, 154, 100}};   /* the gulch's ledges, side to side */
static const int16_t CZ7b[][5] = {{5, 86, 104, 68, 42}};
static const int16_t CP6[][2] = {{16, 34}, {224, 34}, {224, 232}, {150, 232}, {150, 110}, {16, 110}};   /* an L */
static const int16_t CB3[][2] = {{80, 92}, {160, 92}, {120, 132}, {70, 172}, {170, 172}};
static const int16_t CZ4[][5] = {{2, 16, 120, 150, 30}, {2, 196, 120, 28, 30}};   /* the ford, far right */
static const int16_t CW5[][4] = {{16, 180, 170, 180}, {70, 130, 224, 130}, {16, 80, 170, 80}};
static const int16_t CZ6[][5] = {{1, 60, 48, 60, 40}, {1, 156, 100, 62, 40}, {1, 160, 170, 50, 36}};
static const int16_t CP7[][2] = {{86, 34}, {154, 34}, {154, 232}, {86, 232}};
static const int16_t CW8[][4] = {{40, 122, 96, 122}, {144, 122, 200, 122}};
static const int16_t CW9[][4] = {{120, 64, 224, 64}};
static const int16_t CZ9[][5] = {{2, 16, 102, 120, 30}, {1, 150, 160, 54, 40}};
static const int16_t CB9[][2] = {{170, 92}, {60, 176}};
static const gf_hole_t GF_CANYON[9] = {
    {"THE WASH", 3, 120, 214, 40, 52, CP1, GN(CP1), 0, 0, CZ1, GN(CZ1), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"THE MESA", 4, 120, 212, 120, 82, GPW, GN(GPW), CW2, GN(CW2), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"TUMBLEWEEDS", 3, 196, 212, 40, 60, GP8, GN(GP8), 0, 0, 0, 0, CB3, GN(CB3), 0, 0, 0, 0, 0, 0, 0, 0},
    {"DRY RIVER", 4, 40, 212, 60, 52, GPW, GN(GPW), 0, 0, CZ4, GN(CZ4), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"SWITCHBACK", 4, 40, 212, 60, 54, GPW, GN(GPW), CW5, GN(CW5), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"SANDSTORM", 3, 188, 214, 46, 60, CP6, GN(CP6), 0, 0, CZ6, GN(CZ6), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"THE GULCH", 3, 120, 212, 120, 52, CP7, GN(CP7), CW7, GN(CW7), CZ7b, GN(CZ7b), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"CANYON MILL", 3, 120, 212, 120, 58, GP3, GN(GP3), CW8, GN(CW8), 0, 0, 0, 0, 120, 122, 22, 60, 150, 172, 32, 10},
    {"THE BIG ONE", 4, 40, 212, 190, 46, GPW, GN(GPW), CW9, GN(CW9), CZ9, GN(CZ9), CB9, GN(CB9), 0, 0, 0, 0, 0, 0, 0, 0}};
/* course 3: THE CASTLE, its halls by torchlight */
static const int16_t KW1[][4] = {{16, 130, 150, 130}, {190, 130, 224, 130}};   /* the gate, off to the right */
static const int16_t KB1[][2] = {{120, 88}, {204, 182}};
static const int16_t KZ2[][5] = {{2, 16, 92, 140, 30}, {2, 84, 152, 140, 30}};
static const int16_t KW3[][4] = {{50, 80, 190, 80}, {190, 80, 190, 190}, {80, 190, 190, 190}, {50, 80, 50, 150}};
static const int16_t KP4[][2] = {{16, 34}, {224, 34}, {224, 110}, {96, 110}, {96, 232}, {16, 232}};   /* an L: the hall turns */
static const int16_t KB4[][2] = {{36, 150}, {76, 164}, {48, 74}, {110, 58}, {150, 86}, {186, 56}};
static const int16_t KZ5[][5] = {{2, 16, 112, 90, 28}, {2, 134, 112, 90, 28}};
static const int16_t KP6[][2] = {{16, 34}, {224, 34}, {224, 232}, {136, 232}, {136, 118}, {16, 118}};
static const int16_t KB6[][2] = {{180, 176}};
static const int16_t KZ7[][5] = {{4, 16, 78, 208, 24}, {4, 16, 128, 208, 24}};
static const int16_t KW7[][4] = {{16, 120, 160, 120}, {80, 70, 224, 70}};   /* the steps' landings */
static const int16_t KW8[][4] = {{40, 142, 96, 142}, {144, 142, 200, 142}};
static const int16_t KB8[][2] = {{80, 84}, {160, 84}};
static const int16_t KW9[][4] = {{16, 172, 150, 172}, {90, 112, 224, 112}};
static const int16_t KZ9[][5] = {{2, 16, 40, 60, 40}, {1, 170, 40, 54, 40}};
static const int16_t KB9[][2] = {{60, 142}};
static const gf_hole_t GF_CASTLE[9] = {
    {"GATEHOUSE", 3, 60, 212, 60, 56, GPW, GN(GPW), KW1, GN(KW1), 0, 0, KB1, GN(KB1), 0, 0, 0, 0, 0, 0, 0, 0},
    {"THE MOAT", 3, 200, 212, 40, 56, GPW, GN(GPW), 0, 0, KZ2, GN(KZ2), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"SPIRAL STAIR", 3, 120, 214, 120, 135, GPW, GN(GPW), KW3, GN(KW3), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"THE ARMORY", 3, 56, 214, 200, 66, KP4, GN(KP4), 0, 0, 0, 0, KB4, GN(KB4), 0, 0, 0, 0, 0, 0, 0, 0},
    {"DRAWBRIDGE", 3, 120, 212, 120, 56, GPW, GN(GPW), 0, 0, KZ5, GN(KZ5), 0, 0, 0, 0, 0, 80, 128, 96, 32, 8},
    {"THRONE ROOM", 3, 180, 208, 44, 72, KP6, GN(KP6), 0, 0, 0, 0, KB6, GN(KB6), 0, 0, 0, 0, 0, 0, 0, 0},
    {"DUNGEON STEPS", 4, 200, 214, 40, 48, GPW, GN(GPW), KW7, GN(KW7), KZ7, GN(KZ7), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"TWIN TOWERS", 3, 120, 212, 120, 52, GP3, GN(GP3), KW8, GN(KW8), 0, 0, KB8, GN(KB8), 120, 142, 22, 0, 0, 0, 0, 0},
    {"KING'S HOLE", 4, 200, 212, 120, 50, GPW, GN(GPW), KW9, GN(KW9), KZ9, GN(KZ9), KB9, GN(KB9), 0, 0, 0, 0, 0, 0, 0, 0}};
#define GF_NCOURSE 3
static const gf_hole_t *const GF_COURSE[GF_NCOURSE] = {GF_HOLES, GF_CANYON, GF_CASTLE};
static const char *const GF_CNAME[GF_NCOURSE] = {"THE GARDEN", "THE CANYON", "THE CASTLE"};
static const uint16_t GF_TURF[GF_NCOURSE][2] = {{RGB(46, 150, 70), RGB(40, 136, 62)}, {RGB(176, 140, 80), RGB(160, 126, 72)},
                                               {RGB(92, 74, 150), RGB(82, 64, 136)}};   /* each course its green */
static const uint16_t GF_SKY[GF_NCOURSE][2] = {{RGB(10, 30, 24), RGB(12, 34, 28)}, {RGB(40, 20, 26), RGB(46, 24, 30)},
                                              {RGB(16, 12, 30), RGB(20, 15, 36)}};    /* and what's round it */
static uint8_t gf_course;                /* the course being played (it lasts) */
static uint32_t gf_cbest[GF_NCOURSE];    /* each course's best round */
typedef struct {
    uint8_t hole, strokes, phase;        /* phase: 0 aiming, 1 rolling, 2 sinking, 3 the hole's card, 4 in the water,
                                          * 5 the round's card, 6 choosing the course */
    uint8_t pick;                        /* the course the chooser is on */
    uint8_t card[9], card2[9];           /* card2: player 2's (two players: each hole, P1 then P2) */
    uint8_t who;                         /* whose turn: 0 P1, 1 P2 */
    float x, y, vx, vy, aim, lx, ly;     /* the ball, the aim, where it was putted from */
    uint8_t charging, armed, bounces, lipped;
    uint32_t charge_at, phase_at, last, hit_at, wall_at, still_at, roll_at, tick_at, intro_at, flag_at;
    float charge;
    int16_t tx[6], ty[6];
    uint32_t sand_in;
    /* the hole as it's played: its green's edge rounded at the corners and bowed out along the sides, its walls bent
     * into arcs (gf_shape: no straight-ruled course, Nick 17 Nov 2026) */
    int16_t pl[GF_NPL][2], wl[GF_NWL][4];
    uint8_t npl, nwl;
} gf_t;
static uint32_t gf_best;                 /* the best round: strokes */
#define gf (*(gf_t *)g_arena)
_Static_assert(sizeof(gf_t) <= sizeof g_arena, "gf: the arena is too small");
static const gf_hole_t *gf_h(void) { return &GF_COURSE[gf_course][gf.hole]; }
static int gf_two(void) { return players == 2; }   /* two players, taking each hole in turn (Nick, 19 Nov 2026) */
static uint8_t *gf_card(void) { return gf.who ? gf.card2 : gf.card; }   /* the card of whoever's playing */

static void gf_sounds(void)
{
    k_slot_set(GF_PUTT, &KSND[S_MARIMBA]);
    k_slot_set(GF_WOOD, &KSND[S_LOGDRUM]);
    k_slot_set(GF_BUMP, &KSND[S_VIBES]);
    k_slot_set(GF_CUP, &KSND[S_CELESTA]);
    k_slot_set(GF_WIN, &KSND[S_TUBE]);
    k_slot_set(GF_BASS, &KSND[S_FMBASS]);
    k_slot_set(GF_HAT, &KSND[S_HAT]);
    k_slot_set(GF_KICK, &KSND[S_KICK]);
    k_slot_set(GF_BOOM, &KSND[S_SONAR]);
    k_slot_set(GF_PAD, &KSND[S_GLASSPAD]);
    k_slot_rel(GF_PUTT, 0.5f);
    k_slot_rel(GF_WOOD, 0.3f);
    k_slot_rel(GF_BUMP, 0.6f);
    k_slot_rel(GF_CUP, 1.0f);
    k_slot_rel(GF_WIN, 1.2f);
    k_slot_rel(GF_BASS, 0.2f);
    k_slot_rel(GF_PAD, 1.6f);
    k_main_slot = GF_PUTT;
    k_main.rev_mix = 0.2f, k_main.rev_secs = 1.8f, k_main.shimmer = 0.04f, k_main.dly_mix = 0.1f;
}
static void gf_groove(uint32_t t, uint32_t step)   /* a soft night groove; the last hole, a kick under it */
{
    uint32_t p = step % 16u;
    h_deg_at(step / 16u);
    if (gf.phase == 5)
        return;
    if (p == 0u || p == 10u)
        k_on(t, h_bass(), 70, GF_BASS, 0.2f);
    if (p == 4u || p == 12u)
        k_on(t, 92, 34, GF_HAT, 0.04f);
    if (gf.hole == 8 && (p == 0u || p == 8u))
        k_on(t, 36, 80, GF_KICK, 0.2f);
    if (p == 0u && step % 64u == 0u)
        h_pad(t, 36, GF_PAD, 3.0f, 0);
}
static float gf_atan2(float y, float x)   /* (no atan2 here: a close one, to aim the first putt at the cup) */
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), a, r;
    if (ax < 1e-6f && ay < 1e-6f)
        return 0.0f;
    r = ax > ay ? ay / ax : ax / ay;
    a = r * (0.7853982f + 0.273f * (1.0f - r));
    if (ay > ax)
        a = 1.5707963f - a;
    if (x < 0.0f)
        a = 3.1415927f - a;
    return y < 0.0f ? -a : a;
}
/* the curved course from the hole's straight one. Each corner: a round (a curve through it, up to 20 px back along
 * each side). Each side: bowed out in a gentle arc, unless a wall or a zone meets it there (so nothing leaks round
 * them), clamped to the screen. Each wall inside: an arc between its ends (they stay where they were) */
static int gf_touches(const gf_hole_t *h, float ax, float ay, float bx, float by)   /* a wall's end or a zone on it */
{
    float ex = bx - ax, ey = by - ay, l = fm_sqrtf(ex * ex + ey * ey), px[2], py[2];
    int i, k;
    for (i = 0; i < h->nw + h->nz; i++) {
        if (i < h->nw)
            px[0] = h->w[i][0], py[0] = h->w[i][1], px[1] = h->w[i][2], py[1] = h->w[i][3];
        else {
            const int16_t *z = h->z[i - h->nw];   /* a zone: its corners nearest */
            float cx = fm_clampf(0.5f * (ax + bx), (float)z[1], (float)(z[1] + z[3]));
            float cy = fm_clampf(0.5f * (ay + by), (float)z[2], (float)(z[2] + z[4]));
            px[0] = cx, py[0] = cy, px[1] = cx, py[1] = cy;
            {                            /* (and along the whole side: the zone's nearest point to each end) */
                float t;
                for (t = 0.0f; t <= 1.0f; t += 0.05f) {
                    float qx = ax + ex * t, qy = ay + ey * t;
                    float zx = fm_clampf(qx, (float)z[1], (float)(z[1] + z[3])), zy = fm_clampf(qy, (float)z[2], (float)(z[2] + z[4]));
                    if ((zx - qx) * (zx - qx) + (zy - qy) * (zy - qy) < 16.0f)
                        return 1;
                }
            }
        }
        for (k = 0; k < 2; k++) {
            float d = fm_fabsf((px[k] - ax) * ey - (py[k] - ay) * ex) / l, t = ((px[k] - ax) * ex + (py[k] - ay) * ey) / (l * l);
            if (d < 4.0f && t > -0.05f && t < 1.05f)
                return 1;
        }
    }
    return 0;
}
static void gf_pt(float x, float y)
{
    if (gf.npl < GF_NPL) {
        gf.pl[gf.npl][0] = (int16_t)fm_clampf(x + 0.5f, 4.0f, 236.0f);
        gf.pl[gf.npl][1] = (int16_t)fm_clampf(y + 0.5f, 26.0f, 238.0f);
        if (!gf.npl || gf.pl[gf.npl][0] != gf.pl[gf.npl - 1][0] || gf.pl[gf.npl][1] != gf.pl[gf.npl - 1][1])
            gf.npl++;
    }
}
static void gf_shape(void)
{
    const gf_hole_t *h = gf_h();
    int i, n = h->np, k;
    float area = 0.0f, out;
    for (i = 0; i < n; i++)              /* its turning: which way is out */
        area += (float)h->poly[i][0] * (float)h->poly[(i + 1) % n][1] - (float)h->poly[(i + 1) % n][0] * (float)h->poly[i][1];
    out = area > 0.0f ? -1.0f : 1.0f;
    gf.npl = 0;
    for (i = 0; i < n; i++) {
        float vx = h->poly[i][0], vy = h->poly[i][1];
        float px = h->poly[(i + n - 1) % n][0], py = h->poly[(i + n - 1) % n][1];
        float nx = h->poly[(i + 1) % n][0], ny = h->poly[(i + 1) % n][1];
        float l1 = fm_sqrtf((px - vx) * (px - vx) + (py - vy) * (py - vy)), l2 = fm_sqrtf((nx - vx) * (nx - vx) + (ny - vy) * (ny - vy));
        float rr = fm_clampf(0.35f * (l1 < l2 ? l1 : l2), 0.0f, 20.0f);
        float ax = vx + (px - vx) / l1 * rr, ay = vy + (py - vy) / l1 * rr, bx = vx + (nx - vx) / l2 * rr, by = vy + (ny - vy) / l2 * rr;
        for (k = 0; k <= 6; k++) {       /* the corner: a curve from a, by the corner, to b */
            float t = (float)k / 6.0f, u = 1.0f - t;
            gf_pt(u * u * ax + 2.0f * u * t * vx + t * t * bx, u * u * ay + 2.0f * u * t * vy + t * t * by);
        }
        {                                /* the side on to the next corner: bowed out */
            float n2x = h->poly[(i + 2) % n][0], n2y = h->poly[(i + 2) % n][1];
            float l3 = fm_sqrtf((n2x - nx) * (n2x - nx) + (n2y - ny) * (n2y - ny));
            float r2 = fm_clampf(0.35f * (l2 < l3 ? l2 : l3), 0.0f, 20.0f);
            float sx = bx, sy = by, ex = nx + (vx - nx) / l2 * r2, ey = ny + (vy - ny) / l2 * r2;
            float dx = ex - sx, dy = ey - sy, sl = fm_sqrtf(dx * dx + dy * dy);
            float amp = gf_touches(h, vx, vy, nx, ny) ? 0.0f : fm_clampf(sl / 16.0f, 0.0f, 9.0f);
            int m = (int)(sl / 14.0f), j;
            if (sl < 1.0f || amp <= 0.0f)
                continue;                /* (straight: the next corner's start joins it) */
            for (j = 1; j < m; j++) {
                float t = (float)j / (float)m, b = amp * fm_sinf(3.1415927f * t);
                gf_pt(sx + dx * t + out * (dy / sl) * b, sy + dy * t - out * (dx / sl) * b);
            }
        }
    }
    gf.nwl = 0;
    for (i = 0; i < h->nw; i++) {        /* the walls: arcs, alternately bowed */
        float ax = h->w[i][0], ay = h->w[i][1], bx = h->w[i][2], by = h->w[i][3];
        float dx = bx - ax, dy = by - ay, l = fm_sqrtf(dx * dx + dy * dy), sag = fm_clampf(l / 7.0f, 0.0f, 10.0f) * (i & 1 ? -1.0f : 1.0f);
        float lx = ax, ly = ay;
        for (k = 1; k <= 6 && gf.nwl < GF_NWL; k++) {
            float t = (float)k / 6.0f, b = sag * fm_sinf(3.1415927f * t);
            float x = ax + dx * t - (dy / l) * b, y = ay + dy * t + (dx / l) * b;
            gf.wl[gf.nwl][0] = (int16_t)lx, gf.wl[gf.nwl][1] = (int16_t)ly;
            gf.wl[gf.nwl][2] = (int16_t)x, gf.wl[gf.nwl][3] = (int16_t)y;
            gf.nwl++;
            lx = x, ly = y;
        }
    }
}
static void gf_start_hole(void)
{
    const gf_hole_t *h = gf_h();
    int i;
    juice_reset();                       /* a fresh hole: nothing left over */
    gf.strokes = 0;
    gf.phase = 0;
    gf.x = gf.lx = h->tx;
    gf.y = gf.ly = h->ty;
    gf.vx = gf.vy = 0.0f;
    gf.aim = gf_atan2((float)(h->cy - h->ty), (float)(h->cx - h->tx));
    gf.charging = 0;
    gf.armed = 0;
    gf_shape();
    gf.intro_at = plat_ms() | 1u;
    gf.flag_at = 0;
    for (i = 0; i < 6; i++)
        gf.tx[i] = (int16_t)gf.x, gf.ty[i] = (int16_t)gf.y;
    {
        char hs[12] = "HOLE ";
        itos(gf.hole + 1, hs + 5);
        juice_pop(120, 96, hs, C_CYAN);
        juice_text(120, 172, h->name, C_YEL);   /* (well under HOLE n: it rises as it fades, and at 130 it rose
                                          * into it: Nick, 19 Nov 2026) */
        if (gf_two())
            juice_text(120, 196, gf.who ? "PLAYER 2" : "PLAYER 1", gf.who ? C_PINK : C_CYAN);
    }
    k_on(0, h_note(1) + 12, 80, GF_CUP, 0.6f);
    k_on(k_at(0.12f), h_note(3) + 12, 80, GF_CUP, 0.6f);
}
static void gf_begin(void)               /* the chosen course, from its first hole */
{
    int i;
    gf.hole = 0;
    gf.who = 0;
    for (i = 0; i < 9; i++)
        gf.card[i] = gf.card2[i] = 0;
    gf.last = 0;
    gf_start_hole();
}
static void gf_new(void)                 /* first, the course: a quick list (up, down, A) */
{
    juice_reset();
    gf.hole = 0;
    gf.phase = 6;
    gf.pick = gf_course;
    gf.phase_at = plat_ms();
    gf.x = gf_h()->tx, gf.y = gf_h()->ty;
    gf_shape();                          /* (the chooser shows the course's first hole behind it) */
}
/* the mill's blades and the sliding block, where they are now (they go on their own clocks) */
static float gf_mill_ang(uint32_t ms) { return (float)ms * 0.0021f; }
static float gf_block_x(uint32_t ms, float *v)
{
    const gf_hole_t *h = gf_h();
    float ph = (float)(ms % 1800u) / 1800.0f * 6.2831853f, a = (float)(h->vx1 - h->vx0);
    if (v)
        *v = a * 0.5f * fm_sinf(ph) * 6.2831853f / 1800.0f;
    return (float)h->vx0 + a * (0.5f - 0.5f * fm_cosf(ph));
}
static float gf_hit_seg(float ax, float ay, float bx, float by, float rad, float e, float vsx, float vsy)
{
    float dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy, t, cx, cy, nx, ny, d, R = GF_R + rad, rvx, rvy, vn;
    t = l2 > 0.0f ? ((gf.x - ax) * dx + (gf.y - ay) * dy) / l2 : 0.0f;
    t = fm_clampf(t, 0.0f, 1.0f);
    cx = ax + dx * t;
    cy = ay + dy * t;
    nx = gf.x - cx;
    ny = gf.y - cy;
    d = nx * nx + ny * ny;
    if (d >= R * R || d < 0.0001f)
        return 0.0f;
    d = fm_sqrtf(d);
    nx /= d;
    ny /= d;
    gf.x = cx + nx * R;
    gf.y = cy + ny * R;
    rvx = gf.vx - vsx;
    rvy = gf.vy - vsy;
    vn = rvx * nx + rvy * ny;
    if (vn >= 0.0f)
        return 0.0f;
    gf.vx = rvx - (1.0f + e) * vn * nx + vsx;
    gf.vy = rvy - (1.0f + e) * vn * ny + vsy;
    return -vn;
}
static void gf_bank(float hard)          /* off a wall: the next note up the chord, a knock */
{
    uint32_t now = plat_ms();
    if (hard < 0.05f || now - gf.wall_at < 60u)
        return;
    gf.wall_at = now;
    gf.hit_at = now | 1u;
    k_on(0, h_note(1 + gf.bounces % 5) + 12 * (gf.bounces / 5 % 2), (uint32_t)(60 + (int)(hard * 100.0f)), GF_WOOD, 0.3f);
    gf.bounces++;
    juice_burst((int32_t)gf.x, (int32_t)gf.y, 3 + (int32_t)(hard * 10.0f), 2, RGB(230, 190, 120), C_INK, 0);
    juice_shake(1 + (int32_t)(hard * 8.0f), 120);
    juice_heat(2);
    if (gf.bounces >= 3 && gf.bounces % 2 == 1) {
        char bs[12];
        itos(gf.bounces, bs);
        tcat(bs, " BANKS");
        juice_text((int32_t)gf.x, (int32_t)gf.y - 10, bs, C_YEL);
    }
}
/* sand and water lie in soft blobs, not boxes: a squircle in the zone's box, its edge wandering a little (the sand's
 * more than the water's); the ball's test and the drawing share it. A row y of zone i: the span x0..x1 (0: none) */
static int gf_zone_span(const int16_t *z, int i, float y, float *x0, float *x1)
{
    float hw = 0.5f * (float)z[3], hh = 0.5f * (float)z[4], v = (y - ((float)z[2] + hh)) / hh, f;
    if (v <= -1.0f || v >= 1.0f)
        return 0;
    if (z[0] > 2) {                      /* (a slope: its box) */
        *x0 = (float)z[1], *x1 = (float)(z[1] + z[3]);
        return 1;
    }
    v *= v;
    f = fm_sqrtf(fm_sqrtf(1.0f - v * v));
    f *= 1.0f - (z[0] == 1 ? 0.07f : 0.03f) * (1.0f + fm_sinf(y * 0.13f + (float)i * 2.1f));
    *x0 = (float)z[1] + hw * (1.0f - f), *x1 = (float)z[1] + hw * (1.0f + f);
    return 1;
}
static int gf_zone_at(float x, float y)  /* the zone the ball is in (0 none) */
{
    const gf_hole_t *h = gf_h();
    int i;
    float x0, x1;
    for (i = 0; i < h->nz; i++)
        if (gf_zone_span(h->z[i], i, y, &x0, &x1) && x >= x0 && x < x1)
            return h->z[i][0];
    return 0;
}
static void gf_sink(void)
{
    const gf_hole_t *h = gf_h();
    int d = (int)gf.strokes - (int)h->par, k;
    uint32_t now = plat_ms();
    static const char *const WORD[7] = {"ALBATROSS!", "EAGLE!", "BIRDIE!", "PAR", "BOGEY", "DOUBLE BOGEY", "OUCH"};
    gf.phase = 2;
    gf.phase_at = now;
    gf.flag_at = now | 1u;
    gf_card()[gf.hole] = gf.strokes;
    juice_freeze(120);
    juice_flash(RGB(255, 255, 255), 170, 300);
    juice_shake(7, 400);
    juice_kick(0, 6);
    juice_ring(h->cx, h->cy, 70, C_YEL, 450);
    juice_ring(h->cx, h->cy, 40, RGB(255, 255, 255), 300);
    juice_confetti(h->cx, h->cy, 40);
    for (k = 0; k < 6; k++)              /* the cup's arpeggio, up the chord */
        k_on(k_at(0.06f * (float)k), h_note(1 + k) + 12, 100, GF_CUP, 0.8f);
    k_on(0, 36, 110, GF_KICK, 0.3f);
    if (gf.strokes == 1) {               /* a hole in one: everything */
        juice_pop(120, 100, "HOLE IN ONE!", C_YEL);
        juice_celebrate(h->cx, h->cy);
        juice_glitch(200);
        for (k = 0; k < 8; k++)
            k_on(k_at(0.4f + 0.09f * (float)k), h_note(1 + k) + 24, 110, GF_WIN, 1.2f);
    } else {
        juice_pop(120, 100, WORD[d < -3 ? 0 : d > 3 ? 6 : d + 3], d < 0 ? C_YEL : d == 0 ? C_CYAN : C_PINK);
        if (d < 0) {
            juice_fountain(h->cx, 236, 900);
            juice_sting(2);
        } else
            juice_sting(1);
    }
}
static void gf_step(float h)             /* one small step of the rolling ball */
{
    const gf_hole_t *H = gf_h();
    uint32_t now = plat_ms();
    float sp, dec, ax = 0.0f, ay = 0.0f, hard;
    int z = gf_zone_at(gf.x, gf.y), i;
    if (z == 2) {                        /* in the water: a stroke, and back */
        gf.phase = 4;
        gf.phase_at = now;
        gf.strokes++;
        juice_burst((int32_t)gf.x, (int32_t)gf.y, 24, 3, RGB(90, 170, 255), RGB(220, 240, 255), 1);
        juice_ring((int32_t)gf.x, (int32_t)gf.y, 34, RGB(90, 170, 255), 400);
        juice_flash(RGB(60, 120, 255), 90, 300);
        juice_shake(5, 300);
        juice_pop(120, 120, "SPLASH! +1", RGB(90, 170, 255));
        k_on(0, degree_note(-7), 100, GF_BOOM, 0.6f);
        J.heat /= 2;
        return;
    }
    dec = z == 1 ? 0.0011f : 0.00016f;   /* sand drags; the grass a little */
    if (z == 1 && !gf.sand_in) {
        gf.sand_in = 1;
        juice_burst((int32_t)gf.x, (int32_t)gf.y, 10, 2, RGB(230, 200, 130), RGB(180, 150, 90), 1);
        k_on(0, 40, 80, GF_HAT, 0.2f);
    } else if (z != 1)
        gf.sand_in = 0;
    if (z >= 3) {                        /* a slope pushes */
        ax = z == 5 ? -0.00028f : z == 6 ? 0.00028f : 0.0f;
        ay = z == 3 ? -0.00028f : z == 4 ? 0.00028f : 0.0f;
    }
    gf.vx += ax * h;
    gf.vy += ay * h;
    sp = fm_sqrtf(gf.vx * gf.vx + gf.vy * gf.vy);
    if (sp > 0.0f) {
        float ns = sp - dec * h;
        if (ns < 0.0f)
            ns = 0.0f;
        gf.vx *= ns / sp;
        gf.vy *= ns / sp;
    }
    {                                    /* near the cup and slow: it leans in */
        float cx = (float)H->cx - gf.x, cy = (float)H->cy - gf.y, d = fm_sqrtf(cx * cx + cy * cy);
        if (d < 10.0f && d > 0.5f && sp < 0.14f) {
            gf.vx += cx / d * 0.0005f * h;
            gf.vy += cy / d * 0.0005f * h;
        }
        if (d < 26.0f && sp > 0.02f && sp < 0.4f && (gf.vx * cx + gf.vy * cy) > 0.0f && now - gf.tick_at > 70u) {
            gf.tick_at = now;            /* rolling at the cup: a drumroll */
            k_on(0, 90, 50 + (int)((26.0f - d) * 2.0f), GF_HAT, 0.03f);
        }
        if (d < GF_CUP_R - 0.5f) {
            if (sp < 0.3f) {
                gf_sink();
                return;
            }
            if (!gf.lipped) {            /* too fast: it lips out */
                float a = (kr() & 1u) ? 0.5f : -0.5f, c = fm_cosf(a), s = fm_sinf(a), vx = gf.vx;
                gf.lipped = 1;
                gf.vx = (vx * c - gf.vy * s) * 0.75f;
                gf.vy = (vx * s + gf.vy * c) * 0.75f;
                juice_text(H->cx, H->cy - 14, "LIP OUT!", C_PINK);
                juice_kick(3, 0);
                juice_shake(3, 200);
                k_on(0, 64, 100, GF_WOOD, 0.1f);
                k_on(k_at(0.05f), 61, 80, GF_WOOD, 0.1f);
            }
        } else if (d > GF_CUP_R + 2.0f)
            gf.lipped = 0;
    }
    gf.x += gf.vx * h;
    gf.y += gf.vy * h;
    for (i = 0; i < gf.npl; i++) {        /* the green's edge */
        int j = (i + 1) % gf.npl;
        gf_bank(gf_hit_seg(gf.pl[i][0], gf.pl[i][1], gf.pl[j][0], gf.pl[j][1], 1.5f, 0.7f, 0.0f, 0.0f));
    }
    for (i = 0; i < gf.nwl; i++)
        gf_bank(gf_hit_seg(gf.wl[i][0], gf.wl[i][1], gf.wl[i][2], gf.wl[i][3], 1.5f, 0.7f, 0.0f, 0.0f));
    for (i = 0; i < H->nb; i++)          /* the bumpers: a kick and the chord */
        if ((hard = gf_hit_seg(H->b[i][0], H->b[i][1], H->b[i][0], H->b[i][1], 7.0f, 1.0f, 0.0f, 0.0f)) > 0.02f &&
            now - gf.wall_at > 60u) {
            float nx = gf.x - (float)H->b[i][0], ny = gf.y - (float)H->b[i][1], l = fm_sqrtf(nx * nx + ny * ny);
            gf.vx += nx / l * 0.08f, gf.vy += ny / l * 0.08f;
            gf.wall_at = now;
            gf.hit_at = now | 1u;
            k_on(0, h_note(1 + i % 4) + 12, 110, GF_BUMP, 0.5f);
            juice_ring(H->b[i][0], H->b[i][1], 22, C_PINK, 220);
            juice_burst(H->b[i][0], H->b[i][1], 10, 3, C_PINK, C_INK, 0);
            juice_shake(3, 150);
            juice_freeze(20);
            juice_heat(6);
        }
    if (H->ml) {                         /* the windmill: two blades, one bar through the hub (Nick, 17 Nov 2026: two,
                                          * not four), turning; it pushes as it goes */
        float a = gf_mill_ang(now), w = 0.0021f;
        int k;
        for (k = 0; k < 1; k++) {
            float c = fm_cosf(a + (float)k * 1.5708f), s = fm_sinf(a + (float)k * 1.5708f), L = (float)H->ml;
            float rx = gf.x - (float)H->mx, ry = gf.y - (float)H->my;
            hard = gf_hit_seg((float)H->mx - c * L, (float)H->my - s * L, (float)H->mx + c * L, (float)H->my + s * L,
                              1.8f, 0.5f, -w * ry, w * rx);
            if (hard > 0.03f && now - gf.wall_at > 80u) {
                gf.wall_at = now;
                gf.hit_at = now | 1u;
                k_on(0, h_note(2) + 12, 100, GF_WOOD, 0.3f);
                k_on(0, 38, 90, GF_KICK, 0.15f);
                juice_shake(4, 200);
                juice_burst((int32_t)gf.x, (int32_t)gf.y, 8, 3, RGB(240, 240, 240), C_PINK, 0);
                juice_text((int32_t)gf.x, (int32_t)gf.y - 10, "WHACK!", C_INK);
            }
        }
    }
    if (H->vw) {                         /* the sliding block */
        float v, bx = gf_block_x(now, &v), x0 = bx, x1 = bx + (float)H->vw, y0 = (float)H->vy, y1 = (float)(H->vy + H->vh);
        hard = gf_hit_seg(x0, y0, x1, y0, 1.0f, 0.6f, v, 0.0f);
        hard += gf_hit_seg(x0, y1, x1, y1, 1.0f, 0.6f, v, 0.0f);
        hard += gf_hit_seg(x0, y0, x0, y1, 1.0f, 0.6f, v, 0.0f);
        hard += gf_hit_seg(x1, y0, x1, y1, 1.0f, 0.6f, v, 0.0f);
        gf_bank(hard);
    }
    sp = gf.vx * gf.vx + gf.vy * gf.vy;
    if (sp < 0.004f * 0.004f || (sp < 0.03f * 0.03f && now - gf.roll_at > 9000u)) {
        if (z < 3 || now - gf.roll_at > 9000u) {   /* at rest (a slope keeps it going, a while) */
            gf.vx = gf.vy = 0.0f;
            gf.phase = 0;
            gf.armed = 0;
            if (gf.strokes >= GF_MAXSHOTS) {   /* that's enough: picked up */
                gf_card()[gf.hole] = (uint8_t)(gf_h()->par + 4);
                gf.phase = 3;
                gf.phase_at = now;
                juice_pop(120, 110, "PICKED UP", C_PINK);
                k_on(0, degree_note(-5), 90, GF_BOOM, 0.6f);
            }
        }
    }
}
static void gf_putt(void)
{
    float p = gf.charge, sp = 0.06f + (GF_VMAX - 0.06f) * p * p;
    gf.vx = fm_cosf(gf.aim) * sp;
    gf.vy = fm_sinf(gf.aim) * sp;
    gf.lx = gf.x, gf.ly = gf.y;
    gf.strokes++;
    gf.phase = 1;
    gf.bounces = 0;
    gf.lipped = 0;
    gf.roll_at = plat_ms();
    gf.hit_at = plat_ms() | 1u;
    k_on(0, h_note(1) + (int)(p * 7.0f), (uint32_t)(80 + (int)(p * 40.0f)), GF_PUTT, 0.4f);   /* harder: higher */
    k_on(0, 36, (uint32_t)(50 + (int)(p * 60.0f)), GF_KICK, 0.15f);
    juice_burst((int32_t)gf.x, (int32_t)gf.y, 4 + (int32_t)(p * 12.0f), 2, RGB(120, 220, 120), C_INK, 0);
    juice_kick((int32_t)(fm_cosf(gf.aim) * p * 6.0f), (int32_t)(fm_sinf(gf.aim) * p * 6.0f));
    juice_ring((int32_t)gf.x, (int32_t)gf.y, 10 + (int32_t)(p * 20.0f), C_INK, 200);
    if (p > 0.97f) {
        juice_text((int32_t)gf.x, (int32_t)gf.y - 12, "MAX!", C_YEL);
        juice_shake(4, 200);
    }
    juice_heat(4 + (int32_t)(p * 8.0f));
}
static void gf_nav(int dx, int dy, int sel)
{
    if (gf.phase == 6) {                 /* the course chooser */
        int d = dy ? -dy : dx;
        if (d) {
            gf.pick = (uint8_t)((gf.pick + GF_NCOURSE + (d > 0 ? 1 : -1)) % GF_NCOURSE);
            gf_course = gf.pick;
            gf.x = gf_h()->tx, gf.y = gf_h()->ty;
            gf_shape();
            k_on(0, h_note(1 + gf.pick * 2) + 12, 70, GF_CUP, 0.4f);
        }
        if (sel && plat_ms() - gf.phase_at > 300u) {
            gf_course = gf.pick;
            gf_begin();
        }
        return;
    }
    if (!sel)
        return;
    if (gf.phase == 3 && plat_ms() - gf.phase_at > 900u) {   /* the next hole, or the round's card */
        if (gf_two() && gf.who == 0) {    /* two players: P2 plays the same hole */
            gf.who = 1;
            gf_start_hole();
        } else if (gf.hole < 8) {
            gf.who = 0;
            gf.hole++;
            gf_start_hole();
        } else {
            int i, tot = 0;
            gf.who = 0;
            for (i = 0; i < 9; i++)
                tot += gf.card[i];
            if (!gf_two() && (!gf_cbest[gf_course] || (uint32_t)tot < gf_cbest[gf_course]))
                gf_cbest[gf_course] = (uint32_t)tot;
            gf_best = gf_cbest[gf_course];
            gf.phase = 5;
            gf.phase_at = plat_ms();
            juice_reset();
            juice_confetti(120, 60, 40);
            for (i = 0; i < 6; i++)
                k_on(k_at(0.1f * (float)i), h_note(1 + i) + 12, 100, GF_WIN, 1.0f);
        }
    } else if (gf.phase == 5 && plat_ms() - gf.phase_at > 900u)
        gf_new();
}
static void gf_tick(void)
{
    uint32_t dt = arc_dt(&gf.last), now = plat_ms();
    int i, n, pull = HELD_S || HELD_D;
    if (gf.phase == 6)                   /* (choosing the course: nothing rolls) */
        return;
    if (gf.phase == 2 && now - gf.phase_at > (gf_card()[gf.hole] == 1 ? 2600u : 1700u)) {   /* sunk: the card */
        gf.phase = 3;
        gf.phase_at = now;
        juice_calm();
    }
    if (gf.phase == 4 && now - gf.phase_at > 800u) {    /* out of the water: back where it was putted from */
        gf.x = gf.lx, gf.y = gf.ly;
        gf.vx = gf.vy = 0.0f;
        gf.phase = 0;
        gf.armed = 0;
        juice_ring((int32_t)gf.x, (int32_t)gf.y, 16, C_CYAN, 250);
        if (gf.strokes >= GF_MAXSHOTS) { /* that's enough: picked up */
            gf_card()[gf.hole] = (uint8_t)(gf_h()->par + 4);
            gf.phase = 3;
            gf.phase_at = now;
            juice_pop(120, 110, "PICKED UP", C_PINK);
            k_on(0, degree_note(-5), 90, GF_BOOM, 0.6f);
        }
    }
    if (gf.phase == 0) {                 /* aiming: C, E turn it (faster, the longer held); A pulses the power */
        static uint32_t held_since;
        int l = HELD_L, r = HELD_R;
        if (l || r) {
            float rate;
            if (!held_since)
                held_since = now;
            rate = now - held_since < 400u ? 0.0007f : 0.0022f;
            gf.aim += (r ? rate : -rate) * (float)dt;
        } else
            held_since = 0;
        if (!pull)
            gf.armed = 1;
        if (pull && gf.armed && !gf.charging) {
            gf.charging = 1;
            gf.charge_at = now;
        }
        if (gf.charging) {
            uint32_t q = (now - gf.charge_at) % 1600u;   /* up and down, 1.6 s round */
            gf.charge = q < 800u ? (float)q / 800.0f : (float)(1600u - q) / 800.0f;
            if (!pull) {
                gf.charging = 0;
                gf_putt();
            }
        }
    }
    if (gf.phase == 1) {
        n = (int)(dt / 2u) + 1;
        for (i = 0; i < n && gf.phase == 1; i++)
            gf_step((float)dt / (float)n);
    }
    if (gf.phase == 2) {                 /* sinking: drawn in, spinning down */
        const gf_hole_t *h = gf_h();
        gf.x += ((float)h->cx - gf.x) * 0.25f;
        gf.y += ((float)h->cy - gf.y) * 0.25f;
    }
    for (i = 5; i > 0; i--)
        gf.tx[i] = gf.tx[i - 1], gf.ty[i] = gf.ty[i - 1];
    gf.tx[0] = (int16_t)gf.x, gf.ty[0] = (int16_t)gf.y;
}

/* ------------------------------------------------------------- drawing --- */
static void gf_thick(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t r, uint16_t c)
{
    int32_t n = iabs(x2 - x1) > iabs(y2 - y1) ? iabs(x2 - x1) : iabs(y2 - y1), i;
    for (i = 0; i <= n; i++) {
        int32_t x = x1 + (x2 - x1) * i / (n ? n : 1), y = y1 + (y2 - y1) * i / (n ? n : 1);
        cv_rect(x - r, y - r, 2 * r + 1, 2 * r + 1, c);
    }
}
static void gf_rail(int32_t x1, int32_t y1, int32_t x2, int32_t y2)   /* a wooden rail */
{
    gf_thick(x1 + 1, y1 + 2, x2 + 1, y2 + 2, 3, RGB(8, 20, 14));
    gf_thick(x1, y1, x2, y2, 3, RGB(90, 50, 25));
    gf_thick(x1, y1, x2, y2, 2, RGB(170, 105, 55));
    gf_thick(x1, y1 - 1, x2, y2 - 1, 0, RGB(230, 170, 110));
}
static void gf_green(const gf_hole_t *h, uint32_t ms)   /* the green: a filled polygon, mown in stripes */
{
    int32_t y;
    for (y = 28; y < 240; y++) {
        int32_t xs[8], k = 0, i, j;
        float fy = (float)y + 0.5f;
        for (i = 0; i < gf.npl; i++) {
            int32_t x0 = gf.pl[i][0], y0 = gf.pl[i][1], x1 = gf.pl[(i + 1) % gf.npl][0], y1 = gf.pl[(i + 1) % gf.npl][1];
            if ((fy >= (float)y0) != (fy >= (float)y1) && k < 8)
                xs[k++] = x0 + (int32_t)((fy - (float)y0) * (float)(x1 - x0) / (float)(y1 - y0));
        }
        for (i = 1; i < k; i++)          /* (sorted: a few) */
            for (j = i; j > 0 && xs[j] < xs[j - 1]; j--) {
                int32_t t = xs[j];
                xs[j] = xs[j - 1], xs[j - 1] = t;
            }
        for (i = 0; i + 1 < k; i += 2) {
            int32_t x;
            for (x = xs[i]; x < xs[i + 1]; x += 12) {
                int32_t w = xs[i + 1] - x < 12 ? xs[i + 1] - x : 12;
                int stripe = (((x + y) / 14) & 1);
                cv_rect(x, y, w, 1, GF_TURF[gf_course][stripe ? 0 : 1]);
            }
        }
    }
    (void)ms;
}
static void gf_ball(int32_t x, int32_t y, int32_t sq)   /* the ball: white, dimpled, a shadow; squashed a moment */
{
    disc(x + 1, y + 2, 3, RGB(10, 40, 20));
    if (sq) {
        cv_rect(x - 4, y - 2, 9, 5, RGB(240, 240, 250));
        cv_rect(x - 3, y - 3, 7, 7, RGB(240, 240, 250));
    } else
        disc(x, y, 3, RGB(240, 240, 250));
    cv_pset(x + 1, y + 1, RGB(180, 180, 200));
    cv_pset(x + 2, y, RGB(180, 180, 200));
    cv_pset(x - 1, y - 2, RGB(255, 255, 255));
    cv_pset(x - 2, y - 1, RGB(255, 255, 255));
}
static void gf_aim_line(const gf_hole_t *h, float len)   /* dots to the first wall, then the bounce */
{
    float x = gf.x, y = gf.y, dx = fm_cosf(gf.aim), dy = fm_sinf(gf.aim), left = len, step = 6.0f;
    int bounce = 0, i, seg_n = gf.npl + gf.nwl;
    uint32_t ms = plat_ms();
    while (left > 0.0f && bounce < 2) {
        float best = left, bnx = 0.0f, bny = 0.0f;
        for (i = 0; i < seg_n; i++) {    /* the nearest wall along the ray */
            float ax, ay, bx, by, ex, ey, den, t, u;
            if (i < gf.npl) {
                ax = gf.pl[i][0], ay = gf.pl[i][1];
                bx = gf.pl[(i + 1) % gf.npl][0], by = gf.pl[(i + 1) % gf.npl][1];
            } else {
                ax = gf.wl[i - gf.npl][0], ay = gf.wl[i - gf.npl][1], bx = gf.wl[i - gf.npl][2], by = gf.wl[i - gf.npl][3];
            }
            ex = bx - ax, ey = by - ay;
            den = dx * ey - dy * ex;
            if (den > -0.0001f && den < 0.0001f)
                continue;
            t = ((ax - x) * ey - (ay - y) * ex) / den;
            u = ((ax - x) * dy - (ay - y) * dx) / den;
            if (t > 0.5f && t < best && u >= 0.0f && u <= 1.0f) {
                float l = fm_sqrtf(ex * ex + ey * ey);
                best = t - GF_R;
                bnx = -ey / l, bny = ex / l;
            }
        }
        {                                /* the dots, marching */
            float off = (float)(ms % 300u) / 300.0f * step, d;
            for (d = off; d < best; d += step) {
                int32_t px = (int32_t)(x + dx * d), py = (int32_t)(y + dy * d);
                uint16_t c = bounce ? RGB(255, 210, 90) : C_INK;
                cv_rect(px - (bounce ? 0 : 1), py - (bounce ? 0 : 1), bounce ? 2 : 2, 2, c);
            }
        }
        if (best >= left)
            break;
        x += dx * best, y += dy * best;
        left -= best;
        {                                /* the bounce: reflected, the rest of it half as long */
            float dn = dx * bnx + dy * bny;
            dx -= 2.0f * dn * bnx, dy -= 2.0f * dn * bny;
            left *= 0.5f;
            disc((int32_t)x, (int32_t)y, 2, RGB(255, 210, 90));
        }
        bounce++;
    }
}
static void gf_scene(void)
{
    const gf_hole_t *h = gf_h();
    uint32_t ms = plat_ms();
    int i;
    char s[32], n[8];
    {                                    /* the night around: hedges, fireflies */
        int32_t y, x;
        for (y = 24; y < 240; y += 8)
            for (x = 0; x < 240; x += 8)
                cv_rect(x, y, 8, 8, GF_SKY[gf_course][((x + y) / 8) & 1]);
        for (i = 0; i < 6; i++) {
            float t = (float)ms * 0.0007f + (float)i * 1.7f;
            int32_t fx = 120 + (int32_t)(fm_sinf(t * 1.3f + (float)i) * 110.0f), fy = 132 + (int32_t)(fm_cosf(t * 0.9f + (float)i * 2.0f) * 100.0f);
            if (((ms / 200u) + (uint32_t)i) % 5u)
                cv_rect(fx, fy, 2, 2, RGB(220, 255, 120));
        }
    }
    gf_green(h, ms);
    for (i = 0; i < h->nz; i++) {        /* the zones */
        int32_t zx = h->z[i][1], zy = h->z[i][2], zw = h->z[i][3], zh = h->z[i][4], t = h->z[i][0], k;
        if (t == 1 || t == 2) {          /* sand (speckled) and water (waves moving): a blob, a darker rim */
            int32_t yy;
            for (yy = zy; yy < zy + zh; yy++) {
                float x0, x1;
                int32_t a, b;
                if (!gf_zone_span(h->z[i], i, (float)yy + 0.5f, &x0, &x1))
                    continue;
                a = (int32_t)(x0 + 0.5f), b = (int32_t)(x1 + 0.5f);
                if (b <= a)
                    continue;
                cv_rect(a, yy, b - a, 1, t == 1 ? RGB(222, 192, 122) : RGB(40, 100, 200));
                cv_rect(a, yy, 2, 1, t == 1 ? RGB(196, 164, 100) : RGB(20, 60, 140));
                cv_rect(b - 2, yy, 2, 1, t == 1 ? RGB(196, 164, 100) : RGB(20, 60, 140));
                if (t == 1) {
                    for (k = a + (yy * 7) % 9; k < b - 2; k += 9)   /* speckles */
                        if ((k * 37 + yy * 53) % 5 == 0)
                            cv_pset(k, yy, RGB(190, 160, 100));
                } else if ((yy - zy) % 7 == 4 && yy < zy + zh - 3) {   /* waves */
                    for (k = 0; k < b - a; k += 16) {
                        int32_t wx = a + (k + (int32_t)(ms / 60u) + yy * 3) % (b - a);
                        if (wx + 6 < b - 2)
                            cv_rect(wx, yy, 6, 1, RGB(140, 200, 255));
                    }
                }
            }
            for (yy = zy; yy < zy + zh; yy += zh - 1) {   /* (the top and bottom rims) */
                float x0, x1;
                if (gf_zone_span(h->z[i], i, (float)yy + (yy == zy ? 1.5f : -0.5f), &x0, &x1))
                    cv_rect((int32_t)x0 + 2, yy, (int32_t)(x1 - x0) - 4, 1, t == 1 ? RGB(196, 164, 100) : RGB(20, 60, 140));
            }
        } else {                         /* a slope: lighter, chevrons rolling its way */
            int32_t ox = t == 5 ? -1 : t == 6 ? 1 : 0, oy = t == 3 ? -1 : t == 4 ? 1 : 0, ph = (int32_t)(ms / 50u) % 16, xx, yy;
            for (yy = zy; yy < zy + zh; yy += 16)
                for (xx = zx; xx < zx + zw; xx += 16) {
                    int32_t cx = xx + 8 + ox * (ph - 8), cy = yy + 8 + oy * (ph - 8);
                    if (cx < zx + 3 || cx > zx + zw - 3 || cy < zy + 3 || cy > zy + zh - 3)
                        continue;
                    if (oy) {
                        cv_line(cx - 4, cy - oy * 2, cx, cy + oy * 2, RGB(110, 200, 120));
                        cv_line(cx, cy + oy * 2, cx + 4, cy - oy * 2, RGB(110, 200, 120));
                    } else {
                        cv_line(cx - ox * 2, cy - 4, cx + ox * 2, cy, RGB(110, 200, 120));
                        cv_line(cx + ox * 2, cy, cx - ox * 2, cy + 4, RGB(110, 200, 120));
                    }
                }
        }
    }
    {                                    /* the cup and its flag (lifted as the ball comes near; it flies off when the
                                          * ball drops), a ring pulsing round the cup while the ball rolls at it */
        uint32_t fq = ms - gf.flag_at;
        float ddx = gf.x - (float)h->cx, ddy = gf.y - (float)h->cy, dd = ddx * ddx + ddy * ddy;
        int32_t fx = h->cx, fy = h->cy, lift = gf.flag_at ? (int32_t)(fq < 800u ? fq / 8u : 100u) : 0, k;
        if (!gf.flag_at && gf.phase == 1 && dd < 40.0f * 40.0f) {
            int32_t pr = 8 + (int32_t)((ms / 40u) % 10u);
            lift = 10;
            for (k = 0; k < 24; k++) {
                float th = (float)k * 0.2618f;
                cv_pset(fx + (int32_t)(fm_cosf(th) * (float)pr), fy + (int32_t)(fm_sinf(th) * (float)pr), C_YEL);
            }
        }
        disc(fx, fy, (int32_t)GF_CUP_R + 1, RGB(20, 80, 40));
        disc(fx, fy, (int32_t)GF_CUP_R, RGB(10, 10, 14));
        if (lift < 100) {
            cv_rect(fx, fy - 24 - lift, 1, 24, RGB(230, 230, 230));
            for (k = 0; k < 9; k++) {    /* the flag, waving */
                int32_t wv = (int32_t)(fm_sinf((float)ms * 0.008f + (float)k * 0.6f) * 1.5f);
                cv_rect(fx + 1 + k, fy - 24 - lift + k / 2 + wv, 1, 8 - k, C_PINK);
            }
        }
    }
    cv_rect(h->tx - 5, h->ty + 6, 2, 2, C_INK);   /* the tee */
    cv_rect(h->tx + 4, h->ty + 6, 2, 2, C_INK);
    for (i = 0; i < h->nb; i++) {        /* the bumpers */
        int lit = gf.hit_at && ms - gf.hit_at < 120u && (gf.x - h->b[i][0]) * (gf.x - h->b[i][0]) + (gf.y - h->b[i][1]) * (gf.y - h->b[i][1]) < 200.0f;
        disc(h->b[i][0] + 1, h->b[i][1] + 2, 7, RGB(8, 30, 18));
        disc(h->b[i][0], h->b[i][1], 7, lit ? C_INK : C_PINK);
        disc(h->b[i][0], h->b[i][1], 4, lit ? C_YEL : RGB(255, 240, 245));
        disc(h->b[i][0], h->b[i][1], 2, C_PINK);
    }
    for (i = 0; i < gf.npl; i++)
        gf_rail(gf.pl[i][0], gf.pl[i][1], gf.pl[(i + 1) % gf.npl][0], gf.pl[(i + 1) % gf.npl][1]);
    for (i = 0; i < gf.nwl; i++)
        gf_rail(gf.wl[i][0], gf.wl[i][1], gf.wl[i][2], gf.wl[i][3]);
    if (h->vw) {                         /* the sliding block */
        int32_t bx = (int32_t)gf_block_x(ms, 0);
        cv_rect(bx + 1, h->vy + 2, h->vw, h->vh, RGB(8, 20, 14));
        cv_rect(bx, h->vy, h->vw, h->vh, RGB(170, 105, 55));
        cv_rect(bx, h->vy, h->vw, 2, RGB(230, 170, 110));
        for (i = 6; i < h->vw; i += 8)
            cv_rect(bx + i, h->vy + 3, 2, h->vh - 4, C_YEL);
    }
    if (h->ml) {                         /* the windmill: its house, its turning sails */
        float a = gf_mill_ang(ms);
        int k;
        cv_rect(h->mx - 6, h->my - 6, 12, 12, RGB(200, 70, 70));
        cv_rect(h->mx - 3, h->my, 6, 6, RGB(60, 30, 20));
        for (k = 0; k < 2; k++) {        /* (two sails, opposite) */
            float c = fm_cosf(a + (float)k * 3.1415927f), s = fm_sinf(a + (float)k * 3.1415927f);
            int32_t ex = h->mx + (int32_t)(c * (float)h->ml), ey = h->my + (int32_t)(s * (float)h->ml);
            gf_thick(h->mx, h->my, ex, ey, 2, RGB(70, 40, 25));
            gf_thick(h->mx + (int32_t)(c * 6.0f), h->my + (int32_t)(s * 6.0f), ex, ey, 1, k & 1 ? RGB(250, 250, 250) : C_PINK);
        }
        disc(h->mx, h->my, 3, C_YEL);
    }
    if (gf.phase == 0) {                 /* aiming: the line; the power, pulsing */
        gf_aim_line(h, gf.charging ? 24.0f + 150.0f * gf.charge : 60.0f);
        if (gf.charging) {
            int32_t bx = (int32_t)gf.x - 14, by = (int32_t)gf.y + 8, w = (int32_t)(gf.charge * 28.0f);
            uint16_t pc = gf.charge < 0.5f ? RGB(120, 230, 120) : gf.charge < 0.85f ? C_YEL : C_PINK;
            cv_rect(bx - 1, by - 1, 30, 6, RGB(10, 10, 14));
            cv_rect(bx, by, w, 4, pc);
            if (gf.charge > 0.97f && (ms / 60u) & 1u)
                frame_rect(bx - 2, by - 2, 32, 8, C_INK);
        }
    }
    if (gf.phase == 1 && gf.vx * gf.vx + gf.vy * gf.vy > 0.04f)   /* fast: a streak behind */
        for (i = 5; i > 0; i--)
            disc(gf.tx[i], gf.ty[i], i > 3 ? 1 : 2, mix565(RGB(40, 136, 62), RGB(240, 240, 250), 160 - i * 26));
    if (gf.phase != 4) {
        if (gf.phase == 2) {             /* dropping into the cup: smaller */
            uint32_t q = ms - gf.phase_at;
            if (q < 260u)
                disc((int32_t)gf.x, (int32_t)gf.y, 3 - (int32_t)q / 90, RGB(240, 240, 250));
        } else if (gf.phase != 3 && gf.phase != 5)
            gf_ball((int32_t)gf.x, (int32_t)gf.y, gf.hit_at && ms - gf.hit_at < 70u);
    }
    tcpy(s, gf_two() ? (gf.who ? "P2  HOLE " : "P1  HOLE ") : "HOLE ");
    itos(gf.hole + 1, n);
    tcat(s, n);
    if (!gf_two()) {
        tcat(s, "  PAR ");
        itos(h->par, n);
        tcat(s, n);
    }
    tcat(s, "  SHOT ");
    itos(gf.strokes + (gf.phase == 0 ? 1 : 0), n);
    tcat(s, n);
    header("GOLF", C_CYAN, "", C_PINK, gf.phase == 6 ? "" : s);
    if (gf.phase == 3) {                 /* the hole's card */
        int d = (int)gf_card()[gf.hole] - (int)h->par;
        cv_rect(40, 84, 160, 76, C_BG);
        frame_rect(40, 84, 160, 76, d < 0 ? C_YEL : d == 0 ? C_CYAN : C_PINK);
        text_c(120, 90, &FONT_B, h->name, C_INK);
        tcpy(s, "");
        itos(gf_card()[gf.hole], n);
        tcat(s, n);
        tcat(s, gf_card()[gf.hole] == 1 ? " STROKE" : " STROKES");
        text_c(120, 114, &FONT_M, s, d < 0 ? C_YEL : d == 0 ? C_CYAN : C_PINK);
        text_c(120, 142, &FONT_T, gf_two() && gf.who == 0 ? "TOP KEY: PLAYER 2" : gf.hole < 8 ? "TOP KEY: NEXT HOLE"
                                                                                             : "TOP KEY: THE CARD",
               C_GHOST);
    } else if (gf.phase == 5) {          /* the round's card (two players: a row each, the lower total wins) */
        int tot = 0, tot2 = 0, par = 0, k;
        cv_rect(14, 40, 212, 170, C_BG);
        frame_rect(14, 40, 212, 170, C_YEL);
        text_c(120, 46, &FONT_M, "THE CARD", C_YEL);
        for (k = 0; k < 9; k++) {
            int32_t x = 24 + k * 22, pk = GF_COURSE[gf_course][k].par, d = (int32_t)gf.card[k] - pk,
                    d2 = (int32_t)gf.card2[k] - pk;
            itos(k + 1, n);
            text_c(x + 8, 76, &FONT_T, n, C_GHOST);
            itos(gf.card[k], n);
            text_c(x + 8, 90, &FONT_XS, n, d < 0 ? C_YEL : d == 0 ? C_CYAN : C_PINK);
            if (gf_two()) {
                itos(gf.card2[k], n);
                text_c(x + 8, 108, &FONT_XS, n, d2 < 0 ? C_YEL : d2 == 0 ? C_CYAN : C_PINK);
            }
            tot += gf.card[k];
            tot2 += gf.card2[k];
            par += pk;
        }
        if (gf_two()) {
            tcpy(s, "P1 ");
            itos(tot, n);
            tcat(s, n);
            tcat(s, "   P2 ");
            itos(tot2, n);
            tcat(s, n);
            text_c(120, 134, &FONT_B, s, C_INK);
            text_c(120, 156, &FONT_M, tot < tot2 ? "P1 WINS!" : tot2 < tot ? "P2 WINS!" : "A TIE!",
                   tot < tot2 ? C_CYAN : tot2 < tot ? C_PINK : C_YEL);
        } else {
            tcpy(s, "TOTAL ");
            itos(tot, n);
            tcat(s, n);
            tcat(s, tot < par ? "  UNDER PAR!" : tot == par ? "  EVEN" : "  OVER PAR");
            text_c(120, 128, &FONT_B, s, tot <= par ? C_YEL : C_INK);
            tcpy(s, "BEST ");
            itos((int32_t)gf_best, n);
            tcat(s, n);
            text_c(120, 160, &FONT_XS, s, C_CYAN);
        }
        text_c(120, 186, &FONT_T, "TOP KEY: NEW COURSE   HOME: MENU", C_GHOST);
    } else if (gf.phase == 6) {          /* the course chooser: three courses, nine holes each */
        int k;
        cv_rect(24, 52, 192, 150, C_BG);  /* the title and the three names (Nick, 19 Nov 2026: the small print
                                          * couldn't be read) */
        frame_rect(24, 52, 192, 150, C_YEL);
        text_c(120, 60, &FONT_M, "CHOOSE A COURSE", C_YEL);
        for (k = 0; k < GF_NCOURSE; k++) {
            int32_t y = 100 + k * 34, on = k == gf.pick;
            if (on) {
                cv_rect(32, y - 6, 176, 28, mix565(C_BG, GF_TURF[k][0], 90));
                frame_rect(32, y - 6, 176, 28, C_YEL);
            }
            text_c(120, y, &FONT_B, GF_CNAME[k], on ? C_INK : C_GHOST);
        }
    } else if (gf.phase == 0 && !gf.charging && gf.strokes == 0 && gf.hole == 0 && ms - gf.intro_at > 1800u)
        text_c(120, 226, &FONT_T, "LEFT RIGHT: AIM   HOLD TOP KEY: PUTT", RGB(225, 250, 225));
}
