/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Nick Culbertson */
/* pool.h: POOL, a sequencer game (17 Nov 2026, the eighteenth). Included once from app.c, after seq.h; its state lives
 * in the shared arena.
 *
 *  Nine balls racked in a diamond, the cue ball below. C and E aim (held: faster), C# and D nudge it finely; hold A,
 *  the power pulses; let go to shoot. No fouls, no rules: clear the table, in as few shots as you can.
 *  The music: every knock is a note, landed on the next sixteenth (ball on ball, a chord tone; off a cushion, a wood
 *  knock). The six pockets are six notes of the chord, low at your end and high at the far one, each ringed in its
 *  colour: a ball sunk drops into the RACK, eight slots under the header that loop as a bar of eighths, playing the
 *  pocket's note, so the tune is where you sank them, each into a random slot (a full rack: a random one goes); a cleared table empties it. The more of the table you
 *  clear, the more of the drums come in. Two or more down in one shot: a combo. The cue ball down: a scratch, back on
 *  its spot, the combo gone. The table cleared: the next rack, quicker, the loop playing on.
 *  Knobs, as the other sequencer games: K1 TEMPO, K2 CHANCE, K3 SOUND, K4 EFFECTS, PRESET the scale, ALGORITHM the key */

enum { PO_CLACK, PO_RAIL, PO_POCKET, PO_WIN, PO_BASS, PO_HAT, PO_KICK, PO_SNARE, PO_PAD, PO_SEQ };
#define PO_NB 10                         /* the cue ball (0) and nine */
#define PO_R 5.0f                        /* a ball */
#define PO_SLOTS 8                       /* the rack: a bar of eighths */
#define PO_X0 18.0f                      /* the cloth, inside the cushions */
#define PO_X1 222.0f
#define PO_Y0 52.0f
#define PO_Y1 220.0f                      /* (was 230: the bottom pockets ran off the screen, Nick, 19 Nov 2026) */
#define PO_MOUTH 11.0f                   /* a pocket's mouth: the cushions stop this far from its middle */
#define PO_CAP 8.5f                      /* a ball whose middle comes this near a pocket's: down */
#define PO_VMAX 0.95f                    /* a full shot, px/ms */
#define PO_CUE_X 120.0f                  /* the cue ball's spot */
#define PO_CUE_Y 192.0f
static const int16_t PO_POCK[6][2] = {{16, 50}, {120, 47}, {224, 50}, {16, 222}, {120, 225}, {224, 222}};
static const uint8_t PO_PTONE[6] = {4, 5, 6, 1, 2, 3};   /* each pocket's chord tone (h_tone's): the far end higher */
static const uint16_t PO_BCOL[PO_NB] = {RGB(244, 242, 232), RGB(255, 210, 63), RGB(90, 140, 255), RGB(255, 70, 80),
                                        RGB(176, 124, 255), RGB(255, 150, 60), RGB(110, 240, 140), RGB(170, 60, 70),
                                        RGB(36, 32, 48), RGB(255, 210, 63)};   /* the 9: a yellow stripe */
typedef struct {
    float x[PO_NB], y[PO_NB], vx[PO_NB], vy[PO_NB];
    uint8_t on[PO_NB];
    float aim, charge;
    uint8_t phase;                       /* 0 aiming, 1 rolling, 2 the rack cleared (its card) */
    uint8_t charging, armed, scratched, potted, tier, rackn, left;
    uint8_t slot[PO_SLOTS], skind[PO_SLOTS], snext;   /* the rack: each slot's chord tone + 1 (0 empty), its ball */
    uint32_t seq_ms[PO_SLOTS], slot_ms[PO_SLOTS];
    uint32_t last, phase_at, clack_at, rail_at, hit_at, shot_at, intro_at, pock_at[6];
    uint16_t shots, best_shots;
    uint32_t score;
    float sink_x[4], sink_y[4];          /* balls dropping into a pocket (drawn shrinking) */
    uint8_t sink_k[4];
    uint32_t sink_at[4];
} po_t;
#define PO_PAR 6                         /* a rack in this many shots or fewer: par */
static uint32_t po_best;                 /* the fewest shots to clear a table */
#define po (*(po_t *)g_arena)
_Static_assert(sizeof(po_t) <= sizeof g_arena, "po: the arena is too small");

static void po_sounds(void)
{
    k_slot_set(PO_CLACK, &KSND[S_MARIMBA]);
    k_slot_set(PO_RAIL, &KSND[S_LOGDRUM]);
    k_slot_set(PO_POCKET, &KSND[S_CELESTA]);
    k_slot_set(PO_WIN, &KSND[S_TUBE]);
    k_slot_set(PO_BASS, &KSND[S_FMBASS]);
    k_slot_set(PO_HAT, &KSND[S_HAT]);
    k_slot_set(PO_KICK, &KSND[S_KICK]);
    k_slot_set(PO_SNARE, &KSND[S_SNARE]);
    k_slot_set(PO_PAD, &KSND[S_GLASSPAD]);
    k_slot_rel(PO_CLACK, 0.3f);
    k_slot_rel(PO_RAIL, 0.25f);
    k_slot_rel(PO_POCKET, 0.9f);
    k_slot_rel(PO_WIN, 1.2f);
    k_slot_rel(PO_BASS, 0.18f);
    k_slot_rel(PO_PAD, 1.6f);
    k_main_slot = PO_CLACK;
    k_main.rev_mix = 0.16f, k_main.rev_secs = 1.8f, k_main.shimmer = 0.03f, k_main.dly_mix = 0.1f;
    sq_slot = PO_SEQ;
    sq_sound(SQ_POOL);
}
static uint32_t po_bpm(void)             /* TEMPO (K1), a little quicker each rack */
{
    return (uint32_t)sq_cfg[SQ_POOL].bpm + (uint32_t)(po.rackn > 6 ? 6 : po.rackn) * 3u;
}
static void po_rack(void)                /* a new table: nine in a diamond, its point toward you; the cue on its spot */
{
    static const int8_t ROW[5] = {1, 2, 3, 2, 1};
    float dy = PO_R * 2.0f * 0.866f + 0.3f, cy = 112.0f;
    int r, k, n = 1, i;
    uint8_t order[9] = {1, 2, 3, 4, 9, 5, 6, 7, 8};   /* the 1 at the point, the 9 in the middle */
    static const uint8_t FREE[7] = {1, 2, 3, 5, 6, 7, 8};   /* (the rest shuffled round them) */
    for (i = 6; i > 0; i--) {
        uint8_t t;
        k = (int)((kr() >> 16) % (uint32_t)(i + 1));
        t = order[FREE[i]], order[FREE[i]] = order[FREE[k]], order[FREE[k]] = t;
    }
    for (r = 0; r < 5; r++)
        for (k = 0; k < ROW[r]; k++) {
            int b = order[n - 1];
            po.x[b] = 120.0f + ((float)k - (float)(ROW[r] - 1) * 0.5f) * (PO_R * 2.0f + 0.3f);
            po.y[b] = cy - dy * (float)r;
            po.vx[b] = po.vy[b] = 0.0f;
            po.on[b] = 1;
            n++;
        }
    po.x[0] = PO_CUE_X, po.y[0] = PO_CUE_Y, po.vx[0] = po.vy[0] = 0.0f, po.on[0] = 1;
    po.aim = -1.5707963f + 0.02f;
    po.phase = 0;
    po.charging = 0;
    po.armed = 0;
    po.left = 9;
    po.shots = 0;
    po.tier = 0;
    po.intro_at = plat_ms() | 1u;
    {
        char rs[12] = "RACK ";
        itos(po.rackn + 1, rs + 5);
        juice_pop(120, 120, rs, C_CYAN);   /* (the middle, as SWARM's wave: low, its slam fell toward the bottom) */
    }
}
static void po_new(void)
{
    int k;
    juice_reset();
    po.rackn = 0;
    po.score = 0;
    po.last = 0;
    for (k = 0; k < PO_SLOTS; k++)
        po.slot[k] = 0, po.seq_ms[k] = po.slot_ms[k] = 0;
    po.snext = 0;
    po_rack();
}
/* the music: the rack is the sequence. A playhead walks its eight slots, an eighth each, each playing its pocket's
 * note on the chord now (CHANCE how many sound); an empty slot rests. Under it a bass, and the drums coming in as the
 * table clears */
static void po_groove(uint32_t t, uint32_t step)
{
    uint32_t p = step % 16u, ms = sq_ms(t);
    int tier;
    h_deg_at(step / 16u);
    tier = sq_tier_of(9 - po.left, 9);
    if (tier < (po.rackn > 2 ? 2 : po.rackn))   /* a later rack starts with the kick */
        tier = po.rackn > 2 ? 2 : po.rackn;
    sq_tier_say(tier, &po.tier, 170);
    sq_drums(t, p, tier, PO_HAT, PO_KICK, PO_SNARE, 80);
    if (p == 0u || p == 6u || (p == 10u && tier >= 2))
        k_on(t, h_bass() + (p == 6u ? 7 : 0), 78, PO_BASS, 0.16f);
    if (p == 0u && step % 64u == 0u)
        h_pad(t, 30, PO_PAD, 3.0f, 0);
    if (!(p & 1u)) {                     /* the rack's slot, this eighth */
        int c = (int)(p / 2u);
        if (po.slot[c] && sq_roll(SQ_POOL)) {
            k_on(t, h_tone(po.slot[c] - 1 + (po.skind[c] > 4 ? 3 : 0)) + 12, 78u + (p % 8u == 0u ? 12u : 0u), PO_SEQ, 0.3f);
            po.seq_ms[c] = ms;
        }
    }
}

/* ------------------------------------------------------------- physics --- */
static float po_cushion(int b, float ax, float ay, float bx, float by)   /* ball b off a cushion: how hard (0 none) */
{
    float dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy, t, cx, cy, nx, ny, d, vn;
    t = l2 > 0.0f ? ((po.x[b] - ax) * dx + (po.y[b] - ay) * dy) / l2 : 0.0f;
    t = fm_clampf(t, 0.0f, 1.0f);
    cx = ax + dx * t, cy = ay + dy * t;
    nx = po.x[b] - cx, ny = po.y[b] - cy;
    d = nx * nx + ny * ny;
    if (d >= PO_R * PO_R || d < 0.0001f)
        return 0.0f;
    d = fm_sqrtf(d);
    nx /= d, ny /= d;
    po.x[b] = cx + nx * PO_R, po.y[b] = cy + ny * PO_R;
    vn = po.vx[b] * nx + po.vy[b] * ny;
    if (vn >= 0.0f)
        return 0.0f;
    po.vx[b] -= 1.78f * vn * nx;         /* (the cushion gives back most of it) */
    po.vy[b] -= 1.78f * vn * ny;
    return -vn;
}
static void po_rail_hit(int b, float hard)   /* a knock off the cushion: wood, low in the chord */
{
    uint32_t now = plat_ms();
    if (hard < 0.04f || now - po.rail_at < 50u)
        return;
    po.rail_at = now;
    k_on(grid_next(), h_note(1 + b % 3) - 12, (uint32_t)(50 + (int)fm_clampf(hard * 90.0f, 0.0f, 60.0f)), PO_RAIL, 0.25f);
    juice_burst((int32_t)po.x[b], (int32_t)po.y[b], 2 + (int32_t)(hard * 8.0f), 2, RGB(220, 180, 120), C_INK, 0);
    if (hard > 0.3f)
        juice_shake(1 + (int32_t)(hard * 4.0f), 100);
}
static void po_clack(int a, int b, float hard)   /* ball on ball: a chord tone, on the next sixteenth */
{
    uint32_t now = plat_ms();
    int k = a ? a : b;
    if (hard < 0.03f || now - po.clack_at < 45u)
        return;
    po.clack_at = now;
    po.hit_at = now | 1u;
    k_on(grid_next(), h_tone(1 + k % 5), (uint32_t)(60 + (int)fm_clampf(hard * 80.0f, 0.0f, 60.0f)), PO_CLACK, 0.3f);
    juice_burst((int32_t)(0.5f * (po.x[a] + po.x[b])), (int32_t)(0.5f * (po.y[a] + po.y[b])), 3 + (int32_t)(hard * 10.0f),
                2, RGB(255, 255, 255), PO_BCOL[k], 0);
    if (hard > 0.4f) {                   /* the break */
        juice_shake(2 + (int32_t)(hard * 6.0f), 200);
        juice_freeze(20);
    }
    juice_heat(2 + (int32_t)(hard * 6.0f));
}
static void po_sink(int b, int p)        /* ball b down pocket p */
{
    int f;
    uint32_t now = plat_ms();
    po.on[b] = 0;
    po.vx[b] = po.vy[b] = 0.0f;
    for (f = 0; f < 4 && po.sink_at[f]; f++)
        ;
    if (f < 4)
        po.sink_x[f] = po.x[b], po.sink_y[f] = po.y[b], po.sink_k[f] = (uint8_t)b, po.sink_at[f] = now | 1u;
    po.pock_at[p] = now | 1u;
    if (b == 0) {                        /* a scratch */
        po.scratched = 1;
        juice_text(PO_POCK[p][0] < 60 ? 50 : PO_POCK[p][0] > 180 ? 190 : 120, PO_POCK[p][1] < 100 ? 70 : 214,
                   "SCRATCH", C_PINK);
        k_on(grid_next(), degree_note(-5), 90, PO_RAIL, 0.4f);
        juice_shake(3, 200);
        J.heat /= 2;
        return;
    }
    {                                    /* into the rack: the pocket's note, at once in the loop */
        int s = seq_free_slot(po.slot, PO_SLOTS);
        uint32_t pts;
        char ps[12] = {'+', 0};
        po.slot[s] = (uint8_t)(PO_PTONE[p] + 1);
        po.skind[s] = (uint8_t)b;
        po.slot_ms[s] = now | 1u;
        po.potted++;
        po.left--;
        pts = 10u * po.potted * (1u + po.rackn);
        po.score += pts;
        k_on(grid_next(), h_tone(PO_PTONE[p]) + 12, 104, PO_POCKET, 0.8f);
        k_on(grid_next(), 36, 90, PO_KICK, 0.2f);
        juice_ring(PO_POCK[p][0], PO_POCK[p][1], 26, deg_col(PO_PTONE[p]), 300);
        juice_burst(PO_POCK[p][0], PO_POCK[p][1], 12, 3, PO_BCOL[b], deg_col(PO_PTONE[p]), 0);
        juice_shake(3, 160);
        juice_heat(8);
        (void)ps;                        /* (no points shown: POOL plays against par) */
        if (po.potted == 2)
            juice_pop(120, 120, "DOUBLE!", C_YEL);
        else if (po.potted == 3)
            juice_pop(120, 120, "TRIPLE!", C_YEL);
        else if (po.potted >= 4)
            juice_pop(120, 120, "ON FIRE!", C_PINK);
        if (po.potted >= 2)
            juice_sting(1);
    }
}
static void po_step(float h)             /* one small step of every ball rolling */
{
    int i, j, p;
    static const float DEC = 0.00019f;   /* the cloth: a steady drag */
    for (i = 0; i < PO_NB; i++) {
        float sp;
        if (!po.on[i])
            continue;
        sp = fm_sqrtf(po.vx[i] * po.vx[i] + po.vy[i] * po.vy[i]);
        if (sp > 0.0f) {
            float ns = sp - (DEC + sp * 0.0004f) * h;
            if (ns < 0.0f)
                ns = 0.0f;
            po.vx[i] *= ns / sp, po.vy[i] *= ns / sp;
        }
        po.x[i] += po.vx[i] * h;
        po.y[i] += po.vy[i] * h;
    }
    for (i = 0; i < PO_NB; i++)          /* ball on ball: equal weights, the push along the line between them */
        for (j = i + 1; j < PO_NB; j++) {
            float dx, dy, d2, d, nx, ny, vn, ov;
            if (!po.on[i] || !po.on[j])
                continue;
            dx = po.x[j] - po.x[i], dy = po.y[j] - po.y[i], d2 = dx * dx + dy * dy;
            if (d2 >= 4.0f * PO_R * PO_R || d2 < 0.0001f)
                continue;
            d = fm_sqrtf(d2);
            nx = dx / d, ny = dy / d;
            ov = 2.0f * PO_R - d;
            po.x[i] -= nx * ov * 0.5f, po.y[i] -= ny * ov * 0.5f;
            po.x[j] += nx * ov * 0.5f, po.y[j] += ny * ov * 0.5f;
            vn = (po.vx[j] - po.vx[i]) * nx + (po.vy[j] - po.vy[i]) * ny;
            if (vn >= 0.0f)
                continue;
            vn *= 0.97f;                 /* (a little lost to the click) */
            po.vx[i] += vn * nx, po.vy[i] += vn * ny;
            po.vx[j] -= vn * nx, po.vy[j] -= vn * ny;
            po_clack(i, j, -vn);
        }
    for (i = 0; i < PO_NB; i++) {        /* the pockets, then the cushions between them */
        if (!po.on[i])
            continue;
        for (p = 0; p < 6; p++) {
            float dx = po.x[i] - (float)PO_POCK[p][0], dy = po.y[i] - (float)PO_POCK[p][1];
            if (dx * dx + dy * dy < PO_CAP * PO_CAP) {
                po_sink(i, p);
                break;
            }
        }
        if (!po.on[i])
            continue;
        {
            float hard = 0.0f, M = PO_MOUTH, xm = 120.0f;
            hard += po_cushion(i, PO_X0 + M, PO_Y0, xm - M, PO_Y0);
            hard += po_cushion(i, xm + M, PO_Y0, PO_X1 - M, PO_Y0);
            hard += po_cushion(i, PO_X0 + M, PO_Y1, xm - M, PO_Y1);
            hard += po_cushion(i, xm + M, PO_Y1, PO_X1 - M, PO_Y1);
            hard += po_cushion(i, PO_X0, PO_Y0 + M, PO_X0, PO_Y1 - M);
            hard += po_cushion(i, PO_X1, PO_Y0 + M, PO_X1, PO_Y1 - M);
            po_rail_hit(i, hard);
        }
        if (po.x[i] < PO_X0 - 3.0f || po.x[i] > PO_X1 + 3.0f || po.y[i] < PO_Y0 - 3.0f || po.y[i] > PO_Y1 + 3.0f) {
            int best = 0;                /* through a mouth past the cushions: down the nearest pocket */
            float bd = 1e9f;
            for (p = 0; p < 6; p++) {
                float dx = po.x[i] - (float)PO_POCK[p][0], dy = po.y[i] - (float)PO_POCK[p][1];
                if (dx * dx + dy * dy < bd)
                    bd = dx * dx + dy * dy, best = p;
            }
            po_sink(i, best);
        }
    }
}
static int po_moving(void)
{
    int i;
    for (i = 0; i < PO_NB; i++)
        if (po.on[i] && po.vx[i] * po.vx[i] + po.vy[i] * po.vy[i] > 0.004f * 0.004f)
            return 1;
    return 0;
}
static void po_spot_cue(void)            /* the cue ball back on its spot (along the line, clear of the others) */
{
    float y;
    int i, ok;
    for (y = PO_CUE_Y; y > PO_Y0 + 10.0f; y -= 3.0f) {
        ok = 1;
        for (i = 1; i < PO_NB; i++)
            if (po.on[i] && (po.x[i] - PO_CUE_X) * (po.x[i] - PO_CUE_X) + (po.y[i] - y) * (po.y[i] - y) < 4.4f * PO_R * PO_R)
                ok = 0;
        if (ok)
            break;
    }
    po.x[0] = PO_CUE_X, po.y[0] = y, po.vx[0] = po.vy[0] = 0.0f, po.on[0] = 1;
    juice_ring((int32_t)po.x[0], (int32_t)po.y[0], 14, C_CYAN, 250);
}
static void po_shoot(void)
{
    float p = po.charge, sp = 0.08f + (PO_VMAX - 0.08f) * p * p;
    po.vx[0] = fm_cosf(po.aim) * sp;
    po.vy[0] = fm_sinf(po.aim) * sp;
    po.shots++;
    po.phase = 1;
    po.potted = 0;
    po.scratched = 0;
    po.shot_at = plat_ms();
    k_on(grid_next(), h_note(1) + (int)(p * 7.0f), (uint32_t)(70 + (int)(p * 40.0f)), PO_CLACK, 0.3f);
    juice_kick((int32_t)(fm_cosf(po.aim) * p * 5.0f), (int32_t)(fm_sinf(po.aim) * p * 5.0f));
    juice_ring((int32_t)po.x[0], (int32_t)po.y[0], 8 + (int32_t)(p * 16.0f), C_INK, 180);
    if (p > 0.97f) {
        juice_text((int32_t)po.x[0], (int32_t)po.y[0] - 12, "MAX!", C_YEL);
        juice_shake(3, 200);
    }
}
static void po_settled(void)             /* everything stopped: the cue ball back if it went down; the table cleared? */
{
    uint32_t now = plat_ms();
    if (po.scratched)
        po_spot_cue();
    else if (po.shots && !po.potted)
        J.heat = J.heat * 3 / 4;         /* (a miss cools it a little) */
    if (!po.left) {                      /* cleared */
        int k;
        po.phase = 2;
        po.phase_at = now;
        if (!po_best || po.shots < po_best)
            po_best = po.shots;
        juice_flash(RGB(255, 255, 255), 150, 500);
        juice_celebrate(120, 140);
        for (k = 0; k < 6; k++)
            k_on(k_at(0.08f * (float)k), h_tone(2 + k) + 12, 100, PO_WIN, 1.0f);
        juice_sting(2);
        return;
    }
    po.phase = 0;
    po.armed = 0;
}
static void po_nav(int dx, int dy, int sel)
{
    int k;
    (void)dx, (void)dy;
    if (sel && po.phase == 2 && plat_ms() - po.phase_at > 900u) {   /* the next rack */
        juice_calm();
        po.rackn++;
        for (k = 0; k < PO_SLOTS; k++)   /* (a fresh table, a fresh loop: Nick, 17 Nov 2026) */
            po.slot[k] = 0, po.seq_ms[k] = po.slot_ms[k] = 0;
        po_rack();
    }
}
static void po_tick(void)
{
    uint32_t dt = arc_dt(&po.last), now = plat_ms();
    int i, n, pull = HELD_S;
    for (i = 0; i < 4; i++)
        if (po.sink_at[i] && now - po.sink_at[i] > 300u)
            po.sink_at[i] = 0;
    if (po.phase == 0) {                 /* aiming: C, E turn it (faster, the longer held); C#, D finely; A pulses */
        static uint32_t held_since;
        int l = HELD_L, r = HELD_R, u = HELD_U, d = HELD_D;
        if (l || r) {
            float rate;
            if (!held_since)
                held_since = now;
            rate = now - held_since < 400u ? 0.0006f : 0.0022f;
            po.aim += (r ? rate : -rate) * (float)dt;
        } else
            held_since = 0;
        if (u || d)
            po.aim += (u ? 0.00008f : -0.00008f) * (float)dt;
        if (!pull)
            po.armed = 1;
        if (pull && po.armed && !po.charging) {
            po.charging = 1;
            po.phase_at = now;
        }
        if (po.charging) {
            uint32_t q = (now - po.phase_at) % 1600u;   /* up and down, 1.6 s round */
            po.charge = q < 800u ? (float)q / 800.0f : (float)(1600u - q) / 800.0f;
            if (!pull) {
                po.charging = 0;
                po_shoot();
            }
        }
    }
    if (po.phase == 1) {
        n = (int)(dt / 2u) + 1;
        for (i = 0; i < n; i++)
            po_step((float)dt / (float)n);
        if (!po_moving() || now - po.shot_at > 12000u) {
            for (i = 0; i < PO_NB; i++)
                po.vx[i] = po.vy[i] = 0.0f;
            po_settled();
        }
    }
}

/* ------------------------------------------------------------- drawing --- */
static void po_ball(int b, int32_t x, int32_t y, int32_t r)   /* a ball: its colour, a stripe for the 9, a shine */
{
    uint16_t c = PO_BCOL[b];
    disc(x + 1, y + 2, r, RGB(6, 40, 34));
    if (b == 9) {
        disc(x, y, r, RGB(244, 242, 232));
        cv_rect(x - r, y - 2, 2 * r + 1, 5, c);
        cv_rect(x - r + 1, y - 3, 2 * r - 1, 7, c);
    } else
        disc(x, y, r, c);
    if (b && r >= 4)
        disc(x, y, 1, b == 8 ? RGB(240, 240, 240) : RGB(250, 250, 245));
    cv_pset(x - 2, y - 2, RGB(255, 255, 255));
    cv_pset(x - 1, y - 3, RGB(255, 255, 255));
}
static void po_aim_line(void)            /* dots to what the cue ball meets first: a ball (its ghost, and where that
                                          * ball goes), or a cushion */
{
    float x = po.x[0], y = po.y[0], dx = fm_cosf(po.aim), dy = fm_sinf(po.aim), best = 400.0f, d;
    int hit = -1, i;
    uint32_t ms = plat_ms();
    for (i = 1; i < PO_NB; i++) {        /* the first ball along the ray: where the two would touch */
        float ox, oy, t, q;
        if (!po.on[i])
            continue;
        ox = po.x[i] - x, oy = po.y[i] - y;
        t = ox * dx + oy * dy;
        q = ox * ox + oy * oy - t * t;
        if (t > 0.0f && q < 4.0f * PO_R * PO_R) {
            float s = t - fm_sqrtf(4.0f * PO_R * PO_R - q);
            if (s < best)
                best = s, hit = i;
        }
    }
    {                                    /* or a cushion first */
        float tx = dx > 0.0f ? (PO_X1 - PO_R - x) / dx : dx < 0.0f ? (PO_X0 + PO_R - x) / dx : 1e9f;
        float ty = dy > 0.0f ? (PO_Y1 - PO_R - y) / dy : dy < 0.0f ? (PO_Y0 + PO_R - y) / dy : 1e9f;
        float tc = tx < ty ? tx : ty;
        if (tc < best)
            best = tc, hit = -1;
    }
    {
        float off = (float)(ms % 300u) / 300.0f * 6.0f;
        for (d = 8.0f + off; d < best; d += 6.0f)
            cv_rect((int32_t)(x + dx * d) - 1, (int32_t)(y + dy * d) - 1, 2, 2, C_INK);
    }
    {
        int32_t gx = (int32_t)(x + dx * best), gy = (int32_t)(y + dy * best), k;
        for (k = 0; k < 16; k++) {       /* the ghost ball: an outline */
            float th = (float)k * 0.3927f;
            cv_pset(gx + (int32_t)(fm_cosf(th) * PO_R), gy + (int32_t)(fm_sinf(th) * PO_R), C_INK);
        }
        if (hit > 0) {                   /* the object ball's way: along the line through the two */
            float nx = po.x[hit] - (float)gx, ny = po.y[hit] - (float)gy, l = fm_sqrtf(nx * nx + ny * ny);
            if (l > 0.1f)
                for (d = 8.0f; d < 30.0f; d += 4.0f)
                    cv_pset((int32_t)(po.x[hit] + nx / l * d), (int32_t)(po.y[hit] + ny / l * d), PO_BCOL[hit]);
        }
    }
}
static void po_scene(void)
{
    uint32_t ms = plat_ms();
    int i;
    char s[32], n[8];
    {                                    /* the room, the rails (wood, its diamonds), the cloth */
        int32_t y;
        cv_rect(0, 38, 240, 202, RGB(20, 14, 30));
        cv_rect(6, 40, 228, 192, RGB(70, 38, 24));
        cv_rect(8, 42, 224, 188, RGB(120, 70, 40));
        cv_rect(8, 42, 224, 2, RGB(170, 110, 65));
        for (y = (int32_t)PO_Y0 - 4; y < (int32_t)PO_Y1 + 4; y++)
            cv_rect((int32_t)PO_X0 - 4, y, (int32_t)(PO_X1 - PO_X0) + 8, 1,
                    ((y / 6) & 1) ? RGB(16, 104, 96) : RGB(14, 96, 90));
        cv_rect((int32_t)PO_X0 - 4, (int32_t)PO_Y0 - 4, (int32_t)(PO_X1 - PO_X0) + 8, 3, RGB(10, 70, 64));
        cv_rect((int32_t)PO_X0 - 4, (int32_t)PO_Y0 - 4, 3, (int32_t)(PO_Y1 - PO_Y0) + 8, RGB(10, 70, 64));
        for (i = 1; i < 8; i++) {        /* the diamonds on the rails */
            if (i == 4)
                continue;
            cv_rect(16 + i * 26, 44, 2, 2, RGB(240, 220, 170));
            cv_rect(16 + i * 26, 226, 2, 2, RGB(240, 220, 170));
        }
        for (i = 1; i < 4; i++) {
            cv_rect(10, 52 + i * 45, 2, 2, RGB(240, 220, 170));
            cv_rect(228, 52 + i * 45, 2, 2, RGB(240, 220, 170));
        }
        cv_rect((int32_t)PO_CUE_X, (int32_t)PO_CUE_Y + 7, 1, 1, RGB(120, 200, 190));   /* the spots */
        cv_rect(119, 120, 2, 2, RGB(30, 130, 120));
    }
    for (i = 0; i < 6; i++) {            /* the pockets, each ringed in its note's colour (lit as a ball drops) */
        uint32_t q = ms - po.pock_at[i];
        int lit = po.pock_at[i] && q < 400u;
        uint16_t c = deg_col(PO_PTONE[i]);
        disc(PO_POCK[i][0], PO_POCK[i][1], lit ? 11 : 10, lit ? C_INK : mix565(RGB(70, 38, 24), c, 150));
        disc(PO_POCK[i][0], PO_POCK[i][1], 8, RGB(6, 6, 10));
    }
    for (i = 0; i < 4; i++)              /* balls dropping: shrinking into the dark */
        if (po.sink_at[i]) {
            uint32_t q = ms - po.sink_at[i];
            int32_t r = 5 - (int32_t)(q / 60u);
            if (r > 0)
                disc((int32_t)po.sink_x[i], (int32_t)po.sink_y[i], r, PO_BCOL[po.sink_k[i]]);
        }
    if (po.phase == 0)
        po_aim_line();
    for (i = PO_NB - 1; i >= 0; i--)
        if (po.on[i])
            po_ball(i, (int32_t)(po.x[i] + 0.5f), (int32_t)(po.y[i] + 0.5f), (int32_t)PO_R);
    if (po.phase == 0) {                 /* the cue: drawn back as the power builds */
        float dx = fm_cosf(po.aim), dy = fm_sinf(po.aim), back = 8.0f + (po.charging ? po.charge * 18.0f : 0.0f);
        int32_t ax = (int32_t)(po.x[0] - dx * back), ay = (int32_t)(po.y[0] - dy * back);
        int32_t bx = (int32_t)(po.x[0] - dx * (back + 60.0f)), by = (int32_t)(po.y[0] - dy * (back + 60.0f));
        int32_t mx = (int32_t)(po.x[0] - dx * (back + 6.0f)), my = (int32_t)(po.y[0] - dy * (back + 6.0f));
        cv_line(ax + 1, ay + 1, bx + 1, by + 1, RGB(10, 50, 44));
        cv_line(ax, ay, bx, by, RGB(210, 170, 110));
        cv_line(ax + (dy > 0.0f ? 1 : 0), ay + (dx > 0.0f ? 0 : 1), mx, my, RGB(240, 240, 240));
        cv_line(ax, ay, ax - (int32_t)(dx * 2.0f), ay - (int32_t)(dy * 2.0f), RGB(90, 140, 255));
        if (po.charging) {
            int32_t px = (int32_t)po.x[0] - 14, py = (int32_t)po.y[0] + 9, w = (int32_t)(po.charge * 28.0f);
            uint16_t pc = po.charge < 0.5f ? RGB(120, 230, 120) : po.charge < 0.85f ? C_YEL : C_PINK;
            if (py > 222)
                py = (int32_t)po.y[0] - 14;
            cv_rect(px - 1, py - 1, 30, 6, RGB(10, 10, 14));
            cv_rect(px, py, w, 4, pc);
        }
    }
    {                                    /* the rack: eight slots under the header, lit as each one sounds */
        int c;
        for (c = 0; c < PO_SLOTS; c++) {
            int32_t x = 8 + c * 28, y = 24;
            uint32_t q = ms - po.seq_ms[c], q2 = ms - po.slot_ms[c];
            int lit = po.seq_ms[c] && (int32_t)q >= 0 && q < 160u;
            uint16_t col = po.slot[c] ? deg_col(po.slot[c] - 1) : C_LINE2;
            cv_rect(x, y, 26, 13, lit ? mix565(C_BG, col, 120) : RGB(18, 13, 32));
            frame_rect(x, y, 26, 13, po.slot[c] ? col : C_LINE2);
            if (po.slot[c]) {
                int32_t r = po.slot_ms[c] && q2 < 200u ? 6 : 4;
                po_ball(po.skind[c], x + 13, y + 6, lit ? r + 1 : r);
            }
        }
    }
    tcpy(s, "RACK ");
    itos(po.rackn + 1, n);
    tcat(s, n);
    tcat(s, "  SHOT ");
    itos(po.shots + (po.phase == 0 ? 1 : 0), n);
    tcat(s, n);
    tcat(s, " / PAR ");                  /* (no points: shots against par, as golf) */
    itos(PO_PAR, n);
    tcat(s, n);
    header("POOL", C_CYAN, "", C_PINK, s);
    if (po.phase == 2) {                 /* the table cleared: its card */
        cv_rect(36, 92, 168, 80, C_BG);
        frame_rect(36, 92, 168, 80, C_YEL);
        text_c(120, 98, &FONT_B, po.shots < PO_PAR ? "UNDER PAR!" : po.shots == PO_PAR ? "PAR!" : "TABLE CLEARED!",
               C_YEL);
        tcpy(s, "");
        itos(po.shots, n);
        tcat(s, n);
        tcat(s, po.shots == 1 ? " SHOT" : " SHOTS");
        if (po_best) {
            tcat(s, "   BEST ");
            itos((int32_t)po_best, n);
            tcat(s, n);
        }
        text_c(120, 124, &FONT_M, s, C_CYAN);
        text_c(120, 152, &FONT_T, "TOP KEY: NEXT RACK   HOME: MENU", C_GHOST);
    } else if (po.phase == 0 && !po.charging && po.shots == 0 && po.rackn == 0 && ms - po.intro_at > 1800u)
        text_c(120, 214, &FONT_T, "LEFT RIGHT: AIM   HOLD TOP KEY: SHOOT", RGB(170, 235, 225));
}
