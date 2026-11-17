/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Nick Culbertson */
/* MIGHTY ARCADE (prototype): a box of casual games that make music. This file: the menu, the modes, and CHESS (two
 * engines, or you against one, or two players), where the board is the score. A playhead sweeps the files a to h,
 * an eighth note each, a bar a sweep; each piece it passes may sound (DENSITY), its pitch from its rank, its voice from
 * what it is: pawns plucked, knights a marimba, bishops a celesta, rooks a low electric piano, queens a glass pad, the
 * king a drone. Black is an octave down. A move every bar or so (MOVE): the piece's from and to sung on vibraphone; a
 * capture swells, a check pings, castling and promotion have their figures, mate rings the bells. As pieces come off
 * the music thins out: the game's arc is the piece's.
 *
 *   PLAY   pause / play on                 HOME   a new game
 *   SEL    YOU VS CPU: you play white. Keys, in any octave: C left, E right, C# up, D down, A select (a piece,
 *          then where to; its moves are dotted; A on it again, or OCT-, lets go). The music goes on as you think
 *   K1 TEMPO   K2 DENSITY   K3 MOVE (every 1, 2 or 4 bars)   K4 BLUNDER (how often a player plays any old move)
 *   PRESET the scale   ALGO how deep they think (1..4)   keys: play along in the scale */
#include "kit.h"
#include "chess.h"

#define C_BG RGB(11, 8, 20)
#define C_PINK RGB(255, 46, 151)
#define C_CYAN RGB(95, 227, 238)
#define C_PURP RGB(176, 124, 255)
#define C_YEL RGB(255, 210, 63)
#define C_INK RGB(244, 240, 255)
#define C_GHOST RGB(84, 62, 136)
#define C_LINE2 RGB(42, 33, 64)
#define C_SQ_D RGB(24, 17, 42)
#define C_SQ_L RGB(40, 30, 68)

/* the scales: a root (MIDI) and seven steps */
typedef struct {
    const char *name;
    int8_t root;
    int8_t step[7];
} scale_t;
static const scale_t SCALES[] = {     /* PRESET's order (Nick, 17 Nov 2026): the pentatonic first, the default */
    {"G PENTA", 55, {0, 2, 4, 7, 9, 12, 14}},  {"C MAJOR", 48, {0, 2, 4, 5, 7, 9, 11}},
    {"A MINOR", 45, {0, 2, 3, 5, 7, 8, 10}},   {"D DORIAN", 50, {0, 2, 3, 5, 7, 9, 10}},
    {"F LYDIAN", 53, {0, 2, 4, 6, 7, 9, 11}},  {"E PHRYGIAN", 52, {0, 1, 3, 5, 7, 8, 10}},
    {"G MIXOLYDIAN", 55, {0, 2, 4, 5, 7, 9, 10}},
};
#define NSCALE (sizeof SCALES / sizeof SCALES[0])
enum { SL_PAWN, SL_KNIGHT, SL_BISHOP, SL_ROOK, SL_QUEEN, SL_KING, SL_MOVE, SL_SWELL, SL_PING, SL_BELLS, SL_PLAY };
/* a piece's voice: its sound slot, octave (in scale degrees), how likely it sounds, how long, how loud */
static const struct {
    int8_t deg;
    uint8_t chance;                      /* of 100, at DENSITY 50 */
    float secs;
    uint8_t vel;
} VOICE[7] = {{0, 0, 0, 0},       {0, 34, 0.4f, 74},  {7, 45, 0.3f, 80},  {9, 40, 0.7f, 70},
              {-7, 30, 0.9f, 82}, {2, 50, 1.8f, 64},  {-14, 100, 3.0f, 70}};

static cpos_t pos;
static uint32_t hashes[320];
static int8_t sq_last[2] = {-1, -1};      /* the last move's from and to */
static struct {
    uint8_t scale, depth, move_every, density, blunder, tempo;   /* tempo: BPM */
    uint8_t paused, over;                /* over: 1 a game ended (its result in result[]) */
    uint8_t vs;                          /* 1: you play white against the computer (SEL) */
} st;
static char result[24], comment[48], san_last[12];
static int eval_now;                     /* white's view, centipawns (the bar) */

/* timing: an eighth note a step, a sweep (8 steps) a bar */
static uint32_t step_samples(void) { return (uint32_t)(SR_F * 30.0f / (float)st.tempo); }
static uint32_t next_step_t, step_no;
static struct {
    uint32_t t;
    uint8_t file;
} step_hist[16];                         /* the steps scheduled lately, for the playhead */
static uint32_t flash_t[64];             /* when a square last sounded (sample time; it lights then) */
static uint8_t flash_c[64];              /* by which side: 1 white, 2 black */

/* the move: thought out ahead, played on its bar */
static cmove_t pending;
static uint32_t apply_t;                 /* its sample time, once scheduled (0: not yet) */
static int pending_ok, pending_blunder, pending_now;   /* pending_now: yours, played on the next step */
/* your move, with a cursor: C left, E right, C# up, D down, A selects (any octave) */
static struct {
    int8_t cur;                          /* the cursor's square */
    int8_t from;                         /* the piece picked, -1 none */
} you = {SQ(4, 1), -1};
static int your_turn(void) { return st.vs && !st.over && (st.vs == 2 || pos.side > 0) && !pending_ok && !apply_t; }
static uint32_t over_t;                  /* when the next game starts */

/* the key: the scale's own root, moved (SOLITAIRE's ALGORITHM knob), kept within G#2 .. G3 */
static int8_t key_shift;
static int key_root(void)
{
    int r = SCALES[st.scale].root + key_shift;
    while (r > 55)
        r -= 12;
    while (r < 44)
        r += 12;
    return r;
}
static const char *const NOTE_NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static void scale_name(char *t)          /* "E DORIAN": the key's note, then the scale's kind */
{
    const char *k = SCALES[st.scale].name;
    while (*k && *k != ' ')
        k++;
    tcpy(t, NOTE_NAME[key_root() % 12]);
    tcat(t, k);
}
static int degree_note(int deg)          /* a scale degree (0 = the root) as a MIDI note */
{
    const scale_t *s = &SCALES[st.scale];
    int oct = deg >= 0 ? deg / 7 : -((-deg + 6) / 7), d = deg - oct * 7;
    return key_root() + 12 * oct + s->step[d];
}
/* --------------------------------------------------------------- harmony --- */
/* the chords every game plays over: Nick's voicings (Mighty Arp's LUSH, from his Macro MIDI Pad), notes from the
 * key's root for each degree, the bass first: the major key's with their ninths, the minor's. Whenever a game's chord
 * changes, a new one at random in the key, never the same twice running */
#define HV 127
static const int8_t H_LUSH_MAJ[7][5] = {{-12, 7, 12, 14, 16}, {-10, 5, 7, 12, 16}, {-8, 7, 11, 12, 16},
                                        {-7, 7, 9, 12, 16}, {-5, 7, 11, 12, 14}, {-3, 7, 11, 12, 16},
                                        {-1, 7, 11, 12, 14}};
static const int8_t H_LUSH_MIN[7][5] = {{0, 7, 12, 15, HV}, {2, 10, 14, 17, HV}, {3, 7, 10, 15, HV}, {5, 8, 12, 17, HV},
                                        {7, 10, 14, 19, HV}, {3, 8, 12, 15, HV}, {5, 10, 14, 17, HV}};
static uint8_t h_deg;                    /* the chord now: its degree (0 the key's own) */
static uint32_t h_bar = 0xFFFFFFFFu;     /* the bar (or turn) it was chosen for */
static int h_minor(void) { return SCALES[st.scale].step[2] == 3; }   /* a minor third: the minor voicings */
static const int8_t *h_voice(void) { return h_minor() ? H_LUSH_MIN[h_deg] : H_LUSH_MAJ[h_deg]; }
static void h_change(void)               /* a new chord, at random in the key, not the one before */
{
    uint8_t d;
    do
        d = (uint8_t)(kr() % 7u);
    while (d == h_deg);
    h_deg = d;
}
static int h_deg_at(uint32_t bar)        /* the chord for a bar: a new one each time the bar changes */
{
    if (bar != h_bar) {
        h_bar = bar;
        h_change();
    }
    return h_deg;
}
static void h_reset(void)                /* a game starts: on the key's own chord */
{
    h_deg = 0;
    h_bar = 0;
}
static int h_bass(void)                  /* the chord's bass, low (C2 .. B2) */
{
    int n = key_root() + h_voice()[0];
    while (n > 47)
        n -= 12;
    while (n < 36)
        n += 12;
    return n;
}
static int h_note(int k)                 /* the chord's voice k (1..4 the upper ones; past the last, round again an
                                          * octave up) */
{
    const int8_t *v = h_voice();
    int n = h_minor() ? 4 : 5, up = 0;
    if (k < 1)
        return h_bass();
    while (v[k] == HV || k >= n) {      /* (the minor ones have four) */
        k -= n - 1;
        up += 12;
        if (k < 1)
            k = 1;
        if (v[k] != HV && k < n)
            break;
    }
    return key_root() + v[k] + up;
}
static int h_tone(int i)                 /* the chord's tones, from the key's root up: i 0 the lowest, past the last
                                          * round again an octave up (every one of them in the pad's chord) */
{
    const int8_t *v = h_voice();
    int pc[5], n = 0, j, k;
    for (j = 0; j < 5; j++) {
        int c;
        if (v[j] == HV)
            continue;
        c = ((v[j] % 12) + 12) % 12;
        for (k = 0; k < n && pc[k] != c; k++)
            ;
        if (k == n)
            pc[n++] = c;
    }
    for (j = 1; j < n; j++)              /* lowest first */
        for (k = j; k > 0 && pc[k - 1] > pc[k]; k--) {
            int x = pc[k];
            pc[k] = pc[k - 1], pc[k - 1] = x;
        }
    if (i < 0)
        i = 0;
    return key_root() + pc[i % n] + 12 * (i / n);
}
static void h_pad(uint32_t t, int vel, int slot, float secs, int oct)   /* the voicing whole, softly spread */
{
    int k, n = h_minor() ? 4 : 5;
    k_on(t, h_bass() + 12 * (oct < 0 ? oct + 1 : oct), vel, (uint32_t)slot, secs);
    for (k = 1; k < n; k++)
        k_on(t + (uint32_t)k * 600u, h_note(k) + 12 * oct, vel - 6, (uint32_t)slot, secs);
}

static int piece_note(int pc, int sq)     /* where a piece stands, as its note */
{
    int t = pc < 0 ? -pc : pc, side = pc > 0 ? 1 : -1, r = RANK_OF(sq);
    int rel = side > 0 ? r : 7 - r;       /* its own rank, so both sides' pawns start alike */
    return degree_note(rel + VOICE[t].deg) - (side < 0 ? 12 : 0);
}
static void sound(uint32_t t, int pc, int sq, int vel_add)
{
    int ty = pc < 0 ? -pc : pc;
    k_on(t, piece_note(pc, sq), (uint32_t)clamp(VOICE[ty].vel + vel_add + kr_in(-10, 10), 20, 127),
         (uint32_t)(SL_PAWN + ty - 1), VOICE[ty].secs);
    flash_t[FILE_OF(sq) + 8 * RANK_OF(sq)] = t;
    flash_c[FILE_OF(sq) + 8 * RANK_OF(sq)] = (uint8_t)(pc > 0 ? 1 : 2);
}

/* ------------------------------------------------------------- the game --- */
static const char *const P_NAME[7] = {"", "PAWN", "KNIGHT", "BISHOP", "ROOK", "QUEEN", "KING"};
static const char *const QUIET[] = {"Hmm.", "Bold.", "Elegant.", "A bold choice.", "No comment.", "Deep thoughts.",
                                    "Interesting.", "Brave.", "Sure, why not.", "Very French.", "The crowd murmurs.",
                                    "Textbook.", "Questionable.", "Mysterious.", "A quiet move."};
static const char *const RUDE[] = {"Rude.", "The audacity.", "Unforgivable.", "Delicious.", "Ouch.",
                                   "That one had a family.", "No mercy."};
static const char *const OOPS[] = {"Oops.", "Was that on purpose?", "The crowd gasps.", "Avant-garde.",
                                   "Pure jazz.", "Nobody saw that coming.", "A blunder. Beautiful."};
#define PICK(a) (a)[kr() % (sizeof(a) / sizeof((a)[0]))]

static cthink_t thought;                  /* the computer's move, being thought out */
static void think(void)                  /* the next move, ahead of its bar */
{
    pending_ok = 0;
    pending_blunder = 0;
    pending_now = 0;
    thought.busy = thought.done = 0;
    you.from = -1;
    if (st.over || st.vs == 2 || (st.vs && pos.side > 0))   /* yours: the keys pick it */
        return;
    if (kr_pc(st.blunder)) {             /* BLUNDER: any old legal move */
        cmove_t m[128];
        int n = c_legal(&pos, m);
        if (n) {
            pending = m[kr() % (uint32_t)n];
            pending_ok = pending_blunder = 1;
        }
    } else {                             /* thought out a little each frame (thinking_step) */
        ct_start(&thought, &pos, st.depth, 12);
    }
    apply_t = 0;
}

static void game_new(void)
{
    uint32_t i;
    c_reset(&pos);
    c_rng ^= plat_ms() * 2654435761u + 1u;
    for (i = 0; i < 64u; i++)
        flash_t[i] = 0;
    sq_last[0] = sq_last[1] = -1;
    st.over = 0;
    result[0] = 0;
    tcpy(comment, "A new game. Everyone is optimistic.");
    san_last[0] = 0;
    eval_now = 0;
    think();
}

static void game_over(const char *res, const char *why)
{
    st.over = 1;
    tcpy(result, res);
    tcpy(comment, why);
    over_t = k_now() + step_samples() * 8u * 4u;   /* four bars of quiet, then a new game */
}

/* the move, on the board: its sounds were scheduled with it; here the board changes as they play */
static void chess_juice(int from, int to, int cap, int check, int mate, int promo, int castle);
static void move_apply(void)
{
    cundo_t u;
    cmove_t m = pending;
    int pc = pos.b[m.from], check, n, i;
    cmove_t legal[128];
    char t[48], num[8];
    c_san(&m, pc, san_last);
    c_make(&pos, &m, &u);
    sq_last[0] = (int8_t)m.from;
    sq_last[1] = (int8_t)m.to;
    check = c_in_check(&pos, pos.side);
    n = c_legal(&pos, legal);
    chess_juice(m.from, m.to, m.cap, check, check && !n, m.promo, (m.flags & CF_CASTLE) != 0);
    if (check)
        tcat(san_last, n ? "+" : "#");
    /* the commentary */
    itos((pos.ply + 1) / 2, num);
    tcpy(t, num);
    tcat(t, pos.side < 0 ? ". " : "... ");
    tcat(t, san_last);
    tcat(t, "  ");
    if (pending_blunder)
        tcat(t, PICK(OOPS));
    else if (m.flags & CF_CASTLE)
        tcat(t, "Castles. Sensible. Boring.");
    else if (m.promo)
        tcat(t, "A pawn becomes a queen!");
    else if (check && n)
        tcat(t, "Check. Nervous king.");
    else if (m.cap)
        tcat(t, PICK(RUDE));
    else
        tcat(t, PICK(QUIET));
    tcpy(comment, t);
    eval_now = c_eval(&pos);
    hashes[pos.ply % 320u] = c_hash(&pos);
    /* the end? */
    if (!n)
        game_over(check ? (pos.side < 0 ? "1-0" : "0-1") : "1/2-1/2",
                  check ? (pos.side < 0 ? "Checkmate. White wins." : "Checkmate. Black wins.")
                        : "Stalemate. Nobody wins.");
    else if (pos.half >= 100)
        game_over("1/2-1/2", "Fifty moves of nothing. Draw.");
    else if (c_too_little(&pos))
        game_over("1/2-1/2", "Not enough left to mate. Draw.");
    else if (pos.ply >= 300)
        game_over("1/2-1/2", "Draw by boredom.");
    else {
        uint32_t reps = 0;
        for (i = 4; i <= 12 && i <= (int)pos.ply; i += 2)
            if (hashes[(pos.ply - (uint32_t)i) % 320u] == hashes[pos.ply % 320u])
                reps++;
        if (reps >= 2)
            game_over("1/2-1/2", "Repetition. They agree to disagree.");
    }
    think();
}

/* a move's sounds, at t: from and to on vibraphone, then its figure */
static void move_sounds(uint32_t t, uint32_t step)
{
    cmove_t m = pending;
    int pc = pos.b[m.from], side = pc > 0 ? 1 : -1, i;
    cpos_t after = pos;
    cundo_t u;
    int check, a = piece_note(pc, m.from) + 12, b = piece_note(m.promo ? side * QUEEN : pc, m.to) + 12;
    c_make(&after, &m, &u);
    check = c_in_check(&after, after.side);
    k_on(t, a, 92, SL_MOVE, 0.6f);
    k_on(t + step, b, 104, SL_MOVE, 1.2f);
    if (pending_blunder)                 /* a wrong note, on purpose */
        k_on(t + step, b + 1, 70, SL_MOVE, 0.8f);
    if (m.cap) {                         /* a capture: a swell under the taken piece */
        int n = piece_note(m.cap, m.to) - 12;
        k_on(t + step, n, 70, SL_SWELL, 1.6f);
        k_on(t + step, n + 7, 60, SL_SWELL, 1.6f);
    }
    if (check)
        k_on(t + 2u * step, degree_note(14 + 4), 96, SL_PING, 1.5f);
    if (m.flags & CF_CASTLE)
        for (i = 0; i < 3; i++)
            k_on(t + step + (uint32_t)i * step / 2u, degree_note(7 + 2 * i), 80, SL_MOVE, 0.6f);
    if (m.promo)
        for (i = 0; i < 5; i++)
            k_on(t + step + (uint32_t)i * step / 2u, degree_note(7 + 2 * i), 90, SL_BISHOP, 1.0f);
    if (!c_legal(&after, (cmove_t[128]){0}) && check) {   /* mate: the bells, the chord of the key */
        for (i = 0; i < 4; i++)
            k_on(t + 3u * step + (uint32_t)i * 2205u, degree_note(2 * i), 100, SL_BELLS, 3.0f);
        k_on(t + 3u * step, degree_note(-14), 90, SL_KING, 8.0f);
    }
    flash_t[FILE_OF(m.to) + 8 * RANK_OF(m.to)] = t + step;
    flash_c[FILE_OF(m.to) + 8 * RANK_OF(m.to)] = (uint8_t)(side > 0 ? 1 : 2);
}

/* one step of the sweep, scheduled at t: the file's pieces may sound; a bar's first step may bring the move */
static void step_at(uint32_t t)
{
    uint32_t file = step_no % 8u, bar = step_no / 8u, sp = step_samples(), i, n = 0;
    int r, cand[8], nc = 0;
    step_hist[step_no % 16u].t = t;
    step_hist[step_no % 16u].file = (uint8_t)file;
    if (!st.over && pending_ok && !apply_t &&
        (pending_now || (file == 0u && bar % st.move_every == 0u && bar > 0u))) {
        apply_t = t + sp;                /* the board changes as the second note sounds */
        move_sounds(t, sp);
    }
    if (file == 0u && bar % 2u == 0u) {  /* the kings' drone, every other bar, quietly */
        for (i = 0; i < 2u; i++) {
            int ks = pos.kpos[i];
            if (kr_pc(55))
                sound(t, i ? -KING : KING, ks, -10);
        }
    }
    for (r = 0; r < 8; r++) {            /* the file's other pieces */
        int s = SQ((int)file, r), pc = pos.b[s], ty = pc < 0 ? -pc : pc;
        uint32_t ch;
        if (!pc || ty == KING)
            continue;
        ch = (uint32_t)VOICE[ty].chance * st.density / 50u;
        if (kr() % 100u < ch)
            cand[nc++] = s;
    }
    if (eng_active() >= 7u)              /* nearly all ten voices sounding: this step rests (no voice is cut off) */
        nc = 0;
    while (nc && n < 3u) {               /* three at most a step: never a cluster */
        int k = (int)(kr() % (uint32_t)nc), s = cand[k];
        sound(t + (uint32_t)kr_in(0, (int32_t)sp / 12), pos.b[s], s, 0);
        cand[k] = cand[--nc];
        n++;
    }
}

/* -------------------------------------------------------------------- audio --- */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12) { k_render(lr, n, master_q12); }
void app_panic(void) { k_panic(); }
void app_overload(void) { eng_shed(); }

/* -------------------------------------------------------------------- UI --- */
static struct {
    char msg[2][24];
    uint32_t msg_until, keys, btn, drawn_at;
    int32_t held[NKEYS];
    int8_t rep_k;                        /* the direction key held (it repeats), -1 none */
    uint32_t rep_at;
} ui = {.rep_k = -1};

void app_say(const char *a, const char *b)
{
    uint32_t i;
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    if (ann_on()) {
        char t[48];
        t[0] = 0;
        tcat(t, ui.msg[0]);
        tcat(t, " ");
        tcat(t, ui.msg[1]);
        ann_say(t, 0, ANN_NOW);
    }
}

static void fx(void)                     /* the room: long and wide, shimmering */
{
    kp_defaults(&k_main);
    k_main.rev_mix = 0.36f;
    k_main.rev_secs = 7.0f;
    k_main.rev_dark = 0.35f;
    k_main.shimmer = 0.22f;
    k_main.dly_samples = step_samples() * 3.0f;
    k_main.dly_fb = 0.42f;
    k_main.dly_mix = 0.2f;
    k_main.chorus = 0.3f;
    k_main.lfo_shape = LFO_DRIFT;
    k_main.lfo_hz = 0.2f;
    k_main.vib = 0.04f;
}

static void knob_say(const char *name, int32_t v, const char *unit)
{
    char t[16];
    itos(v, t);
    tcat(t, unit);
    app_say(name, t);
}

/* your move: the cursor, and A on a piece of yours picks it, on one of its squares plays it, on it again lets go */
static int can_move_from(const cmove_t *m, int n, int sq)
{
    int i;
    for (i = 0; i < n; i++)
        if (m[i].from == sq)
            return 1;
    return 0;
}
static void cursor(int df, int dr)
{
    int f = clamp(FILE_OF(you.cur) + df, 0, 7), r = clamp(RANK_OF(you.cur) + dr, 0, 7);
    you.cur = (int8_t)SQ(f, r);
    k_on(0, degree_note(r + 7), 50, SL_MOVE, 0.15f);   /* the cursor sings its rank, softly */
}
static void select_here(void)
{
    cmove_t m[128];
    int n, i;
    if (!your_turn()) {
        app_say("WAIT", st.over ? "GAME OVER" : "MY MOVE");
        return;
    }
    n = c_legal(&pos, m);
    if (you.from >= 0) {
        if (you.cur == you.from) {       /* again: let it go */
            you.from = -1;
            return;
        }
        for (i = 0; i < n && !(m[i].from == you.from && m[i].to == you.cur); i++)
            ;
        if (i < n) {                     /* one of its squares: the move */
            pending = m[i];
            pending_ok = 1;
            pending_blunder = 0;
            pending_now = 1;
            you.from = -1;
            return;
        }
    }
    if (pos.b[you.cur] * pos.side > 0 && can_move_from(m, n, you.cur)) {   /* a piece of yours that can move: picked */
        you.from = you.cur;
        k_on(0, piece_note(pos.b[you.cur], you.cur) + 12, 80, SL_MOVE, 0.4f);
        juice_ring(4 + FILE_OF(you.cur) * 24 + 12, 25 + (7 - RANK_OF(you.cur)) * 24 + 12, 16, C_CYAN, 250);
        return;
    }
    app_say(you.from >= 0 ? "IT CANNOT GO" : pos.b[you.cur] * pos.side > 0 ? "IT CANNOT MOVE" : "NOT YOURS",
            you.from >= 0 ? "THERE" : "");
}

/* ------------------------------------------------------------------ modes --- */
/* the games, as the menu's grid has them (three a row). Page 1, the favourites: the arcade three on top, PINBALL
 * in the middle with CHESS and SOLITAIRE, WORM, CUBES and DEFUSE below. Page 2, the rest, DICE SYNTH in its middle
 * (Nick, 17 Nov 2026) */
enum { M_MENU,
       M_BRICK, M_SWARM, M_DRIFT,        /* page 1 */
       M_CHESS, M_PINBALL, M_SOLI,
       M_SNAKE, M_CUBE, M_MINES,
       M_GOLF, M_DROP, M_BOUNCE,         /* page 2 */
       M_LIGHTS, M_DICE, M_CHECK,
       M_FOUR, M_2048,
       NMODE };
static const char *const MODE_NAME[NMODE] = {"MENU",
                                             "BOUNCE", "SWARM", "DRIFT",
                                             "CHESS", "PINBALL", "SOLITAIRE",
                                             "WORM", "CUBES", "DEFUSE",
                                             "GOLF", "DROP", "DODGE",
                                             "LIGHTS", "DICE SYNTH", "CHECKERS",
                                             "ROWS", "2048"};
static uint8_t mode = M_MENU, menu_at = M_PINBALL;   /* menu_at 0: the PLAYERS switch */
/* K4 in the games: how loud their effects are (kit.h's k_fx); chess (its knobs its own), the menu and DICE SYNTH
 * (an instrument, its knobs its own) keep theirs */
static uint8_t fx_level = 100;
static void fx_apply(void) { k_fx = mode == M_CHESS || mode == M_MENU || mode == M_DICE ? 100u : fx_level; }
static uint32_t menu_scale_at;           /* the scale or key changed (ms): its name in the corner lights */
static uint32_t menu_moved_at;           /* the cursor moved (ms): the new tile pops */
static uint8_t players = 1;              /* 1: against the computer (or alone); 2: taking turns */
static const char *const DIFF_NAME[5] = {"", "EASY", "MEDIUM", "HARD", "EXPERT"};
static int board_game(uint32_t m)
{
    return m == M_CHESS || m == M_CHECK || m == M_FOUR;
}
static int score_game(uint32_t m)
{
    return m == M_CUBE || m == M_2048 || m == M_SNAKE || m == M_MINES || m == M_BRICK || m == M_SWARM ||
           m == M_DRIFT;
}
static int solo_game(uint32_t m)
{
    return m == M_SOLI || m == M_LIGHTS || m == M_PINBALL || m == M_GOLF ||
           m == M_DROP || m == M_BOUNCE || m == M_DICE;
}   /* one player */
static int arcade_game(uint32_t m) { return m == M_BRICK || m == M_SWARM || m == M_DRIFT; }

/* the games' state: one block they share, a game at a time (enter() clears it) */
static uint32_t g_arena[1536];           /* 6 KiB */
static void arena_clear(void)
{
    uint32_t i;
    for (i = 0; i < sizeof g_arena / sizeof g_arena[0]; i++)
        g_arena[i] = 0;
}

/* a game's header: its name in two colours, a word on the right; a message over it for a moment */
static void header(const char *a, uint16_t ca, const char *b, uint16_t cb, const char *right)
{
    int32_t x;
    if (plat_ms() < ui.msg_until) {
        x = cv_text(6, 2, &FONT_B, ui.msg[0], C_INK);
        cv_text(x + 8, 2, &FONT_B, ui.msg[1], C_YEL);
        return;
    }
    x = cv_text(6, 2, &FONT_B, a, ca);
    cv_text(x + 5, 2, &FONT_B, b, cb);
    if (right && right[0])
        text_r(234, 4, &FONT_XS, right, C_GHOST);
}

static uint32_t grid_next(void);         /* the next sixteenth of the groove not yet past (below) */
static uint32_t grid_next_n(uint32_t n); /* the next n-th of a beat's quarter (2: the next eighth) */
#include "seq.h"
#include "soli.h"
#include "cube.h"
#include "games2.h"
#include "board2.h"
#include "games3.h"
#include "arcade.h"
#include "pinball.h"
#include "golf.h"
#include "toys.h"
#include "dice.h"

/* --------------------------------------------- two players, taking turns --- */
/* a score game for two: player 1 plays a round, then player 2, then the scores side by side */
static struct {
    uint8_t turn;                        /* 0 player 1, 1 player 2 */
    uint8_t phase;                       /* 0 playing, 1 player 2's turn next (A starts it), 2 the results */
    uint32_t score[2];
    uint32_t ended_at;                   /* the game ended (ms), 0 not yet */
} tp;
static int game_over_now(void)
{
    switch (mode) {
    case M_CUBE: return cu.over;
    case M_2048: return tw.over;
    case M_SNAKE: return sn.over;
    case M_MINES: return mi.over;
    case M_BRICK: return br.over;
    case M_SWARM: return sw.over;
    case M_DRIFT: return dr.over;
    default: return 0;
    }
}

/* the keys each game listens to (as pitch classes: bit 0 C, 1 C#, 2 D, 4 E, 9 A), so only those light. C left,
 * E right, C# up, D down, A select; A also lit whenever it's what starts the next game or turn */
#define PK_C (1u << 0)
#define PK_U (1u << 1)
#define PK_D (1u << 2)
#define PK_E (1u << 4)
#define PK_A (1u << 9)
static uint32_t play_pcs(void)
{
    uint32_t all = PK_C | PK_U | PK_D | PK_E | PK_A, arrows = PK_C | PK_U | PK_D | PK_E, w;
    switch (mode) {
    case M_BRICK: w = PK_C | PK_E | PK_A; break;               /* the paddle: left, right; A launches */
    case M_SWARM: w = PK_C | PK_U | PK_D | PK_E | PK_A; break; /* the rocket: left, right, a little up and down; A fires */
    case M_DRIFT: w = PK_C | PK_E | PK_U | PK_A; break;          /* turn, thrust (up), fire */
    case M_FOUR: w = PK_C | PK_E | PK_A; break;                  /* a column, then drop */
    case M_GOLF: w = gf.phase == 6 ? PK_U | PK_D | PK_A : PK_C | PK_E | PK_A | PK_D; break;   /* aim; A (or D)
                                                                  * held: the putt (first, the course: up, down, A) */
    case M_BOUNCE: w = arrows; break;                            /* DODGE: move, nothing else */
    case M_SNAKE: case M_2048: w = arrows; break;                /* steer, slide */
    default: w = all; break;                                     /* the menu, the board and puzzle games, CUBE
                                                                  * DROP, BUMPIN BALL, DEFUSE, LIGHTS, SOLITAIRE */
    }
    if (mode != M_MENU && (game_over_now() || tp.phase))   /* A: again, or the next player's turn */
        w |= PK_A;
    return w;
}
static uint32_t game_score(void)
{
    switch (mode) {
    case M_CUBE: return cu.score;
    case M_2048: return tw.score;
    case M_SNAKE: return (uint32_t)(sn.len - 3);
    case M_MINES: return mi_score();
    case M_BRICK: return br.score;
    case M_SWARM: return sw.score;
    case M_DRIFT: return dr.score;
    default: return 0;
    }
}
static void game_restart(void)
{
    switch (mode) {
    case M_CUBE: cu_new(); break;
    case M_2048: tw_new(); break;
    case M_SNAKE: sn_new(); break;
    case M_MINES: mi_new(); break;
    case M_BRICK: br_new(); break;
    case M_SWARM: sw_new(); break;
    case M_DRIFT: dr_new(); break;
    default: break;
    }
}
static void turns_tick(void)
{
    if (players != 2 || !score_game(mode) || tp.phase)
        return;
    if (!game_over_now()) {
        tp.ended_at = 0;
        return;
    }
    if (!tp.ended_at)
        tp.ended_at = plat_ms() | 1u;
    if (plat_ms() - tp.ended_at < 1500u)   /* a moment to see how it ended */
        return;
    tp.score[tp.turn] = game_score();
    tp.phase = (uint8_t)(tp.turn == 0 ? 1 : 2);
    k_on(0, degree_note(tp.phase == 1 ? 9 : 14), 90, 0, 0.6f);
}
static int turns_select(void)            /* A, while a turn changes hands: 1 used */
{
    if (players != 2 || !score_game(mode))
        return 0;
    if (tp.phase == 1) {
        tp.turn = 1;
        tp.phase = 0;
        tp.ended_at = 0;
        game_restart();
        return 1;
    }
    if (tp.phase == 2) {
        tp.turn = 0;
        tp.phase = 0;
        tp.ended_at = 0;
        tp.score[0] = tp.score[1] = 0;
        game_restart();
        return 1;
    }
    return game_over_now();              /* (its own "A: again" waits for the turn) */
}
static void turns_scene(void)
{
    char t[24], n[12];
    if (players != 2 || !score_game(mode))
        return;
    if (!tp.phase) {                     /* whose turn: a tag on the right of the header */
        cv_rect(186, 0, 54, 20, C_BG);
        text_r(234, 4, &FONT_XS, tp.turn ? "PLAYER 2" : "PLAYER 1", tp.turn ? C_PINK : C_CYAN);
        return;
    }
    cv_rect(20, 60, 200, 120, C_BG);
    frame_rect(20, 60, 200, 120, C_YEL);
    frame_rect(21, 61, 198, 118, C_YEL);
    itos((int32_t)tp.score[0], n);
    tcpy(t, "PLAYER 1   ");
    tcat(t, n);
    text_c(120, 72, &FONT_B, t, C_CYAN);
    if (tp.phase == 1) {
        text_c(120, 104, &FONT_M, "PLAYER 2", C_PINK);
        text_c(120, 130, &FONT_XS, "YOUR TURN", C_INK);
        text_c(120, 156, &FONT_T, "A: START", C_GHOST);
    } else {
        itos((int32_t)tp.score[1], n);
        tcpy(t, "PLAYER 2   ");
        tcat(t, n);
        text_c(120, 94, &FONT_B, t, C_PINK);
        text_c(120, 122, &FONT_M, tp.score[0] > tp.score[1] ? "P1 WINS!" : tp.score[1] > tp.score[0] ? "P2 WINS!"
                                                                                                  : "A TIE!",
               C_YEL);
        text_c(120, 156, &FONT_T, "A: AGAIN   HOME: MENU", C_GHOST);
    }
}

/* the music under every game: the heat your play builds (kit.h) grows a haze over the game's own groove: held
 * tones of the chord it's on, soft and few, at no set place in the bar (no run up and down the chord: Nick, 17 Nov
 * 2026, it was too specific a tune over every game); warm, one now and then on a beat, hot, a few more, an octave
 * up. A big moment's stinger lands on the next sixteenth (a phrase up the chord, or the whole fanfare); and each beat
 * is sent to the screen to pulse on */
#define SL_MUSIC 11u
static void music_layer(uint32_t t, uint32_t step, uint32_t six)
{
    static uint8_t last;                 /* the chord tone it held last: never the same twice */
    int32_t heat = J.heat, ahead = (int32_t)(t - k_now());
    uint32_t p = step % 16u, want;
    if (step % 4u == 0u)
        juice_beat_at(plat_ms() + (uint32_t)(ahead > 0 ? ahead / 44 : 0), step % 16u == 0u);
    if (J.sting) {
        int i, n = J.sting == 2 ? 8 : 3;
        for (i = 0; i < n; i++)
            k_on(t + (uint32_t)i * six, h_note(J.sting == 2 ? 1 + i : 3 + i) + 12, 92 + 4 * i, SL_MUSIC, 0.5f);
        if (J.sting == 2)
            k_on(t + 8u * six, h_note(1) + 24, 110, SL_MUSIC, 1.6f);
        J.sting = 0;
        return;
    }
    if (heat < 25 || p % 4u)             /* (only ever on a beat) */
        return;
    want = heat >= 60 ? 45u : 22u;       /* the chance a beat gets a tone, percent */
    if (kr() % 100u >= want)
        return;
    {
        uint8_t d;
        do
            d = (uint8_t)(1u + kr() % 5u);
        while (d == last);
        last = d;
        k_on(t, h_note(d) + (heat >= 60 && (kr() & 1u) ? 24 : 12), (uint32_t)(22 + heat / 5), SL_MUSIC,
             heat >= 60 ? 0.9f : 1.4f);
    }
}

/* the groove of the games that have one: sixteenths, scheduled a little ahead */
static uint32_t gr_next_t, gr_step;
static uint32_t gr_six;                  /* the groove's sixteenth now, in samples */
static uint32_t grid_next(void)          /* the next sixteenth not yet past: what a toy's collision plays on (it can't
                                          * be one the groove hasn't reached: those are all ahead of this) */
{
    uint32_t now = k_now() + (uint32_t)(SR_F * 0.006f), t = gr_next_t;
    if (!gr_six)
        return now;
    while ((int32_t)(t - gr_six - now) >= 0)
        t -= gr_six;
    return t;
}
static uint32_t grid_next_n(uint32_t n)
{
    uint32_t now = k_now() + (uint32_t)(SR_F * 0.006f), t = gr_next_t, s = gr_step;
    if (!gr_six)
        return now;
    while ((int32_t)(t - gr_six - now) >= 0)
        t -= gr_six, s--;
    while (n > 1u && s % n)
        t += gr_six, s++;
    return t;
}
static void groove_reset(void)
{
    gr_next_t = k_at(0.2f);
    gr_step = 0;
    h_reset();
}
static void groove_run(void)
{
    uint32_t bpm = mode == M_CUBE ? cu_bpm() : mode == M_SNAKE ? sn_bpm() : mode == M_CHECK ? 84u : mode == M_SWARM ? sw_bpm() : mode == M_DRIFT ? dr_bpm()
                 : sq_game() >= 0 ? sq_cfg[sq_game()].bpm : 72u;
    uint32_t six = (uint32_t)(SR_F * 15.0f / (float)bpm);
    gr_six = six;
    k_in_music = 1;
    while ((int32_t)(gr_next_t - k_at(0.2f)) < 0) {
        switch (mode) {
        case M_CUBE: cu_groove(gr_next_t, gr_step); break;
        case M_SNAKE: sn_groove(gr_next_t, gr_step); break;
        case M_DICE: break;              /* (an instrument: no groove under it) */
        case M_MINES: mi_groove(gr_next_t, gr_step); break;
        case M_2048: tw_groove(gr_next_t, gr_step); break;
        case M_CHECK: ck_groove(gr_next_t, gr_step); break;
        case M_FOUR: f_groove(gr_next_t, gr_step); break;
        case M_LIGHTS: li_groove(gr_next_t, gr_step); break;
        case M_BRICK: br_groove(gr_next_t, gr_step); break;
        case M_SWARM: sw_groove(gr_next_t, gr_step); break;
        case M_DRIFT: dr_groove(gr_next_t, gr_step); break;
        case M_PINBALL: pb_groove(gr_next_t, gr_step); break;
        case M_GOLF: gf_groove(gr_next_t, gr_step); break;
        case M_DROP: dp_groove(gr_next_t, gr_step); break;
        case M_BOUNCE: bx_groove(gr_next_t, gr_step); break;
        default: so_groove(gr_next_t, gr_step); break;
        }
        if (mode != M_MENU && mode != M_DICE)
            music_layer(gr_next_t, gr_step, six);
        gr_next_t += six;
        gr_step++;
    }
    k_in_music = 0;
}

static void chess_sounds(void);
static void enter(uint32_t m)            /* into a game (or the menu): its sounds, a fresh start */
{
    k_push(0, KE_KILL, 0, 0, 0, 0);      /* every voice stopped and every waiting note dropped, before the slots
                                          * change: a note left sounding (or still to come) would play on with the
                                          * next game's sound in its slot, a stray tone (Nick heard it on HOME). The
                                          * reverb's tail carries on, so it doesn't click */
    mode = (uint8_t)m;
    fx_apply();
    juice_reset();
    if (m != M_MENU)
        arena_clear();
    fx();
    k_slot_set(SL_MUSIC, &KSND[S_CELESTA]);   /* the music layer's sparkle (no game's own slot) */
    tp.turn = tp.phase = 0;
    tp.ended_at = 0;
    tp.score[0] = tp.score[1] = 0;
    switch (m) {
    case M_CHESS:
        st.vs = (uint8_t)(players == 2 ? 2 : 1);
        chess_sounds();
        game_new();
        next_step_t = k_at(0.3f);
        st.paused = 0;
        break;
    case M_SOLI:                         /* a bigger room, a longer echo: the keys hang in it */
        so_sounds();
        so_room();
        so_deal();
        break;
    case M_CUBE:
        cu_sounds();
        k_main.rev_mix = 0.16f, k_main.rev_secs = 1.8f, k_main.shimmer = 0.0f;
        k_main.dly_mix = 0.1f;
        cu_new();
        break;
    case M_2048: tw_sounds(); tw_new(); break;
    case M_SNAKE:
        sn_sounds();
        k_main.rev_mix = 0.14f, k_main.shimmer = 0.0f;
        sn_new();
        break;
    case M_DICE: dc_sounds(); break;
    case M_MINES: mi_sounds(); mi_new(); break;
    case M_CHECK: ck_sounds(); ck_new(); break;
    case M_FOUR: f_sounds(); f_new(); break;
    case M_LIGHTS: li_sounds(); li_new(); break;
    case M_BRICK: arc_sounds(0); br_new(); break;
    case M_SWARM: arc_sounds(1); sw_new(); break;
    case M_DRIFT: arc_sounds(2); dr_new(); break;
    case M_PINBALL: pb_sounds(); pb_new(); break;
    case M_GOLF: gf_sounds(); gf_new(); break;
    case M_DROP: toy_sounds(SQ_DROP); dp_new(); break;
    case M_BOUNCE: toy_sounds(SQ_BOUNCE); bx_new(); break;
    default: so_sounds(); break;
    }
    groove_reset();
}

/* the five keys, in any octave: C left, E right, C# up, D down, A select */
enum { D_NONE, D_LEFT, D_RIGHT, D_UP, D_DOWN, D_SEL };
static int key_dir(uint32_t k)
{
    switch ((53u + k) % 12u) {
    case 0: return D_LEFT;
    case 4: return D_RIGHT;
    case 1: return D_UP;
    case 2: return D_DOWN;
    case 9: return D_SEL;
    default: return D_NONE;
    }
}
static void act(int d)                   /* a direction or a select, to the game on screen */
{
    int dx = d == D_LEFT ? -1 : d == D_RIGHT ? 1 : 0, dy = d == D_UP ? 1 : d == D_DOWN ? -1 : 0, sel = d == D_SEL;
    if (sel && turns_select())
        return;
    if (players == 2 && score_game(mode) && (tp.phase || game_over_now()))
        return;
    switch (mode) {
    case M_MENU:
        if (dy > 0 && menu_at && (menu_at - 1) % 9 < 3)
            menu_at = 0;                 /* up from a top row: the PLAYERS switch */
        else if (dy < 0 && menu_at == 0)
            menu_at = 1;
        else if (menu_at && (dx || dy)) {   /* a grid of nine a page; off its right edge, the next page */
            int i = menu_at - 1, pg = i / 9, r = i % 9 / 3 - dy, c = i % 3 + dx, n = NMODE - 1;
            if (c > 2 && (pg + 1) * 9 < n)
                pg++, c = 0;
            else if (c < 0 && pg > 0)
                pg--, c = 2;
            r = clamp(r, 0, 2), c = clamp(c, 0, 2);
            i = pg * 9 + r * 3 + c;
            while (i >= n)               /* (a page not full: the last there is) */
                i--;
            menu_at = (uint8_t)(1 + i);
        }
        if (dx || dy) {
            /* the chime: a tone of the chord the menu's music is on (not just the scale: those clashed, Nick heard
             * it), up the chord across a page (page 2 the same as page 1); the PLAYERS switch below them */
            k_on(0, menu_at ? h_tone(2 + (menu_at - 1) % 9) : h_tone(1), 50, SS_PICK, 0.25f);   /* (each page the same
                                                                                * notes: Nick found page 2's higher ones too high) */
            menu_moved_at = plat_ms() | 1u;
        }
        if (sel) {
            if (menu_at == 0) {
                players = (uint8_t)(3 - players);
                k_on(0, h_tone(players == 2 ? 8 : 6), 80, SS_PICK, 0.4f);
            } else if (players == 2 && solo_game(menu_at))
                app_say(MODE_NAME[menu_at], "IS ONE PLAYER");
            else
                enter(menu_at);
        }
        break;
    case M_CHESS:
        if (!st.vs)
            break;
        if (sel)
            select_here();
        else if (dx || dy)
            cursor(dx, dy);
        break;
    case M_SOLI:
        if (sel)
            so_select();
        else
            so_nav(dx, dy);
        break;
    case M_CUBE: cu_nav(dx, dy, sel); break;
    case M_2048: tw_nav(dx, dy, sel); break;
    case M_SNAKE:
        if (sel && sn.over)
            sn_new();
        else
            sn_nav(dx, dy, sel);
        break;
    case M_MINES: mi_nav(dx, dy, sel); break;
    case M_CHECK: ck_nav(dx, dy, sel); break;
    case M_FOUR: f_nav(dx, dy, sel); break;
    case M_LIGHTS: li_nav(dx, dy, sel); break;
    case M_BRICK: br_nav(dx, dy, sel); break;
    case M_SWARM: sw_nav(dx, dy, sel); break;
    case M_DRIFT: dr_nav(dx, dy, sel); break;
    case M_PINBALL: pb_nav(dx, dy, sel); break;
    case M_GOLF: gf_nav(dx, dy, sel); break;
    case M_DROP: dp_nav(dx, dy, sel); break;
    case M_BOUNCE: bx_nav(dx, dy, sel); break;
    default: break;
    }
}
static int repeats(int d)
{
    return d != D_SEL && !(mode == M_CUBE && d == D_UP) && mode != M_2048 && mode != M_SNAKE && !arcade_game(mode) &&
           mode != M_PINBALL && mode != M_GOLF &&
           mode != M_DROP && mode != M_BOUNCE;   /* (a move a press: a
                                                                     * handshake's and a stroke's are the game) */
}

static void input(void)
{
    uint32_t keys, btn = plat_buttons(), k, pressed;
    int32_t e;
    touch_poll();
    keys = touch_keys();
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit)) {
            static const uint8_t WK[16] = PLAT_WHITE_KEYS;
            int deg = 0, i;
            for (i = 0; i < 16; i++)
                if (WK[i] <= k)
                    deg = i;
            if (mode == M_DICE) {        /* dice synth: the keys play the sound */
                dc_key(k, 1);
                ui.held[k] = -1;
                continue;
            }
            if (mode != M_CHESS || st.vs) {   /* the five keys drive the game; held, a direction repeats */
                int d = key_dir(k);
                if (d != D_NONE) {
                    act(d);
                    if (repeats(d)) {
                        ui.rep_k = (int8_t)k;
                        ui.rep_at = plat_ms() + 220u;
                    }
                }
                ui.held[k] = -1;
                continue;
            }
            /* else the keys play along: white keys up the scale, on the queen's pad */
            ui.held[k] = degree_note(deg + 2) + ((k == 1 || k == 3 || k == 5 || k == 8 || k == 10 || k == 13 ||
                                                 k == 15 || k == 17 || k == 20 || k == 22 || k == 25) ? 12 : 0);
            k_on(0, ui.held[k], touch_vel(k), SL_PLAY, 0.0f);
        } else if (!(keys & bit) && (ui.keys & bit) && mode == M_DICE) {
            dc_key(k, 0);
        } else if (!(keys & bit) && (ui.keys & bit) && mode == M_MINES && key_dir(k) == D_SEL) {
            mi_release();                /* mines: a tap of A opens, a hold flags */
        } else if (!(keys & bit) && (ui.keys & bit) && ui.held[k] >= 0) {
            k_off(0, ui.held[k]);
        }
    }
    if (ui.rep_k >= 0) {                 /* a held direction repeats */
        if (!(keys >> ui.rep_k & 1u))
            ui.rep_k = -1;
        else if ((int32_t)(plat_ms() - ui.rep_at) >= 0) {
            ui.rep_at = plat_ms() + 70u;
            act(key_dir((uint32_t)ui.rep_k));
        }
    }
    pressed = btn & ~ui.btn;
    if (pressed >> B_HOME & 1u && mode != M_MENU) {   /* HOME: the menu, from any game */
        enter(M_MENU);
        app_say("MENU", "");
    }
    if (pressed >> B_PLAY & 1u && mode == M_SOLI) {
        so_deal();
        app_say("NEW", "DEAL");
    }
    if (mode == M_DICE) {                /* its buttons and knobs are its own (HOME above) */
        dc_input(pressed & ~(1u << B_HOME));
        pressed = 0;
    }
    if (pressed >> B_PLAY & 1u && mode != M_CHESS && mode != M_SOLI && mode != M_MENU &&
        !(players == 2 && score_game(mode))) {
        enter(mode);
        app_say("NEW", "GAME");
    }
    if (mode != M_CHESS)
        pressed &= ~(1u << B_PLAY | 1u << B_SEL | 1u << B_HOME);
    if (pressed >> B_PLAY & 1u) {
        st.paused ^= 1u;
        app_say(st.paused ? "PAUSED" : "PLAYING", "");
        if (!st.paused)
            next_step_t = k_at(0.1f);
    }
    if (pressed >> B_SEL & 1u) {
        st.vs = (uint8_t)(st.vs ? 0 : players == 2 ? 2 : 1);
        game_new();
        app_say(st.vs == 2 ? "TWO PLAYERS" : st.vs ? "YOU VS CPU" : "CPU VS CPU", st.vs == 1 ? "YOU ARE WHITE" : "");
        if (st.vs)
            tcpy(comment, "C left, E right, C# up, D down, A select.");
    }
    if (pressed >> B_OCTDN & 1u && st.vs && you.from >= 0)   /* let the piece go */
        you.from = -1;
    if (mode != M_CHESS) {               /* the knobs are the chess music's (ALGORITHM: the difficulty, here too;
                                          * K4: EFFECTS, in every game) */
        uint32_t i;
        if (mode == M_MENU) {            /* the menu: PRESET the scale, ALGORITHM the key, for every game (they
                                          * share them); the new chord sounds at once */
            char t[16];
            int sc = 0;
            if ((e = plat_enc(EN_PRESET)) != 0) {
                st.scale = (uint8_t)clamp((int32_t)st.scale + e, 0, (int32_t)NSCALE - 1);
                sc = 1;
            }
            if ((e = plat_enc(EN_ALGO)) != 0) {
                key_shift = (int8_t)((key_shift + e + 12) % 12);
                sc = 2;
            }
            if (sc) {
                scale_name(t);
                app_say(sc == 1 ? "SCALE" : "KEY", t);
                h_pad(0, 60, SS_PAD, 1.6f, 0);
                menu_scale_at = plat_ms() | 1u;
            }
        }
        if (mode == M_DROP) {            /* NOTE DROP: K1..K4 tilt its platforms, SELECT the board */
            int k;
            for (k = 0; k < 4; k++)
                if ((e = plat_enc(EN_K1 + k)) != 0)
                    dp_tilt(k, e);
            if ((e = plat_enc(EN_SELECT)) != 0) {
                dp_set_board(e);
                app_say("BOARD", DP_NAME[ndrop.board]);
            }
        }
        if ((e = plat_enc(EN_K4)) != 0) {
            fx_level = (uint8_t)clamp((int32_t)fx_level + 5 * e, 0, 100);
            fx_apply();
            if (fx_level)
                knob_say("EFFECTS", fx_level, "%");
            else
                app_say("EFFECTS", "OFF");
        }
        if (sq_game() >= 0) {            /* the sequencer games, played as instruments (seq.h): K1 TEMPO, K2 CHANCE,
                                          * K3 SOUND, PRESET the scale, ALGORITHM the key */
            char t[16];
            int g = sq_game();
            if ((e = plat_enc(EN_K1)) != 0) {
                if (g == SQ_CUBE)        /* CUBE DROP: its levels set the tempo */
                    app_say("TEMPO", "BY THE LEVEL");
                else {
                    sq_cfg[g].bpm = (uint8_t)clamp((int32_t)sq_cfg[g].bpm + e, 40, 180);
                    if (g == SQ_SOLI)
                        so_room();
                    knob_say("TEMPO", sq_cfg[g].bpm, " BPM");
                }
            }
            if ((e = plat_enc(EN_K2)) != 0) {
                if (g == SQ_BOUNCE) {    /* DODGE: the balls' speed */
                    toy_speed = (uint8_t)clamp((int32_t)toy_speed + e, 1, 10);
                    knob_say("SPEED", toy_speed, "");
                } else {
                    sq_cfg[g].chance = (uint8_t)clamp((int32_t)sq_cfg[g].chance + 5 * e, 0, 100);
                    knob_say("CHANCE", sq_cfg[g].chance, "%");
                }
            }
            if ((e = plat_enc(EN_K3)) != 0) {
                sq_cfg[g].inst = (uint8_t)clamp((int32_t)sq_cfg[g].inst + e, 0, (int32_t)SQ_NINST - 1);
                sq_sound(g);
                app_say("SOUND", KSND[SQ_INST[sq_cfg[g].inst]].name);
            }
            if ((e = plat_enc(EN_PRESET)) != 0) {
                st.scale = (uint8_t)clamp((int32_t)st.scale + e, 0, (int32_t)NSCALE - 1);
                scale_name(t);
                app_say("SCALE", t);
            }
            if ((e = plat_enc(EN_ALGO)) != 0) {
                key_shift = (int8_t)((key_shift + e + 12) % 12);
                scale_name(t);
                app_say("KEY", t);
            }
        }
        if ((e = plat_enc(EN_ALGO)) != 0 && (mode == M_CHECK || mode == M_FOUR)) {
            st.depth = (uint8_t)clamp(st.depth + e, 1, 4);
            app_say("DIFFICULTY", DIFF_NAME[st.depth]);
        }
        for (i = 0; i < NE; i++)
            (void)plat_enc((int)i);
    }
    if ((e = plat_enc(EN_K1)) != 0) {
        st.tempo = (uint8_t)clamp(st.tempo + e, 40, 140);
        fx();
        knob_say("TEMPO", st.tempo, " BPM");
    }
    if ((e = plat_enc(EN_K2)) != 0) {
        st.density = (uint8_t)clamp(st.density + 2 * e, 0, 100);
        knob_say("DENSITY", st.density, "%");
    }
    if ((e = plat_enc(EN_K3)) != 0) {
        static const uint8_t EV[3] = {1, 2, 4};
        int i = st.move_every == 4 ? 2 : st.move_every == 2 ? 1 : 0;
        i = clamp(i + e, 0, 2);
        st.move_every = EV[i];
        knob_say("MOVE EVERY", st.move_every, st.move_every == 1 ? " BAR" : " BARS");
    }
    if ((e = plat_enc(EN_K4)) != 0) {
        st.blunder = (uint8_t)clamp(st.blunder + 2 * e, 0, 60);
        knob_say("BLUNDER", st.blunder, "%");
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {
        char t[16];
        st.scale = (uint8_t)clamp((int32_t)st.scale + e, 0, (int32_t)NSCALE - 1);
        scale_name(t);
        app_say("SCALE", t);
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {
        st.depth = (uint8_t)clamp(st.depth + e, 1, 4);
        app_say("DIFFICULTY", DIFF_NAME[st.depth]);
    }
    (void)plat_enc(EN_SELECT);
    ui.keys = keys;
    ui.btn = btn;
    plat_glow(1);
    {
        uint32_t lit = 0, i;
        if (mode == M_DROP || mode == M_DICE)   /* NOTE DROP, DICE SYNTH: every key plays, so every key is lit */
            lit = (1u << NKEYS) - 1u;
        else if (mode != M_CHESS || your_turn()) {   /* the keys this game uses now, lit, in every octave */
            uint32_t pcs = play_pcs();
            for (i = 0; i < NKEYS; i++)
                if (pcs >> ((53u + i) % 12u) & 1u)
                    lit |= 1u << i;
        }
        if (mode != M_CHESS)
            plat_leds(mode == M_DICE ? dc_lit() | btn : (mode != M_MENU ? 1u << B_HOME : 0u) | (mode == M_SOLI || mode == M_CUBE ? 1u << B_PLAY : 0u) |
                      btn, lit | keys);
        else
        plat_leds((st.paused ? 0u : 1u << B_PLAY) | 1u << B_HOME | (st.vs ? 1u << B_SEL : 0u) |
                  (st.vs && you.from >= 0 ? 1u << B_OCTDN : 0u) | btn, lit | keys);
    }
}
void app_input_only(void) { input(); }

/* ---------------------------------------------------------------- drawing --- */
static const char *const SPR[7][10] = {
    {0},
    {"..........", "....aa....", "...aaaa...", "...aaaa...", "....aa....", "...aaaa...", "...aaaa...",
     "..aaaaaa..", ".aaaaaaaa.", ".bbbbbbbb."},
    {"..........", "...aaa....", "..aaaaa...", ".aaabaaa..", ".aaaaaaa..", "..bbaaaa..", "....aaaa..",
     "...aaaaa..", "..aaaaaaa.", "..bbbbbbb."},
    {"....aa....", "...aaaa...", "..aabaaa..", "..aaabaa..", "..aaaaaa..", "...aaaa...", "...aaaa...",
     "..aaaaaa..", ".aaaaaaaa.", ".bbbbbbbb."},
    {"..........", ".aa.aa.aa.", ".aaaaaaaa.", "..aaaaaa..", "..aaaaaa..", "..aaaaaa..", "..aaaaaa..",
     ".aaaaaaaa.", ".aaaaaaaa.", ".bbbbbbbb."},
    {".a..aa..a.", ".aa.aa.aa.", ".aaaaaaaa.", "..aaaaaa..", "...aaaa...", "...aaaa...", "..aaaaaa..",
     ".aaaaaaaa.", ".aaaaaaaa.", ".bbbbbbbb."},
    {"....aa....", "...aaaa...", "....aa....", "..aaaaaa..", ".aaaaaaaa.", "..aaaaaa..", "...aaaa...",
     "..aaaaaa..", ".aaaaaaaa.", ".bbbbbbbb."},
};
#define BX 4
#define BY 25
#define SQS 24
/* the juice: a move slides (and overshoots), a taken piece is knocked off the board spinning, the board shakes as hard
 * as the piece was worth; check rings the king, mate blows the board up */
static struct {
    int8_t from, to, cap;                /* the move being drawn sliding, and the piece it took */
    uint32_t at;
    int32_t cx, cy, cvx, cvy;            /* the taken piece flying (1/16 px) */
    uint32_t check_at;
    int8_t king;
} cj;
static const int8_t PIECE_VAL[7] = {0, 1, 3, 3, 5, 9, 0};
static void chess_juice(int from, int to, int cap, int check, int mate, int promo, int castle)
{
    int32_t x = BX + FILE_OF(to) * SQS + SQS / 2, y = BY + (7 - RANK_OF(to)) * SQS + SQS / 2;
    cj.from = (int8_t)from;
    cj.to = (int8_t)to;
    cj.at = plat_ms() | 1u;
    cj.cap = (int8_t)cap;
    cj.check_at = 0;
    if (cap) {
        int v = PIECE_VAL[cap < 0 ? -cap : cap];
        static const char *const NM[7] = {"", "", "KNIGHT!", "BISHOP!", "ROOK!", "QUEEN!", ""};
        cj.cx = (x - SQS / 2) * 16;
        cj.cy = (y - SQS / 2) * 16;
        cj.cvx = (FILE_OF(to) < 4 ? -1 : 1) * (24 + v * 6);
        cj.cvy = -(70 + v * 8);
        juice_freeze(50u + 15u * (uint32_t)v);
        juice_shake(3 + v, 220u + 40u * (uint32_t)v);
        juice_ring(x, y, 18 + 5 * v, cap > 0 ? C_CYAN : C_PINK, 260u + 30u * (uint32_t)v);
        juice_burst(x, y, 10 + 4 * v, 2 + v / 2, C_YEL, cap > 0 ? C_CYAN : C_PINK, 1);
        juice_flash(RGB(255, 255, 255), 30 + 10 * v, 160);
        {                                /* the board knocked the way the taker came */
            int32_t dx = FILE_OF(to) - FILE_OF(from), dy = RANK_OF(from) - RANK_OF(to);
            int32_t m = iabs(dx) > iabs(dy) ? iabs(dx) : iabs(dy);
            if (m)
                juice_kick(dx * (3 + v) / m, dy * (3 + v) / m);
        }
        juice_heat(6 + 3 * v);
        juice_embers(x, y, 4 + v, cap > 0 ? C_CYAN : C_PINK);
        if (v >= 5) {                    /* a rook, a queen: the hit's frame, a second ring, a phrase */
            juice_impact(v >= 9 ? 80u : 45u);
            juice_ring(x, y, 50 + 4 * v, RGB(255, 255, 255), 420);
            juice_sting(1);
        }
        if (v >= 9)
            juice_pop(BX + 4 * SQS, BY + 4 * SQS, "QUEEN!", C_YEL);
        else if (v >= 3)
            juice_text(x, y - 14, NM[cap < 0 ? -cap : cap], C_INK);
    } else
        juice_burst(x, y + SQS / 2 - 2, 4, 1, RGB(170, 160, 200), RGB(90, 80, 110), 0);   /* a little dust */
    if (castle)
        juice_ring(x, y, 22, C_PURP, 300);
    if (promo) {
        juice_confetti(x, y, 24);
        juice_pop(BX + 4 * SQS, BY + 3 * SQS, "PROMOTED!", C_YEL);
        juice_ring(x, y, 40, C_YEL, 400);
    }
    if (check && !mate) {
        int k;
        for (k = 0; k < 128; k++)        /* the king in check: rings round it, its square pulsing a while */
            if (pos.b[k] == pos.side * KING) {
                int32_t kx = BX + FILE_OF(k) * SQS + SQS / 2, ky = BY + (7 - RANK_OF(k)) * SQS + SQS / 2;
                juice_ring(kx, ky, 26, C_PINK, 350);
                juice_ring(kx, ky, 40, RGB(255, 255, 255), 500);
                juice_burst(kx, ky, 12, 2, C_PINK, RGB(255, 255, 255), 0);
                cj.king = (int8_t)k;
                cj.check_at = plat_ms() | 1u;
                break;
            }
        juice_flash(RGB(255, 40, 80), 70, 300);
        juice_shake(5, 250);
        juice_glitch(140);
        juice_heat(15);
        juice_pop(BX + 4 * SQS, BY + 4 * SQS, "CHECK!", C_PINK);
    }
    if (mate) {
        juice_freeze(260);
        juice_shake(12, 900);
        juice_flash(RGB(255, 255, 255), 220, 900);
        juice_burst(x, y, 50, 6, C_YEL, C_PINK, 1);
        juice_confetti(BX + 4 * SQS, BY + 2 * SQS, 40);
        juice_ring(x, y, 120, C_YEL, 700);
        juice_ring(x, y, 80, C_PINK, 500);
        juice_pop(BX + 4 * SQS, BY + 2 * SQS, "CHECKMATE", C_YEL);
        juice_celebrate(x, y);
        juice_glitch(400);
    }
}
/* a sprite turned a quarter turn r times (the taken piece, spinning) */
static void sprite_rot(int32_t x, int32_t y, const char *const *rows, int32_t n, int32_t scale, const uint16_t *map, int r)
{
    int32_t a, b;
    for (a = 0; a < n; a++)
        for (b = 0; b < n; b++) {
            int32_t sr = a, sc = b;
            char ch;
            if (r == 1)
                sr = n - 1 - b, sc = a;
            else if (r == 2)
                sr = n - 1 - a, sc = n - 1 - b;
            else if (r == 3)
                sr = b, sc = n - 1 - a;
            ch = rows[sr][sc];
            if (ch && ch != '.')
                cv_rect(x + b * scale, y + a * scale, scale, scale, map[ch - 'a']);
        }
}

static void chess_scene(void)
{
    uint32_t now = k_now(), sp = step_samples(), i;
    int32_t f, r, x;
    char t[32], num[8];
    uint32_t file_now = 99u;
    for (i = 0; i < 16u; i++)            /* the step sounding now: the latest scheduled whose time has come */
        if (step_hist[i].t && (int32_t)(now - step_hist[i].t) >= 0 && now - step_hist[i].t < sp)
            file_now = step_hist[i].file;
    if (st.paused)
        file_now = 99u;
    /* the header */
    if (plat_ms() < ui.msg_until) {
        x = cv_text(6, 4, &FONT_B, ui.msg[0], C_INK);
        cv_text(x + 8, 4, &FONT_B, ui.msg[1], C_YEL);
    } else {
        cv_text(6, 4, &FONT_B, "CHESS", C_CYAN);
        tcpy(t, st.vs == 2 ? (pos.side > 0 ? "P1 (CYAN)" : "P2 (PINK)") : st.vs ? DIFF_NAME[st.depth]
                                                                                 : SCALES[st.scale].name);
        if (st.vs == 1)
            tcat(t, " VS YOU");
        text_r(234, 6, &FONT_XS, t, st.vs ? C_YEL : C_GHOST);
    }
    /* the board: white at the bottom */
    for (r = 0; r < 8; r++)
        for (f = 0; f < 8; f++) {
            int32_t sx = BX + f * SQS, sy = BY + (7 - r) * SQS, sq = SQ(f, r), k = f + 8 * r, pc = pos.b[sq];
            uint16_t c = (f + r) & 1 ? C_SQ_L : C_SQ_D;
            uint32_t ft = flash_t[k];
            if ((uint32_t)f == file_now)
                c = mix565(c, C_PURP, 70);
            if (sq == sq_last[0] || sq == sq_last[1])
                c = mix565(c, C_YEL, 60);
            if (st.vs && your_turn() && sq == you.from)   /* the piece you picked */
                c = mix565(c, C_CYAN, 170);
            if (ft && (int32_t)(now - ft) >= 0 && now - ft < 14000u)   /* it just sounded: lit, fading */
                c = mix565(c, flash_c[k] == 1 ? C_CYAN : C_PINK, 200 - (int32_t)((now - ft) / 70u));
            if (cj.check_at && sq == cj.king && pc == pos.side * KING && plat_ms() - cj.check_at < 1500u)
                c = mix565(c, C_PINK, 90 + (int32_t)(fm_sinf((float)(plat_ms() - cj.check_at) * 0.02f) * 70.0f));
            cv_rect(sx, sy, SQS, SQS, c);
            if (pc && !(sq == cj.to && cj.at && plat_ms() - cj.at < 200u)) {
                int ty = pc < 0 ? -pc : pc;
                uint16_t map[2] = {pc > 0 ? C_CYAN : C_PINK, pc > 0 ? RGB(40, 110, 130) : RGB(130, 20, 80)};
                uint16_t sh[2] = {RGB(4, 2, 10), RGB(4, 2, 10)};
                sprite(sx + 3, sy + 3, SPR[ty], 10, 2, sh);
                sprite(sx + 2, sy + 2, SPR[ty], 10, 2, map);
            }
        }
    frame_rect(BX - 1, BY - 1, 8 * SQS + 2, 8 * SQS + 2, C_LINE2);
    if (cj.at && plat_ms() - cj.at < 200u && pos.b[cj.to]) {   /* the move, sliding: eased, a touch past, back */
        float q = (float)(plat_ms() - cj.at) / 200.0f, e;
        int32_t x0 = BX + FILE_OF(cj.from) * SQS, y0 = BY + (7 - RANK_OF(cj.from)) * SQS;
        int32_t x1 = BX + FILE_OF(cj.to) * SQS, y1 = BY + (7 - RANK_OF(cj.to)) * SQS, px, py, lift;
        int pc = pos.b[cj.to], ty = pc < 0 ? -pc : pc;
        uint16_t map[2] = {pc > 0 ? C_CYAN : C_PINK, pc > 0 ? RGB(40, 110, 130) : RGB(130, 20, 80)};
        uint16_t sh[2] = {RGB(4, 2, 10), RGB(4, 2, 10)};
        q -= 1.0f;
        e = 1.0f + q * q * (2.7f * q + 1.7f);   /* ease out, with a little overshoot */
        px = x0 + (int32_t)((float)(x1 - x0) * e);
        py = y0 + (int32_t)((float)(y1 - y0) * e);
        lift = (int32_t)(fm_sinf((q + 1.0f) * 3.14159f) * 6.0f);   /* lifted off the board, set down */
        sprite(px + 3 + lift / 2, py + 3, SPR[ty], 10, 2, sh);
        sprite(px + 2, py + 2 - lift, SPR[ty], 10, 2, map);
    }
    if (cj.cap && cj.at && plat_ms() - cj.at < 1400u) {   /* the taken piece: knocked off, spinning, falling */
        uint32_t q = plat_ms() - cj.at;
        int32_t nf = (int32_t)q / 16, cx = cj.cx + cj.cvx * nf, cy = cj.cy + cj.cvy * nf + 4 * nf * nf / 2;
        int pc = cj.cap, ty = pc < 0 ? -pc : pc;
        uint16_t map[2] = {pc > 0 ? C_CYAN : C_PINK, pc > 0 ? RGB(40, 110, 130) : RGB(130, 20, 80)};
        if (cy / 16 < 260)
            sprite_rot(cx / 16, cy / 16, SPR[ty], 10, 2, map, (int)(q / 60u) & 3);
    }
    if (st.vs && your_turn()) {          /* where the picked piece can go, and the cursor */
        int32_t cx = BX + FILE_OF(you.cur) * SQS, cy = BY + (7 - RANK_OF(you.cur)) * SQS;
        if (you.from >= 0) {
            cmove_t m[128];
            int n = c_legal(&pos, m), j;
            for (j = 0; j < n; j++)
                if (m[j].from == you.from)
                    disc(BX + FILE_OF(m[j].to) * SQS + SQS / 2, BY + (7 - RANK_OF(m[j].to)) * SQS + SQS / 2, 3,
                         C_YEL);
        }
        {
            int32_t g = (plat_ms() / 140u) % 4u == 0u ? 1 : 0;   /* a beat of a breath */
            frame_rect(cx - g, cy - g, SQS + 2 * g, SQS + 2 * g, C_YEL);
            frame_rect(cx + 1 - g, cy + 1 - g, SQS - 2 + 2 * g, SQS - 2 + 2 * g, C_YEL);
            frame_rect(cx + 2, cy + 2, SQS - 4, SQS - 4, C_BG);
        }
    }
    /* the bar: who is ahead */
    {
        int32_t h = 8 * SQS, e = clamp(eval_now, -900, 900), wh = h / 2 + e * (h / 2) / 1000;
        cv_rect(202, BY, 8, h, C_PINK);
        cv_rect(202, BY + h - wh, 8, wh, C_CYAN);
        cv_rect(201, BY + h / 2, 10, 1, C_INK);
    }
    /* the move number, the move, the pieces still on */
    itos((pos.ply + 2) / 2, num);
    cv_text(215, BY, &FONT_T, "MOVE", C_GHOST);
    cv_text(215, BY + 12, &FONT_XS, num, C_INK);
    {
        int32_t wm = 0, bm = 0, s;
        for (s = 0; s < 128; s++)
            if (ONB(s) && pos.b[s] && pos.b[s] != KING && pos.b[s] != -KING)
                *(pos.b[s] > 0 ? &wm : &bm) += 1;
        itos(wm, num);
        cv_text(215, BY + 40, &FONT_T, "LEFT", C_GHOST);
        cv_text(215, BY + 52, &FONT_XS, num, C_CYAN);
        itos(bm, num);
        cv_text(215, BY + 66, &FONT_XS, num, C_PINK);
    }
    {                                    /* the knobs, small */
        static const char *const KN[4] = {"BPM", "DENS", "EVERY", "OOPS"};
        int32_t v[4];
        v[0] = st.tempo, v[1] = st.density, v[2] = st.move_every, v[3] = st.blunder;
        for (i = 0; i < 4u; i++) {
            cv_text(214, BY + 100 + (int32_t)i * 23, &FONT_T, KN[i], C_GHOST);
            itos(v[i], num);
            cv_text(214, BY + 110 + (int32_t)i * 23, &FONT_T, num, i == 3u && st.blunder ? C_YEL : C_PURP);
        }
    }
    /* the commentary, or the result */
    if (st.over) {
        cv_rect(BX + 16, BY + 72, 8 * SQS - 32, 48, RGB(11, 8, 20));
        frame_rect(BX + 16, BY + 72, 8 * SQS - 32, 48, C_YEL);
        text_c(BX + 4 * SQS, BY + 78, &FONT_M, result, C_YEL);
        text_c(BX + 4 * SQS, BY + 102, &FONT_T, st.vs ? "HOME: MENU" : "A NEW GAME IN A MOMENT", C_GHOST);
    }
    if (your_turn()) {                   /* what the keys do now */
        cv_text(6, 222, &FONT_XS, you.from >= 0 ? "WHERE TO? MOVE, THEN A" : "YOUR MOVE: C E C# D MOVE, A PICKS",
                C_CYAN);
    } else
        cv_text(6, 222, &FONT_XS, comment, st.over ? C_YEL : C_INK);
}

/* the menu: nine games in a grid, a picture each; above, the PLAYERS switch */
static void menu_icon(uint32_t m, int32_t x, int32_t y, int on)
{
    uint32_t ms = plat_ms();
    uint16_t a = on ? C_CYAN : C_GHOST, b = on ? C_PINK : C_LINE2, c = on ? C_YEL : C_LINE2;
    switch (m) {
    case M_CHESS: {
        uint16_t map[2] = {a, on ? RGB(40, 110, 130) : C_LINE2};
        sprite(x + 5, y, SPR[KNIGHT], 10, 3, map);
        break;
    }
    case M_CHECK:
        disc(x + 12, y + 18, 9, a);
        disc(x + 24, y + 12, 9, b);
        disc(x + 24, y + 12, 3, c);
        break;
    case M_FOUR: {
        int i, j;
        for (j = 0; j < 3; j++)
            for (i = 0; i < 4; i++)
                disc(x + 4 + i * 9, y + 6 + j * 9, 3, (i + j) % 3 == 0 ? a : (i == j) ? b : C_LINE2);
        break;
    }
    case M_SOLI: {                       /* the ace of hearts: its corner, a big heart in the middle */
        uint16_t h = on ? C_PINK : C_LINE2;
        int32_t k;
        cv_rect(x + 6, y, 24, 30, on ? C_INK : C_GHOST);
        cv_text(x + 8, y - 2, &FONT_T, "A", h);
        disc(x + 15, y + 15, 4, h);      /* the heart: two lobes, a point */
        disc(x + 22, y + 15, 4, h);
        for (k = 0; k < 7; k++)
            cv_rect(x + 12 + k, y + 17 + k, 14 - 2 * k, 1, h);
        break;
    }
    case M_CUBE: {                       /* CUBES: a gem among four of its colour, about to pop */
        uint32_t ph = (ms / 700u) % 4u, pc = on ? 1u + ph : 0u;
        uint16_t col = on ? CUBE_COL[pc] : C_GHOST;
        int pop = on && (ms % 700u) > 520u;
        cube(x + 2, y + 15, pop ? C_INK : col);
        cube(x + 16, y + 15, pop ? C_INK : col);
        cube(x + 2, y + 1, pop ? C_INK : col);
        if (on)
            cu_gem(x + 16, y + 1, CUBE_COL[pc], ms, 0);
        else
            cube(x + 16, y + 1, C_LINE2);
        break;
    }
    case M_2048:
        cv_rect(x + 2, y + 2, 32, 26, on ? deg_col(10) : C_LINE2);
        text_c(x + 18, y + 8, &FONT_T, "2048", C_BG);
        break;
    case M_SNAKE: {                      /* the sand worm: rings along a curve, its maw open */
        int i;
        static const int8_t P[6][2] = {{30, 20}, {24, 22}, {18, 21}, {12, 17}, {7, 12}, {3, 7}};
        for (i = 5; i >= 0; i--) {
            int32_t r = i == 0 ? 6 : 5 - i / 2;
            disc(x + P[i][0], y + P[i][1], r, on ? ((i & 1) ? RGB(196, 140, 110) : RGB(176, 122, 96)) : C_GHOST);
            if (i)
                disc(x + P[i][0], y + P[i][1], r - 2, on ? deg_col(i * 2) : C_LINE2);
        }
        disc(x + 31, y + 20, 3, on ? RGB(40, 12, 20) : C_LINE2);
        break;
    }
    case M_DICE:                         /* a die, rolling to a new face now and then */
        if (on)
            dc_die(x + 5, y + 2, 26, 1u + (ms / 700u * 2654435761u >> 29) % 6u, C_PINK);
        else                             /* (dim, in the menu's purple, as the others) */
            dc_die_in(x + 5, y + 2, 26, 5u, RGB(18, 13, 32), C_GHOST, C_LINE2);
        break;
    case M_MINES:                        /* DEFUSE: a bomb, its fuse lit */
        disc(x + 16, y + 18, 10, on ? RGB(40, 34, 60) : C_GHOST);
        disc(x + 16, y + 18, 9, on ? RGB(70, 62, 100) : C_LINE2);
        disc(x + 12, y + 14, 3, on ? RGB(170, 160, 200) : C_GHOST);
        cv_rect(x + 22, y + 5, 3, 5, on ? RGB(200, 170, 120) : C_GHOST);
        cv_line(x + 24, y + 5, x + 27, y + 2, on ? RGB(200, 170, 120) : C_GHOST);
        if (on)
            disc(x + 28, y + 1, (ms / 90u) & 1u ? 3 : 2, (ms / 60u) & 1u ? C_YEL : RGB(255, 120, 40));
        break;
    case M_LIGHTS: {
        int i;
        for (i = 0; i < 9; i++)
            cv_rect(x + 4 + (i % 3) * 10, y + 2 + (i / 3) * 10, 8, 8,
                    on && ((ms / 300u + (uint32_t)i) % 3u == 0u) ? C_YEL : (i % 2) ? b : C_LINE2);
        break;
    }
    case M_BRICK: {
        int i;
        for (i = 0; i < 6; i++)
            cv_rect(x + (i % 3) * 12, y + 2 + (i / 3) * 6, 11, 5, on ? deg_col(i * 2) : C_LINE2);
        disc(x + 18, y + 20, 2, c);
        cv_rect(x + 10, y + 27, 16, 3, a);
        break;
    }
    case M_SWARM: {                      /* SWARM: your rocket, big, its flame lit when picked */
        int32_t cx = x + 18, top = y - 2, fl = on ? 4 + (int32_t)((ms / 60u) % 3u) * 2 : 0;
        uint16_t hull = on ? C_CYAN : C_GHOST, nose = on ? C_INK : C_GHOST;
        if (on) {                        /* the flame */
            cv_rect(cx - 4, top + 26, 9, fl, (ms / 50u) & 1u ? C_YEL : RGB(255, 150, 60));
            cv_rect(cx - 2, top + 26, 5, fl - 2, C_INK);
        }
        cv_rect(cx - 2, top, 5, 12, nose);                    /* the nose */
        cv_rect(cx - 8, top + 12, 17, 12, hull);              /* the body */
        cv_rect(cx - 14, top + 20, 6, 8, hull);               /* the fins */
        cv_rect(cx + 9, top + 20, 6, 8, hull);
        cv_rect(cx - 4, top + 14, 4, 4, on ? C_BG : RGB(30, 22, 52));   /* its window */
        break;
    }
    case M_DRIFT: {                      /* DRIFT: just the ship, nose up and right, its flame lit when picked */
        float c = 0.7071f, s = -0.7071f;
        int32_t cx = x + 17, cy = y + 15, j;
        int32_t nx = cx + (int32_t)(c * 14.0f), ny = cy + (int32_t)(s * 14.0f);
        int32_t lx = cx + (int32_t)(-c * 10.0f + s * 10.0f), ly = cy + (int32_t)(-s * 10.0f - c * 10.0f);
        int32_t rx = cx + (int32_t)(-c * 10.0f - s * 10.0f), ry = cy + (int32_t)(-s * 10.0f + c * 10.0f);
        int32_t bx = cx - (int32_t)(c * 4.0f), by = cy - (int32_t)(s * 4.0f);
        if (on) {                        /* the flame */
            int32_t fl = 7 + (int32_t)((ms / 40u) % 3u) * 2;
            cv_line(bx, by, bx - (int32_t)(c * (float)fl), by - (int32_t)(s * (float)fl), C_YEL);
            cv_line(bx - 1, by, bx - 1 - (int32_t)(c * (float)(fl - 3)), by - (int32_t)(s * (float)(fl - 3)), C_PINK);
        }
        for (j = 0; j <= 14; j++) {      /* the hull, fanned from the nose (as in the game) */
            int32_t ex = j <= 7 ? lx + (bx - lx) * j / 7 : bx + (rx - bx) * (j - 7) / 7;
            int32_t ey = j <= 7 ? ly + (by - ly) * j / 7 : by + (ry - by) * (j - 7) / 7;
            cv_line(nx, ny, ex, ey, on ? (j == 7 ? RGB(60, 150, 200) : C_CYAN) : (j == 7 ? C_LINE2 : C_GHOST));
        }
        if (on)
            disc(cx + 2, cy - 2, 2, C_PINK);   /* the cockpit */
        break;
    }
    case M_PINBALL: {                    /* two flippers and the shot between: flapping, spinning, highlighted */
        float fa = on && (ms / 300u) % 3u == 0u ? -0.4f : 0.45f, fb = on && (ms / 300u) % 3u == 1u ? 3.54f : 2.69f;
        uint16_t fc = on ? C_CYAN : C_GHOST;
        int32_t k;
        for (k = 0; k <= 11; k++) {
            cv_rect(x + 3 + (int32_t)(fm_cosf(fa) * (float)k), y + 22 + (int32_t)(fm_sinf(fa) * (float)k), 3, 3, fc);
            cv_rect(x + 31 + (int32_t)(fm_cosf(fb) * (float)k), y + 22 + (int32_t)(fm_sinf(fb) * (float)k), 3, 3, fc);
        }
        if (on) {                        /* the steel ball, bouncing */
            int32_t by = y + 7 + (int32_t)(fm_fabsf(fm_sinf((float)ms * 0.004f)) * -4.0f);
            disc(x + 17, by, 5, RGB(180, 186, 200));
            disc(x + 15, by - 2, 2, C_INK);
        } else
            disc(x + 17, y + 7, 4, C_LINE2);
        break;
    }
    case M_GOLF: {                       /* the cup, the flag waving, the ball rolling up to it */
        int32_t k, roll = on ? (int32_t)((ms / 40u) % 40u) : 20;
        cv_rect(x + 2, y + 24, 32, 6, on ? RGB(46, 150, 70) : C_LINE2);
        disc(x + 26, y + 26, 3, on ? RGB(10, 10, 14) : C_BG);
        cv_rect(x + 26, y + 2, 1, 24, on ? C_INK : C_GHOST);
        for (k = 0; k < 8; k++)
            cv_rect(x + 27 + k, y + 2 + k / 2 + (on ? (int32_t)(fm_sinf((float)ms * 0.01f + (float)k * 0.6f) * 1.2f) : 0), 1,
                    8 - k, on ? C_PINK : C_GHOST);
        disc(x + 2 + (on ? roll * 22 / 40 : 6), y + 24, 3, on ? C_INK : C_GHOST);
        break;
    }
    case M_DROP: {                       /* a slope, a ball tumbling down it, the floor's bars lighting */
        int32_t k, q = on ? (int32_t)((ms / 30u) % 40u) : 14, bxp = x + 6 + q * 22 / 40, byp = y + 2 + q * 18 / 40;
        for (k = 0; k < 12; k++)
            cv_rect(x + 4 + k * 2, y + 12 + k / 2, 2, 2, on ? deg_col(4) : C_LINE2);
        for (k = 0; k < 4; k++)
            cv_rect(x + 2 + k * 8, y + 26, 7, 4, on && q / 10 == k ? C_INK : on ? deg_col(k * 2) : C_LINE2);
        disc(bxp, byp, 3, on ? C_YEL : C_GHOST);
        break;
    }
    case M_BOUNCE: {                     /* DODGE: the rolling ball in its box of tiles, a ball bouncing past it */
        int32_t k, t = (int32_t)(ms / 25u), px = t % 48, py = t % 36;
        px = px < 24 ? px : 48 - px;
        py = py < 18 ? py : 36 - py;
        for (k = 0; k < 4; k++) {
            cv_rect(x + 2 + k * 8, y + 2, 7, 2, on ? deg_col(k * 2) : C_LINE2);
            cv_rect(x + 2 + k * 8, y + 28, 7, 2, on ? deg_col(k * 2 + 1) : C_LINE2);
        }
        cv_rect(x + 2, y + 5, 2, 22, on ? deg_col(5) : C_LINE2);
        cv_rect(x + 32, y + 5, 2, 22, on ? deg_col(6) : C_LINE2);
        {                                /* the rolling ball, its stripe going round */
            float rl = on ? (float)ms * 0.006f : 0.0f;
            disc(x + 18, y + 16, 8, on ? C_CYAN : C_GHOST);
            cv_rect(x + 18 + (int32_t)(fm_sinf(rl) * 5.0f) - 1, y + 9, 3, 15, on ? C_PINK : C_LINE2);
            disc(x + 15, y + 13, 2, on ? C_INK : C_LINE2);
        }
        if (on)
            disc(x + 7 + px, y + 7 + py, 2, C_PINK);
        break;
    }
    }
}
static void menu_scene(void)
{
    uint32_t i;
    int32_t x;
    x = cv_text(10, 4, &FONT_M, "MIGHTY", C_CYAN);
    cv_text(x + 6, 4, &FONT_M, "ARCADE", C_PINK);
    {                                    /* the PLAYERS switch */
        int on = menu_at == 0;
        cv_rect(196, 6, 38, 22, on ? RGB(34, 26, 58) : RGB(18, 13, 32));
        if (on)
            frame_rect(195, 5, 40, 24, C_YEL);
        text_c(215, 9, &FONT_B, players == 2 ? "2P" : "1P", players == 2 ? C_PINK : C_CYAN);
    }
    {
    uint32_t pg = menu_at ? (menu_at - 1u) / 9u : 0u, npg = (NMODE - 1u + 8u) / 9u;
    for (i = 1 + pg * 9u; i < NMODE && i <= pg * 9u + 9u; i++) {
        int32_t gx = 6 + (int32_t)((i - 1) % 3u) * 77, gy = 36 + (int32_t)((i - 1) % 9u / 3u) * 62;
        int on = i == menu_at, dim = players == 2 && solo_game(i);
        uint32_t mq = plat_ms() - menu_moved_at;
        int32_t pop = on && menu_moved_at && mq < 160u ? (int32_t)(160u - mq) / 40 : 0;   /* it pops as you land on it */
        int32_t bob = on ? (int32_t)(fm_sinf((float)plat_ms() * 0.008f) * 2.0f) : 0;   /* and its picture bobs */
        cv_rect(gx - pop, gy - pop, 74 + 2 * pop, 58 + 2 * pop,
                on ? mix565(RGB(34, 26, 58), RGB(70, 50, 110), 128 + (int32_t)(fm_sinf((float)plat_ms() * 0.004f) * 100.0f))
                   : RGB(18, 13, 32));
        if (on) {
            frame_rect(gx - 1 - pop, gy - 1 - pop, 76 + 2 * pop, 60 + 2 * pop, C_YEL);
            frame_rect(gx - 2 - pop, gy - 2 - pop, 78 + 2 * pop, 62 + 2 * pop, C_YEL);
            cv_rect(gx + 2, gy + 56 + pop, 70, 2, RGB(8, 5, 16));
        }
        menu_icon(i, gx + 19, gy + 4 + bob - pop, on && !dim);
        text_c(gx + 37, gy + 40, &FONT_T, MODE_NAME[i], dim ? C_LINE2 : on ? C_INK : C_PURP);
    }
    for (i = 0; i < npg; i++)            /* the pages: a dot each */
        disc(120 - (int32_t)(npg - 1u) * 6 + (int32_t)i * 12, 228, 3, i == pg ? C_YEL : C_LINE2);
    if (pg + 1u < npg)
        text_r(234, 223, &FONT_T, "MORE >", C_GHOST);
    if (pg > 0u)
        cv_text(6, 223, &FONT_T, "< BACK", C_GHOST);
    {                                    /* the scale and key every game plays in (PRESET, ALGORITHM): in the corner
                                          * the page arrows leave free */
        char t[16];
        uint32_t q = plat_ms() - menu_scale_at;
        uint16_t c = menu_scale_at && q < 1500u ? mix565(C_GHOST, C_YEL, 255 - (int32_t)q * 255 / 1500) : C_GHOST;
        scale_name(t);
        if (pg > 0u)
            text_r(234, 223, &FONT_T, t, c);
        else
            cv_text(6, 223, &FONT_T, t, c);
    }
    }
}

static void scene(void)
{
    switch (mode) {
    case M_CHESS: chess_scene(); break;
    case M_SOLI: so_scene(); break;
    case M_CUBE: cu_scene(); break;
    case M_2048: tw_scene(); break;
    case M_SNAKE: sn_scene(); break;
    case M_DICE: dc_scene(); break;
    case M_MINES: mi_scene(); break;
    case M_CHECK: ck_scene(); break;
    case M_FOUR: f_scene(); break;
    case M_LIGHTS: li_scene(); break;
    case M_BRICK: br_scene(); break;
    case M_SWARM: sw_scene(); break;
    case M_DRIFT: dr_scene(); break;
    case M_PINBALL: pb_scene(); break;
    case M_GOLF: gf_scene(); break;
    case M_DROP: dp_scene(); break;
    case M_BOUNCE: bx_scene(); break;
    default: menu_scene(); break;
    }
    turns_scene();
}

/* ------------------------------------------------------------------- boot --- */
void app_init(void)
{
    eng_init();
    if (plat_cpu2_set(1) == 0)
        eng_cpu2(plat_cpu2_run, plat_cpu2_wait);
    st.scale = 0;
    st.vs = 1;                           /* you against the computer, at its easiest: it looks one move ahead */
    st.depth = 1;
    st.move_every = 1;
    st.density = 50;
    st.blunder = 4;
    st.tempo = 76;
    fx();
    dc_init();
    kr_s ^= 0x5bd1e995u;
    chess_sounds();
    game_new();
    enter(M_MENU);
}

static void chess_sounds(void)
{
    k_slot_set(SL_PAWN, &KSND[S_NYLON]);
    k_slot_set(SL_KNIGHT, &KSND[S_MARIMBA]);
    k_slot_set(SL_BISHOP, &KSND[S_CELESTA]);
    k_slot_set(SL_ROOK, &KSND[S_TINES]);
    k_slot_set(SL_QUEEN, &KSND[S_GLASSPAD]);
    k_slot_set(SL_KING, &KSND[S_DRONE]);
    k_slot_set(SL_MOVE, &KSND[S_VIBES]);
    k_slot_set(SL_SWELL, &KSND[S_BRASSPAD]);
    k_slot_set(SL_PING, &KSND[S_SONAR]);
    k_slot_set(SL_BELLS, &KSND[S_TUBE]);
    k_slot_set(SL_PLAY, &KSND[S_DREAM]);
    {                                    /* the tails short enough that ten voices go round (the reverb carries on) */
        static const float REL[11] = {0.5f, 0.6f, 0.9f, 0.9f, 1.4f, 1.6f, 0.9f, 1.2f, 1.2f, 2.0f, 1.2f};
        uint32_t i;
        for (i = 0; i < 11u; i++)
            k_slot_rel(i, REL[i]);
    }
    k_main_slot = SL_QUEEN;
}

void app_ui_init(void)
{
    touch_on(1);
    lcd_fill(0, 0, 240, 240, C_BG);
    next_step_t = k_at(0.3f);
}

void app_frame(void)
{
    uint32_t now = plat_ms();
    input();
    ann_tick(now);
    if (mode != M_CHESS)
        groove_run();
    if (!juice_frozen())                 /* (a hit-stop: the game holds still; the screen and the music go on) */
    switch (mode) {
    case M_CUBE: cu_tick(); break;
    case M_SNAKE: if (!(players == 2 && tp.phase)) sn_tick(); break;
    case M_MINES: mi_tick(); break;
    case M_CHECK: ck_tick(); break;
    case M_FOUR: f_tick(); break;
    case M_BRICK: if (!(players == 2 && tp.phase)) br_tick(); break;
    case M_SWARM: if (!(players == 2 && tp.phase)) sw_tick(); break;
    case M_DRIFT: if (!(players == 2 && tp.phase)) dr_tick(); break;
    case M_PINBALL: pb_tick(); break;
    case M_GOLF: gf_tick(); break;
    case M_DROP: dp_tick(); break;
    case M_BOUNCE: bx_tick(); break;
    default: break;
    }
    turns_tick();
    if (mode == M_CHESS && thought.busy && ct_step(&thought, 800u)) {   /* a slice of thinking a frame */
        if (thought.bi >= 0) {
            pending = thought.m[thought.bi];
            pending_ok = 1;
        }
    }
    if (mode == M_CHESS && !st.paused) {
        while ((int32_t)(next_step_t - k_at(0.25f)) < 0) {   /* a quarter second ahead */
            step_at(next_step_t);
            music_layer(next_step_t, step_no * 2u, step_samples() / 2u);   /* (its steps: eighths) */
            next_step_t += step_samples();
            step_no++;
        }
        if (apply_t && (int32_t)(k_now() - apply_t) >= 0) {
            apply_t = 0;
            move_apply();
        }
        if (st.over && !st.vs && (int32_t)(k_now() - over_t) >= 0)
            game_new();
    }
    if (now - ui.drawn_at >= 33u) {
        ui.drawn_at = now;
        screen_draw(scene, C_BG);
    }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "ply"))
        *got = pos.ply;
    else if (!strcmp(what, "over"))
        *got = st.over;
    else if (!strcmp(what, "active"))
        *got = (int)eng_active();
    else if (!strcmp(what, "vs"))
        *got = st.vs;
    else if (!strcmp(what, "cur"))
        *got = you.cur;
    else if (!strcmp(what, "from"))
        *got = you.from;
    else if (!strcmp(what, "mode"))
        *got = mode;
    else if (!strncmp(what, "menu_to_", 8) || !strncmp(what, "mode_is_", 8)) {   /* (tests, by the game's name, its
                                          * spaces as _: menu_to_ puts the menu's cursor on it; mode_is_: 1 if it's
                                          * the one being played. So the tests don't care where the menu has it) */
        int m, i;
        *got = 0;
        for (m = 1; m < NMODE; m++) {
            const char *nm = MODE_NAME[m], *w = what + 8;
            for (i = 0; nm[i] && w[i] && (nm[i] == w[i] || (nm[i] == ' ' && w[i] == '_')); i++)
                ;
            if (!nm[i] && !w[i])
                break;
        }
        if (m < NMODE) {
            if (what[1] == 'e' && mode == M_MENU)
                menu_at = (uint8_t)m, *got = 1;
            else if (what[1] == 'o')
                *got = mode == m;
        }
    }

    else if (!strcmp(what, "depth"))
        *got = st.depth;
    else if (!strcmp(what, "so_moves"))
        *got = so.moves;
    else if (!strcmp(what, "so_found"))
        *got = so.nf[0] + so.nf[1] + so.nf[2] + so.nf[3];
    else if (!strcmp(what, "so_held"))
        *got = so.held_area;
    else if (!strcmp(what, "cu_bombs"))
        *got = cu.bombs;
    else if (!strcmp(what, "cu_gems"))
        *got = cu.gems;
    else if (!strcmp(what, "cu_chain"))
        *got = cu.best_chain;
    else if (!strcmp(what, "cu_level"))
        *got = (int)cu.level;
    else if (!strcmp(what, "cu_bomb")) {     /* (a test: a bomb next) */
        cu.bombs = 1;
        *got = 1;
    }
    else if (!strcmp(what, "cu_score"))
        *got = (int)cu.score;
    else if (!strcmp(what, "cu_lines"))
        *got = (int)cu.lines;
    else if (!strncmp(what, "cu_fill", 7)) {   /* cu_fillN: N rows full and cleared (a test) */
        cu_test_fill(atoi(what + 7));
        *got = 0;
    } else if (!strcmp(what, "cu_harm"))
        *got = cu_harm;
    else if (!strcmp(what, "pb_combo"))
        *got = pb.combo;
    else if (!strcmp(what, "pb_lit"))
        *got = pb.lit[0] + pb.lit[1] + pb.lit[2];
    else if (!strcmp(what, "pb_ball_y"))
        *got = (int)pb.b[0].y;
    else if (!strcmp(what, "pb_ball_x"))
        *got = (int)pb.b[0].x;
    else if (!strncmp(what, "pb_fire", 7)) {   /* (a test: ball 0 up a lane: pb_fire1 the stairs, pb_fire3 the orbit,
                                                 * at speed pb_fireN's tenths .. ) */
        pb_ball_t *b = &pb.b[0];
        b->on = 1, b->lane = 0, b->held = 0, b->fly = 0, b->via = 0;
        b->x = what[7] == '1' ? 26.0f : 196.0f;
        b->y = what[7] == '1' ? 296.0f : 326.0f;
        b->vx = 0.0f, b->vy = -(float)atoi(what + 8) / 100.0f;
        pb.armed = 1;
        *got = 1;
    }
    else if (!strcmp(what, "pb_portal")) {   /* (a test: ball 0 dropped onto the portal) */
        pb_ball_t *b = &pb.b[0];
        b->on = 1, b->lane = 0, b->held = 0, b->fly = 0, b->via = 0, b->cool_until = 0;
        b->x = PB_TELE_X, b->y = PB_TELE_Y - 2, b->vx = b->vy = 0.0f;
        *got = 1;
    }
    else if (!strcmp(what, "pb_score"))
        *got = (int32_t)pb.score;
    else if (!strcmp(what, "pb_balls"))
        *got = pb.balls;
    else if (!strcmp(what, "pb_over"))
        *got = pb.over;
    else if (!strcmp(what, "gf_next")) {    /* (a test: on to the next hole) */
        gf.hole = (uint8_t)((gf.hole + 1u) % 9u);
        gf_start_hole();
        *got = gf.hole;
    }
    else if (!strcmp(what, "gf_hole"))
        *got = gf.hole;
    else if (!strcmp(what, "gf_strokes"))
        *got = gf.strokes;
    else if (!strcmp(what, "gf_phase"))
        *got = gf.phase;
    else if (!strcmp(what, "pb_live"))
        *got = pb_live();
    else if (!strcmp(what, "so_tier_now"))   /* the drums SOLITAIRE's music would play now (the menu runs it) */
        *got = so.won ? -1 : so_tier();
    else if (!strcmp(what, "so_tier"))
        *got = so.tier;
    else if (!strcmp(what, "dp_lit"))
        *got = dp_lit_n();
    else if (!strcmp(what, "dp_np"))
        *got = ndrop.np;
    else if (!strcmp(what, "dp_board"))
        *got = ndrop.board;
    else if (!strcmp(what, "dp_score"))
        *got = (int)ndrop.score;
    else if (!strcmp(what, "dp_balls")) {
        int i;
        *got = 0;
        for (i = 0; i < DP_NB; i++)
            *got += ndrop.b[i].on;
    } else if (!strcmp(what, "dp_floor")) {   /* how many of the floor's bars have sounded */
        int i;
        *got = 0;
        for (i = 0; i < 8; i++)
            *got += ndrop.floor_ms[i] != 0;
    } else if (!strcmp(what, "bx_lit"))
        *got = bx_lit_n();
    else if (!strcmp(what, "bx_balls"))
        *got = bbox.nballs;
    else if (!strcmp(what, "bx_score"))
        *got = (int)bbox.score;
    else if (!strcmp(what, "bx_round"))
        *got = bbox.round;
    else if (!strcmp(what, "bx_god")) {      /* (a test: no ball can hit you for a minute) */
        bbox.safe_until = plat_ms() + 60000u;
        *got = 0;
    } else if (!strcmp(what, "bx_hurt")) {   /* (a test: a ball gets you now) */
        bx_hurt(0);
        *got = 0;
    } else if (!strcmp(what, "bx_bars"))
        *got = bbox.bars;
    else if (!strcmp(what, "dp_floorlit"))
        *got = dp_floor_n();
    else if (!strcmp(what, "br_seq") || !strcmp(what, "sw_seq")) {   /* how many columns the playhead has sounded */
        int i, n = what[0] == 'b' ? 10 : 8;
        *got = 0;
        for (i = 0; i < n; i++)
            *got += (what[0] == 'b' ? br.seq_ms[i] : sw.seq_ms[i]) != 0;
    } else if (!strcmp(what, "br_tier"))
        *got = br.tier;
    else if (!strcmp(what, "sw_tier"))
        *got = sw.tier;
    else if (!strcmp(what, "sw_steps"))      /* the flock's wingbeats this wave */
        *got = sw.mq_r;
    else if (!strcmp(what, "sw_left"))       /* bugs still flying */
        *got = sw.n_alive;
    else if (!strcmp(what, "sw_jar"))        /* the jar's slots filled */
        *got = (sw.jar[0] != 0) + (sw.jar[1] != 0) + (sw.jar[2] != 0) + (sw.jar[3] != 0) + (sw.jar[4] != 0) +
               (sw.jar[5] != 0) + (sw.jar[6] != 0) + (sw.jar[7] != 0);
    else if (!strncmp(what, "br_clear", 8)) {   /* br_clearN: N bricks gone at once, from the top (a test) */
        int n = atoi(what + 8), r, c;
        for (r = 0; r < 6 && n > 0; r++)
            for (c = 0; c < 10 && n > 0; c++)
                if (br.b[r][c])
                    br.b[r][c] = 0, n--;
        *got = 0;
    } else if (!strcmp(what, "sw_next")) {   /* (a test: on to the next wave) */
        sw.wave++;
        sw_wave();
        *got = sw.wave;
    } else if (!strncmp(what, "sw_kill", 7)) {  /* sw_killN: N of the swarm caught at once (a test) */
        int n = atoi(what + 7), k;
        for (k = 0; k < SW_NB && n > 0; k++)
            if (sw.b[k].on && sw.n_alive > 1)
                sw_catch(k), n--;
        *got = 0;
    } else if (!strcmp(what, "so_seq")) {   /* how many places the playhead has sounded */
        int i;
        *got = 0;
        for (i = 0; i < 8; i++)
            *got += so.seq_ms[i] != 0;
    } else if (!strncmp(what, "so_home", 7)) {   /* so_homeN: N cards home at once (a test) */
        int n = atoi(what + 7), f;
        for (f = 0; f < 4; f++) {
            so.nf[f] = (uint8_t)clamp(n - 13 * f, 0, 13);
            for (int k = 0; k < so.nf[f]; k++)
                so.found[f][k] = (uint8_t)(f << 4 | (k + 1) | CARD_UP);
        }
        *got = 0;
    } else if (!strcmp(what, "cu_over"))
        *got = cu.over;
    else if (!strcmp(what, "players"))
        *got = players;
    else if (!strcmp(what, "menu_at"))
        *got = menu_at;
    else if (!strcmp(what, "tp_phase"))
        *got = tp.phase;
    else if (!strcmp(what, "tp_turn"))
        *got = tp.turn;
    else if (!strcmp(what, "sn_len"))
        *got = sn.len;
    else if (!strcmp(what, "sn_over"))
        *got = sn.over;
    else if (!strcmp(what, "dr_pushes"))
        *got = dr.pushes;
    else if (!strcmp(what, "dr_sweep"))
        *got = dr.sweep_notes;
    else if (!strcmp(what, "dr_broken"))
        *got = dr.broken;
    else if (!strcmp(what, "dc_rolls"))
        *got = (int)dc_rolls;
    else if (!strcmp(what, "dc_kind"))
        *got = dcs.kind;
    else if (!strcmp(what, "dc_made"))
        *got = dgen.kind;
    else if (!strcmp(what, "dc_locks"))
        *got = dcs.locks;
    else if (!strcmp(what, "dc_nfav"))
        *got = dcs.nfav;
    else if (!strcmp(what, "dc_cursed"))
        *got = dcs.cur.cursed;
    else if (!strcmp(what, "dc_face"))
        *got = dgen.face;
    else if (!strcmp(what, "dc_seed0"))
        *got = (int)(dcs.cur.seed[0] & 0x7FFFFFFF);
    else if (!strcmp(what, "dp_tilt0"))
        *got = ndrop.tilt[0];
    else if (!strcmp(what, "mi_field"))
        *got = mi_field;
    else if (!strcmp(what, "mi_opened"))
        *got = mi.opened;
    else if (!strcmp(what, "mi_over"))
        *got = mi.over;
    else if (!strcmp(what, "tw_score"))
        *got = (int)tw.score;
    else if (!strcmp(what, "ck_side"))
        *got = ck.p.side;
    else if (!strcmp(what, "ck_over"))
        *got = ck.over;
    else if (!strcmp(what, "f_side"))
        *got = fr.side;
    else if (!strcmp(what, "f_over"))
        *got = fr.over;
    else if (!strcmp(what, "eval"))
        *got = eval_now;
    else
        return 0;
    return 1;
}
#endif
