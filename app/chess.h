/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Nick Culbertson */
/* chess.h: a small chess engine for Mighty Arcade (prototype). 0x88 board, every rule (castling, en passant, promotion
 * to a queen), a negamax alpha-beta search with captures searched out (quiescence), a node budget so a move never takes
 * long, and the draws (fifty moves, repetition, too little to mate, and boredom: 300 plies). Included once from app.c.
 *
 * Squares: rank * 16 + file, rank 0 white's first. Pieces: 1 P, 2 N, 3 B, 4 R, 5 Q, 6 K; white +, black -. */
enum { EMPTY, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };
#define SQ(f, r) ((r) * 16 + (f))
#define ONB(s) (!((s) & 0x88))
#define FILE_OF(s) ((s) & 7)
#define RANK_OF(s) ((s) >> 4)
typedef struct {
    uint8_t from, to, promo, flags;      /* flags: CF_* */
    int8_t cap;                          /* the piece taken (signed), 0 none */
} cmove_t;
enum { CF_EP = 1, CF_CASTLE = 2, CF_DOUBLE = 4 };
typedef struct {
    int8_t b[128];
    int8_t side;                         /* +1 white to move, -1 black */
    uint8_t castle;                      /* 1 white short, 2 white long, 4 black short, 8 black long */
    int8_t ep;                           /* the en passant square, -1 none */
    uint8_t half;                        /* plies since a capture or a pawn move */
    uint16_t ply;
    uint8_t kpos[2];                     /* [0] white's king, [1] black's */
} cpos_t;
typedef struct {
    uint8_t castle, half;
    int8_t ep;
} cundo_t;

static const int8_t N_OFF[8] = {33, 31, 18, 14, -33, -31, -18, -14};
static const int8_t K_OFF[8] = {1, -1, 16, -16, 17, 15, -17, -15};
static const int8_t B_OFF[4] = {17, 15, -17, -15};
static const int8_t R_OFF[4] = {1, -1, 16, -16};
static const int16_t VAL[7] = {0, 100, 320, 330, 500, 900, 0};
static const char PCH[7] = {' ', 'P', 'N', 'B', 'R', 'Q', 'K'};

static void c_reset(cpos_t *p)
{
    static const int8_t back[8] = {ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK};
    int i;
    for (i = 0; i < 128; i++)
        p->b[i] = 0;
    for (i = 0; i < 8; i++) {
        p->b[SQ(i, 0)] = back[i];
        p->b[SQ(i, 1)] = PAWN;
        p->b[SQ(i, 6)] = -PAWN;
        p->b[SQ(i, 7)] = (int8_t)-back[i];
    }
    p->side = 1;
    p->castle = 15;
    p->ep = -1;
    p->half = 0;
    p->ply = 0;
    p->kpos[0] = SQ(4, 0);
    p->kpos[1] = SQ(4, 7);
}

/* is sq attacked by side by? */
static int c_attacked(const cpos_t *p, int sq, int by)
{
    int i, s, d;
    int pd = by > 0 ? -16 : 16;          /* where an attacking pawn stands, from sq */
    for (i = -1; i <= 1; i += 2) {
        s = sq + pd + i;
        if (ONB(s) && p->b[s] == by * PAWN)
            return 1;
    }
    for (i = 0; i < 8; i++) {
        s = sq + N_OFF[i];
        if (ONB(s) && p->b[s] == by * KNIGHT)
            return 1;
        s = sq + K_OFF[i];
        if (ONB(s) && p->b[s] == by * KING)
            return 1;
    }
    for (i = 0; i < 4; i++) {
        for (d = B_OFF[i], s = sq + d; ONB(s); s += d)
            if (p->b[s]) {
                if (p->b[s] == by * BISHOP || p->b[s] == by * QUEEN)
                    return 1;
                break;
            }
        for (d = R_OFF[i], s = sq + d; ONB(s); s += d)
            if (p->b[s]) {
                if (p->b[s] == by * ROOK || p->b[s] == by * QUEEN)
                    return 1;
                break;
            }
    }
    return 0;
}
static int c_in_check(const cpos_t *p, int side) { return c_attacked(p, p->kpos[side < 0], -side); }

/* how many moves the list being filled holds: c_gen sets it (every list is 128, the captures-only ones in quiesce
 * 64). A move past it is dropped, never written (a position with many promoted pieces can have more: GPT's review,
 * 18 Nov 2026) */
#define C_MAXMOVES 128
#define C_MAXCAPS 64
static int c_room = C_MAXMOVES;
static int add(cmove_t *m, int n, int from, int to, int cap, int flags, int promo)
{
    if (n >= c_room)
        return n;
    m[n].from = (uint8_t)from;
    m[n].to = (uint8_t)to;
    m[n].cap = (int8_t)cap;
    m[n].flags = (uint8_t)flags;
    m[n].promo = (uint8_t)promo;
    return n + 1;
}

/* every move by the side to move, legal or not (caps_only: captures and promotions) */
static int c_gen(const cpos_t *p, cmove_t *m, int caps_only)
{
    int n = 0, s, t, i, d, side = p->side;
    c_room = caps_only ? C_MAXCAPS : C_MAXMOVES;
    for (s = 0; s < 128; s++) {
        int pc = p->b[s] * side;
        if (!ONB(s)) {
            s += 7;
            continue;
        }
        if (pc <= 0)
            continue;
        if (pc == PAWN) {
            int fwd = 16 * side, r = RANK_OF(s), last = side > 0 ? 6 : 1, start = side > 0 ? 1 : 6;
            t = s + fwd;
            if (ONB(t) && !p->b[t] && (!caps_only || r == last)) {
                n = add(m, n, s, t, 0, 0, r == last ? QUEEN : 0);
                if (r == start && !p->b[t + fwd] && !caps_only)
                    n = add(m, n, s, t + fwd, 0, CF_DOUBLE, 0);
            }
            for (i = -1; i <= 1; i += 2) {
                t = s + fwd + i;
                if (!ONB(t))
                    continue;
                if (p->b[t] * side < 0)
                    n = add(m, n, s, t, p->b[t], 0, r == last ? QUEEN : 0);
                else if (t == p->ep)
                    n = add(m, n, s, t, -side * PAWN, CF_EP, 0);
            }
            continue;
        }
        if (pc == KNIGHT || pc == KING) {
            const int8_t *off = pc == KNIGHT ? N_OFF : K_OFF;
            for (i = 0; i < 8; i++) {
                t = s + off[i];
                if (ONB(t) && p->b[t] * side <= 0 && (!caps_only || p->b[t]))
                    n = add(m, n, s, t, p->b[t], 0, 0);
            }
            if (pc == KING && !caps_only) {  /* castling: the squares between empty, none it crosses attacked */
                int sh = side > 0 ? 1 : 4, lo = side > 0 ? 2 : 8, r0 = side > 0 ? 0 : 7;
                if ((p->castle & sh) && !p->b[SQ(5, r0)] && !p->b[SQ(6, r0)] && p->b[SQ(7, r0)] == side * ROOK &&
                    !c_attacked(p, SQ(4, r0), -side) && !c_attacked(p, SQ(5, r0), -side) &&
                    !c_attacked(p, SQ(6, r0), -side))
                    n = add(m, n, s, SQ(6, r0), 0, CF_CASTLE, 0);
                if ((p->castle & lo) && !p->b[SQ(3, r0)] && !p->b[SQ(2, r0)] && !p->b[SQ(1, r0)] &&
                    p->b[SQ(0, r0)] == side * ROOK && !c_attacked(p, SQ(4, r0), -side) &&
                    !c_attacked(p, SQ(3, r0), -side) && !c_attacked(p, SQ(2, r0), -side))
                    n = add(m, n, s, SQ(2, r0), 0, CF_CASTLE, 0);
            }
            continue;
        }
        if (pc == BISHOP || pc == QUEEN)
            for (i = 0; i < 4; i++)
                for (d = B_OFF[i], t = s + d; ONB(t); t += d) {
                    if (p->b[t] * side > 0)
                        break;
                    if (!caps_only || p->b[t])
                        n = add(m, n, s, t, p->b[t], 0, 0);
                    if (p->b[t])
                        break;
                }
        if (pc == ROOK || pc == QUEEN)
            for (i = 0; i < 4; i++)
                for (d = R_OFF[i], t = s + d; ONB(t); t += d) {
                    if (p->b[t] * side > 0)
                        break;
                    if (!caps_only || p->b[t])
                        n = add(m, n, s, t, p->b[t], 0, 0);
                    if (p->b[t])
                        break;
                }
    }
    return n;
}

static void c_make(cpos_t *p, const cmove_t *m, cundo_t *u)
{
    int side = p->side, pc = p->b[m->from];
    u->castle = p->castle;
    u->ep = p->ep;
    u->half = p->half;
    p->b[m->to] = pc;
    p->b[m->from] = 0;
    if (m->flags & CF_EP)
        p->b[m->to - 16 * side] = 0;
    if (m->promo)
        p->b[m->to] = (int8_t)(side * m->promo);
    if (pc * side == KING) {
        p->kpos[side < 0] = m->to;
        p->castle &= (uint8_t)(side > 0 ? ~3u : ~12u);
        if (m->flags & CF_CASTLE) {      /* the rook comes round */
            int r0 = RANK_OF(m->to);
            if (FILE_OF(m->to) == 6) {
                p->b[SQ(5, r0)] = p->b[SQ(7, r0)];
                p->b[SQ(7, r0)] = 0;
            } else {
                p->b[SQ(3, r0)] = p->b[SQ(0, r0)];
                p->b[SQ(0, r0)] = 0;
            }
        }
    }
    if (m->from == SQ(0, 0) || m->to == SQ(0, 0))
        p->castle &= ~2u;
    if (m->from == SQ(7, 0) || m->to == SQ(7, 0))
        p->castle &= ~1u;
    if (m->from == SQ(0, 7) || m->to == SQ(0, 7))
        p->castle &= ~8u;
    if (m->from == SQ(7, 7) || m->to == SQ(7, 7))
        p->castle &= ~4u;
    p->ep = (int8_t)(m->flags & CF_DOUBLE ? m->from + 16 * side : -1);
    p->half = (uint8_t)(pc * side == PAWN || m->cap ? 0 : p->half + 1);
    p->side = (int8_t)-side;
    p->ply++;
}

static void c_unmake(cpos_t *p, const cmove_t *m, const cundo_t *u)
{
    int side = -p->side, pc = p->b[m->to];
    p->side = (int8_t)side;
    p->ply--;
    if (m->promo)
        pc = side * PAWN;
    p->b[m->from] = (int8_t)pc;
    p->b[m->to] = (m->flags & CF_EP) ? 0 : m->cap;
    if (m->flags & CF_EP)
        p->b[m->to - 16 * side] = m->cap;
    if (pc * side == KING) {
        p->kpos[side < 0] = m->from;
        if (m->flags & CF_CASTLE) {
            int r0 = RANK_OF(m->to);
            if (FILE_OF(m->to) == 6) {
                p->b[SQ(7, r0)] = p->b[SQ(5, r0)];
                p->b[SQ(5, r0)] = 0;
            } else {
                p->b[SQ(0, r0)] = p->b[SQ(3, r0)];
                p->b[SQ(3, r0)] = 0;
            }
        }
    }
    p->castle = u->castle;
    p->ep = u->ep;
    p->half = u->half;
}

/* the legal moves */
static int c_legal(cpos_t *p, cmove_t *out)
{
    cmove_t m[128];
    cundo_t u;
    int n = c_gen(p, m, 0), i, k = 0;
    for (i = 0; i < n; i++) {
        c_make(p, &m[i], &u);
        if (!c_in_check(p, -p->side))
            out[k++] = m[i];
        c_unmake(p, &m[i], &u);
    }
    return k;
}

/* --------------------------------------------------------------- evaluation --- */
static int centre(int s) /* 0 .. 6: how central */
{
    int f = FILE_OF(s), r = RANK_OF(s);
    int df = f < 4 ? 3 - f : f - 4, dr = r < 4 ? 3 - r : r - 4;
    return 6 - df - dr;
}
static int c_material(const cpos_t *p, int side)   /* the side's pieces but pawns and the king, in pawns */
{
    int s, m = 0;
    for (s = 0; s < 128; s++)
        if (ONB(s) && p->b[s] * side > 1 && p->b[s] * side < KING)
            m += VAL[p->b[s] * side];
    return m;
}
/* white's view, in centipawns */
static int c_eval(const cpos_t *p)
{
    int s, e = 0, endgame = c_material(p, 1) + c_material(p, -1) < 2600;
    for (s = 0; s < 128; s++) {
        int pc, side, t, adv;
        if (!ONB(s)) {
            s += 7;
            continue;
        }
        if (!(pc = p->b[s]))
            continue;
        side = pc > 0 ? 1 : -1;
        t = pc * side;
        adv = side > 0 ? RANK_OF(s) : 7 - RANK_OF(s);
        e += side * VAL[t];
        switch (t) {
        case PAWN:
            e += side * (adv * (endgame ? 12 : 5) + ((FILE_OF(s) == 3 || FILE_OF(s) == 4) && adv >= 2 ? 12 : 0));
            break;
        case KNIGHT:
            e += side * (centre(s) * 6 - 12);
            break;
        case BISHOP:
            e += side * (centre(s) * 3 + (adv ? 6 : 0));
            break;
        case ROOK:
            e += side * (adv == 6 ? 18 : 0);
            break;
        case QUEEN:
            e += side * (centre(s) * 2 - (p->ply < 14 && adv > 1 ? 15 : 0));
            break;
        case KING:
            e += side * (endgame ? centre(s) * 8 : -adv * 15 + (FILE_OF(s) == 6 || FILE_OF(s) == 1 ? 20 : 0));
            break;
        }
    }
    return e;
}

/* -------------------------------------------------------------------- search --- */
#define MATE 30000
static uint32_t c_nodes, c_budget;
static uint32_t c_rng = 0x1234567u;
static uint32_t c_rand(void)
{
    c_rng ^= c_rng << 13;
    c_rng ^= c_rng >> 17;
    c_rng ^= c_rng << 5;
    return c_rng;
}
static void order(cmove_t *m, int n)    /* captures first, the most valuable victim by the least valuable attacker */
{
    int i, j;
    int16_t key[128];
    for (i = 0; i < n; i++)
        key[i] = (int16_t)(m[i].cap ? 1000 + VAL[m[i].cap < 0 ? -m[i].cap : m[i].cap] * 4 - (int)m[i].from % 7
                          : m[i].promo ? 900 : 0);
    for (i = 1; i < n; i++) {
        cmove_t mm = m[i];
        int16_t kk = key[i];
        for (j = i; j > 0 && key[j - 1] < kk; j--) {
            m[j] = m[j - 1];
            key[j] = key[j - 1];
        }
        m[j] = mm;
        key[j] = kk;
    }
}
static int quiesce(cpos_t *p, int alpha, int beta, int depth)
{
    cmove_t m[C_MAXCAPS];
    cundo_t u;
    int n, i, stand = c_eval(p) * p->side;
    c_nodes++;
    if (stand >= beta)
        return stand;
    if (stand > alpha)
        alpha = stand;
    if (depth <= 0 || c_nodes > c_budget)
        return alpha;
    n = c_gen(p, m, 1);
    order(m, n);
    for (i = 0; i < n; i++) {
        int sc;
        c_make(p, &m[i], &u);
        if (c_in_check(p, -p->side)) {
            c_unmake(p, &m[i], &u);
            continue;
        }
        sc = -quiesce(p, -beta, -alpha, depth - 1);
        c_unmake(p, &m[i], &u);
        if (sc >= beta)
            return sc;
        if (sc > alpha)
            alpha = sc;
    }
    return alpha;
}
static int negamax(cpos_t *p, int depth, int alpha, int beta, int ply)
{
    cmove_t m[128];
    cundo_t u;
    int n, i, legal = 0, best = -MATE;
    if (depth <= 0)
        return quiesce(p, alpha, beta, 4);
    c_nodes++;
    n = c_gen(p, m, 0);
    order(m, n);
    for (i = 0; i < n; i++) {
        int sc;
        c_make(p, &m[i], &u);
        if (c_in_check(p, -p->side)) {
            c_unmake(p, &m[i], &u);
            continue;
        }
        legal++;
        sc = -negamax(p, depth - 1, -beta, -alpha, ply + 1);
        c_unmake(p, &m[i], &u);
        if (sc > best)
            best = sc;
        if (sc > alpha)
            alpha = sc;
        if (alpha >= beta || c_nodes > c_budget)
            break;
    }
    if (!legal)
        return c_in_check(p, p->side) ? -MATE + ply : 0;
    return best;
}
/* the side to move's choice: depth plies (a little noise, so games differ); -1 none (mate or stalemate) */
static int c_search(cpos_t *p, int depth, int noise, cmove_t *best, int *score)
{
    cmove_t m[128];
    cundo_t u;
    int n = c_legal(p, m), i, d, bi = -1, bs = -MATE - 1;
    if (!n)
        return -1;
    c_nodes = 0;
    c_budget = 60000u;
    order(m, n);
    for (d = 1; d <= depth; d++) {      /* deeper while the budget lasts; the last finished depth decides */
        int dbi = -1, dbs = -MATE - 1;
        for (i = 0; i < n; i++) {
            int sc;
            c_make(p, &m[i], &u);
            sc = -negamax(p, d - 1, -MATE, MATE, 1) + (noise ? (int)(c_rand() % (uint32_t)(2 * noise + 1)) - noise : 0);
            c_unmake(p, &m[i], &u);
            if (sc > dbs)
                dbs = sc, dbi = i;
        }
        if (c_nodes > c_budget && d > 1)
            break;
        bi = dbi, bs = dbs;
        if (bi > 0) {                    /* the best first, for the next depth */
            cmove_t t = m[bi];
            for (i = bi; i > 0; i--)
                m[i] = m[i - 1];
            m[0] = t;
            bi = 0;
        }
    }
    *best = m[bi];
    *score = bs;
    return n;
}

/* a position's fingerprint, for repetitions */
static uint32_t c_hash(const cpos_t *p)
{
    uint32_t h = 2166136261u ^ (uint32_t)(p->side + 2) ^ (uint32_t)p->castle << 8 ^ (uint32_t)(p->ep + 1) << 12;
    int s;
    for (s = 0; s < 128; s++)
        if (ONB(s) && p->b[s]) {
            h ^= (uint32_t)(p->b[s] + 8) * 0x9E3779B1u + (uint32_t)s * 0x85EBCA77u;
            h *= 16777619u;
        }
    return h;
}
static int c_too_little(const cpos_t *p)   /* neither side can mate: kings alone, or a king and one minor piece */
{
    int s, minors = 0;
    for (s = 0; s < 128; s++)
        if (ONB(s) && p->b[s]) {
            int t = p->b[s] < 0 ? -p->b[s] : p->b[s];
            if (t == PAWN || t == ROOK || t == QUEEN)
                return 0;
            if (t == KNIGHT || t == BISHOP)
                minors++;
        }
    return minors <= 1;
}

/* a move as people write it: Nf3, exd5, O-O, e8=Q (check marks added by the caller) */
static void c_san(const cmove_t *m, int piece, char *o)
{
    int t = piece < 0 ? -piece : piece, k = 0;
    if (m->flags & CF_CASTLE) {
        const char *s = FILE_OF(m->to) == 6 ? "O-O" : "O-O-O";
        while ((o[k] = s[k]))
            k++;
        return;
    }
    if (t != PAWN)
        o[k++] = PCH[t];
    else if (m->cap)
        o[k++] = (char)('a' + FILE_OF(m->from));
    if (m->cap)
        o[k++] = 'x';
    o[k++] = (char)('a' + FILE_OF(m->to));
    o[k++] = (char)('1' + RANK_OF(m->to));
    if (m->promo) {
        o[k++] = '=';
        o[k++] = 'Q';
    }
    o[k] = 0;
}

/* ----------------------------------------------------- thinking in slices --- */
/* c_search's work spread over many calls: each call searches root moves until its node budget is spent (a root move
 * at a time, never split), so nothing stalls for long: in the browser emulator the app runs on the audio thread, and
 * on the FM-1 the screen keeps moving. Deeper while the total lasts, as c_search */
typedef struct {
    cpos_t p;
    cmove_t m[128];
    int n, i, d, maxd, noise;
    int dbs, dbi, bs, bi;
    uint32_t used;
    uint8_t busy, done;
} cthink_t;
static void ct_start(cthink_t *t, const cpos_t *p, int depth, int noise)
{
    t->p = *p;
    t->n = c_legal(&t->p, t->m);
    order(t->m, t->n);
    t->i = 0;
    t->d = 1;
    t->maxd = depth;
    t->noise = noise;
    t->dbs = t->bs = -MATE - 1;
    t->dbi = t->bi = -1;
    t->used = 0;
    t->busy = (uint8_t)(t->n > 0);
    t->done = (uint8_t)(t->n == 0);
}
/* 1: done (t->m[t->bi] the move, bi -1: none) */
static int ct_step(cthink_t *t, uint32_t budget)
{
    uint32_t spent = 0;
    cundo_t u;
    while (t->busy && spent < budget) {
        int sc;
        c_nodes = 0;
        c_budget = 3000u;                /* one root move's most: a slice never runs long */
        c_make(&t->p, &t->m[t->i], &u);
        sc = -negamax(&t->p, t->d - 1, -MATE, MATE, 1) +
             (t->noise ? (int)(c_rand() % (uint32_t)(2 * t->noise + 1)) - t->noise : 0);
        c_unmake(&t->p, &t->m[t->i], &u);
        spent += c_nodes;
        t->used += c_nodes;
        if (sc > t->dbs)
            t->dbs = sc, t->dbi = t->i;
        if (++t->i < t->n)
            continue;
        t->bi = t->dbi;                  /* a depth done: its best, first for the next */
        t->bs = t->dbs;
        if (t->bi > 0) {
            cmove_t mm = t->m[t->bi];
            int k;
            for (k = t->bi; k > 0; k--)
                t->m[k] = t->m[k - 1];
            t->m[0] = mm;
            t->bi = 0;
        }
        if (++t->d > t->maxd || t->used > 60000u) {
            t->busy = 0;
            t->done = 1;
        } else {
            t->i = 0;
            t->dbs = -MATE - 1;
            t->dbi = -1;
        }
    }
    return t->done;
}
