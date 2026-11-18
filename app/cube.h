/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Nick Culbertson */
/* cube.h: CUBE DROP, for Mighty Arcade's menu: a colour puzzle. Included once from app.c, after its helpers.
 *
 * Reworked 17 Nov 2026 (Nick: make it play nothing like the falling-block game, the way the colour-matching
 * puzzlers did). Pieces are pairs and threes of coloured cubes; nothing clears by rows. Four or more touching cubes of
 * one colour POP; the cubes above them fall, and if that makes another group, it pops too: a CHAIN, each link higher
 * up the chord. Each level begins with NOTE GEMS set in the well (they stay put, holding up what lands on them): pop
 * a gem in a group of its colour and it plays its note. Clear every gem and the level is done; the gems are its
 * melody. A chain of two or more earns a BOMB: it falls with a fuse that sparks on the beat and blasts a hole five
 * wide and three deep, gems and all.
 *
 *   left / right   move (held: it repeats)     up   turn     down   faster     A   drop it
 * And a sequencer: a playhead walks the twelve columns, an eighth each, and each plays its top cube (a gem its own
 * note; else the taller the column, the higher) (CHANCE, SOUND, scale and key on the knobs; the tempo is the level's). */

enum { CS_KICK, CS_SNARE, CS_HAT, CS_BASS, CS_LAND, CS_CLEAR, CS_BIG, CS_OVER, CS_TURN, CS_PAD, CS_SEQ };   /* CS_SEQ:
                                                                                            * the playhead's */
static uint8_t cu_harm;                  /* the chord the last pop chose (kept: the tests read it) */
#define WW 12
#define WH 13
#define CELL 16
#define WX 4
#define WY 26
#define CU_NC 4                          /* the colours */
#define CU_GEM 8u                        /* a cell: its colour 1 .. 4, | CU_GEM a note gem */
#define CU_POP 4                         /* touching cubes of a colour that pop */

typedef struct {
    int8_t n;
    int8_t c[3][2];
} cpiece_t;
static const cpiece_t CP[4] = {
    {2, {{0, 0}, {1, 0}}},               /* a pair */
    {3, {{0, 0}, {1, 0}, {2, 0}}},       /* three in a row */
    {3, {{0, 0}, {1, 0}, {0, 1}}},       /* three in a corner */
    {1, {{0, 0}}},                       /* the bomb (CU_BOMB: earned, never dealt) */
};
#define CU_BOMB 3
static const uint8_t CU_KINDS[] = {0, 0, 0, 1, 2};   /* mostly pairs, a three now and then */
#define CU_NKIND (sizeof CU_KINDS / sizeof CU_KINDS[0])
static const uint16_t CUBE_COL[11] = {   /* 1 .. 4 the puzzle's colours: pink, cyan, yellow, violet (a green was too
                                          * near the cyan, Nick 17 Nov 2026); the rest: the menu's icon */
    0, RGB(255, 46, 151), RGB(95, 227, 238), RGB(255, 210, 63), RGB(150, 96, 255), RGB(255, 150, 60),
    RGB(90, 140, 255), RGB(176, 124, 255), RGB(244, 240, 255), RGB(255, 112, 92), RGB(60, 52, 80)};

typedef struct {
    uint8_t well[WH][WW];                /* 0 empty, else a colour (| CU_GEM) */
    uint8_t gnote[WH][WW];               /* a gem's note: a tone up the chord (h_tone) */
    int8_t pc[3][2], pn, kind, px, py;   /* the falling piece: its cubes (turned), where */
    uint8_t pcol[3];                     /* its cubes' colours */
    uint32_t score, lines, level;        /* lines: the groups popped */
    uint32_t fall_at;                    /* the next step down (ms) */
    uint16_t pop[WH];                    /* cubes popping (a bit a column) */
    uint32_t clear_at;                   /* the pop (or the level's end) done at */
    uint8_t phase;                       /* 0 a piece falling, 1 popping, 2 the level cleared */
    uint8_t chain, best_chain;
    uint8_t over, playing;
    uint32_t over_at;
    uint32_t crumble[WH];                /* at the end: which cubes are still there (a bit a column) */
    int8_t land[3][2];                   /* the last landing's cubes: they flash */
    uint32_t land_at;
    uint32_t seq_ms[WW];                 /* when the playhead's note in each column last sounded */
    uint8_t bombs;                       /* bombs earned, still to come (at most 2) */
    uint32_t blast_at;                   /* the last blast (ms) */
    int8_t bx, by;                       /* where */
    uint8_t gems;                        /* gems left this level */
} cu_t;
#define cu (*(cu_t *)g_arena)   /* in the games' shared memory (app.c) */
_Static_assert(sizeof(cu_t) <= sizeof g_arena, "cu: the arena is too small");

static uint32_t cu_bpm(void) { return 96u + 4u * (cu.level > 10u ? 10u : cu.level); }
static uint32_t cu_six(void) { return (uint32_t)(SR_F * 15.0f / (float)cu_bpm()); }   /* a sixteenth (samples) */
static uint32_t cu_fall_ms(void)         /* a step down: 760 ms at level 1, 70 ms quicker each, 160 ms at the most */
{
    return cu.level >= 9u ? 160u : 760u - 70u * (cu.level - 1u);
}

static int cu_hits(const int8_t c[3][2], int n, int x, int y)
{
    int i;
    for (i = 0; i < n; i++) {
        int cx = x + c[i][0], cy = y + c[i][1];
        if (cx < 0 || cx >= WW || cy >= WH)
            return 1;
        if (cy >= 0 && cu.well[cy][cx])
            return 1;
    }
    return 0;
}

static void cu_spawn(void)
{
    int i, w = 0;
    if (cu.bombs) {                      /* an earned bomb comes first */
        cu.bombs--;
        cu.kind = CU_BOMB;
    } else
        cu.kind = (int8_t)CU_KINDS[kr() % CU_NKIND];
    cu.pn = CP[cu.kind].n;
    for (i = 0; i < cu.pn; i++) {
        cu.pc[i][0] = CP[cu.kind].c[i][0];
        cu.pc[i][1] = CP[cu.kind].c[i][1];
        cu.pcol[i] = (uint8_t)(1u + (kr() >> 16) % CU_NC);   /* (the high bits: the low ones of kr()
                                                                * follow each other too closely, all one colour) */
        if (cu.pc[i][0] + 1 > w)
            w = cu.pc[i][0] + 1;
    }
    cu.px = (int8_t)((WW - w) / 2);
    cu.py = -1;
    cu.phase = 0;
    cu.fall_at = plat_ms() + cu_fall_ms();
    if (cu_hits(cu.pc, cu.pn, cu.px, cu.py + 1)) {   /* no room: the end */
        uint32_t r;
        cu.over = 1;
        cu.over_at = plat_ms();
        juice_shake(9, 700);
        juice_flash(RGB(255, 40, 80), 170, 700);
        juice_freeze(150);
        juice_glitch(700);
        juice_cool();
        juice_ring(WX + WW * CELL / 2, WY + 40, 140, RGB(255, 40, 80), 700);
        for (r = 0; r < WH; r++) {
            uint32_t m = 0, x;
            for (x = 0; x < WW; x++)
                if (cu.well[r][x])
                    m |= 1u << x;
            cu.crumble[r] = m;
        }
        for (i = 0; i < 5; i++)
            k_on(k_at(0.2f * (float)i), degree_note(9 - 2 * i), 90, CS_OVER, 1.0f);
    }
}

static void cu_level_start(void)         /* a level: the well cleared, its gems set (more each level, higher up) */
{
    int y, x, n, placed = 0, tries = 0, top = WH - 4 - (int)(cu.level > 10u ? 5u : cu.level / 2u), note = 4;
    for (y = 0; y < WH; y++)
        for (x = 0; x < WW; x++)
            cu.well[y][x] = 0, cu.gnote[y][x] = 0;
    n = 3 + 2 * (int)cu.level;
    if (n > 22)
        n = 22;
    while (placed < n && tries++ < 2000) {
        int c = (int)(1u + kr() % CU_NC), ok = 1, k;
        static const int8_t D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        x = (int)(kr() % WW), y = top + (int)(kr() % (uint32_t)(WH - top));
        if (cu.well[y][x])
            continue;
        for (k = 0; k < 4; k++) {        /* (never two of a colour touching: each gem wants cubes brought to it) */
            int ax = x + D[k][0], ay = y + D[k][1];
            if (ax >= 0 && ax < WW && ay >= 0 && ay < WH && (cu.well[ay][ax] & 7u) == (uint32_t)c)
                ok = 0;
        }
        if (!ok)
            continue;
        cu.well[y][x] = (uint8_t)(c | CU_GEM);
        cu.gnote[y][x] = (uint8_t)note;  /* the level's melody: a walk up and down the chord */
        note += (int)(kr() % 5u) - 2;
        if (note < 2 || note > 11)
            note = note < 2 ? 3 : 10;
        placed++;
    }
    cu.gems = (uint8_t)placed;
    for (y = 0; y < WH; y++)
        cu.pop[y] = 0;
    cu.chain = 0;
    cu.blast_at = 0;
    {
        char ls[12] = "LEVEL ";
        itos((int32_t)cu.level, ls + 6);
        juice_pop(WX + WW * CELL / 2, WY + 60, ls, C_PINK);
        juice_text(WX + WW * CELL / 2, WY + 90, "POP THE GEMS", C_CYAN);
    }
    cu_spawn();
}

static void cu_new(void)
{
    uint32_t x;
    cu.score = cu.lines = 0;
    cu.level = 1;
    cu.land_at = 0;
    cu.bombs = 0;
    cu.best_chain = 0;
    for (x = 0; x < WW; x++)
        cu.seq_ms[x] = 0;
    cu.over = 0;
    cu.playing = 1;
    cu_harm = 0;
    cu_level_start();
}

static void cu_gravity(void)             /* every cube with room below it falls (the gems stay where they are) */
{
    int x, y;
    for (x = 0; x < WW; x++)
        for (y = WH - 2; y >= 0; y--) {
            int k = y;
            uint8_t c = cu.well[y][x];
            if (!c || (c & CU_GEM))
                continue;
            while (k + 1 < WH && !cu.well[k + 1][x])
                k++;
            if (k != y) {
                cu.well[k][x] = c;
                cu.well[y][x] = 0;
            }
        }
}

/* the groups of CU_POP or more touching cubes of a colour: marked to pop; how many groups */
static int cu_find(void)
{
    static uint8_t seen[WH][WW];
    static int8_t qx[WH * WW], qy[WH * WW];
    static const int8_t D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int x, y, groups = 0;
    for (y = 0; y < WH; y++) {
        cu.pop[y] = 0;
        for (x = 0; x < WW; x++)
            seen[y][x] = 0;
    }
    for (y = 0; y < WH; y++)
        for (x = 0; x < WW; x++) {
            int h = 0, t = 0, k;
            uint8_t c = cu.well[y][x] & 7u;
            if (!c || seen[y][x])
                continue;
            seen[y][x] = 1;
            qx[t] = (int8_t)x, qy[t++] = (int8_t)y;
            while (h < t) {
                int cx = qx[h], cy = qy[h++];
                for (k = 0; k < 4; k++) {
                    int ax = cx + D[k][0], ay = cy + D[k][1];
                    if (ax < 0 || ax >= WW || ay < 0 || ay >= WH || seen[ay][ax] || (cu.well[ay][ax] & 7u) != c)
                        continue;
                    seen[ay][ax] = 1;
                    qx[t] = (int8_t)ax, qy[t++] = (int8_t)ay;
                }
            }
            if (t >= CU_POP) {
                groups++;
                for (k = 0; k < t; k++)
                    cu.pop[qy[k]] |= (uint16_t)(1u << qx[k]);
            }
        }
    return groups;
}

static void cu_level_check(void)         /* no gems left: the level is done */
{
    if (cu.gems || cu.phase == 2)
        return;
    cu.phase = 2;
    cu.clear_at = plat_ms() + 1800u;
    cu.score += 1000u * cu.level;
    juice_calm();
    juice_pop(WX + WW * CELL / 2, WY + 70, "LEVEL CLEAR!", C_YEL);
    juice_freeze(90);
    juice_flash(C_INK, 140, 400);
    juice_ring(WX + WW * CELL / 2, WY + 100, 120, C_YEL, 600);
    juice_confetti(WX + WW * CELL / 2, WY + 100, 50);
    juice_fireworks(1500);
    juice_sting(2);
}

/* after a landing (or a blast, or a pop): what falls, falls; then any groups pop (a link more in the chain), or the
 * chain is over: a bomb for a long one, and the next piece */
static void cu_resolve(void)
{
    int groups, x, y, n = 0, k = 0;
    uint32_t t = grid_next(), six = cu_six();
    cu_gravity();
    groups = cu_find();
    if (!groups) {
        if (cu.chain >= 2 && cu.bombs < 2) {   /* a chain of two or more: a bomb next */
            cu.bombs++;
            juice_text(WX + WW * CELL / 2, WY + 140, "BOMB EARNED", RGB(255, 170, 60));
        }
        cu.chain = 0;
        cu.phase = 0;
        cu_level_check();
        if (cu.phase == 0)
            cu_spawn();
        return;
    }
    cu.chain++;
    if (cu.chain > cu.best_chain)
        cu.best_chain = cu.chain;
    cu.lines += (uint32_t)groups;
    cu.phase = 1;
    cu.clear_at = plat_ms() + 340u;
    if (cu.chain == 1) {                 /* a new chord with the first pop: the pad swells it */
        int i;
        h_change();
        cu_harm = (uint8_t)(h_deg + 1u);
        k_on(t, h_bass(), 84, CS_PAD, 2.4f);
        for (i = 1; i <= 4; i++)
            k_on(t + (uint32_t)i * six / 4u, h_note(i), 70, CS_PAD, 2.4f);
    }
    for (y = 0; y < WH; y++)             /* each cube: a spark; a run up the chord, higher each link */
        for (x = 0; x < WW; x++)
            if (cu.pop[y] >> x & 1u) {
                uint8_t c = cu.well[y][x];
                int32_t px = WX + x * CELL + CELL / 2, py = WY + y * CELL + CELL / 2;
                juice_burst(px, py, 4, 3, CUBE_COL[c & 7u], C_INK, 1);
                if (c & CU_GEM) {        /* a gem: its own note, out loud */
                    k_on(t + (uint32_t)(k % 8) * six / 2u, h_tone(cu.gnote[y][x]) + 12, 112, CS_BIG, 1.0f);
                    juice_ring(px, py, 26, CUBE_COL[c & 7u], 360);
                    juice_embers(px, py, 6, CUBE_COL[c & 7u]);
                    cu.score += 100u * cu.level;
                } else if (n < 8)
                    k_on(t + (uint32_t)n * six / 2u, h_tone(2 + 2 * cu.chain + n % 4) + 12, 80 + 4 * n, CS_CLEAR, 0.4f);
                n++, k++;
            }
    cu.score += (uint32_t)(10 * n) * cu.chain * cu.level;
    juice_shake(2 + 2 * cu.chain, 200u + 60u * cu.chain);
    juice_heat(8 * cu.chain + 2 * n);
    if (cu.chain >= 2) {
        char cs[12] = "CHAIN X";
        itos(cu.chain, cs + 7);
        juice_pop(WX + WW * CELL / 2, WY + 60, cs, cu.chain >= 4 ? C_YEL : C_CYAN);
        juice_flash(C_INK, 40 + 20 * cu.chain, 240);
        juice_freeze(30u + 20u * cu.chain);
        if (cu.chain >= 3)
            juice_sting(cu.chain >= 4 ? 2 : 1);
    }
}

static void cu_unpop(void)               /* the popped cubes gone (the gems among them counted) */
{
    int x, y;
    for (y = 0; y < WH; y++) {
        for (x = 0; x < WW; x++)
            if (cu.pop[y] >> x & 1u) {
                if ((cu.well[y][x] & CU_GEM) && cu.gems)
                    cu.gems--;
                cu.well[y][x] = 0;
            }
        cu.pop[y] = 0;
    }
}

static void cu_blast(int bx, int by)     /* the bomb goes off at bx, by: a hole five wide, three deep */
{
    int x, y, n = 0;
    uint32_t t = grid_next(), six = cu_six();
    int32_t px = WX + bx * CELL + CELL / 2, py = WY + by * CELL + CELL / 2;
    cu.blast_at = plat_ms() | 1u;
    cu.bx = (int8_t)bx, cu.by = (int8_t)by;
    for (y = by; y < by + 3 && y < WH; y++)
        for (x = bx - 2; x <= bx + 2; x++) {
            uint8_t c;
            if (x < 0 || x >= WW || y < 0 || !(c = cu.well[y][x]))
                continue;
            if (c & CU_GEM) {            /* a gem in the blast: gone, its note sounding */
                k_on(t + (uint32_t)n * six / 2u, h_tone(cu.gnote[y][x]) + 12, 100, CS_BIG, 0.8f);
                if (cu.gems)
                    cu.gems--;
                cu.score += 100u * cu.level;
            }
            cu.well[y][x] = 0;
            juice_burst(WX + x * CELL + CELL / 2, WY + y * CELL + CELL / 2, 6, 4, CUBE_COL[c & 7u], C_INK, 1);
            if (n < 12)                  /* each cube broken: a hit of the fill, a 32nd apart, louder */
                k_on(t + (uint32_t)n * six / 2u, n & 1 ? 50 : 47, 60 + 5 * n, CS_SNARE, 0.15f);
            n++;
        }
    k_on(t, 36, 127, CS_KICK, 0.4f);
    k_on(t, h_bass() - 12, 110, CS_BASS, 0.6f);
    k_on(t + (uint32_t)(n < 12 ? n : 12) * six / 2u, h_note(1) + 12, 100, CS_BIG, 1.2f);
    cu.score += (uint32_t)(40 * n) * cu.level;
    juice_freeze(110);
    juice_shake(11, 600);
    juice_flash(RGB(255, 170, 60), 200, 450);
    juice_ring(px, py, 90, RGB(255, 170, 60), 500);
    juice_ring(px, py, 50, C_INK, 300);
    juice_burst(px, py, 30, 5, RGB(255, 200, 80), RGB(255, 80, 40), 1);
    juice_embers(px, py, 18, RGB(255, 120, 40));
    juice_impact(110);
    juice_glitch(250);
    juice_heat(25);
    juice_pop(WX + WW * CELL / 2, WY + 60, "BOOM!", RGB(255, 170, 60));
}

/* the piece lands: into the well, its sound; then what falls falls, and what pops pops */
static void cu_land(void)
{
    int i, top = WH, col = 0;
    if (cu.kind == CU_BOMB) {
        cu_blast(cu.px, cu.py < 0 ? 0 : cu.py);
        cu_resolve();
        return;
    }
    for (i = 0; i < cu.pn; i++) {
        int cx = cu.px + cu.pc[i][0], cy = cu.py + cu.pc[i][1];
        if (cy >= 0)
            cu.well[cy][cx] = cu.pcol[i];
        if (cy < top)
            top = cy;
        col += cx;
        cu.land[i][0] = (int8_t)cx, cu.land[i][1] = (int8_t)cy;
    }
    col /= cu.pn;
    cu.land_at = plat_ms() | 1u;
    juice_shake(1, 80);
    k_on(0, degree_note(col / 2 + (WH - top) / 3 + 7), 96, CS_LAND, 0.4f);   /* its column; higher as it stacks */
    cu_resolve();
}

static void cu_move(int dx)
{
    if (!cu_hits(cu.pc, cu.pn, cu.px + dx, cu.py))
        cu.px = (int8_t)(cu.px + dx);
}
static void cu_turn(void)
{
    int8_t t[3][2];
    int i, mx = 0, k;
    uint8_t c0;
    static const int8_t KICK[5] = {0, -1, 1, -2, 2};
    for (i = 0; i < cu.pn; i++)
        if (cu.pc[i][1] > mx)
            mx = cu.pc[i][1];
    for (i = 0; i < cu.pn; i++) {        /* a quarter turn, then back into its box */
        t[i][0] = (int8_t)(mx - cu.pc[i][1]);
        t[i][1] = cu.pc[i][0];
    }
    for (k = 0; k < 5; k++)
        if (!cu_hits(t, cu.pn, cu.px + KICK[k], cu.py)) {
            for (i = 0; i < cu.pn; i++)
                cu.pc[i][0] = t[i][0], cu.pc[i][1] = t[i][1];
            cu.px = (int8_t)(cu.px + KICK[k]);
            k_on(0, 96, 50, CS_TURN, 0.03f);
            return;
        }
    if (cu.pn > 1) {                     /* no room to turn: its colours go round instead */
        c0 = cu.pcol[0];
        for (i = 0; i + 1 < cu.pn; i++)
            cu.pcol[i] = cu.pcol[i + 1];
        cu.pcol[cu.pn - 1] = c0;
        k_on(0, 96, 40, CS_TURN, 0.03f);
    }
}
static int cu_down(void)                 /* 1: it moved; 0: it landed */
{
    if (!cu_hits(cu.pc, cu.pn, cu.px, cu.py + 1)) {
        cu.py++;
        return 1;
    }
    cu_land();
    return 0;
}

static void cu_nav(int dx, int dy, int sel)
{
    if (cu.over) {
        if (sel && plat_ms() - cu.over_at > 800u)
            cu_new();
        return;
    }
    if (cu.phase)
        return;
    if (dx)
        cu_move(dx);
    if (dy > 0)
        cu_turn();
    if (dy < 0 && cu_down()) {
        cu.score += 1u;
        cu.fall_at = plat_ms() + cu_fall_ms();
    }
    if (sel) {                           /* all the way down: a slam */
        int i, n = 0, my = 0, sx = 0, py0 = cu.py;
        int32_t bx, by;
        for (i = 0; i < cu.pn; i++) {
            if (cu.pc[i][1] > my)
                my = cu.pc[i][1];
            sx += cu.pc[i][0];
        }
        bx = (cu.px * cu.pn + sx) * CELL / cu.pn + CELL / 2;
        while (cu.phase == 0 && !cu.over && cu_down()) {
            cu.score += 2u;
            n++;
        }
        by = (py0 + n + my + 1) * CELL;
        if (n > 1) {
            juice_shake(n > 8 ? 5 : 3, 160);
            juice_kick(0, n > 8 ? 6 : 3);    /* the slam */
            juice_ring(WX + bx, WY + by, 16 + n, C_INK, 200);
            juice_burst(WX + bx, WY + by, 6, 2, RGB(170, 160, 200), C_INK, 0);
        }
    }
}

static void cu_tick(void)
{
    uint32_t ms = plat_ms(), r;
    if (cu.over) {                       /* the stack crumbles: a cube or two goes every frame */
        for (r = 0; r < WH; r++)
            if (cu.crumble[r] && kr() % 3u == 0u) {
                uint32_t x = kr() % WW;
                if (cu.crumble[r] >> x & 1u)
                    juice_burst(WX + (int32_t)x * CELL + CELL / 2, WY + (int32_t)r * CELL + CELL / 2, 3, 2,
                                CUBE_COL[cu.well[r][x] & 7u], C_INK, 1);
                cu.crumble[r] &= ~(1u << x);
            }
        return;
    }
    if (cu.phase == 1) {                 /* the pop done: gone, then what falls falls, and maybe more pops */
        if ((int32_t)(ms - cu.clear_at) >= 0) {
            cu_unpop();
            cu_resolve();
        }
        return;
    }
    if (cu.phase == 2) {                 /* the level cleared: a moment, then the next */
        if ((int32_t)(ms - cu.clear_at) >= 0) {
            cu.level++;
            cu_level_start();
        }
        return;
    }
    if ((int32_t)(ms - cu.fall_at) >= 0) {
        cu.fall_at = ms + cu_fall_ms();
        cu_down();
    }
}

/* the groove: bass and hats from the start, a kick from level 2, a snare from level 3; quicker each level */
static void cu_groove(uint32_t t, uint32_t step)
{
    uint32_t p = step % 16u, lv = cu.level;
    int bass = h_bass();                 /* the bass on the chord the last pop chose */
    if (cu.over || !cu.playing)
        return;
    if (p % 2u == 0u)
        k_on(t, 90, p % 4u ? 40 : 60, CS_HAT, 0.04f);
    if (p % 4u == 0u || (p % 4u == 3u && lv >= 4u))
        k_on(t, bass + (p == 8u ? 12 : 0), 88, CS_BASS, 0.12f);
    if (lv >= 2u && (p == 0u || p == 8u || (lv >= 5u && p == 10u)))
        k_on(t, 36, 100, CS_KICK, 0.2f);
    if (lv >= 3u && (p == 4u || p == 12u))
        k_on(t, 50, 84, CS_SNARE, 0.2f);
    if (!(p & 1u)) {                     /* the playhead: a column an eighth, its top cube */
        int c = (int)(step / 2u % (uint32_t)WW), r;
        for (r = 0; r < WH && !cu.well[r][c]; r++)
            ;
        if (r < WH && sq_roll(SQ_CUBE)) {
            k_on(t, cu.well[r][c] & CU_GEM ? h_tone(cu.gnote[r][c]) : h_tone((WH - r) / 2 + 1), 78, CS_SEQ, 0.3f);
            cu.seq_ms[c] = sq_ms(t);
        }
    }
}

static void cu_sounds(void)
{
    k_slot_set(CS_KICK, &KSND[S_KICK]);
    k_slot_set(CS_SNARE, &KSND[S_SNARE]);
    k_slot_set(CS_HAT, &KSND[S_HAT]);
    k_slot_set(CS_BASS, &KSND[S_FMBASS]);
    k_slot_set(CS_LAND, &KSND[S_MARIMBA]);
    k_slot_set(CS_CLEAR, &KSND[S_CELESTA]);
    k_slot_set(CS_BIG, &KSND[S_TUBE]);
    k_slot_set(CS_OVER, &KSND[S_SONAR]);
    k_slot_set(CS_TURN, &KSND[S_HAT]);
    k_slot_set(CS_PAD, &KSND[S_BRASSPAD]);
    sq_slot = CS_SEQ;
    sq_sound(SQ_CUBE);
    k_slot_rel(CS_PAD, 1.6f);
    k_slot_rel(CS_BASS, 0.12f);
    k_slot_rel(CS_LAND, 0.4f);
    k_slot_rel(CS_CLEAR, 0.6f);
    k_slot_rel(CS_BIG, 1.2f);
    k_slot_rel(CS_OVER, 1.0f);
    k_main_slot = CS_LAND;
}

/* ---------------------------------------------------------------- drawing --- */
static void cube(int32_t x, int32_t y, uint16_t c)   /* a cube: its face, a lighter top, a darker side */
{
    cv_rect(x, y + 3, CELL - 3, CELL - 3, c);
    cv_rect(x + 1, y + 1, CELL - 2, 2, mix565(c, RGB(255, 255, 255), 110));
    cv_rect(x + CELL - 3, y + 2, 2, CELL - 3, mix565(c, RGB(0, 0, 0), 110));
    cv_rect(x + 2, y + 5, 3, 3, mix565(c, RGB(255, 255, 255), 60));
}
static void cu_gem(int32_t x, int32_t y, uint16_t c, uint32_t ms, int i)   /* a note gem: a faceted jewel, glowing */
{
    int32_t cx = x + CELL / 2 - 1, cy = y + CELL / 2, k, g = 120 + (int32_t)((ms / 9u + (uint32_t)i * 37u) % 120u);
    cv_rect(x, y + 1, CELL - 2, CELL - 2, mix565(RGB(10, 7, 18), c, 60));
    for (k = -6; k <= 6; k++) {          /* a diamond, lit on its upper left */
        int32_t w = 6 - (k < 0 ? -k : k);
        cv_rect(cx - w, cy + k, w, 1, mix565(c, C_INK, k < 0 ? 120 : 0));
        cv_rect(cx, cy + k, w + 1, 1, k < 0 ? c : mix565(c, RGB(0, 0, 0), 90));
    }
    cv_rect(cx - 2, cy - 3, 2, 2, mix565(c, C_INK, g));   /* its glint, pulsing */
}

static void cu_scene(void)
{
    int y, x, i;
    char n[12];
    uint32_t ms = plat_ms();
    header("CUBES", C_CYAN, "", C_PINK, cu.over ? "THE END" : "");
    cv_rect(WX - 1, WY - 1, WW * CELL + 2, WH * CELL + 2, RGB(18, 13, 32));
    {                                    /* the frame breathes with the beat */
        uint32_t beat = (ms * cu_bpm() / 60000u), ph = (ms * cu_bpm() / 600u) % 100u;
        uint16_t fc = mix565(C_LINE2, beat & 3u ? C_PURP : C_PINK, ph < 30u ? 200 - (int32_t)ph * 6 : 20);
        frame_rect(WX - 2, WY - 2, WW * CELL + 4, WH * CELL + 4, fc);
        frame_rect(WX - 3, WY - 3, WW * CELL + 6, WH * CELL + 6, ph < 20u ? fc : C_BG);
    }
    for (y = 0; y < WH; y++)
        for (x = 0; x < WW; x++) {
            uint8_t c = cu.well[y][x];
            uint16_t col = CUBE_COL[c & 7u];
            int32_t px = WX + x * CELL, py = WY + y * CELL;
            if (!c || (cu.over && !(cu.crumble[y] >> x & 1u)))
                continue;
            if (cu.phase == 1 && (cu.pop[y] >> x & 1u)) {   /* popping: white, then shrinking away */
                int32_t q = 340 - (int32_t)(cu.clear_at - ms), s = q < 140 ? 0 : (q - 140) * 7 / 200;
                if (q < 140)
                    cube(px, py, (ms / 50u) & 1u ? C_INK : col);
                else if (s < 7)
                    cv_rect(px + s, py + s, CELL - 2 * s, CELL - 2 * s, mix565(col, C_INK, 200));
                continue;
            }
            if (c & CU_GEM)
                cu_gem(px, py, col, ms, x + y * WW);
            else {
                int k, lit = 0;
                if (cu.land_at && ms - cu.land_at < 160u)
                    for (k = 0; k < 3; k++)
                        lit |= cu.land[k][0] == x && cu.land[k][1] == y;
                cube(px, py, lit ? mix565(col, C_INK, 230 - (int32_t)(ms - cu.land_at)) : col);
            }
        }
    for (x = 0; x < WW; x++)             /* the playhead's column, lit as it sounds */
        sq_col_glow(WX + x * CELL, WY, CELL, WH * CELL, cu.seq_ms[x], C_INK);
    if (!cu.over && cu.phase == 0)
        for (i = 0; i < cu.pn; i++) {
            int cx = cu.px + cu.pc[i][0], cy = cu.py + cu.pc[i][1];
            if (cy < 0)
                continue;
            if (cu.kind == CU_BOMB) {    /* the bomb: dark, round, its fuse sparking on the beat */
                uint32_t ph = (ms * cu_bpm() / 600u) % 100u;
                int32_t bx = WX + cx * CELL + CELL / 2, by = WY + cy * CELL + CELL / 2 + 1;
                disc(bx, by, 7 + (ph < 15u ? 1 : 0), ph < 15u ? RGB(255, 120, 40) : RGB(20, 14, 30));
                disc(bx, by, 6, RGB(60, 52, 80));
                disc(bx - 2, by - 2, 2, RGB(150, 140, 180));
                cv_line(bx + 2, by - 6, bx + 5, by - 9, RGB(200, 170, 120));
                disc(bx + 5, by - 9, ph < 15u ? 3 : 1, ph < 15u ? C_YEL : RGB(255, 120, 40));
            } else
                cube(WX + cx * CELL, WY + cy * CELL, CUBE_COL[cu.pcol[i]]);
        }
    if (cu.blast_at && ms - cu.blast_at < 400u) {   /* the blast's hole: a glow fading */
        int32_t g = 255 - (int32_t)(ms - cu.blast_at) * 255 / 400;
        frame_rect(WX + (cu.bx - 2) * CELL, WY + cu.by * CELL, 5 * CELL, 3 * CELL, mix565(C_BG, RGB(255, 170, 60), g));
    }
    /* the numbers */
    cv_text(202, WY, &FONT_T, "SCORE", C_GHOST);
    itos((int32_t)cu.score, n);
    cv_text(202, WY + 11, &FONT_XS, n, C_INK);
    cv_text(202, WY + 36, &FONT_T, "GEMS", C_GHOST);
    itos((int32_t)cu.gems, n);
    cv_text(202, WY + 47, &FONT_XS, n, C_CYAN);
    cv_text(202, WY + 72, &FONT_T, "LEVEL", C_GHOST);
    itos((int32_t)cu.level, n);
    cv_text(202, WY + 83, &FONT_XS, n, C_YEL);
    if (cu.bombs) {                      /* bombs to come */
        cv_text(202, WY + 104, &FONT_T, "BOMB", RGB(255, 170, 60));
        for (i = 0; i < cu.bombs; i++)
            disc(206 + i * 10, WY + 120, 3, RGB(255, 170, 60));
    }
    cv_text(202, WY + 130, &FONT_T, "UP", C_GHOST);
    cv_text(202, WY + 141, &FONT_T, "TURNS", C_PURP);
    cv_text(202, WY + 160, &FONT_T, "TOP", C_GHOST);
    cv_text(202, WY + 171, &FONT_T, "DROPS", C_PURP);
    cv_text(202, WY + 188, &FONT_T, "4 POP", C_GHOST);
    if (cu.over) {
        cv_rect(WX + 16, WY + 70, WW * CELL - 32, 56, C_BG);
        frame_rect(WX + 16, WY + 70, WW * CELL - 32, 56, C_YEL);
        text_c(WX + WW * CELL / 2, WY + 76, &FONT_M, "THE END", C_YEL);
        text_c(WX + WW * CELL / 2, WY + 104, &FONT_T, "TOP KEY: AGAIN   HOME: MENU", C_GHOST);
    }
}

#ifdef PLAT_HOST
static void cu_test_fill(int n)          /* (the host's test) n groups of four, apart, at the bottom: they pop */
{
    int y, x;
    for (y = 0; y < WH; y++)
        for (x = 0; x < WW; x++)
            cu.well[y][x] = 0;
    cu.gems = 99;                        /* (no level's end in the way) */
    for (x = 0; x < n && 2 * x < WW; x++)
        for (y = WH - 4; y < WH; y++)
            cu.well[y][2 * x] = (uint8_t)(1 + x % CU_NC);
    cu.chain = 0;
    cu_resolve();
}
#endif
