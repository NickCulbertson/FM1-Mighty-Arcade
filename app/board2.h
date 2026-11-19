/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Nick Culbertson */
/* board2.h: CHECKERS and FOUR IN A ROW for Mighty Arcade's menu, against the computer (players 1: you are the first)
 * or each other (players 2: taking turns). The computer thinks a root move a frame (a node budget each), so nothing
 * stalls; DIFFICULTY (st.depth 1..4) sets how far ahead. Included once from app.c, after its helpers.
 *
 *  CHECKERS      the cursor and A: a piece, then where it lands (a jump of several in one: its last square). Taking is
 *                a must. The board is a score as in chess: a playhead sweeps it, men pluck, kings ring
 *  FOUR IN A ROW left and right pick a column, A drops; a column is a note, a row higher up the scale; four in a row,
 *                their chord. A sequencer too: a playhead walks the seven columns and a rest, an eighth each, and each
 *                plays its top disc (higher up, higher; yours and theirs an octave apart). The fuller, the more drums */

/* ============================================================== CHECKERS === */
enum { NC_MAN, NC_KING, NC_MOVE, NC_TAKE, NC_CROWN, NC_PAD };
typedef struct {
    int8_t b[8][8];                      /* 1 white man, 2 white king, -1 / -2 black; row 0 white's side */
    int8_t side;
    uint8_t quiet;                       /* plies without a capture */
} ckpos_t;
typedef struct {
    uint8_t from, n, to[10], ncap, cap[10];
} ckmove_t;
typedef struct {
    ckpos_t p;
    int8_t cur, sel;                     /* the cursor (r * 8 + c), the piece picked (-1 none) */
    uint8_t over;
    int8_t winner;                       /* 1 white, -1 black, 0 a draw */
    uint32_t over_at;
    int8_t last_from, last_to;
    uint32_t flash_t[64];
    /* the computer, thinking a root move a frame */
    ckmove_t rm[48];
    int rn, ri, rd, rbest, rscore, dbest, dscore;
    uint8_t thinking;
    /* the last move, played out: the piece slides (a step) or hops over what it takes, a hop at a time */
    ckmove_t an;
    int8_t an_pc, an_cv[10];             /* the piece moving; what each taken one was */
    uint32_t an_at;                      /* when it set off (ms), 0 none */
    uint8_t an_hit;                      /* the taken ones already burst (a bit each) */
} ck_t;
#define CK_HOP 0.17f                     /* a hop's time (s): each landing sounds as it lands */
#define ck (*(ck_t *)g_arena)   /* in the games' shared memory (app.c) */
_Static_assert(sizeof(ck_t) <= sizeof g_arena, "ck: the arena is too small");
static int ck_anim(void) { return ck.an_at && plat_ms() - ck.an_at < (uint32_t)(CK_HOP * 1000.0f) * ck.an.n + 60u; }

static int ck_dark(int r, int c) { return ((r + c) & 1) == 0; }
static void ck_reset(void)
{
    int r, c;
    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++)
            ck.p.b[r][c] = (int8_t)(!ck_dark(r, c) ? 0 : r < 3 ? 1 : r > 4 ? -1 : 0);
    ck.p.side = 1;
    ck.p.quiet = 0;
    ck.cur = 2 * 8 + 2;
    ck.sel = -1;
    ck.over = 0;
    ck.last_from = ck.last_to = -1;
    ck.thinking = 0;
    ck.an_at = 0;
}

/* the jumps on from (r, c) by piece pc, its path so far in m; each finished one goes into out */
static void ck_jumps(ckpos_t *p, int r, int c, int pc, ckmove_t *m, ckmove_t *out, int *n)
{
    static const int8_t DR[4] = {1, 1, -1, -1}, DC[4] = {1, -1, 1, -1};
    int d, found = 0, side = pc > 0 ? 1 : -1, king = pc == 2 || pc == -2;
    for (d = 0; d < 4; d++) {
        int mr = r + DR[d], mc = c + DC[d], lr = r + 2 * DR[d], lc = c + 2 * DC[d], k;
        if (!king && DR[d] != side)
            continue;
        if (lr < 0 || lr > 7 || lc < 0 || lc > 7 || p->b[lr][lc] || p->b[mr][mc] * side >= 0)
            continue;
        for (k = 0; k < m->ncap && m->cap[k] != mr * 8 + mc; k++)
            ;
        if (k < m->ncap || m->n >= 9)
            continue;
        found = 1;
        {
            int8_t taken = p->b[mr][mc];
            ckmove_t mm = *m;
            mm.to[mm.n++] = (uint8_t)(lr * 8 + lc);
            mm.cap[mm.ncap++] = (uint8_t)(mr * 8 + mc);
            p->b[mr][mc] = 0;
            p->b[r][c] = 0;
            p->b[lr][lc] = (int8_t)pc;
            if (!king && (lr == 7 || lr == 0)) {   /* crowned: the move ends there */
                if (*n < 48)
                    out[(*n)++] = mm;
            } else
                ck_jumps(p, lr, lc, pc, &mm, out, n);
            p->b[lr][lc] = 0;
            p->b[r][c] = (int8_t)pc;
            p->b[mr][mc] = taken;
        }
    }
    if (!found && m->ncap && *n < 48)
        out[(*n)++] = *m;
}
static int ck_gen(ckpos_t *p, ckmove_t *out)
{
    static const int8_t DR[4] = {1, 1, -1, -1}, DC[4] = {1, -1, 1, -1};
    int r, c, d, n = 0;
    for (r = 0; r < 8; r++)              /* jumps first: if there is one, it must be taken */
        for (c = 0; c < 8; c++)
            if (p->b[r][c] * p->side > 0) {
                ckmove_t m;
                m.from = (uint8_t)(r * 8 + c), m.n = 0, m.ncap = 0;
                ck_jumps(p, r, c, p->b[r][c], &m, out, &n);
            }
    if (n)
        return n;
    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++) {
            int pc = p->b[r][c];
            if (pc * p->side <= 0)
                continue;
            for (d = 0; d < 4; d++) {
                int tr = r + DR[d], tc = c + DC[d];
                if ((pc == 1 || pc == -1) && DR[d] != p->side)
                    continue;
                if (tr < 0 || tr > 7 || tc < 0 || tc > 7 || p->b[tr][tc] || n >= 48)
                    continue;
                out[n].from = (uint8_t)(r * 8 + c);
                out[n].n = 1;
                out[n].to[0] = (uint8_t)(tr * 8 + tc);
                out[n].ncap = 0;
                n++;
            }
        }
    return n;
}
static void ck_make(ckpos_t *p, const ckmove_t *m)
{
    int fr = m->from / 8, fc = m->from % 8, to = m->to[m->n - 1], tr = to / 8, tc = to % 8, k;
    int8_t pc = p->b[fr][fc];
    p->b[fr][fc] = 0;
    for (k = 0; k < m->ncap; k++)
        p->b[m->cap[k] / 8][m->cap[k] % 8] = 0;
    if (pc == 1 && tr == 7)
        pc = 2;
    if (pc == -1 && tr == 0)
        pc = -2;
    p->b[tr][tc] = pc;
    p->quiet = (uint8_t)(m->ncap ? 0 : p->quiet + 1);
    p->side = (int8_t)-p->side;
}
static int ck_eval(const ckpos_t *p)     /* the side to move's view */
{
    int r, c, e = 0;
    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++) {
            int pc = p->b[r][c];
            if (pc == 1)
                e += 100 + r * 4 + (c > 1 && c < 6 ? 3 : 0);
            else if (pc == 2)
                e += 165;
            else if (pc == -1)
                e -= 100 + (7 - r) * 4 + (c > 1 && c < 6 ? 3 : 0);
            else if (pc == -2)
                e -= 165;
        }
    return e * p->side;
}
static uint32_t ck_nodes;
static int ck_negamax(ckpos_t *p, int depth, int alpha, int beta)
{
    ckmove_t m[48];
    int n, i;
    ck_nodes++;
    if (depth <= 0 || ck_nodes > 4000u)
        return ck_eval(p);
    n = ck_gen(p, m);
    if (!n)
        return -10000 + (8 - depth);
    for (i = 0; i < n; i++) {
        ckpos_t q = *p;
        int sc;
        ck_make(&q, &m[i]);
        sc = -ck_negamax(&q, depth - 1 - (m[i].ncap && depth == 1 ? -1 : 0), -beta, -alpha);   /* a capture looks one
                                                                                               * further */
        if (sc > alpha)
            alpha = sc;
        if (alpha >= beta)
            break;
    }
    return alpha;
}

static int ck_your_turn(void) { return !ck.over && (players == 2 || ck.p.side > 0); }
static void ck_end_check(void)
{
    ckmove_t m[48];
    if (!ck_gen(&ck.p, m)) {
        ck.over = 1;
        ck.winner = (int8_t)-ck.p.side;
        ck.over_at = plat_ms();
    } else if (ck.p.quiet >= 80u) {
        ck.over = 1;
        ck.winner = 0;
        ck.over_at = plat_ms();
    }
}
static int ck_note(int sq, int pc)
{
    int r = sq / 8, rel = pc > 0 ? r : 7 - r;
    return degree_note(rel + (pc == 2 || pc == -2 ? 7 : 0)) - (pc < 0 ? 12 : 0);
}
static void ck_play(const ckmove_t *m)
{
    int8_t pc = ck.p.b[m->from / 8][m->from % 8];
    int k, to = m->to[m->n - 1];
    k_on(0, ck_note(m->from, pc) + 12, 90, NC_MOVE, 0.4f);
    for (k = 0; k < m->n; k++)           /* each landing a step up; each jump a swell under */
        k_on(k_at(CK_HOP * (float)(k + 1)), ck_note(m->to[k], pc) + 12, 100, NC_MOVE, 0.6f);
    for (k = 0; k < m->ncap; k++)
        k_on(k_at(CK_HOP * (float)(k + 1)), ck_note(m->cap[k], -pc) - 12, 80, NC_TAKE, 1.2f);
    ck.an = *m;                          /* played out on the board (ck_scene) */
    ck.an_pc = pc;
    for (k = 0; k < m->ncap; k++)
        ck.an_cv[k] = ck.p.b[m->cap[k] / 8][m->cap[k] % 8];
    ck.an_at = plat_ms() | 1u;
    ck.an_hit = 0;
    ck.last_from = (int8_t)m->from;
    ck.last_to = (int8_t)to;
    ck_make(&ck.p, m);
    if ((pc == 1 && to / 8 == 7) || (pc == -1 && to / 8 == 0))
        for (k = 0; k < 3; k++)
            k_on(k_at(0.3f + 0.1f * (float)k), degree_note(9 + 2 * k), 100, NC_CROWN, 1.0f);
    ck.sel = -1;
    ck_end_check();
    if (!ck.over && players == 1 && ck.p.side < 0) {   /* the computer's turn: it starts thinking */
        ck.rn = ck_gen(&ck.p, ck.rm);
        ck.ri = 0;
        ck.rd = 1;
        ck.rbest = 0;
        ck.dbest = -1;
        ck.dscore = -100000;
        ck.thinking = 1;
    }
}
/* how hard the computer plays (DIFFICULTY, st.depth 1..4): how far it looks ahead, and how often it makes a beginner's
 * mistake instead (a legal move at random). EASY and MEDIUM can be beaten by someone new to the game */
static const uint8_t BD_BLUNDER[5] = {0, 40, 15, 0, 0};   /* percent */
static int bd_blunder(void) { return kr() % 100u < BD_BLUNDER[st.depth & 3 ? st.depth : 4]; }
static void ck_tick(void)
{
    static const int8_t DEPTH[5] = {0, 1, 3, 5, 7};   /* plies: EASY sees only its own move */
    if (!ck.thinking || ck_anim())       /* (it waits for the last move to land) */
        return;
    {                                    /* a root move a frame */
        ckpos_t q = ck.p;
        int sc;
        ck_nodes = 0;
        ck_make(&q, &ck.rm[ck.ri]);
        sc = -ck_negamax(&q, ck.rd - 1, -100000, 100000) + (int)(kr() % 7u) - 3;
        if (sc > ck.dscore)
            ck.dscore = sc, ck.dbest = ck.ri;
        if (++ck.ri < ck.rn)
            return;
        ck.rbest = ck.dbest;
        if (ck.rd < DEPTH[st.depth & 3 ? st.depth : 4] && ck.rn > 1) {
            ck.rd += 2;
            ck.ri = 0;
            ck.dbest = -1;
            ck.dscore = -100000;
            return;
        }
    }
    ck.thinking = 0;
    if (ck.rn > 1 && bd_blunder())       /* a beginner's move */
        ck.rbest = (int)(kr() % (uint32_t)ck.rn);
    if (ck.rbest >= 0 && ck.rbest < ck.rn)
        ck_play(&ck.rm[ck.rbest]);
}
static void ck_new(void) { ck_reset(); }
static void ck_nav(int dx, int dy, int sel)
{
    ckmove_t m[48];
    int n, i;
    if (ck.over) {
        if (sel && plat_ms() - ck.over_at > 800u)
            ck_new();
        return;
    }
    if (dx || dy) {
        int r = clamp(ck.cur / 8 + dy, 0, 7), c = clamp(ck.cur % 8 + dx, 0, 7);
        ck.cur = (int8_t)(r * 8 + c);
        return;
    }
    if (!sel || !ck_your_turn() || ck.thinking || ck_anim())
        return;
    n = ck_gen(&ck.p, m);
    if (ck.sel >= 0) {
        for (i = 0; i < n; i++)
            if (m[i].from == ck.sel && m[i].to[m[i].n - 1] == ck.cur) {
                ck_play(&m[i]);
                return;
            }
        if (ck.cur == ck.sel) {
            ck.sel = -1;
            return;
        }
    }
    for (i = 0; i < n && m[i].from != ck.cur; i++)
        ;
    if (i < n) {
        ck.sel = ck.cur;
        k_on(0, ck_note(ck.cur, ck.p.b[ck.cur / 8][ck.cur % 8]) + 12, 70, NC_MOVE, 0.3f);
    } else {
        app_say(ck.sel >= 0 ? "IT CANNOT GO" : "PICK ONE", ck.sel >= 0 ? "THERE" : "THAT CAN MOVE");
        k_on(0, 41, 70, NC_TAKE, 0.1f);
    }
}
/* the score: a playhead along the columns, an eighth note each; each piece it passes may sound */
static void ck_groove(uint32_t t, uint32_t step)
{
    int c = (int)((step / 2u) % 8u), r;
    if (step % 2u)
        return;
    if (step % 64u == 0u)
        k_on(t, degree_note(-7), 40, NC_PAD, 3.0f);
    for (r = 0; r < 8; r++) {
        int pc = ck.p.b[r][c];
        if (pc && kr() % 100u < 30u) {
            k_on(t, ck_note(r * 8 + c, pc) + 12, 64, pc == 2 || pc == -2 ? NC_KING : NC_MAN, 0.4f);
            ck.flash_t[r * 8 + c] = t;
        }
    }
}
static void ck_sounds(void)
{
    k_slot_set(NC_MAN, &KSND[S_NYLON]);
    k_slot_set(NC_KING, &KSND[S_VIBES]);
    k_slot_set(NC_MOVE, &KSND[S_MARIMBA]);
    k_slot_set(NC_TAKE, &KSND[S_BRASSPAD]);
    k_slot_set(NC_CROWN, &KSND[S_TUBE]);
    k_slot_set(NC_PAD, &KSND[S_GLASSPAD]);
    k_slot_rel(NC_MAN, 0.4f);
    k_slot_rel(NC_KING, 0.6f);
    k_slot_rel(NC_TAKE, 1.0f);
    k_slot_rel(NC_CROWN, 1.0f);
    k_slot_rel(NC_PAD, 1.4f);
    k_main_slot = NC_MOVE;
}
static void ck_piece(int32_t x, int32_t y, int pc)   /* a piece centred at x, y */
{
    uint16_t pcol = pc > 0 ? C_CYAN : C_PINK;
    disc(x, y + 1, 9, RGB(4, 2, 10));
    disc(x, y, 9, pcol);
    disc(x, y, 6, mix565(pcol, C_BG, 70));
    if (pc == 2 || pc == -2)             /* a king: crowned */
        disc(x, y, 3, C_YEL);
}
#define CK_X(sq) (4 + ((sq) % 8) * 24 + 12)
#define CK_Y(sq) (25 + (7 - (sq) / 8) * 24 + 12)
static void ck_scene(void)
{
    int r, c;
    ckmove_t m[48];
    int n = ck_your_turn() && !ck.thinking ? ck_gen(&ck.p, m) : 0, i;
    uint32_t now = k_now();
    int anim = ck_anim(), hop = 0;
    int32_t hf = 0;                      /* the hop under way, how far through (0..256) */
    if (anim) {
        uint32_t q = plat_ms() - ck.an_at, hm = (uint32_t)(CK_HOP * 1000.0f);
        hop = (int)(q / hm);
        hf = (int32_t)((q % hm) * 256u / hm);
        if (hop >= ck.an.n)
            hop = ck.an.n - 1, hf = 256;
    }
    const char *who = ck.over ? "" : players == 2 ? (ck.p.side > 0 ? "P1 (CYAN)" : "P2 (PINK)")
                                                  : ck.p.side > 0 ? "YOUR MOVE" : "THINKING";
    header("CHECKERS", C_PINK, "", C_PINK, who);
    for (r = 0; r < 8; r++)
        for (c = 0; c < 8; c++) {
            int32_t x = 4 + c * 24, y = 25 + (7 - r) * 24, sq = r * 8 + c, pc = ck.p.b[r][c];
            uint16_t col = ck_dark(r, c) ? RGB(40, 30, 68) : RGB(24, 17, 42);
            uint32_t ft = ck.flash_t[sq];
            if (sq == ck.last_from || sq == ck.last_to)
                col = mix565(col, C_YEL, 60);
            if (sq == ck.sel)
                col = mix565(col, C_CYAN, 170);
            if (ft && (int32_t)(now - ft) >= 0 && now - ft < 12000u)
                col = mix565(col, pc > 0 ? C_CYAN : C_PINK, 180 - (int32_t)((now - ft) / 70u));
            cv_rect(x, y, 24, 24, col);
            if (pc && !(anim && sq == ck.an.to[ck.an.n - 1]))   /* (the moving one: drawn in flight) */
                ck_piece(x + 12, y + 12, pc);
        }
    if (anim) {                          /* the move, played out */
        int k, from = hop ? ck.an.to[hop - 1] : ck.an.from, to = ck.an.to[hop];
        int jump = ck.an.ncap > 0;
        int32_t x0 = CK_X(from), y0 = CK_Y(from), x1 = CK_X(to), y1 = CK_Y(to);
        int32_t mx = x0 + (x1 - x0) * hf / 256, my = y0 + (y1 - y0) * hf / 256;
        int32_t lift = (jump ? 18 : 5) * 4 * hf * (256 - hf) / (256 * 256);
        for (k = 0; k < ck.an.ncap; k++) {   /* the taken: there until the jumper is over them */
            int sq = ck.an.cap[k];
            if (k > hop || (k == hop && hf < 128))
                ck_piece(CK_X(sq), CK_Y(sq), ck.an_cv[k]);
            else if (!(ck.an_hit >> k & 1u)) {   /* gone: a burst where it was */
                uint16_t c = ck.an_cv[k] > 0 ? C_CYAN : C_PINK;
                ck.an_hit |= (uint8_t)(1u << k);
                juice_burst(CK_X(sq), CK_Y(sq), 14, 3, c, C_YEL, 0);
                juice_ring(CK_X(sq), CK_Y(sq), 22, c, 260);
                juice_shake(2 + k, 160);
            }
        }
        disc(mx, my + 3, 8 - lift / 4, RGB(8, 5, 16));   /* its shadow on the board */
        ck_piece(mx, my - lift, ck.an_pc);
    }
    for (i = 0; i < n; i++)              /* where the picked piece can land */
        if (m[i].from == ck.sel) {
            int to = m[i].to[m[i].n - 1];
            disc(4 + (to % 8) * 24 + 12, 25 + (7 - to / 8) * 24 + 12, 3, C_YEL);
        }
    frame_rect(4 + (ck.cur % 8) * 24, 25 + (7 - ck.cur / 8) * 24, 24, 24, C_YEL);
    frame_rect(5 + (ck.cur % 8) * 24, 26 + (7 - ck.cur / 8) * 24, 22, 22, C_YEL);
    {                                    /* the pieces left */
        int w = 0, b = 0;
        char t[8];
        for (r = 0; r < 64; r++)
            if (ck.p.b[r / 8][r % 8])
                *(ck.p.b[r / 8][r % 8] > 0 ? &w : &b) += 1;
        cv_text(204, 30, &FONT_T, "LEFT", C_GHOST);
        itos(w, t);
        cv_text(204, 42, &FONT_M, t, C_CYAN);
        itos(b, t);
        cv_text(204, 70, &FONT_M, t, C_PINK);
        if (players == 1) {
            cv_text(204, 120, &FONT_T, "LEVEL", C_GHOST);
            cv_text(204, 132, &FONT_T, DIFF_NAME[st.depth], C_YEL);
        }
    }
    if (ck.over) {
        cv_rect(30, 92, 160, 50, C_BG);
        frame_rect(30, 92, 160, 50, C_YEL);
        text_c(110, 96, &FONT_M, ck.winner == 0 ? "A DRAW" : players == 1 ? (ck.winner > 0 ? "YOU WIN" : "I WIN")
                                                 : ck.winner > 0 ? "P1 WINS" : "P2 WINS", C_YEL);
        text_c(110, 124, &FONT_T, "TOP KEY: AGAIN   HOME: MENU", C_GHOST);
    }
    text_c(120, 224, &FONT_T, ck.sel >= 0 ? "WHERE TO? MOVE, THEN TOP KEY" : "PICK A PIECE: MOVE, THEN TOP KEY", C_GHOST);
}

/* ========================================================== FOUR IN A ROW === */
enum { NF_P1, NF_P2, NF_WIN, NF_PAD, NF_HAT, NF_SEQ, NF_KICK, NF_SNARE };   /* NF_SEQ: the playhead's */
#define F_COLS 7
#define F_ROWS 6
#define F_C 30
#define F_X 15
#define F_Y 52
typedef struct {
    int8_t b[F_ROWS][F_COLS];            /* 1 the first player, -1 the second (or the computer); row 0 the bottom */
    int8_t side, col;
    uint8_t over;
    int8_t winner;
    uint32_t over_at;
    int8_t win[4][2];                    /* the four, to light */
    int8_t drop_c, drop_r;               /* the disc falling (its square), -1 none */
    uint32_t drop_at;                    /* (ms) */
    uint8_t landed;                      /* the falling disc has landed (its thud's juice done) */
    float hx;                            /* the waiting disc's x, gliding to its column */
    int8_t last_c, last_r;
    /* the computer */
    uint8_t thinking;
    int ri, rd, best, bscore, dbest, dscore;
    uint32_t seq_ms[F_COLS];             /* when the playhead's note in each column last sounded */
    uint8_t tier, discs;
} fr_t;
#define fr (*(fr_t *)g_arena)   /* in the games' shared memory (app.c) */
_Static_assert(sizeof(fr_t) <= sizeof g_arena, "fr: the arena is too small");
static int f_top(int c)                  /* the row a disc dropped in c lands on (-1 full) */
{
    int r;
    for (r = 0; r < F_ROWS; r++)
        if (!fr.b[r][c])
            return r;
    return -1;
}
static int f_four(int8_t b[F_ROWS][F_COLS], int r, int c, int8_t out[4][2])   /* the four through (r, c), if any */
{
    static const int8_t DR[4] = {0, 1, 1, 1}, DC[4] = {1, 0, 1, -1};
    int d, s = b[r][c];
    for (d = 0; d < 4; d++) {
        int n = 1, k, r0 = r, c0 = c;
        for (k = 1; k < 4; k++) {
            int rr = r + DR[d] * k, cc = c + DC[d] * k;
            if (rr < 0 || rr >= F_ROWS || cc < 0 || cc >= F_COLS || b[rr][cc] != s)
                break;
            n++;
        }
        for (k = 1; k < 4; k++) {
            int rr = r - DR[d] * k, cc = c - DC[d] * k;
            if (rr < 0 || rr >= F_ROWS || cc < 0 || cc >= F_COLS || b[rr][cc] != s)
                break;
            n++, r0 = rr, c0 = cc;
        }
        if (n >= 4) {
            if (out)
                for (k = 0; k < 4; k++)
                    out[k][0] = (int8_t)(r0 + DR[d] * k), out[k][1] = (int8_t)(c0 + DC[d] * k);
            return 1;
        }
    }
    return 0;
}
static int f_eval(int8_t b[F_ROWS][F_COLS], int side)   /* windows of four: how open, for side */
{
    int r, c, d, e = 0;
    static const int8_t DR[4] = {0, 1, 1, 1}, DC[4] = {1, 0, 1, -1};
    for (r = 0; r < F_ROWS; r++)
        for (c = 0; c < F_COLS; c++)
            for (d = 0; d < 4; d++) {
                int k, mine = 0, theirs = 0;
                int r3 = r + 3 * DR[d], c3 = c + 3 * DC[d];
                if (r3 < 0 || r3 >= F_ROWS || c3 < 0 || c3 >= F_COLS)
                    continue;
                for (k = 0; k < 4; k++) {
                    int v = b[r + k * DR[d]][c + k * DC[d]];
                    mine += v == side, theirs += v == -side;
                }
                if (!theirs)
                    e += mine == 3 ? 50 : mine == 2 ? 8 : mine;
                if (!mine)
                    e -= theirs == 3 ? 60 : theirs == 2 ? 8 : theirs;
            }
    for (r = 0; r < F_ROWS; r++)         /* the middle column is worth more */
        e += (b[r][3] == side) * 4;
    return e;
}
static uint32_t f_nodes;
static int f_negamax(int8_t b[F_ROWS][F_COLS], int side, int depth, int alpha, int beta)
{
    static const int8_t ORD[7] = {3, 2, 4, 1, 5, 0, 6};
    int i, any = 0;
    f_nodes++;
    if (depth <= 0 || f_nodes > 6000u)
        return f_eval(b, side);
    for (i = 0; i < 7; i++) {
        int c = ORD[i], r, sc;
        for (r = 0; r < F_ROWS && b[r][c]; r++)
            ;
        if (r == F_ROWS)
            continue;
        any = 1;
        b[r][c] = (int8_t)side;
        sc = f_four(b, r, c, 0) ? 10000 + depth : -f_negamax(b, -side, depth - 1, -beta, -alpha);
        b[r][c] = 0;
        if (sc > alpha)
            alpha = sc;
        if (alpha >= beta)
            break;
    }
    return any ? alpha : 0;
}
static void f_new(void)
{
    int r, c;
    for (r = 0; r < F_ROWS; r++)
        for (c = 0; c < F_COLS; c++)
            fr.b[r][c] = 0;
    fr.side = 1;
    fr.col = 3;
    fr.over = 0;
    fr.drop_c = -1;
    fr.landed = 1;
    fr.hx = 0.0f;
    fr.last_c = -1;
    fr.thinking = 0;
    fr.tier = 0;
    for (c = 0; c < F_COLS; c++)
        fr.seq_ms[c] = 0;
}
#define F_G 0.0022f                      /* the disc's fall: px/ms/ms, from where it waits */
static uint32_t f_fall_ms(int r)         /* how long a disc takes to fall to row r */
{
    float d = (float)(F_Y + (F_ROWS - 1 - r) * F_C + F_C / 2 - 36);
    return (uint32_t)fm_sqrtf(2.0f * d / F_G);
}
static void f_drop(int c)
{
    int r = f_top(c), k, full = 1;
    if (r < 0)
        return;
    fr.b[r][c] = fr.side;
    fr.drop_c = (int8_t)c, fr.drop_r = (int8_t)r;
    fr.drop_at = plat_ms();
    fr.landed = 0;
    fr.last_c = (int8_t)c, fr.last_r = (int8_t)r;
    {                                    /* it sounds as it lands: its column a note, its row higher up the scale */
        float fall = (float)f_fall_ms(r) * 0.001f;
        k_on(k_at(fall), degree_note(c + 2 * r + 7) - (fr.side < 0 ? 12 : 0), 100, fr.side > 0 ? NF_P1 : NF_P2, 0.6f);
    }
    if (f_four(fr.b, r, c, fr.win)) {
        fr.over = 1;
        fr.winner = fr.side;
        fr.over_at = plat_ms();
        for (k = 0; k < 4; k++)
            k_on(k_at(0.4f + 0.08f * (float)k), degree_note(fr.win[k][1] + 2 * fr.win[k][0] + 7), 100, NF_WIN, 1.2f);
        return;
    }
    for (k = 0; k < F_COLS; k++)
        if (f_top(k) >= 0)
            full = 0;
    if (full) {
        fr.over = 1;
        fr.winner = 0;
        fr.over_at = plat_ms();
        return;
    }
    fr.side = (int8_t)-fr.side;
    if (players == 1 && fr.side < 0) {
        fr.thinking = 1;
        fr.ri = 0;
        fr.rd = st.depth <= 1 ? 1 : 2;   /* (EASY: only its own move) */
        fr.best = 3;
        fr.dbest = -1;
        fr.dscore = -1000000;
    }
}
static void f_tick(void)
{
    static const int8_t ORD[7] = {3, 2, 4, 1, 5, 0, 6};
    static const int8_t DEPTH[5] = {0, 1, 2, 4, 8};   /* plies: EASY sees only its own move */
    int c, r, sc;
    if (fr.drop_c >= 0 && !fr.landed && plat_ms() - fr.drop_at >= f_fall_ms(fr.drop_r)) {   /* the thud (Nick, 19
                                          * Nov 2026: a drop with juice) */
        int32_t x = F_X + fr.drop_c * F_C + F_C / 2, y = F_Y + (F_ROWS - 1 - fr.drop_r) * F_C + F_C / 2;
        uint16_t col = fr.b[fr.drop_r][fr.drop_c] > 0 ? C_CYAN : C_PINK;
        fr.landed = 1;
        juice_shake(2 + (F_ROWS - fr.drop_r) / 2, 160);
        juice_kick(0, 2 + (F_ROWS - fr.drop_r) / 2);
        juice_burst(x, y + 10, 10, 2, col, C_INK, 0);
        juice_ring(x, y, 24, col, 260);
        if (fr.over && fr.winner) {      /* four in a row: the line flashes, confetti */
            juice_flash(RGB(255, 255, 255), 140, 300);
            juice_shake(7, 450);
            juice_confetti(120, 110, 50);
            juice_pop(120, 72, players == 1 ? (fr.winner > 0 ? "FOUR!" : "OOPS") : "FOUR!", C_YEL);
        }
    }
    if (!fr.thinking || (fr.drop_c >= 0 && plat_ms() - fr.drop_at < f_fall_ms(fr.drop_r) + 200u))
        return;
    c = ORD[fr.ri];                      /* a column a frame */
    r = f_top(c);
    if (r >= 0) {
        f_nodes = 0;
        fr.b[r][c] = -1;
        sc = f_four(fr.b, r, c, 0) ? 100000 : -f_negamax(fr.b, 1, fr.rd - 1, -1000000, 1000000);
        fr.b[r][c] = 0;
        sc += (int)(kr() % 5u) - 2;
        if (sc > fr.dscore)
            fr.dscore = sc, fr.dbest = c;
    }
    if (++fr.ri < 7)
        return;
    fr.best = fr.dbest;
    if (fr.rd < DEPTH[st.depth & 3 ? st.depth : 4] && fr.dscore < 50000) {
        fr.rd += 2;
        fr.ri = 0;
        fr.dbest = -1;
        fr.dscore = -1000000;
        return;
    }
    fr.thinking = 0;
    if (bd_blunder()) {                  /* a beginner's move: any column with room */
        int tries;
        for (tries = 0; tries < 20; tries++) {
            int c = (int)(kr() % (uint32_t)F_COLS);
            if (f_top(c) >= 0) {
                fr.best = c;
                break;
            }
        }
    }
    if (fr.best >= 0)
        f_drop(fr.best);
}
static void f_nav(int dx, int dy, int sel)
{
    (void)dy;
    if (fr.over) {
        if (sel && plat_ms() - fr.over_at > 800u)
            f_new();
        return;
    }
    if (dx) {
        fr.col = (int8_t)clamp(fr.col + dx, 0, F_COLS - 1);
        k_on(0, degree_note(fr.col + 7), 40, NF_HAT, 0.05f);
    }
    if (sel && !fr.thinking && (players == 2 || fr.side > 0)) {
        if (f_top(fr.col) < 0)
            app_say("THAT ONE", "IS FULL");
        else
            f_drop(fr.col);
    }
}
static void f_groove(uint32_t t, uint32_t step)
{
    uint32_t p = step % 16u;
    int tier, n = 0, r, c;
    if (fr.over)
        return;
    if (step % 32u == 0u) {              /* a new chord, at random in the key */
        h_deg_at(step / 32u);
        h_pad(t, 30, NF_PAD, 3.0f, 0);
    }
    for (r = 0; r < F_ROWS; r++)
        for (c = 0; c < F_COLS; c++)
            n += fr.b[r][c] != 0;
    tier = sq_tier_of(n, 30);            /* the drums, as the board fills */
    sq_tier_say(tier, &fr.tier, 228);
    sq_drums(t, p, tier, NF_HAT, NF_KICK, NF_SNARE, 75);
    if (!(p & 1u) && p / 2u < (uint32_t)F_COLS) {   /* the playhead: seven columns and a rest, an eighth each */
        c = (int)(p / 2u);
        for (r = F_ROWS - 1; r >= 0 && !fr.b[r][c]; r--)
            ;
        if (r >= 0 && sq_roll(SQ_FOUR)) {
            k_on(t, h_tone(r + 2) - (fr.b[r][c] < 0 ? 12 : 0), fr.b[r][c] > 0 ? 84u : 74u, NF_SEQ, 0.4f);
            fr.seq_ms[c] = sq_ms(t);
        }
    }
}
static void f_sounds(void)
{
    k_slot_set(NF_P1, &KSND[S_MARIMBA]);
    k_slot_set(NF_P2, &KSND[S_CELESTA]);
    k_slot_set(NF_WIN, &KSND[S_TUBE]);
    k_slot_set(NF_PAD, &KSND[S_GLASSPAD]);
    k_slot_set(NF_HAT, &KSND[S_HAT]);
    k_slot_set(NF_KICK, &KSND[S_KICK]);
    k_slot_set(NF_SNARE, &KSND[S_SNARE]);
    sq_slot = NF_SEQ;
    sq_sound(SQ_FOUR);
    k_slot_rel(NF_P2, 0.6f);
    k_slot_rel(NF_WIN, 1.2f);
    k_slot_rel(NF_PAD, 1.4f);
    k_main_slot = NF_P1;
}
static void f_scene(void)
{
    int r, c, k;
    uint32_t ms = plat_ms();
    const char *who = fr.over ? "" : players == 2 ? (fr.side > 0 ? "P1 (CYAN)" : "P2 (PINK)")
                                                  : fr.side > 0 ? DIFF_NAME[st.depth] : "THINKING";
    header("ROWS", C_CYAN, "", C_PINK, who);
    if (!fr.over) {                      /* the disc about to drop, gliding over its column, bobbing */
        uint16_t pc = fr.side > 0 ? C_CYAN : C_PINK;
        float tx = (float)(F_X + fr.col * F_C + F_C / 2);
        if (fr.hx < 1.0f)
            fr.hx = tx;
        fr.hx += (tx - fr.hx) * 0.35f;
        if (fr.drop_c < 0 || fr.landed)
            disc((int32_t)fr.hx, 36 + (int32_t)(fm_sinf((float)ms * 0.006f) * 2.0f), 11,
                 players == 1 && fr.side < 0 ? C_GHOST : pc);
    }
    cv_rect(F_X - 4, F_Y - 4, F_COLS * F_C + 8, F_ROWS * F_C + 8, RGB(40, 30, 90));
    for (r = 0; r < F_ROWS; r++)
        for (c = 0; c < F_COLS; c++) {
            int32_t x = F_X + c * F_C + F_C / 2, y = F_Y + (F_ROWS - 1 - r) * F_C + F_C / 2;
            int v = fr.b[r][c], lit = 0;
            if (c == fr.drop_c && r == fr.drop_r && ms - fr.drop_at < f_fall_ms(r) + 140u) {   /* (drawn below, over
                                          * the board) */
                disc(x, y, 12, C_BG);
                continue;
            }
            for (k = 0; fr.over && fr.winner && k < 4; k++)
                if (fr.win[k][0] == r && fr.win[k][1] == c)
                    lit = 1;
            disc(x, y, 12, v > 0 ? C_CYAN : v < 0 ? C_PINK : C_BG);
            if (lit && (ms / 150u) & 1u)
                disc(x, y, 6, C_YEL);
            if (c == fr.last_c && r == fr.last_r && !fr.over)
                disc(x, y, 3, C_INK);
        }
    if (fr.drop_c >= 0 && ms - fr.drop_at < f_fall_ms(fr.drop_r) + 140u) {   /* the disc falling, then a hop */
        int32_t x = F_X + fr.drop_c * F_C + F_C / 2, y = F_Y + (F_ROWS - 1 - fr.drop_r) * F_C + F_C / 2;
        uint32_t q = ms - fr.drop_at, fall = f_fall_ms(fr.drop_r);
        uint16_t dc = fr.b[fr.drop_r][fr.drop_c] > 0 ? C_CYAN : C_PINK;
        if (q < fall) {                  /* falling: faster and faster, a trail behind */
            float t = (float)q, yy = 36.0f + 0.5f * F_G * t * t, sp = F_G * t;
            int j;
            for (j = 3; j >= 1; j--)
                disc(x, (int32_t)(yy - sp * 12.0f * (float)j), 12 - 3 * j, mix565(C_BG, dc, 150 - 40 * j));
            disc(x, (int32_t)yy, 12, dc);
        } else {                         /* landed: a little hop, flashing white */
            float hq = (float)(q - fall) / 140.0f;
            int32_t hop = (int32_t)(fm_sinf(hq * 3.1415927f) * 5.0f);
            disc(x, y - hop, 12, mix565(dc, C_INK, (int32_t)(160.0f * (1.0f - hq))));
        }
    }
    if (fr.over && fr.landed) {          /* (once the last disc has landed) */
        cv_rect(40, 100, 160, 44, C_BG);
        frame_rect(40, 100, 160, 44, C_YEL);
        text_c(120, 102, &FONT_M, fr.winner == 0 ? "A DRAW" : players == 1 ? (fr.winner > 0 ? "YOU WIN" : "I WIN")
                                                 : fr.winner > 0 ? "P1 WINS" : "P2 WINS", C_YEL);
        text_c(120, 128, &FONT_T, "TOP KEY: AGAIN   HOME: MENU", C_GHOST);
    }
    {                                    /* the playhead's column, lit as it sounds */
        int c2;
        for (c2 = 0; c2 < F_COLS; c2++)
            sq_col_glow(F_X + c2 * F_C, F_Y - 2, F_C, F_ROWS * F_C + 4, fr.seq_ms[c2], C_YEL);
    }
}
